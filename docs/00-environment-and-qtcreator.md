# Day 0：环境安装与 Qt Creator 首次运行

## 1. 今天的目标

完成下面四件事就停，不进入串口、插件或 CAN FD 代码：

1. 安装 MSVC C++ 编译环境；
2. 安装 Qt 6.11.0、Qt Creator 和必要工具；
3. 让 Qt Creator 正确识别 `MSVC 2022 64-bit` Kit；
4. 编译并运行当前最小骨架，看到“MCU Debug Tool 教学骨架”窗口。

本工程固定使用 Windows x64、Qt 6.11.0、MSVC 2022、C++17 和 CMake。Qt 6.11 官方 Windows 支持表列出的编译器是 MSVC 2022，因此新手环境优先安装 Visual Studio 2022，不以“机器上恰好能编译”为标准。

## 2. 先安装 Visual Studio 2022

安装顺序建议是 Visual Studio 2022 在前、Qt 在后。这样 Qt Creator 首次启动时更容易自动找到编译器。

下载入口：

- [Microsoft：Visual Studio 2022 Release History](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history)
- [Qt 6.11：Windows 支持配置](https://doc.qt.io/qt-6/windows.html)

个人学习可选 Visual Studio 2022 Community；只想安装命令行编译器也可选 Build Tools 2022。为了后续调试方便，本教程建议 Community。

打开 Visual Studio Installer 后：

### 工作负载

必须勾选：

- `使用 C++ 的桌面开发` / `Desktop development with C++`

### 右侧安装详细信息

确认至少包含：

- MSVC v143 VS 2022 C++ x64/x86 生成工具；
- Windows 11 SDK，或仍受支持的 Windows 10 SDK；
- C++ CMake tools for Windows；
- Windows 调试工具，建议保留，Qt Creator 调试时会用到。

不需要为了本项目勾选 .NET、UWP、游戏、Linux、移动开发等其他工作负载。

安装完成后重启一次 Windows。若已经装好 Visual Studio 2022，只需在 Visual Studio Installer 中点“修改”，核对以上项目。

## 3. 安装 Qt 6.11.0

Qt 官方建议桌面新用户使用图形化 Online Installer；它可以交互选择版本、模块和工具，安装后也能通过 Maintenance Tool 增删组件。

官方入口：

- [Qt：Get and Install Qt](https://doc.qt.io/qt-6/get-and-install-qt.html)
- [Qt：Online Installer 安装步骤](https://doc.qt.io/qt-6/qt-online-installation.html)

### 安装步骤

1. 注册或登录 Qt Account；
2. 下载 Windows x64 的 Qt Online Installer；
3. 安装目录建议设为 `D:\App\Qt`；没有 D 盘可用 `C:\Qt`，但后续路径要相应替换；
4. 安装类型选择 `Custom Installation` / `自定义安装`；
5. 在组件树中按下面表格勾选。

### 必选和建议组件

| 组件树中的项目 | 选择 | 原因 |
| --- | --- | --- |
| `Qt 6.11.0 > MSVC 2022 64-bit` | 必选 | 本项目的 Qt 库和 ABI 基线 |
| `Qt Serial Port` | 必选 | Day 5 起使用 `QSerialPort` 和 `Qt6::SerialPort` |
| `Qt Creator` | 必选 | 本教程使用的 IDE |
| `CMake` | 建议 | Qt Creator 配置 CMake 工程 |
| `Ninja` | 建议 | Qt Creator 默认且快速的生成器 |
| `Qt Designer` / Designer Tools | 建议 | 可视化编辑 `MainWindow.ui`；部分安装器已随 Qt Creator 提供 |
| Debug Information Files | 可选 | 调试 Qt 内部崩溃时有用，但占空间较大 |
| Sources | 可选 | 需要单步进入 Qt 源码时再装 |

第一轮不勾选：

- MinGW 64-bit；
- MSVC ARM64；
- Android、WebAssembly；
- Qt Quick、Qt Quick 3D、Qt WebEngine；
- Qt Charts；本项目曲线和仪表盘使用 `QPainter` 自绘；
- Qt Design Studio。

注意：不要同时选 `MSVC 2022 64-bit` 和 MinGW 后随意混用。主程序和动态插件必须使用相同架构、编译器 ABI 和 Debug/Release 类型。

### 安装后核对文件

如果安装目录是 `D:\App\Qt`，应能找到：

```text
D:\App\Qt\6.11.0\msvc2022_64\bin\qmake.exe
D:\App\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe
D:\App\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
D:\App\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6SerialPort\Qt6SerialPortConfig.cmake
```

Qt Creator 的实际目录会随安装器版本变化，优先从开始菜单启动，不要照抄某一台电脑的绝对路径。

## 4. 在 Qt Creator 中打开本教学工程

### 4.1 切换到教学分支

在仓库终端执行：

```powershell
git switch teaching/from-zero
git pull
git status
```

预期最后一条显示工作区干净。完整成品在 `dev`，不要在学习过程中把 `dev` 合并进教学分支。

### 4.2 打开工程

1. 启动 Qt Creator；
2. 选择 `文件/File > 打开文件或项目/Open File or Project`；
3. 选择仓库根目录的 `CMakeLists.txt`；
4. 进入“Configure Project/配置项目”页；
5. 只勾选 `Desktop Qt 6.11.0 MSVC2022 64bit`；
6. Build type 选 `Debug`；
7. 构建目录使用源码目录外的影子目录，或仓库下独立的 `build-qtcreator-debug`；
8. 点 `Configure Project`。

不要复用曾被 Visual Studio 生成器或另一套 CMake 配置过的 `build` 目录。生成器、CMake 或 Kit 变化时，直接新建一个构建目录最稳妥。

### 4.3 如果没有正确的 Kit

Qt Creator 官方说明：随 Qt 一起安装时通常能自动识别；如果没有识别，需要在 `编辑/Edit > Preferences > Kits` 手动补齐。

按顺序检查：

1. `Qt Versions`：点 `Add`，选择 `D:\App\Qt\6.11.0\msvc2022_64\bin\qmake.exe`；
2. `Compilers`：确认有 Microsoft Visual C++ x64 编译器；
3. `CMake`：确认存在一个可用 CMake；
4. `Debuggers`：确认有适用于 x64 的 CDB；没有时回 Visual Studio Installer 增加 Windows 调试工具；
5. `Kits`：新建或修正 Desktop Kit，Qt version 选 6.11.0，C/C++ compiler 选 MSVC x64，CMake tool 选已检测到的 CMake，device type 选 Desktop。

Kit 是“设备 + 编译器 + Qt 版本 + 调试器 + 构建工具”的一组固定组合。Qt Creator 的官方 Kit 说明见 [Managing kits](https://doc.qt.io/qtcreator/creator-preferences-kits.html)。

## 5. 第一次构建和运行

在 Qt Creator 左下角确认当前配置是：

```text
项目：mcd_app
Kit：Desktop Qt 6.11.0 MSVC2022 64bit
配置：Debug
```

然后：

1. 点左下角锤子，或按 `Ctrl+B` 构建；
2. 编译输出应以 `Build finished` / 构建完成结束；
3. 点绿色三角，或按 `Ctrl+R` 运行；
4. 出现标题为 `MCU Debug Tool` 的窗口；
5. 中间显示 `MCU Debug Tool 教学骨架` 和 `Day 0：Qt Widgets 工程已经成功运行`。

看到窗口后，Day 0 就完成了。先不要加按钮、串口或插件。

## 6. 想亲手从空白创建同样骨架

当前分支已经提供可对照的骨架。若想完整练一次 Qt Creator 向导，可在仓库外另建练习目录：

1. `文件/File > 新建项目/New Project`；
2. `Application > Qt Widgets Application`；
3. 项目名填 `MCUDebugTool`；
4. Build system 选 `CMake`；
5. Details 中 Class name 填 `MainWindow`；
6. Base class 选 `QMainWindow`；
7. 勾选 `Generate form`，得到 `MainWindow.ui`；
8. Kit 只选 `Desktop Qt 6.11.0 MSVC2022 64bit`；
9. 完成后比较向导生成的四个文件与本分支 `src/app/` 下四个文件。

四个文件的职责：

- `main.cpp`：创建 `QApplication`、主窗口并进入事件循环；
- `MainWindow.h`：声明主窗口类；
- `MainWindow.cpp`：构造窗口并调用 `setupUi()`；
- `MainWindow.ui`：Qt Designer 保存的界面 XML，由 CMake 的 AUTOUIC 自动生成 `ui_MainWindow.h`。

## 7. 常见问题

### `Could not find Qt6Config.cmake`

选错 Kit 或 Qt version 没有注册。回到 `Preferences > Kits > Qt Versions`，添加正确的 `qmake.exe`，不要全局乱加 PATH。

### `No CMAKE_CXX_COMPILER could be found`

Visual Studio C++ 工作负载缺失，或 Kit 没选 MSVC x64。回 Visual Studio Installer 修复“使用 C++ 的桌面开发”。

### `Qt6SerialPort not found`

Day 0 不会出现；Day 5 出现时，运行 Qt 安装目录下的 Maintenance Tool，给 Qt 6.11.0 MSVC 2022 64-bit 增加 Qt Serial Port。

### `generator does not match` 或缓存路径指向旧 Qt

不要在旧构建目录上继续修补。到 Qt Creator 的 Projects 页面换一个新的构建目录，然后重新 Configure。

### 插件能编译但加载失败

通常是主程序与插件混用了 MSVC/MinGW、x64/ARM64 或 Debug/Release。后续每天都只使用同一个 Kit。

## 8. Day 0 验收清单

- [ ] Qt Creator 能打开根目录 `CMakeLists.txt`；
- [ ] Kit 名称明确包含 Qt 6.11.0、MSVC 2022 和 64-bit；
- [ ] Debug 构建无错误；
- [ ] 主窗口能启动；
- [ ] 能双击 `MainWindow.ui` 进入 Designer；
- [ ] `git status` 没有误提交构建目录或 `.user` 文件。
