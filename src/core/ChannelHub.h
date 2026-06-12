#pragma once

#include "sdk/DataFrame.h"
#include "sdk/IVisualPlugin.h"

#include <QHash>
#include <QObject>
#include <QSet>

class ChannelHub : public QObject {
    Q_OBJECT
public:
    explicit ChannelHub(QObject* parent = nullptr);

    void subscribe(IVisualPlugin* plugin, const QList<quint16>& channels);
    void unsubscribe(IVisualPlugin* plugin);
    void dispatch(const DataFrame& frame);

private:
    // 空订阅列表进入 wildcard，用于 Raw Viewer；指定通道订阅进入 m_subscriptions。
    // dispatch 前会合并成 QSet，避免同一插件因多个通道命中而收到重复帧。
    QHash<quint16, QSet<IVisualPlugin*>> m_subscriptions;
    QSet<IVisualPlugin*> m_wildcardSubscribers;
};
