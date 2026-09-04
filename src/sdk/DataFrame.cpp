#include "sdk/DataFrame.h"

#include <QMetaType>

#include <chrono>

void registerMcuDebugMetaTypes()
{
    // 自定义类型只有注册后，才能安全地通过 Qt 的跨线程队列信号传递。
    qRegisterMetaType<FrameDirection>("FrameDirection");
    qRegisterMetaType<ChannelSample>("ChannelSample");
    qRegisterMetaType<DataFrame>("DataFrame");
    qRegisterMetaType<QVector<ChannelSample>>("QVector<ChannelSample>");
}

qint64 currentTimestampMicros()
{
    // 系统时间便于和日志、抓包工具对齐，不用于计算严格单调的超时间隔。
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now().time_since_epoch();
    return static_cast<qint64>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}
