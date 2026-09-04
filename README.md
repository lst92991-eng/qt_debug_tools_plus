# MCU Debug Tool：从零复现教学工程

当前分支 `teaching/from-zero` 是可直接编译运行的 Day 0 最小骨架，只包含：

- 一个 CMake 工程；
- 一个 Qt Widgets 主窗口；
- 一个由 Qt Designer 编辑的 `.ui` 文件；
- 一份保留下来的完整产品设计稿。

完整上位机源码保存在 `dev` 分支。学习时不要从 `dev` 复制整块代码，而是按照 `docs/` 中的路线逐层实现，每完成一个阶段就构建、测试并提交一次。

## 教学入口

- [从这里开始：零基础课程说明](docs/START-HERE.md)
- [Day 0：环境安装与 Qt Creator 首次运行](docs/00-environment-and-qtcreator.md)
- [Day 1：用 Designer 搭出主窗口外壳](docs/lessons/day01-main-window-shell.md)
- [十阶段课程大纲](docs/01-reproduction-roadmap.md)

当前只有 Day 0 达到“零基础可照做”的课件标准。Day 1 到 Day 10 目前是课程大纲，后续必须随着对应代码提交逐课补充完整操作、完整代码、预期结果和排错步骤，不能把大纲当成已完成教程。

## Day 0 运行目标

在 Qt Creator 中打开根目录的 `CMakeLists.txt`，选择 `Desktop Qt 6.11.0 MSVC2022 64bit` Kit，完成配置后运行 `mcd_app`。看到“MCU Debug Tool 教学骨架”窗口即表示环境和工程骨架正常。

## 分支用途

- `dev`：完整成品和功能基线；
- `teaching/from-zero`：从最小骨架逐步复现；
- 后续阶段建议打标签 `teaching-day-01`、`teaching-day-02`，便于反复切换练习。
