#pragma once

#include <QVariantMap>
#include <QWidget>

#define IControlPlugin_iid "org.mcd.sdk.IControlPlugin/1.0"

class IControlPlugin : public QWidget {
    Q_OBJECT
public:
    explicit IControlPlugin(QWidget* parent = nullptr) : QWidget(parent) {}
    ~IControlPlugin() override = default;

    virtual QString name() const = 0;

signals:
    // 控制插件只描述“要发送什么”，真正的协议编码和物理写入由 DebugCore 串起来。
    // 常用键：bytes/hex/text/channel/value/period_ms；未知键会作为 TX attributes 保留。
    void commandGenerated(const QVariantMap& command);
};

Q_DECLARE_INTERFACE(IControlPlugin, IControlPlugin_iid)
