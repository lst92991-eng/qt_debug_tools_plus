# 第一步：从零安装开发环境并首次运行

## 1. 这一步的目标和边界

完成下面六件事就停，不进入串口、插件或 CAN FD 代码：

1. 准备 Windows、账号和磁盘空间；
2. 安装 Git 并完成最基本配置；
3. 安装 MSVC C++ 编译环境；
4. 安装 Qt 6.11.0、Qt Creator 和必要工具；
5. 克隆教学分支并让 Qt Creator 识别 `MSVC 2022 64-bit` Kit；
6. 编译并运行当前最小骨架，看到“MCU Debug Tool 教学骨架”窗口。

本工程固定使用 Windows x64、Qt 6.11.0、MSVC 2022、C++17 和 CMake。Qt 6.11 官方 Windows 支持表列出的编译器是 MSVC 2022，因此新手环境优先安装 Visual Studio 2022，不以“机器上恰好能编译”为标准。

这一课从“电脑上没有任何开发工具”开始。学生不需要预先理解 Git、编译器、Qt、CMake 或 Kit。每安装完一项都要立即验证；前一项验证失败时，不继续安装下一项。

本步骤的结束条件只有一个：学生能在 Qt Creator 中亲自构建并运行最小窗口。界面、串口、插件和 CAN FD 都属于后续课程。

> 验证状态：本文中的工具路径、CMake 配置和最小程序已在现有开发机验证。由于现有开发机原本已经安装开发环境，还必须在一台干净 Windows 机器或虚拟机上，从下载 Git 开始完整执行一次，才能把本课标记为最终完成。

## 2. 准备条件

开始前确认：

- Windows 10 1809 或更高版本，推荐 Windows 11 x64；
- 当前 Windows 账号有管理员权限；
- 至少预留 20 GB 可用空间；
- 网络能访问 GitHub、Qt 和 Microsoft；
- 准备一个 GitHub 账号，用于后续拉取和推送；
- 准备一个 Qt Account，Qt Online Installer 登录时使用。

本教程只讲 Windows x64。不要在第一次学习时同时尝试 Linux、ARM64 或 MinGW。

## 3. 安装 Git

