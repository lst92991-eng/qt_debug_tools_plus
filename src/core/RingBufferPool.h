#pragma once

#include <QHash>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#include <QVector>

struct TimedSample {
    qint64 timestamp_us = 0;
    double value = 0.0;
};

struct RingBuffer {
    QVector<TimedSample> data;
    // head 永远指向下一次写入位置；缓冲满后 head 同时也是最老样本的位置。
    int head = 0;
    int capacity = 1'000'000;
    qint64 oldest_ts = 0;
    int sampleCount = 0;

    void push(TimedSample sample);
    QVector<TimedSample> range(qint64 from_us, qint64 to_us) const;
    qint64 newestTimestamp() const;
};

class RingBufferPool {
public:
    explicit RingBufferPool(int defaultCapacity = 1'000'000);

    void push(quint16 channelIdx, TimedSample sample);
    QVector<TimedSample> replay(quint16 channelIdx, qint64 from_us) const;
    qint64 newestTimestamp(quint16 channelIdx) const;
    QList<quint16> activeChannels() const;
    bool saveToFile(const QString& path, QString* errorMessage = nullptr) const;
    bool loadFromFile(const QString& path, QString* errorMessage = nullptr);
    void clear();

private:
    // 多个 UI 插件可能同时读历史，协议路径会持续写入，因此这里用读写锁保护整池。
    int m_defaultCapacity;
    QHash<quint16, RingBuffer> m_buffers;
    mutable QReadWriteLock m_lock;
};
