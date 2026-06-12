#include "core/RingBufferPool.h"

#include <QDataStream>
#include <QFile>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr quint32 kRingFileMagic = 0x4d434452;
constexpr quint16 kRingFileVersion = 2;
constexpr int kMinPoolSamples = 1'000;
constexpr int kMaxPoolSamples = 1'000'000;
}

bool RingBuffer::push(TimedSample sample)
{
    if (capacity <= 0) {
        return false;
    }

    if (static_cast<int>(data.size()) > capacity) {
        resizeCapacity(capacity);
    }

    const bool overwrote = static_cast<int>(data.size()) >= capacity;
    if (overwrote) {
        data.pop_front();
    }
    data.push_back(sample);
    sampleCount = static_cast<int>(data.size());
    oldest_ts = data.empty() ? 0 : data.front().timestamp_us;

    return overwrote;
}

bool RingBuffer::popOldest()
{
    if (sampleCount <= 0 || data.empty()) {
        return false;
    }

    data.pop_front();
    sampleCount = static_cast<int>(data.size());
    oldest_ts = data.empty() ? 0 : data.front().timestamp_us;
    return true;
}

quint64 RingBuffer::resizeCapacity(int newCapacity)
{
    newCapacity = std::max(1, newCapacity);
    QVector<TimedSample> samples = range(0, 0);
    quint64 dropped = 0;
    if (samples.size() > newCapacity) {
        dropped = static_cast<quint64>(samples.size() - newCapacity);
        samples = samples.mid(samples.size() - newCapacity);
    }

    capacity = newCapacity;
    data.clear();
    for (const TimedSample& sample : std::as_const(samples)) {
        data.push_back(sample);
    }
    sampleCount = static_cast<int>(data.size());
    oldest_ts = data.empty() ? 0 : data.front().timestamp_us;
    return dropped;
}

QVector<TimedSample> RingBuffer::range(qint64 from_us, qint64 to_us) const
{
    QVector<TimedSample> result;
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    result.reserve(count);

    for (const TimedSample& sample : data) {
        if (sample.timestamp_us >= from_us && (to_us <= 0 || sample.timestamp_us <= to_us)) {
            result.push_back(sample);
        }
    }

    return result;
}

