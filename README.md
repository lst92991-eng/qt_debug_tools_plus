# MCU Debug Tool：从零复现教学工程

当前分支 `teaching/environment-setup` 是“第一步：环境安装”的固定课件来源。学生通过浏览器或教师发放的文档阅读课件，不克隆本仓库，而是在 Qt Creator 中亲手新建自己的工程。

教师侧保留下面这些最小验证材料，学生不需要下载：

- 一个 CMake 工程；
- 一个 Qt Widgets 主窗口；
- 一个由 Qt Designer 编辑的 `.ui` 文件；
- 一份保留下来的完整产品设计稿。

完整上位机源码保存在 `dev` 分支，课程后续标准答案保存在 `teaching/from-zero`。第一次学习只安装环境并创建一个全新的本地 Qt Widgets 工程，不下载任何现成项目源码。

## 教学入口

- [从这里开始：零基础课程说明](docs/START-HERE.md)
- [第一步：从零安装全部环境并首次运行](docs/00-environment-and-qtcreator.md)
- [十阶段课程大纲](docs/01-reproduction-roadmap.md)

本分支的唯一任务是保存环境安装课件和教师侧验证骨架。学生没有通过全部验收项之前，不创建业务类、不连接硬件、不进入后续课程。

当前状态：安装步骤已经写全，最小工程已在现有开发机验证；仍需在一台未安装 Git、Visual Studio 和 Qt 的干净 Windows 机器或虚拟机上完整走一遍。完成该验证前，本课不标记为最终版。

## 第一步运行目标

学生在 Qt Creator 中选择 `Desktop Qt 6.11.0 MSVC2022 64bit` Kit，通过向导创建 `C:\QtProjects\MCUDebugTool`，完成 Debug 构建并看到空白 `MainWindow`，即表示环境正常。

## 分支用途

- `dev`：完整成品和功能基线；
- `teaching/environment-setup`：固定的零环境安装课件，不要求学生克隆；
- `teaching/from-zero`：课程制作和后续标准答案；
- `teaching-day-00`：环境阶段最小骨架快照。
