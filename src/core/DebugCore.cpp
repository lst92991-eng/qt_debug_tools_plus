#include "core/DebugCore.h"

#include <QMetaObject>

#include <cmath>
#include <limits>

DebugCore* DebugCore::instance()
{
    static DebugCore core;
    return &core;
}

DebugCore::DebugCore(QObject* parent)
    : QObject(parent)
    , m_pluginMgr(this)
    , m_channelHub(this)
{
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
    connect(&m_pluginMgr, &PluginManager::errorOccurred, this, &DebugCore::errorOccurred);
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
        // NaN 表示 raw-only 样本，不进入数值历史，避免 Raw Viewer 的二进制流污染图表数据。
        if (!std::isnan(sample.value)) {
            m_ringPool.push(sample.index, {enriched.timestamp_us, sample.value});
        }
    }

    m_channelHub.dispatch(enriched);
    emit framePublished(enriched);
}

void DebugCore::sendCommand(const QVariantMap& command)
{
    IProtocolPlugin* protocol = m_pluginMgr.activeProtocol();
    IPhysicalPlugin* physical = m_pluginMgr.activePhysical();

    // 控制插件可以直接给 bytes，也可以给协议相关字段；有活动协议时统一走协议编码。
    QByteArray bytes;
    if (protocol) {
        bytes = protocol->encodeCommand(command);
    } else {
        bytes = command.value(QStringLiteral("bytes")).toByteArray();
    }

    if (bytes.isEmpty()) {
        emit errorOccurred(tr("Command produced no bytes"));
        return;
    }

    if (!physical || !physical->isOpen()) {
        emit errorOccurred(tr("No active physical connection"));
        return;
    }

    const qint64 written = physical->write(bytes);
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
}

QVariantMap DebugCore::channelMetadata() const
{
    return m_channelMetadata;
}

void DebugCore::setChannelMetadata(quint16 channel, const QString& name, const QString& unit)
{
    QVariantMap meta;
    meta.insert(QStringLiteral("name"), name);
    meta.insert(QStringLiteral("unit"), unit);
    m_channelMetadata.insert(QString::number(channel), meta);
}

void DebugCore::setChannelMetadata(const QVariantMap& metadata)
{
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
            protocol, &IProtocolPlugin::frameParsed, this, &DebugCore::publish);
    }

    if (physical && protocol) {
        // 物理插件可能在工作线程发信号，QueuedConnection 保证协议解析回到接收者线程执行。
        m_physicalDataConnection = connect(
            physical, &IPhysicalPlugin::dataReceived,
            protocol, &IProtocolPlugin::feedBytes,
            Qt::QueuedConnection);
    }
}
