#pragma once

#include "core/RingBufferPool.h"
#include "sdk/DataFrame.h"
#include "sdk/IVisualPlugin.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>

class ChannelHub : public QObject {
    Q_OBJECT
public:
    explicit ChannelHub(QObject* parent = nullptr);

    void subscribe(IVisualPlugin* plugin, const QList<quint16>& channels);
    void unsubscribe(IVisualPlugin* plugin);
    void dispatch(const DataFrame& frame, const PoolSnapshot& snapshot);

signals:
    void overflowOccurred(const OverflowEvent& event);

private:
    struct DispatchCursor {
        quint64 nextSeq = 0;
        quint64 observedGeneration = 0;
    };

    void subscribeLocal(IVisualPlugin* plugin, const QList<quint16>& channels);
    void unsubscribeLocal(IVisualPlugin* plugin);
    void drainPendingDispatch();
    void dispatchLocal(const DataFrame& frame, const PoolSnapshot& snapshot);

    // 空订阅列表进入 wildcard，用于 Raw Viewer；指定通道订阅进入 m_subscriptions。
    // dispatch 前会合并成 QSet，避免同一插件因多个通道命中而收到重复帧。
    QHash<quint16, QSet<IVisualPlugin*>> m_subscriptions;
    QSet<IVisualPlugin*> m_wildcardSubscribers;
    QHash<IVisualPlugin*, DispatchCursor> m_cursors;
    QMutex m_pendingMutex;
    DataFrame m_pendingFrame;
    PoolSnapshot m_pendingSnapshot;
    bool m_hasPendingDispatch = false;
    bool m_dispatchDrainQueued = false;
};
