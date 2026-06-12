#pragma once

#include <QHash>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#include <QVector>

#include <deque>

struct TimedSample {
    qint64 timestamp_us = 0;
    quint64 sequence = 0;
    double value = 0.0;
};

struct PoolSnapshot {
    quint64 oldestSeq = 0;
    quint64 newestSeq = 0;
    quint64 generation = 0;
    qint64 oldestTimestampUs = 0;
    qint64 newestTimestampUs = 0;
    int maxSamples = 0;
    int totalSamples = 0;
    int channelCount = 0;
};

struct PoolWriteResult {
    quint64 sequence = 0;
    quint64 droppedSamples = 0;
};

struct RingBuffer {
    std::deque<TimedSample> data;
    int capacity = 0;
    qint64 oldest_ts = 0;
    int sampleCount = 0;

    bool push(TimedSample sample);
    bool popOldest();
    quint64 resizeCapacity(int newCapacity);
    QVector<TimedSample> range(qint64 from_us, qint64 to_us) const;
    QVector<TimedSample> rangeBySequence(quint64 fromSeq, quint64 toSeq) const;
    qint64 newestTimestamp() const;
    qint64 oldestTimestamp() const;
    quint64 oldestSequence() const;
    quint64 newestSequence() const;
};

class RingBufferPool {
public:
    explicit RingBufferPool(int maxSamples = 100'000);

    PoolWriteResult push(quint16 channelIdx, TimedSample sample);
    QVector<TimedSample> replay(quint16 channelIdx, qint64 from_us) const;
    QVector<TimedSample> replayBySequence(quint16 channelIdx, quint64 fromSeq, quint64 toSeq) const;
    qint64 newestTimestamp(quint16 channelIdx) const;
    QList<quint16> activeChannels() const;
    PoolSnapshot snapshot() const;
    void configureCapacity(int maxSamples);
    int maxSamples() const;
    bool saveToFile(const QString& path, QString* errorMessage = nullptr) const;
    bool loadFromFile(const QString& path, QString* errorMessage = nullptr);
    void clear();

private:
    void trimToBudgetLocked(quint64* droppedSamples);
    bool removeOldestSampleLocked();
    PoolSnapshot snapshotLocked() const;
    static int clampedCapacity(int value);

    // 多个 UI 插件可能同时读历史，协议路径会持续写入，因此这里用读写锁保护整池。
    int m_maxSamples = 100'000;
    int m_defaultChannelCapacity = 100'000;
    int m_totalSamples = 0;
    quint64 m_nextSequence = 1;
    quint64 m_generation = 1;
    QHash<quint16, RingBuffer> m_buffers;
    mutable QReadWriteLock m_lock;
};
