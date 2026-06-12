#include "core/ChannelHub.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <utility>

ChannelHub::ChannelHub(QObject* parent)
    : QObject(parent)
{
}

void ChannelHub::subscribe(IVisualPlugin* plugin, const QList<quint16>& channels)
{
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(
            this,
            [this, plugin, channels]() {
                subscribeLocal(plugin, channels);
            },
            Qt::BlockingQueuedConnection);
        return;
    }

    subscribeLocal(plugin, channels);
}

void ChannelHub::subscribeLocal(IVisualPlugin* plugin, const QList<quint16>& channels)
{
    if (!plugin) {
        return;
    }

    // 重新订阅前必须清理旧通道，否则插件切换关注通道后会收到过期通道的数据。
    unsubscribe(plugin);
    if (channels.isEmpty()) {
        m_wildcardSubscribers.insert(plugin);
        m_cursors.insert(plugin, {});
        return;
    }

    for (quint16 channel : channels) {
        m_subscriptions[channel].insert(plugin);
    }
    m_cursors.insert(plugin, {});
}

void ChannelHub::unsubscribe(IVisualPlugin* plugin)
{
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(
            this,
            [this, plugin]() {
                unsubscribeLocal(plugin);
            },
            Qt::BlockingQueuedConnection);
        return;
    }

    unsubscribeLocal(plugin);
}

void ChannelHub::unsubscribeLocal(IVisualPlugin* plugin)
{
    if (!plugin) {
        return;
    }

    m_wildcardSubscribers.remove(plugin);
    m_cursors.remove(plugin);
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
        it->remove(plugin);
        if (it->isEmpty()) {
            it = m_subscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

void ChannelHub::dispatch(const DataFrame& frame, const PoolSnapshot& snapshot)
{
    if (thread() != QThread::currentThread()) {
        bool shouldQueueDrain = false;
        {
            QMutexLocker locker(&m_pendingMutex);
            // 分发线程落后时只保留最新待分发帧，避免 Qt 事件队列退化成第二个大缓存。
            m_pendingFrame = frame;
            m_pendingSnapshot = snapshot;
            m_hasPendingDispatch = true;
            if (!m_dispatchDrainQueued) {
                m_dispatchDrainQueued = true;
                shouldQueueDrain = true;
            }
        }

        if (shouldQueueDrain) {
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    drainPendingDispatch();
                },
                Qt::QueuedConnection);
        }
        return;
    }

    dispatchLocal(frame, snapshot);
}

void ChannelHub::drainPendingDispatch()
{
    while (true) {
        DataFrame frame;
        PoolSnapshot snapshot;
        {
            QMutexLocker locker(&m_pendingMutex);
            if (!m_hasPendingDispatch) {
                m_dispatchDrainQueued = false;
                return;
            }

            frame = std::move(m_pendingFrame);
            snapshot = m_pendingSnapshot;
            m_hasPendingDispatch = false;
        }

        dispatchLocal(frame, snapshot);
    }
}

void ChannelHub::dispatchLocal(const DataFrame& frame, const PoolSnapshot& snapshot)
{
    QSet<IVisualPlugin*> targets = m_wildcardSubscribers;

    // 一个 DataFrame 可以携带多个通道样本，先收集所有目标再分发，保证每个插件每帧只回调一次。
    for (const ChannelSample& sample : frame.channels) {
        const auto it = m_subscriptions.constFind(sample.index);
        if (it != m_subscriptions.constEnd()) {
            targets.unite(*it);
        }
    }

    for (IVisualPlugin* plugin : std::as_const(targets)) {
        if (!plugin) {
            continue;
        }

        DispatchCursor& cursor = m_cursors[plugin];
        if (cursor.observedGeneration != snapshot.generation) {
            cursor.observedGeneration = snapshot.generation;
            cursor.nextSeq = snapshot.oldestSeq;
        }

        if (snapshot.oldestSeq > 0 && cursor.nextSeq > 0 && cursor.nextSeq < snapshot.oldestSeq) {
            OverflowEvent event;
            event.stage = QStringLiteral("channel_dispatch");
            event.skippedSeq = snapshot.oldestSeq - cursor.nextSeq;
            event.timestamp_us = frame.timestamp_us;
            emit overflowOccurred(event);
            cursor.nextSeq = snapshot.oldestSeq;
        }

        // 分发线程不碰 QWidget；真正的可视化入口回到插件所属的 UI 线程执行。
        QPointer<IVisualPlugin> guardedPlugin(plugin);
        QMetaObject::invokeMethod(
            plugin,
            [guardedPlugin, frame]() {
                if (guardedPlugin) {
                    guardedPlugin->onChannelData(frame);
                }
            },
            Qt::QueuedConnection);
        if (snapshot.newestSeq > 0) {
            cursor.nextSeq = snapshot.newestSeq + 1;
        }
    }
}
