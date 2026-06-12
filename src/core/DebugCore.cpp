#include "core/DebugCore.h"

#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <limits>

DebugCore* DebugCore::instance()
{
    static DebugCore core;
    return &core;
}

DebugCore::DebugCore(QObject* parent)
    : QObject(parent)
    , m_ingestThread(this)
    , m_dispatchThread(this)
    , m_pluginMgr(this)
{
}

DebugCore::~DebugCore()
{
    m_pluginMgr.deactivateAll();
    m_ingestThread.quit();
    m_ingestThread.wait();

    if (m_channelHub.thread() != QThread::currentThread()) {
        QThread* ownerThread = QThread::currentThread();
        QMetaObject::invokeMethod(
            &m_channelHub,
            [this, ownerThread]() {
                m_channelHub.moveToThread(ownerThread);
            },
            Qt::BlockingQueuedConnection);
    }
    m_dispatchThread.quit();
    m_dispatchThread.wait();
}

PluginManager* DebugCore::pluginManager()
{
    return &m_pluginMgr;
}

ChannelHub* DebugCore::channelHub()
{
    return &m_channelHub;
}

RingBufferPool* DebugCore::ringBufferPool()
{
    return &m_ringPool;
}

void DebugCore::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    registerMcuDebugMetaTypes();
    m_ingestThread.setObjectName(QStringLiteral("mcd-ingest"));
    m_dispatchThread.setObjectName(QStringLiteral("mcd-dispatch"));
    m_ingestThread.start();
    m_dispatchThread.start();
    m_channelHub.moveToThread(&m_dispatchThread);
    m_pluginMgr.setWorkerThread(&m_ingestThread);

    connect(&m_pluginMgr, &PluginManager::errorOccurred, this, &DebugCore::errorOccurred);
    connect(&m_channelHub, &ChannelHub::overflowOccurred, this, &DebugCore::overflowOccurred);
    connect(&m_pluginMgr, &PluginManager::physicalActivated, this, [this](IPhysicalPlugin*) {
        wireDataPath();
    });
    connect(&m_pluginMgr, &PluginManager::protocolActivated, this, [this](IProtocolPlugin*) {
        wireDataPath();
    });
    connect(&m_pluginMgr, &PluginManager::physicalDeactivated, this, [this]() {
        QObject::disconnect(m_physicalDataConnection);
        QObject::disconnect(m_physicalErrorConnection);
        m_physicalDataConnection = {};
        m_physicalErrorConnection = {};
    });
}

void DebugCore::publish(const DataFrame& frame)
{
    DataFrame enriched = frame;
    PoolWriteResult aggregateWrite;

    {
        QWriteLocker metadataLocker(&m_metadataLock);
        for (ChannelSample& sample : enriched.channels) {
            // 通道名/单位可能来自协议帧，也可能来自用户编辑的通道表。
            // 优先保留协议实时提供的信息；协议没给时再用用户元数据补齐。
            const QString key = QString::number(sample.index);
            const QVariantMap existing = m_channelMetadata.value(key).toMap();
            if (!existing.isEmpty()) {
                if (sample.name.isEmpty()) {
                    sample.name = existing.value(QStringLiteral("name")).toString();
                }
                if (sample.unit.isEmpty()) {
                    sample.unit = existing.value(QStringLiteral("unit")).toString();
                }
            }

            if (!sample.name.isEmpty() || !sample.unit.isEmpty()) {
                QVariantMap meta;
                meta.insert(QStringLiteral("name"), sample.name);
                meta.insert(QStringLiteral("unit"), sample.unit);
                m_channelMetadata.insert(key, meta);
            }
        }
    }

    for (const ChannelSample& sample : std::as_const(enriched.channels)) {
        // NaN 表示 raw-only 样本，不进入数值蓄水池，避免 Raw Viewer 的二进制流污染图表数据。
        if (!std::isnan(sample.value)) {
            const PoolWriteResult write = m_ringPool.push(sample.index, {enriched.timestamp_us, 0, sample.value});
            aggregateWrite.sequence = std::max(aggregateWrite.sequence, write.sequence);
            aggregateWrite.droppedSamples += write.droppedSamples;
        }
    }

    const PoolSnapshot snapshot = m_ringPool.snapshot();
    enriched.sequence = aggregateWrite.sequence;
    enriched.generation = snapshot.generation;

    if (aggregateWrite.droppedSamples > 0) {
        OverflowEvent event;
        event.stage = QStringLiteral("ring_buffer_pool");
        event.droppedSamples = aggregateWrite.droppedSamples;
        event.timestamp_us = enriched.timestamp_us;
        emit overflowOccurred(event);
    }

    m_channelHub.dispatch(enriched, snapshot);
    emit framePublished(enriched);
}

