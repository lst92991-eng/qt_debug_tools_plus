# 第一章 开发环境搭建与第一个 Qt 程序

## 1. 环境说明与安装准备

完成下面五件事就停，不进入串口、插件或 CAN FD 代码：

1. 准备 Windows、账号和磁盘空间；
2. 安装 MSVC C++ 编译环境；
3. 安装 Qt 6.11.0、Qt Creator 和必要工具；
4. 安装 VS Code 扩展并检查两个 IDE 的工具链；
5. 不下载任何现成项目，亲手新建、编译并运行一个 Qt Widgets 工程。

本工程固定使用 Windows x64、Qt 6.11.0、MSVC 2022、C++17 和 CMake。Qt 6.11 官方 Windows 支持表列出的编译器是 MSVC 2022，因此新手环境优先安装 Visual Studio 2022，不以“机器上恰好能编译”为标准。

这一课从“电脑上没有任何 Qt/C++ 开发工具”开始。学生不需要预先理解编译器、Qt、CMake 或 Kit。每安装完一项都要立即验证；前一项验证失败时，不继续安装下一项。

本步骤的结束条件只有一个：学生能在 Qt Creator 中从新建项目向导开始，亲自生成、构建并运行一个空白窗口。界面、串口、插件和 CAN FD 都属于后续课程。

> 验证状态：本文中的工具路径、CMake 配置和最小程序已在现有开发机验证。由于现有开发机原本已经安装开发环境，还必须在一台干净 Windows 机器或虚拟机上，从下载 Visual Studio 2022 开始完整执行一次，才能把本课标记为最终完成。

### 安装前检查

开始前确认：

- Windows 10 1809 或更高版本，推荐 Windows 11 x64；
- 当前 Windows 账号有管理员权限；
- 至少预留 30 GB 可用空间；
- 网络能访问清华大学开源软件镜像站和 Microsoft 下载服务；
- 准备一个 Qt Account，Qt Online Installer 登录时使用。

本教程只讲 Windows x64。不要在第一次学习时同时尝试 Linux、ARM64 或 MinGW。

## 2. 安装 Visual Studio 2022 与 MSVC

安装顺序建议是 Visual Studio 2022 在前、Qt 在后。这样 Qt Creator 首次启动时更容易自动找到编译器。

下载入口：

