# Day 2 快照说明

## 当前存在的内容

- Day 1 主窗口外壳；
- `mcd_sdk` 静态库；
- `FrameDirection`、`ChannelSample` 和 `DataFrame`；
- Qt 自定义类型注册；
- 微秒级系统时间函数；
- 第一条 CTest 自动测试。

## 本阶段新增或替换

新增 `src/sdk/DataFrame.h/.cpp` 和 `tests/sdk/DataFrameTest.cpp`，并修改 CMake，使主程序和测试共享 `mcd_sdk`。

## 有意简化的内容

- 没有插件接口和插件装载；
- 没有设备、协议或 UI 数据绑定；
- 没有 sequence、generation 和 overflow；
- 测试使用最简单的退出码断言，没有引入额外测试框架。

## 如何学习和测试

1. 从修正后的标签 `teaching-day-02-start-v2` 创建练习分支；
2. 按 [Day 2 完整课件](../../docs/lessons/day02-data-frame.md) 操作；
3. 构建三个 target；
4. 运行 CTest，确认 `sdk_data_frame` 通过；
5. 运行主程序，确认 Day 1 界面没有回归；
6. 与完成标签 `teaching-day-02` 比较四个实现文件。