void DebugCore::sendCommand(const QVariantMap& command)
{
    IProtocolPlugin* protocol = m_pluginMgr.activeProtocol();
    IPhysicalPlugin* physical = m_pluginMgr.activePhysical();
    if (!physical || !protocol) {
        emit errorOccurred(tr("No active physical connection"));
        return;
    }

    const QPointer<IProtocolPlugin> protocolPtr(protocol);
    const QPointer<IPhysicalPlugin> physicalPtr(physical);
    QMetaObject::invokeMethod(
        protocol,
        [this, command, protocolPtr, physicalPtr]() {
            if (!protocolPtr || !physicalPtr || !physicalPtr->isOpen()) {
                emit errorOccurred(tr("No active physical connection"));
                return;
            }

            // 命令路径不进蓄水池；这里只在采集线程内完成编码和设备写入，失败必须显式上报。
            const QByteArray bytes = protocolPtr->encodeCommand(command);
            if (bytes.isEmpty()) {
                emit errorOccurred(tr("Command produced no bytes"));
                return;
            }

            const qint64 written = physicalPtr->write(bytes);
            if (written < 0 || written != bytes.size()) {
                emit errorOccurred(tr("Failed to write all command bytes"));
                return;
            }

            // TX 也发布为 DataFrame，让 Raw Viewer、日志和会话记录能看到完整双向流量。
            DataFrame txFrame;
            txFrame.timestamp_us = currentTimestampMicros();
            txFrame.channels = {{0, std::numeric_limits<double>::quiet_NaN()}};
            txFrame.rawPayload = bytes;
            txFrame.direction = FrameDirection::Transmit;
            txFrame.attributes = command;
            publish(txFrame);

            emit commandSent(bytes);
        },
        Qt::QueuedConnection);
}

QVariantMap DebugCore::channelMetadata() const
{
    QReadLocker locker(&m_metadataLock);
    return m_channelMetadata;
}

void DebugCore::setChannelMetadata(quint16 channel, const QString& name, const QString& unit)
{
    QWriteLocker locker(&m_metadataLock);
    QVariantMap meta;
    meta.insert(QStringLiteral("name"), name);
    meta.insert(QStringLiteral("unit"), unit);
    m_channelMetadata.insert(QString::number(channel), meta);
}

void DebugCore::setChannelMetadata(const QVariantMap& metadata)
{
    QWriteLocker locker(&m_metadataLock);
    m_channelMetadata = metadata;
}

void DebugCore::wireDataPath()
{
    // 每次切换物理层或协议层都先断开旧连接，避免同一批字节被多个旧插件重复解析。
    QObject::disconnect(m_physicalDataConnection);
    QObject::disconnect(m_physicalErrorConnection);
    QObject::disconnect(m_protocolFrameConnection);
    m_physicalDataConnection = {};
    m_physicalErrorConnection = {};
    m_protocolFrameConnection = {};

    IPhysicalPlugin* physical = m_pluginMgr.activePhysical();
    IProtocolPlugin* protocol = m_pluginMgr.activeProtocol();

    if (physical) {
        m_physicalErrorConnection = connect(
            physical, &IPhysicalPlugin::errorOccurred, this, &DebugCore::errorOccurred);
    }

    if (protocol) {
        m_protocolFrameConnection = connect(
            protocol, &IProtocolPlugin::frameParsed, this, &DebugCore::publish, Qt::DirectConnection);
    }

    if (physical && protocol) {
        // 物理插件可能在工作线程发信号，QueuedConnection 保证协议解析回到接收者线程执行。
        m_physicalDataConnection = connect(
            physical, &IPhysicalPlugin::dataReceived,
            protocol, &IProtocolPlugin::feedBytes,
            Qt::QueuedConnection);
    }
}
