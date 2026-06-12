#pragma once

#include "sdk/DataFrame.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

#define IProtocolPlugin_iid "org.mcd.sdk.IProtocolPlugin/1.0"

class IProtocolPlugin : public QObject {
    Q_OBJECT
public:
    explicit IProtocolPlugin(QObject* parent = nullptr) : QObject(parent) {}
    ~IProtocolPlugin() override = default;

    // 入站只吃物理层原始字节；出站只把 UI/控制插件命令编码成字节。
    // 这里不要直接访问串口/USB，也不要直接更新 UI，保持协议插件可单独替换。
    virtual void feedBytes(const QByteArray& raw) = 0;
    virtual QByteArray encodeCommand(const QVariantMap& command) = 0;

    virtual QString name() const = 0;
    virtual QString version() const = 0;

signals:
    // 解析完成后交给 DebugCore 统一补元数据、入历史缓冲并分发给可视化插件。
    void frameParsed(const DataFrame& frame);
};

Q_DECLARE_INTERFACE(IProtocolPlugin, IProtocolPlugin_iid)
