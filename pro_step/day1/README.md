# Day 1 快照说明

## 当前存在的内容

- 可运行的 Qt Widgets 主程序；
- 左侧设备连接区；
- 右侧数据显示、命令控制和活动日志区域；
- 文件/插件菜单和状态栏；
- “文件 > 退出”信号槽。

## 本阶段新增或替换

本阶段只修改 `MainWindow.ui` 和 `MainWindow.cpp`，把 Day 0 占位窗口替换为静态主窗口外壳。

## 有意简化的内容

- 没有 SDK 和插件；
- 没有串口、USB 或 CAN 设备；
- 没有真实数据和命令；
- 连接相关控件保持禁用；
- visual/control 页签只有占位内容。

## 如何学习和测试

1. 从 Git 标签 `teaching-day-01-start` 创建练习分支；
2. 按 [Day 1 完整课件](../../docs/lessons/day01-main-window-shell.md) 操作；
3. 使用 Qt Creator Debug 构建并运行；
4. 检查缩放布局、禁用状态、启动日志和退出菜单；
5. 与完成标签 `teaching-day-01` 比较两个源码文件。
