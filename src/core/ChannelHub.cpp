#include "core/ChannelHub.h"

#include <utility>

ChannelHub::ChannelHub(QObject* parent)
    : QObject(parent)
{
}

void ChannelHub::subscribe(IVisualPlugin* plugin, const QList<quint16>& channels)
{
    if (!plugin) {
        return;
    }

    // 重新订阅前必须清理旧通道，否则插件切换关注通道后会收到过期通道的数据。
    unsubscribe(plugin);
    if (channels.isEmpty()) {
        m_wildcardSubscribers.insert(plugin);
        return;
    }

    for (quint16 channel : channels) {
        m_subscriptions[channel].insert(plugin);
    }
}

void ChannelHub::unsubscribe(IVisualPlugin* plugin)
{
    if (!plugin) {
        return;
    }

    m_wildcardSubscribers.remove(plugin);
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
        it->remove(plugin);
        if (it->isEmpty()) {
            it = m_subscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

void ChannelHub::dispatch(const DataFrame& frame)
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
        if (plugin) {
            plugin->onChannelData(frame);
        }
    }
}
