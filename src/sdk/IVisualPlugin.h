#pragma once

#include "sdk/DataFrame.h"

#include <QList>
#include <QString>
#include <QWidget>

#define IVisualPlugin_iid "org.mcd.sdk.IVisualPlugin/1.0"

class IVisualPlugin : public QWidget {
    Q_OBJECT
public:
    explicit IVisualPlugin(QWidget* parent = nullptr) : QWidget(parent) {}
    ~IVisualPlugin() override = default;

    // 可视化插件是 QWidget，但数据入口仍是 DataFrame；这样图表、仪表盘、Raw Viewer
    // 可以共享同一条核心分发链路，而不需要了解当前物理连接类型。
    virtual void onChannelData(const DataFrame& frame) = 0;
    // 空列表表示订阅全部通道，适合 Raw Viewer 这类需要观察完整流量的插件。
    virtual QList<quint16> subscribedChannels() = 0;
    // 预留历史回放接口：0 表示不主动要求历史，正数表示从该时间戳之后补数据。
    virtual qint64 historyFrom() = 0;
    virtual QString name() const = 0;
};

Q_DECLARE_INTERFACE(IVisualPlugin, IVisualPlugin_iid)
