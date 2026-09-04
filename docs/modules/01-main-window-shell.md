# 01 主窗口外壳

## Goal

建立稳定的界面挂载区域，让后续设备选择、数据显示、命令控制和日志模块都有明确位置，同时不提前实现任何设备业务。

## Inputs

- Day 0 的 `QMainWindow` 骨架；
- `dev` 分支中已验证的成品界面区域划分；
- Day 1 只学习布局、对象命名和基础信号槽的教学约束。

## Design

界面分成两栏：左侧连接区使用最大宽度 280 像素的 `QGroupBox`，右侧工作区使用可扩展布局。工作区上部并排放置 visual/control 页签，下部放置只读活动日志。

尚未实现的下拉框、按钮和重新扫描动作全部禁用。唯一启用的行为是“文件 > 退出”，直接把 `QAction::triggered` 连接到 `QWidget::close`。

## Files

- `src/app/MainWindow.ui`：控件、布局、菜单、默认文字和禁用状态；
- `src/app/MainWindow.cpp`：退出信号槽、第一条日志和状态栏提示；
- `docs/lessons/day01-main-window-shell.md`：零基础实操课。

## Replacement Impact

后续阶段只替换占位页内容并逐步启用控件，不需要改变 `main.cpp`。当插件系统完成后，`visualTabs` 和 `controlTabs` 成为动态插件挂载点；设备服务完成后再启用连接区控件。

## Validation

- 使用 Qt 6.11.0、MSVC x64 和全新 `build-teaching-day1` 目录完成 Debug 构建；
- `mcd_app.exe` 启动后持续运行，没有启动即退出；
- UIC 成功生成界面代码，所有课件要求的 objectName 均存在；
- `git diff --check` 通过。

人工操作仍需学习者在 Qt Creator 中确认窗口缩放效果和“文件 > 退出”菜单行为。

## Open Questions

无。配色、图标、插件数据和真实连接状态明确留给后续课程。
