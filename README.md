# MCU Debug Tool：从零复现教学工程

当前分支 `teaching/environment-setup` 是给完全没有开发环境的学生使用的固定入口。它只保留可直接编译运行的最小骨架：

- 一个 CMake 工程；
- 一个 Qt Widgets 主窗口；
- 一个由 Qt Designer 编辑的 `.ui` 文件；
- 一份保留下来的完整产品设计稿。

完整上位机源码保存在 `dev` 分支，课程后续标准答案保存在 `teaching/from-zero`。第一次学习只使用本分支完成环境安装，不进入后续代码。

## 教学入口

- [从这里开始：零基础课程说明](docs/START-HERE.md)
- [第一步：从零安装全部环境并首次运行](docs/00-environment-and-qtcreator.md)
- [十阶段课程大纲](docs/01-reproduction-roadmap.md)

本分支的唯一任务是完成环境安装和最小骨架验证。没有通过全部验收项之前，不创建业务类、不连接硬件、不进入 Day 1。

## 第一步运行目标

在 Qt Creator 中打开根目录的 `CMakeLists.txt`，选择 `Desktop Qt 6.11.0 MSVC2022 64bit` Kit，完成配置后运行 `mcd_app`。看到“MCU Debug Tool 教学骨架”窗口即表示环境和工程骨架正常。

## 分支用途

- `dev`：完整成品和功能基线；
- `teaching/environment-setup`：固定的零环境安装入口；
- `teaching/from-zero`：课程制作和后续标准答案；
- `teaching-day-00`：环境阶段最小骨架快照。