QVector<TimedSample> RingBuffer::rangeBySequence(quint64 fromSeq, quint64 toSeq) const
{
    QVector<TimedSample> result;
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    result.reserve(count);

    for (const TimedSample& sample : data) {
        if (sample.sequence >= fromSeq && (toSeq == 0 || sample.sequence <= toSeq)) {
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

    return data.back().timestamp_us;
}

qint64 RingBuffer::oldestTimestamp() const
{
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    if (count == 0) {
        return 0;
    }
    return data.front().timestamp_us;
}

quint64 RingBuffer::oldestSequence() const
{
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    if (count == 0) {
        return 0;
    }
    return data.front().sequence;
}

quint64 RingBuffer::newestSequence() const
{
    const int count = std::min(sampleCount, static_cast<int>(data.size()));
    if (count == 0) {
        return 0;
    }
    return data.back().sequence;
}

RingBufferPool::RingBufferPool(int maxSamples)
    : m_maxSamples(clampedCapacity(maxSamples))
    , m_defaultChannelCapacity(clampedCapacity(maxSamples))
{
}

PoolWriteResult RingBufferPool::push(quint16 channelIdx, TimedSample sample)
{
    QWriteLocker locker(&m_lock);
    sample.sequence = m_nextSequence++;

    RingBuffer& buffer = m_buffers[channelIdx];
    if (buffer.capacity != m_defaultChannelCapacity && buffer.data.empty()) {
        buffer.capacity = m_defaultChannelCapacity;
    }
    if (buffer.capacity <= 0) {
        buffer.capacity = m_defaultChannelCapacity;
    }

    const bool overwrote = buffer.push(sample);
    quint64 droppedSamples = overwrote ? 1 : 0;
    if (!overwrote) {
        ++m_totalSamples;
    }

    trimToBudgetLocked(&droppedSamples);
    return {sample.sequence, droppedSamples};
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

QVector<TimedSample> RingBufferPool::replayBySequence(quint16 channelIdx, quint64 fromSeq, quint64 toSeq) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_buffers.constFind(channelIdx);
    if (it == m_buffers.constEnd()) {
        return {};
    }
    return it->rangeBySequence(fromSeq, toSeq);
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

PoolSnapshot RingBufferPool::snapshot() const
{
    QReadLocker locker(&m_lock);
    return snapshotLocked();
}

void RingBufferPool::configureCapacity(int maxSamples)
{
    QWriteLocker locker(&m_lock);
    m_maxSamples = clampedCapacity(maxSamples);
    m_defaultChannelCapacity = m_maxSamples;
    quint64 dropped = 0;
    trimToBudgetLocked(&dropped);
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        // resize 是少量人工配置动作，宁可在这里做一次整理，也不要让旧的大数组长期占着内存。
        dropped += it->resizeCapacity(m_defaultChannelCapacity);
    }
    if (dropped > 0) {
        m_totalSamples = 0;
        for (auto it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it) {
            m_totalSamples += it->sampleCount;
        }
    }
    ++m_generation;
}

int RingBufferPool::maxSamples() const
{
    QReadLocker locker(&m_lock);
    return m_maxSamples;
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
    out << static_cast<quint32>(m_maxSamples) << m_nextSequence << m_generation;

    QList<quint16> channels = m_buffers.keys();
    std::sort(channels.begin(), channels.end());
    for (quint16 channel : channels) {
        const RingBuffer& buffer = m_buffers.value(channel);
        const QVector<TimedSample> samples = buffer.range(0, 0);
        out << channel << static_cast<quint32>(buffer.capacity) << static_cast<quint32>(samples.size());
        for (const TimedSample& sample : samples) {
            out << sample.timestamp_us << sample.sequence << sample.value;
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
    if (magic != kRingFileMagic || (version != 1 && version != kRingFileVersion)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported ring buffer file");
        }
        return false;
    }

    int loadedMaxSamples = m_maxSamples;
    quint64 loadedNextSequence = 1;
    quint64 loadedGeneration = m_generation + 1;
    if (version >= 2) {
        quint32 storedMaxSamples = 0;
        in >> storedMaxSamples >> loadedNextSequence >> loadedGeneration;
        loadedMaxSamples = clampedCapacity(static_cast<int>(storedMaxSamples));
    }

    QHash<quint16, RingBuffer> loaded;
    int loadedTotal = 0;
    quint64 maxSeenSequence = 0;
    for (quint32 i = 0; i < channelCount; ++i) {
        quint16 channel = 0;
        quint32 capacity = 0;
        quint32 sampleCount = 0;
        in >> channel >> capacity >> sampleCount;

        RingBuffer buffer;
        buffer.capacity = std::max(1, static_cast<int>(std::min<quint32>(capacity, kMaxPoolSamples)));
        for (quint32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            TimedSample sample;
            if (version >= 2) {
                in >> sample.timestamp_us >> sample.sequence >> sample.value;
            } else {
                in >> sample.timestamp_us >> sample.value;
                sample.sequence = maxSeenSequence + 1;
            }
            maxSeenSequence = std::max(maxSeenSequence, sample.sequence);
            if (!buffer.push(sample)) {
                ++loadedTotal;
            }
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
    m_totalSamples = loadedTotal;
    m_maxSamples = loadedMaxSamples;
    m_defaultChannelCapacity = loadedMaxSamples;
    m_nextSequence = std::max(loadedNextSequence, maxSeenSequence + 1);
    m_generation = std::max<quint64>(1, loadedGeneration + 1);
    quint64 dropped = 0;
    trimToBudgetLocked(&dropped);
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        it->resizeCapacity(m_defaultChannelCapacity);
    }
    return true;
}

void RingBufferPool::clear()
{
    QWriteLocker locker(&m_lock);
    m_buffers.clear();
    m_totalSamples = 0;
    ++m_generation;
}

void RingBufferPool::trimToBudgetLocked(quint64* droppedSamples)
{
    while (m_totalSamples > m_maxSamples && removeOldestSampleLocked()) {
        if (droppedSamples) {
            ++(*droppedSamples);
        }
    }
}

bool RingBufferPool::removeOldestSampleLocked()
{
    auto oldestIt = m_buffers.end();
    quint64 oldestSeq = std::numeric_limits<quint64>::max();

    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        const quint64 seq = it->oldestSequence();
        if (seq > 0 && seq < oldestSeq) {
            oldestSeq = seq;
            oldestIt = it;
        }
    }

    if (oldestIt == m_buffers.end()) {
        return false;
    }

    if (oldestIt->popOldest()) {
        --m_totalSamples;
    }
    if (oldestIt->sampleCount == 0) {
        m_buffers.erase(oldestIt);
    }
    return true;
}

PoolSnapshot RingBufferPool::snapshotLocked() const
{
    PoolSnapshot snap;
    snap.generation = m_generation;
    snap.maxSamples = m_maxSamples;
    snap.totalSamples = m_totalSamples;
    snap.channelCount = m_buffers.size();

    quint64 oldestSeq = std::numeric_limits<quint64>::max();
    for (auto it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it) {
        const RingBuffer& buffer = it.value();
        const quint64 bufferOldestSeq = buffer.oldestSequence();
        const quint64 bufferNewestSeq = buffer.newestSequence();
        if (bufferOldestSeq > 0 && bufferOldestSeq < oldestSeq) {
            oldestSeq = bufferOldestSeq;
        }
        snap.newestSeq = std::max(snap.newestSeq, bufferNewestSeq);

        const qint64 oldestTs = buffer.oldestTimestamp();
        const qint64 newestTs = buffer.newestTimestamp();
        if (oldestTs > 0 && (snap.oldestTimestampUs == 0 || oldestTs < snap.oldestTimestampUs)) {
            snap.oldestTimestampUs = oldestTs;
        }
        snap.newestTimestampUs = std::max(snap.newestTimestampUs, newestTs);
    }

    snap.oldestSeq = oldestSeq == std::numeric_limits<quint64>::max() ? 0 : oldestSeq;
    return snap;
}

int RingBufferPool::clampedCapacity(int value)
{
    return std::clamp(value, kMinPoolSamples, kMaxPoolSamples);
}
