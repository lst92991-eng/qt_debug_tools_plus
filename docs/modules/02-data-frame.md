# 02 DataFrame 公共数据

## Goal

定义设备调试链路中唯一的公共帧数据，并用独立测试验证默认值、原始字节、数值通道、扩展属性和时间戳。

## Inputs

- 设备最终会产生原始字节；
- 协议最终会产生一个或多个数值通道；
- 可视化需要同时观察 RX 和 TX；
- 后续跨线程 queued signal 需要 Qt 认识自定义类型。

## Design

`DataFrame` 是没有业务行为的普通结构体。`ChannelSample` 表示一个可绘制数值，默认 NaN 表示只有原始数据、没有数值。`attributes` 只承载少量协议扩展信息，核心层不解释具体键。

`currentTimestampMicros()` 使用系统时间与日志和抓包对齐。它不用于严格超时测量；后续状态机需要单调时钟。

本阶段故意不加入 sequence、generation 和 overflow，它们等 Day 4 出现有界缓冲需求后再添加。

## Files

- `src/sdk/DataFrame.h`：方向、通道样本、公共帧和元类型声明；
- `src/sdk/DataFrame.cpp`：元类型注册和微秒时间戳；
- `tests/sdk/DataFrameTest.cpp`：无 GUI 的行为测试；
- `CMakeLists.txt`：`mcd_sdk`、测试 target 和 CTest 注册；
- `docs/lessons/day02-data-frame.md`：零基础实操课。

## Replacement Impact

后续协议、核心、可视化和历史模块都会依赖这个结构。字段调整必须集中在这里，并同时更新元类型注册和测试，不能让各插件复制自己的帧定义。

## Validation

- 在全新 `build-teaching-day2` 目录完成 Debug 构建；
- `mcd_sdk`、`mcd_app`、`mcd_sdk_data_frame_test` 均生成成功；
- CTest：1/1 通过；
- 主程序启动回归通过；
- 普通 PowerShell 运行 CTest 时，Qt DLL 路径由测试命令局部提供。

## Open Questions

无。缓冲水位和溢出数据明确延后到 Day 4。
