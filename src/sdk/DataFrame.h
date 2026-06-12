#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <limits>

enum class FrameDirection {
    Receive,
    Transmit
};

// 单个数值通道样本。raw-only 数据用 NaN 表示“没有可画的数值”，这样 Raw Viewer
// 仍能看到原始负载，而 RingBufferPool/曲线类插件会自然跳过它。
struct ChannelSample {
    quint16 index = 0;
    double value = std::numeric_limits<double>::quiet_NaN();
    QString name;
    QString unit;
};

// 插件之间唯一流通的数据包。协议插件负责填 timestamp/rawPayload/channels；
// DebugCore 只补充通道元数据并分发，不重新解释协议含义。
struct DataFrame {
    // 微秒级 Unix 时间戳，主要用于跨插件显示、历史保存和用户排查日志。
    qint64 timestamp_us = 0;
    QVector<ChannelSample> channels;
    QByteArray rawPayload;
    // RX/TX 会同时进入可视化链路；UI 计数和 Raw Viewer 依赖该字段区分方向。
    FrameDirection direction = FrameDirection::Receive;
    // 轻量扩展区，例如 can_id、dlc、原始文本行等；核心层不依赖具体键名。
    QVariantMap attributes;
};

Q_DECLARE_METATYPE(FrameDirection)
Q_DECLARE_METATYPE(ChannelSample)
Q_DECLARE_METATYPE(DataFrame)
Q_DECLARE_METATYPE(QVector<ChannelSample>)

void registerMcuDebugMetaTypes();
qint64 currentTimestampMicros();
