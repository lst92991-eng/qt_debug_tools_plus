#include "core/RingBufferPool.h"

#include <QDataStream>
#include <QFile>

#include <algorithm>

namespace {
constexpr quint32 kRingFileMagic = 0x4d434452;
constexpr quint16 kRingFileVersion = 1;
}

void RingBuffer::push(TimedSample sample)
{
    if (capacity <= 0) {
        return;
    }

    if (data.size() != capacity) {
        data.resize(capacity);
    }

    data[head] = sample;
    head = (head + 1) % capacity;
    sampleCount = std::min(sampleCount + 1, capacity);

    // 满缓冲覆盖时，写完后 head 已移动到“当前最老样本”，用它更新历史窗口左边界。
    if (sampleCount == capacity) {
        oldest_ts = data[head].timestamp_us;
    } else if (sampleCount == 1) {
        oldest_ts = sample.timestamp_us;
    }
}

QVector<TimedSample> RingBuffer::range(qint64 from_us, qint64 to_us) const
{
    QVector<TimedSample> result;
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    result.reserve(count);

    // 物理存储是环形的，回放必须从逻辑最老样本开始，保证图表/保存文件时间顺序稳定。
    for (int i = 0; i < count; ++i) {
        const int idx = (head - count + i + data.size()) % data.size();
        const TimedSample sample = data[idx];
        if (sample.timestamp_us >= from_us && (to_us <= 0 || sample.timestamp_us <= to_us)) {
            result.push_back(sample);
        }
    }

    return result;
}

qint64 RingBuffer::newestTimestamp() const
{
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    if (count == 0) {
        return 0;
    }

    const int idx = (head - 1 + data.size()) % data.size();
    return data[idx].timestamp_us;
}

RingBufferPool::RingBufferPool(int defaultCapacity)
    : m_defaultCapacity(defaultCapacity)
{
}

void RingBufferPool::push(quint16 channelIdx, TimedSample sample)
{
    QWriteLocker locker(&m_lock);
    RingBuffer& buffer = m_buffers[channelIdx];
    // 新通道才套用构造时指定的默认容量；已加载/已写入的通道保留自身容量。
    if (buffer.capacity != m_defaultCapacity && buffer.data.isEmpty()) {
        buffer.capacity = m_defaultCapacity;
    }
    buffer.push(sample);
}

QVector<TimedSample> RingBufferPool::replay(quint16 channelIdx, qint64 from_us) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_buffers.constFind(channelIdx);
    if (it == m_buffers.constEnd()) {
        return {};
    }
    return it->range(from_us, 0);
}

qint64 RingBufferPool::newestTimestamp(quint16 channelIdx) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_buffers.constFind(channelIdx);
    return it == m_buffers.constEnd() ? 0 : it->newestTimestamp();
}

QList<quint16> RingBufferPool::activeChannels() const
{
    QReadLocker locker(&m_lock);
    QList<quint16> keys = m_buffers.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

bool RingBufferPool::saveToFile(const QString& path, QString* errorMessage) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QReadLocker locker(&m_lock);
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);
    // 自定义 magic/version 让以后扩展文件格式时能明确拒绝不兼容历史文件。
    out << kRingFileMagic << kRingFileVersion << static_cast<quint32>(m_buffers.size());

    QList<quint16> channels = m_buffers.keys();
    std::sort(channels.begin(), channels.end());
    for (quint16 channel : channels) {
        const RingBuffer& buffer = m_buffers.value(channel);
        const QVector<TimedSample> samples = buffer.range(0, 0);
        out << channel << static_cast<quint32>(buffer.capacity) << static_cast<quint32>(samples.size());
        for (const TimedSample& sample : samples) {
            out << sample.timestamp_us << sample.value;
        }
    }
    return out.status() == QDataStream::Ok;
}

bool RingBufferPool::loadFromFile(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 magic = 0;
    quint16 version = 0;
    quint32 channelCount = 0;
    in >> magic >> version >> channelCount;
    if (magic != kRingFileMagic || version != kRingFileVersion) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported ring buffer file");
        }
        return false;
    }

    QHash<quint16, RingBuffer> loaded;
    for (quint32 i = 0; i < channelCount; ++i) {
        quint16 channel = 0;
        quint32 capacity = 0;
        quint32 sampleCount = 0;
        in >> channel >> capacity >> sampleCount;

        RingBuffer buffer;
        buffer.capacity = static_cast<int>(std::max<quint32>(1, capacity));
        // 通过 push 重建 head/sampleCount/oldest_ts，避免手工恢复环形状态出错。
        for (quint32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            TimedSample sample;
            in >> sample.timestamp_us >> sample.value;
            buffer.push(sample);
        }
        loaded.insert(channel, buffer);
    }

    if (in.status() != QDataStream::Ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to read ring buffer file");
        }
        return false;
    }

    QWriteLocker locker(&m_lock);
    m_buffers = std::move(loaded);
    return true;
}

void RingBufferPool::clear()
{
    QWriteLocker locker(&m_lock);
    m_buffers.clear();
}
