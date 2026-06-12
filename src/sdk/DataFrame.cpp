#include "sdk/DataFrame.h"

#include <QMetaType>

#include <chrono>

void registerMcuDebugMetaTypes()
{
    // 插件信号会跨线程排队投递，自定义类型必须注册到 Qt 元对象系统。
    qRegisterMetaType<FrameDirection>("FrameDirection");
    qRegisterMetaType<ChannelSample>("ChannelSample");
    qRegisterMetaType<DataFrame>("DataFrame");
    qRegisterMetaType<OverflowEvent>("OverflowEvent");
    qRegisterMetaType<QVector<ChannelSample>>("QVector<ChannelSample>");
}

qint64 currentTimestampMicros()
{
    // 使用系统时间方便和日志、抓包工具对齐；不用于测量严格单调的时间间隔。
    using clock = std::chrono::system_clock;
    const auto now = clock::now().time_since_epoch();
    return static_cast<qint64>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}