- [Microsoft 官方：Visual Studio 2022 Community 安装器](https://aka.ms/vs/17/release/vs_Community.exe)
- [Microsoft：Visual Studio 2022 Release History](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history)
- [Qt 6.11：Windows 支持配置](https://doc.qt.io/qt-6/windows.html)

MSVC 和 Windows SDK 是 Microsoft 的专有组件，不使用第三方重打包镜像。国内网络直接下载 Microsoft 官方小型引导程序，再由 Visual Studio Installer 下载组件。个人学习选择 Visual Studio 2022 Community；为了后续调试方便，本教程不选只有命令行工具的 Build Tools 版。

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

Qt 使用清华大学 TUNA 镜像下载 Online Installer，并让安装器后续也从同一镜像获取组件。安装后仍可通过 Maintenance Tool 增删组件。

下载和使用说明：

- [清华大学 TUNA：Qt 镜像使用帮助](https://mirrors.tuna.tsinghua.edu.cn/help/qt/)
- [清华大学 TUNA：Qt Windows 在线安装器目录](https://mirrors.tuna.tsinghua.edu.cn/qt/official_releases/online_installers/)
- [Qt 官方：Online Installer 安装说明](https://doc.qt.io/qt-6/qt-online-installation.html)

### 安装步骤

1. 打开清华镜像的在线安装器目录；
2. 下载 `qt-online-installer-windows-x64-online.exe`，不要选择 ARM64；
3. 在文件资源管理器进入“下载”文件夹；
4. 点击地址栏，输入 `powershell` 后按 Enter；
5. 在打开的 PowerShell 中执行：

```powershell
.\qt-online-installer-windows-x64-online.exe --mirror https://mirrors.tuna.tsinghua.edu.cn/qt
```

6. 登录或注册 Qt Account，并完成邮箱验证；
7. 安装目录统一填写 `D:\QT`；
8. 安装类型选择 `Custom Installation` / `自定义安装`；
9. 在组件树中按下面表格勾选。

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

安装目录统一使用 `D:\QT`，安装后应能找到：

```text
D:\QT\6.11.0\msvc2022_64\bin\qmake.exe
D:\QT\6.11.0\msvc2022_64\bin\windeployqt.exe
D:\QT\6.11.0\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
D:\QT\6.11.0\msvc2022_64\lib\cmake\Qt6SerialPort\Qt6SerialPortConfig.cmake
```

Qt Creator 的实际目录会随安装器版本变化，优先从开始菜单启动，不要照抄某一台电脑的绝对路径。

重新打开 PowerShell，逐条验证 Qt、CMake 和 Ninja：

```powershell
& 'D:\QT\6.11.0\msvc2022_64\bin\qmake.exe' -query QT_VERSION
& 'D:\QT\Tools\CMake_64\bin\cmake.exe' --version
& 'D:\QT\Tools\Ninja\ninja.exe' --version
```

第一条必须输出 `6.11.0`，后两条必须输出各自版本号。任意一条提示“找不到路径”时，先运行 `D:\QT\MaintenanceTool.exe` 补装对应组件，不进入下一步。

## 4. 配置 VS Code 与 Qt Creator

VS Code 是本课程的主要代码编辑器，负责编辑 `.cpp/.h`、修改 CMake、搜索、日常构建和测试。Qt Creator 仍然负责 Qt Kit 基准检查、Qt Designer 界面编辑和必要的 Qt 调试。两者操作同一份源码，不创建两套工程。

### 4.1 安装 VS Code

从 [VS Code 官方 Windows 下载页](https://code.visualstudio.com/Download) 下载 Windows x64 User Installer。运行安装器时：

1. 接受许可协议；
2. 安装路径保持默认；
3. 勾选“将‘通过 Code 打开’操作添加到 Windows 资源管理器文件上下文菜单”；
4. 勾选“将‘通过 Code 打开’操作添加到 Windows 资源管理器目录上下文菜单”；
5. 勾选“将 Code 注册为受支持的文件类型的编辑器”；
6. 勾选“添加到 PATH”；
7. 完成安装并重新打开 PowerShell。

验证：

```powershell
code --version
```

能输出版本号即安装成功。VS Code 官方说明推荐普通个人用户使用 User Setup，它安装在当前用户目录并支持平滑更新。

### 4.2 安装官方扩展

启动 VS Code，按 `Ctrl+Shift+X` 打开扩展，依次安装：

| 搜索名称 | 发布者 | 扩展 ID | 要求 |
| --- | --- | --- | --- |
| Qt C++ Extension Pack | The Qt Company | `TheQtCompany.qt-cpp-pack` | 必装 |
| C/C++ | Microsoft | `ms-vscode.cpptools` | 必装 |
| Chinese (Simplified) Language Pack | Microsoft | `MS-CEINTL.vscode-language-pack-zh-hans` | 中文界面可选 |

Qt C++ Extension Pack 会自动安装 Qt C++、Qt Core、CMake 和 CMake Tools 等依赖。安装完成后仍要在扩展列表确认 `CMake Tools` 的发布者是 Microsoft、扩展 ID 是 `ms-vscode.cmake-tools`。

不要安装名称相似的第三方 Qt 工程生成器、编译按钮或代码模板扩展。第一轮只保留 Qt Company 和 Microsoft 的上述扩展，减少工具冲突。

官方说明：

- [Qt C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.qt-cpp-pack)
- [Qt Extension for VS Code 安装说明](https://doc.qt.io/vscodeext/vscodeext-how-to-install.html)
- [Microsoft CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

### 4.3 在 VS Code 注册 Qt 安装路径

按 `Ctrl+Shift+P` 打开命令面板，执行：

```text
Qt: Register Qt installation
```

选择 Qt 根目录：

```text
D:\QT
```

如果扩展要求选择具体 `qmake.exe`，选择：

```text
D:\QT\6.11.0\msvc2022_64\bin\qmake.exe
```

再执行：

```text
Qt: Open Qt Extension settings
```

检查 Qt Installation Root 为 `D:\QT`，Additional Qt Paths 包含 MSVC 2022 64-bit 的 `qmake.exe`。

### 4.4 两个 IDE 的协作规则

| 工作 | 默认工具 |
| --- | --- |
| 创建第一个 Qt Widgets 工程 | Qt Creator |
| 编辑 `.cpp`、`.h`、`CMakeLists.txt`、Markdown | VS Code |
| 编辑 `.ui` | Qt Creator Design，或 VS Code Qt 扩展调用的 Qt Widgets Designer |
| 日常 Configure/Build/Test | VS Code CMake Tools |
| 检查 Qt Kit、信号槽和 Qt 调试 | Qt Creator |

禁止两个 IDE 同时修改同一个文件。切换前先保存；`.ui` 文件只通过 Designer 修改，不手工编辑 XML。

两个 IDE 使用不同构建目录：

```text
VS Code：D:\QtBuilds\MCUDebugTool-vscode
Qt Creator：项目目录之外的 Qt Creator shadow build 目录
```

它们必须使用相同的 Qt 6.11.0、MSVC 2022 x64、CMake 配置和 Debug/Release 类型。

### 4.5 第一次启动 Qt Creator 并检查 Kit

这一节仍然不下载任何项目。

1. 打开 Windows 开始菜单；
2. 搜索并启动 `Qt Creator`；
3. 第一次启动出现欢迎或隐私设置时保持默认，进入主界面；
4. 选择 `编辑/Edit > Preferences/首选项 > Kits`。

依次打开各标签页检查：

| 标签页 | 必须看到的内容 |
| --- | --- |
| Qt Versions | Qt 6.11.0，路径指向 `D:\QT\6.11.0\msvc2022_64\bin\qmake.exe` |
| Compilers | Microsoft Visual C++ x64 编译器 |
| CMake | 路径指向 `D:\QT\Tools\CMake_64\bin\cmake.exe` 或另一可用 CMake |
| Debuggers | 适用于 x64 的 CDB 调试器 |
| Kits | `Desktop Qt 6.11.0 MSVC2022 64bit`，前面没有红色感叹号 |

Qt Creator 随 Qt 一起安装时通常会自动识别这些工具。如果没有自动识别，按下面顺序修复：

1. 在 `Qt Versions` 点 `Add`，选择 `D:\QT\6.11.0\msvc2022_64\bin\qmake.exe`；
2. 在 `Compilers` 确认有 Microsoft Visual C++ x64；没有时关闭 Qt Creator，修复 Visual Studio C++ 工作负载后重开；
3. 在 `CMake` 点 `Add`，选择 `D:\QT\Tools\CMake_64\bin\cmake.exe`；
4. 在 `Debuggers` 确认有 x64 CDB；没有时回 Visual Studio Installer 增加 Windows 调试工具；
5. 在 `Kits` 点 `Add`，Device type 选 Desktop，Qt version 选 6.11.0，C/C++ compiler 选 MSVC x64，CMake tool 选刚才的 CMake，Debugger 选 x64 CDB；
6. Kit 名称填写 `Desktop Qt 6.11.0 MSVC2022 64bit`；
7. 点 `Apply`，确认 Kit 没有红色错误提示。

Kit 是“设备 + 编译器 + Qt 版本 + 调试器 + 构建工具”的固定组合。官方字段说明见 [Qt Creator：Managing kits](https://doc.qt.io/qtcreator/creator-preferences-kits.html)。

## 5. 从零新建工程

### 5.1 打开新建项目向导

1. 回到 Qt Creator 欢迎页；
2. 选择 `文件/File > 新建项目/New Project`；
3. 左侧选择 `Application (Qt)` 或 `Application`；
4. 右侧选择 `Qt Widgets Application`；
5. 点击 `Choose/选择`。

### 5.2 设置项目名称和保存位置

在 Project Location 页面填写：

```text
Name：MCUDebugTool
Create in：D:\QtProjects
```

如果 `D:\QtProjects` 不存在，允许向导创建。最终项目目录必须是：

```text
D:\QtProjects\MCUDebugTool
```

不要把项目放在 `D:\QT` 安装目录、桌面、下载文件夹或中文/空格很多的路径中。

### 5.3 选择构建系统

Build System 选择：

```text
CMake
```

不要选择 qmake 或 Qbs。点击 Next。

### 5.4 设置主窗口类

在 Class Information 页面填写：

```text
Class name：MainWindow
Base class：QMainWindow
Header file：mainwindow.h
Source file：mainwindow.cpp
Form file：mainwindow.ui
```

勾选 `Generate form/生成窗体`，点击 Next。

### 5.5 翻译和 Kit

1. Translation file 选择 `None/无`，后续课程再讲翻译；
2. Kit 只勾选 `Desktop Qt 6.11.0 MSVC2022 64bit`；
3. 不勾选 MinGW、Android、WebAssembly 或 ARM64 Kit；
4. Project Management 页的 Version Control 选择 `None`；
5. 点击 Finish。

这是学生自己创建的项目，不包含你的仓库、成品源码或教学骨架。

### 5.6 检查向导生成的文件

Qt Creator 左侧项目树应至少出现：

```text
CMakeLists.txt
main.cpp
mainwindow.cpp
mainwindow.h
mainwindow.ui
```

双击 `mainwindow.ui` 应进入 Designer。现在不要添加控件，只确认设计器能打开。

### 5.7 用 VS Code 打开同一工程

在 VS Code 选择 `文件/File > 打开文件夹/Open Folder`，打开：

```text
D:\QtProjects\MCUDebugTool
```

按 `Ctrl+Shift+P`，依次执行：

```text
Qt: Register Qt installation
CMake: Select a Kit
```

Qt 路径选择 `D:\QT`，CMake Kit 选择包含 Qt 6.11.0、MSVC 2022 和 x64 的项目。不要选择 Visual Studio 2026、MinGW 或 x86 Kit。

在项目中创建 `.vscode/settings.json`，内容为：

```json
{
  "qt-core.qtInstallationRoot": "D:\\QT",
  "qt-core.additionalQtPaths": [
    "D:\\QT\\6.11.0\\msvc2022_64\\bin\\qmake.exe"
  ],
  "cmake.cmakePath": "D:\\QT\\Tools\\CMake_64\\bin\\cmake.exe",
  "cmake.buildDirectory": "D:/QtBuilds/MCUDebugTool-vscode"
}
```

## 6. 构建、备份、排错与验收

### 6.1 分别用 Qt Creator 和 VS Code 构建

1. 点击左侧 `Projects/项目`；
2. Build configuration 选择 `Debug`；
3. 确认 Build directory 位于源码目录之外，使用 Qt Creator 自动给出的 shadow build 目录；
4. 返回 Edit 模式；
5. 点击左下角锤子，或按 `Ctrl+B`；
6. 打开底部 `Compile Output/编译输出`；
7. 看到 `Build finished` 且没有红色 error 后，点击绿色三角或按 `Ctrl+R`。

正常结果是出现一个标题为 `MainWindow` 的空白窗口。关闭窗口后，Qt Creator 的 Application Output 不应显示崩溃。

如果空白窗口能够出现，说明 Qt Creator、CMake、Ninja、MSVC、Windows SDK 和 Qt Widgets 已经真正协作成功。

随后在 VS Code 按 `Ctrl+Shift+P`，依次执行：

```text
CMake: Configure
CMake: Build
```

两条命令都成功，且 `build-vscode` 中生成程序后，说明 VS Code 协作环境也正常。日常写代码可以留在 VS Code；每个教学阶段结束后再用 Qt Creator 额外构建一次作为交叉验证。

### 6.2 建立第一个源码备份

1. 保存所有文件并关闭正在运行的程序；
2. 在 D 盘创建 `D:\QtCourseBackups`；
3. 把整个 `D:\QtProjects\MCUDebugTool` 文件夹复制进去；
4. 把副本重命名为 `MCUDebugTool-step01-empty`；
5. 确认副本包含 `CMakeLists.txt`、`main.cpp` 和 `mainwindow.*`；
6. 确认副本中没有 `.exe`、`.obj` 或 build 目录。

后续每完成一步都创建新副本：

```text
MCUDebugTool-step01-empty
MCUDebugTool-step02-main-window
MCUDebugTool-step03-data-frame
```

Git 移到第六章作为可选的专业版本管理方式，不影响前面章节编译和学习。

### 6.3 常见问题

#### `Could not find Qt6Config.cmake`

选错 Kit 或 Qt version 没有注册。回到 `Preferences > Kits > Qt Versions`，添加 `D:\QT\6.11.0\msvc2022_64\bin\qmake.exe`，不要全局乱加 PATH。

#### `No CMAKE_CXX_COMPILER could be found`

Visual Studio C++ 工作负载缺失，或 Kit 没选 MSVC x64。回 Visual Studio Installer 修复“使用 C++ 的桌面开发”。

#### `Qt6SerialPort not found`

Day 0 不会出现；Day 5 出现时，运行 Qt 安装目录下的 Maintenance Tool，给 Qt 6.11.0 MSVC 2022 64-bit 增加 Qt Serial Port。

#### `generator does not match` 或缓存路径指向旧 Qt

不要在旧构建目录上继续修补。到 Qt Creator 的 Projects 页面换一个新的构建目录，然后重新 Configure。

#### 插件能编译但加载失败

通常是主程序与插件混用了 MSVC/MinGW、x64/ARM64 或 Debug/Release。后续每天都只使用同一个 Kit。

#### Qt 安装器下载很慢或组件列表为空

关闭安装器，回到“下载”文件夹重新打开 PowerShell，确认启动命令带有完整的 `--mirror` 参数：

```powershell
.\qt-online-installer-windows-x64-online.exe --mirror https://mirrors.tuna.tsinghua.edu.cn/qt
```

不要直接双击安装器后再寻找镜像设置；本教程从命令行启动时就指定镜像。

#### Visual Studio 安装完成但 Qt Creator 看不到 MSVC

先关闭 Qt Creator。在 Visual Studio Installer 中确认“使用 C++ 的桌面开发”和 MSVC v143 x64/x86 工具已安装，重启 Windows 后再打开 Qt Creator。

### 6.4 第一章验收清单

- [ ] Visual Studio 2022 的“使用 C++ 的桌面开发”已安装；
- [ ] Qt 安装器通过清华 TUNA 镜像启动；
- [ ] `qmake` 输出 Qt 6.11.0；
- [ ] CMake 和 Ninja 都能输出版本号；
- [ ] VS Code 能输出版本号；
- [ ] 已安装 Qt C++ Extension Pack 和 Microsoft C/C++；
- [ ] VS Code 已注册 `D:\QT`，并选中 Qt 6.11.0 MSVC 2022 x64 Kit；
- [ ] Kit 名称明确包含 Qt 6.11.0、MSVC 2022 和 64-bit；
- [ ] 学生没有克隆或复制任何现成项目；
- [ ] 已通过 Qt Creator 向导创建 `D:\QtProjects\MCUDebugTool`；
- [ ] 工程包含 `CMakeLists.txt`、`main.cpp`、`mainwindow.cpp/.h/.ui`；
- [ ] Debug 构建无错误；
- [ ] Qt Creator 和 VS Code 分别使用独立构建目录且都能构建；
- [ ] 空白 MainWindow 能启动；
- [ ] 能双击 `mainwindow.ui` 进入 Designer；
- [ ] 已创建 `MCUDebugTool-step01-empty` 源码备份；
- [ ] 备份中没有 `.exe`、`.obj` 或构建目录。
