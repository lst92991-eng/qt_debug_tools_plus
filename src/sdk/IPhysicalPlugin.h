#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantMap>

#define IPhysicalPlugin_iid "org.mcd.sdk.IPhysicalPlugin/1.0"

class IPhysicalPlugin : public QObject {
    Q_OBJECT
public:
    explicit IPhysicalPlugin(QObject* parent = nullptr) : QObject(parent) {}
    ~IPhysicalPlugin() override = default;

    // 物理插件只负责“字节进出”和连接状态；协议解析、重试策略之外的业务判断
    // 都应留给上层，避免串口/USB 插件被 UI 或协议细节绑死。
    virtual bool open(const QVariantMap& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray& data) = 0;

    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QVariantMap defaultConfig() const = 0;

signals:
    // dataReceived 可能来自插件内部工作线程，DebugCore 会用 QueuedConnection 接到协议层。
    void dataReceived(const QByteArray& rawBytes);
    void errorOccurred(const QString& message);
    void statusChanged(bool connected);
};

Q_DECLARE_INTERFACE(IPhysicalPlugin, IPhysicalPlugin_iid)
