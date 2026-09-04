# MCU Debug Tool：从零复现教学工程

当前分支 `teaching/from-zero` 是可直接编译运行的 Day 0 最小骨架，只包含：

- 一个 CMake 工程；
- 一个 Qt Widgets 主窗口；
- 一个由 Qt Designer 编辑的 `.ui` 文件；
- 一份保留下来的完整产品设计稿。

完整上位机源码保存在 `dev` 分支。学习时不要从 `dev` 复制整块代码，而是按照 `docs/` 中的路线逐层实现，每完成一个阶段就构建、测试并提交一次。

## Day 0 运行目标

在 Qt Creator 中打开根目录的 `CMakeLists.txt`，选择 `Desktop Qt 6.11.0 MSVC2022 64bit` Kit，完成配置后运行 `mcd_app`。看到“MCU Debug Tool 教学骨架”窗口即表示环境和工程骨架正常。

## 分支用途

- `dev`：完整成品和功能基线；
- `teaching/from-zero`：从最小骨架逐步复现；
- 后续阶段建议打标签 `teaching-day-01`、`teaching-day-02`，便于反复切换练习。