官方入口：[Git for Windows](https://git-scm.com/install/windows)。普通 Intel/AMD Windows 电脑下载 x64 Setup，不选 ARM64 和 Portable。

运行安装器时，大部分页面保留默认值。遇到下面选项时这样选：

| 页面或选项 | 选择 |
| --- | --- |
| Select Components | 保留 Windows Explorer integration 和 Git Credential Manager |
| Choosing the default editor | 可保留默认；这套教程主要在 Qt Creator 编辑代码 |
| Adjusting the name of the initial branch | 选择 `Override...` 并填 `main`，或保留安装器默认 |
| Adjusting your PATH environment | `Git from the command line and also from 3rd-party software` |
| Choosing the SSH executable | `Use bundled OpenSSH` |
| Choosing HTTPS transport backend | `Use the OpenSSL library` |
| Configuring line ending conversions | `Checkout Windows-style, commit Unix-style line endings` |
| Choosing the default behavior of git pull | `Fast-forward or merge` |
| Choosing a credential helper | `Git Credential Manager` |
| Configuring extra options | 保留 file system caching，其余默认 |

安装完成后关闭原来的 PowerShell，再从开始菜单打开一个新的 PowerShell，输入：

```powershell
git --version
```

能看到 `git version 2.x.x` 即安装成功。第一次使用还要设置提交署名，把示例替换为你自己的名字和 GitHub 邮箱：

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的GitHub邮箱"
git config --global --list
```

最后一条输出中应能找到刚才填写的 `user.name` 和 `user.email`。

## 4. 安装 Visual Studio 2022

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

## 5. 安装 Qt 6.11.0

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

## 6. 下载教学工程

GitHub 官方把“克隆”定义为把远程仓库完整复制到本机。即使你以前从未使用 Git，也只需要按下面步骤操作。

1. 在 D 盘创建 `D:\Code` 文件夹；没有 D 盘则创建 `C:\Code`；
2. 双击进入该文件夹；
3. 点击文件资源管理器顶部地址栏，输入 `powershell` 并按 Enter；
4. 在打开的蓝色或黑色窗口中逐行执行：

```powershell
git clone --branch teaching/environment-setup https://github.com/lst92991-eng/qt_debug_tools_plus.git
Set-Location .\qt_debug_tools_plus
git status
```

正常结果应包含：

```text
On branch teaching/environment-setup
Your branch is up to date with 'origin/teaching/environment-setup'.
nothing to commit, working tree clean
```

如果你已经有本仓库，不要再次克隆到同一个目录，改为在原目录执行下一小节的切换命令。GitHub 的通用克隆步骤见 [Cloning a repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository)。

## 7. 在 Qt Creator 中打开本教学工程

### 7.1 已有仓库时切换到教学分支

在仓库终端执行：

```powershell
git switch teaching/environment-setup
git pull
git status
```

预期最后一条显示工作区干净。完整成品在 `dev`，不要在学习过程中把 `dev` 合并进教学分支。

### 7.2 打开工程

1. 启动 Qt Creator；
2. 选择 `文件/File > 打开文件或项目/Open File or Project`；
3. 选择仓库根目录的 `CMakeLists.txt`；
4. 进入“Configure Project/配置项目”页；
5. 只勾选 `Desktop Qt 6.11.0 MSVC2022 64bit`；
6. Build type 选 `Debug`；
7. 构建目录使用源码目录外的影子目录，或仓库下独立的 `build-qtcreator-debug`；
8. 点 `Configure Project`。

不要复用曾被 Visual Studio 生成器或另一套 CMake 配置过的 `build` 目录。生成器、CMake 或 Kit 变化时，直接新建一个构建目录最稳妥。

### 7.3 如果没有正确的 Kit

Qt Creator 官方说明：随 Qt 一起安装时通常能自动识别；如果没有识别，需要在 `编辑/Edit > Preferences > Kits` 手动补齐。

按顺序检查：

1. `Qt Versions`：点 `Add`，选择 `D:\App\Qt\6.11.0\msvc2022_64\bin\qmake.exe`；
2. `Compilers`：确认有 Microsoft Visual C++ x64 编译器；
3. `CMake`：确认存在一个可用 CMake；
4. `Debuggers`：确认有适用于 x64 的 CDB；没有时回 Visual Studio Installer 增加 Windows 调试工具；
5. `Kits`：新建或修正 Desktop Kit，Qt version 选 6.11.0，C/C++ compiler 选 MSVC x64，CMake tool 选已检测到的 CMake，device type 选 Desktop。

Kit 是“设备 + 编译器 + Qt 版本 + 调试器 + 构建工具”的一组固定组合。Qt Creator 的官方 Kit 说明见 [Managing kits](https://doc.qt.io/qtcreator/creator-preferences-kits.html)。

## 8. 第一次构建和运行

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

## 9. 想亲手从空白创建同样骨架

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

## 10. 常见问题

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

### Git 报错连接 `127.0.0.1:7890` 失败

这表示 Git 配置了本地代理，但代理软件没有启动。先检查：

```powershell
git config --global --get http.proxy
git config --global --get https.proxy
```

如果你确定自己不使用代理，可以清除这两项后重试：

```powershell
git config --global --unset http.proxy
git config --global --unset https.proxy
```

如果本来就需要代理，则启动代理软件，不要删除配置。

## 11. Day 0 验收清单

- [ ] `git --version` 能输出版本号；
- [ ] 已设置自己的 Git 用户名和邮箱；
- [ ] 已克隆并进入 `teaching/environment-setup` 分支；
- [ ] Qt Creator 能打开根目录 `CMakeLists.txt`；
- [ ] Kit 名称明确包含 Qt 6.11.0、MSVC 2022 和 64-bit；
- [ ] Debug 构建无错误；
- [ ] 主窗口能启动；
- [ ] 能双击 `MainWindow.ui` 进入 Designer；
- [ ] `git status` 没有误提交构建目录或 `.user` 文件。
