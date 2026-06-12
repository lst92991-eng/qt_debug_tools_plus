#pragma once

#include "core/ChannelHub.h"
#include "core/PluginManager.h"
#include "core/RingBufferPool.h"
#include "sdk/DataFrame.h"

#include <QObject>
#include <QVariantMap>

class DebugCore : public QObject {
    Q_OBJECT
public:
    static DebugCore* instance();

    PluginManager* pluginManager();
    ChannelHub* channelHub();
    RingBufferPool* ringBufferPool();

    void initialize();
    void publish(const DataFrame& frame);
    void sendCommand(const QVariantMap& command);
    QVariantMap channelMetadata() const;
    void setChannelMetadata(quint16 channel, const QString& name, const QString& unit);
    void setChannelMetadata(const QVariantMap& metadata);

signals:
    void framePublished(const DataFrame& frame);
    void errorOccurred(const QString& message);
    void commandSent(const QByteArray& bytes);

private:
    explicit DebugCore(QObject* parent = nullptr);
    void wireDataPath();

    // DebugCore 是进程内唯一的数据中枢：APP 只和它交互，插件之间不互相持有。
    // 这样重扫插件、切换协议、替换物理层时，断线重连只需要重接这里的三条信号。
    PluginManager m_pluginMgr;
    ChannelHub m_channelHub;
    RingBufferPool m_ringPool;
    QVariantMap m_channelMetadata;
    QMetaObject::Connection m_physicalDataConnection;
    QMetaObject::Connection m_physicalErrorConnection;
    QMetaObject::Connection m_protocolFrameConnection;
    bool m_initialized = false;
};
