# MCU Debug Tool：Qt 开发环境、类说明与组件化开发指南

> 文档状态：以当前仓库源码、CMake 配置、VS Code 配置和本机 CMake 缓存为依据。
> 当前项目版本：`MCUDebugTool 0.1.0`。
> 推荐团队基线：Windows x64、Qt 6.11.0、MSVC 2022 64-bit Qt Kit、C++17、CMake。
> 本文将“已经实现的事实”和“后续演进建议”分开描述，避免设计文档与代码状态不一致。

## 1. 项目定位

本项目是一个基于 Qt Widgets 的 MCU/设备调试上位机。它把设备接入、协议解析、数据展示和控制命令拆成四类运行时插件，并由核心层统一装载、连接、分发和管理线程。

当前主要能力包括：

- 串口、Windows WinUSB、Linux libusb 和 CandleLight/CANable CAN FD 设备接入；
- 原始数据、串口数值和 CAN 帧协议解析；
- Raw Viewer、时序曲线和仪表盘显示；
- 原始命令和滑块命令控制；
- 通道元数据、会话保存、历史保存、缓冲池容量与溢出统计；
- CAN FD 链路测试以及 AGV 前进 300 mm 的自动状态机测试。

## 2. 版本基线

### 2.1 当前已验证环境

| 项目 | 当前值 | 说明 |
| --- | --- | --- |
| 操作系统 | Windows x64 | 当前构建和 CAN FD 联调环境 |
| Qt | 6.11.0 | 安装目录为 `D:/App/Qt/6.11.0/msvc2022_64` |
| Qt Kit | MSVC 2022 64-bit | 必须保持主程序与插件 ABI、位数和构建类型一致 |
| IDE/编译器环境 | Visual Studio Community 2026 18.5.1 | 当前 CMake 生成器为 `Visual Studio 18 2026`；使用 Qt 提供的 `msvc2022_64` 二进制 Kit |
| C++ 标准 | C++17 | 由根目录 `CMakeLists.txt` 强制指定 |
| CMake | 项目最低 3.20；当前验证 4.2.3-msvc3 | Visual Studio 自带 4.2.3；Qt Tools 另带 3.30.5，系统另装 4.4.2，不要在同一 build 目录混用 |
| Ninja | 1.12.1 | 已安装，但当前工程优先使用 Visual Studio 多配置生成器，原因见 4.4 节 |
| 架构 | x64 | Qt Kit、应用、插件和设备库必须全部使用 x64 |

### 2.2 源码声明的最低约束

根目录 `CMakeLists.txt` 当前只声明：

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort)
```

因此，从语法上看项目接受任意能够提供 `Core`、`Widgets` 和 `SerialPort` 的 Qt 6 版本；但团队开发不应各自选择不同版本。建议统一固定为 Qt 6.11.0，升级 Qt 时单独建升级任务，并重新执行构建、插件扫描、串口和 CAN FD 回归测试。

### 2.3 为什么不用 Qt 5 或 MinGW Kit

- 代码和 CMake 已明确以 Qt 6 为目标，没有维护 Qt 5 兼容层；
- 当前 Windows 设备插件直接链接 `setupapi`、`winusb` 和 `ole32`，已验证路径是 MSVC；
- MSVC Qt Kit 与 MinGW Qt Kit ABI 不兼容，不能混用主程序和插件 DLL；
- Debug 插件不能放进 Release 主程序目录混用，反之亦然。

## 3. 安装时需要勾选的选项

### 3.1 Qt Online Installer / Maintenance Tool

建议在 Qt 安装器中选择以下内容。安装器不同版本的树形名称可能略有变化，应以组件功能为准。

| 组件 | 要求 | 用途 |
| --- | --- | --- |
| `Qt 6.11.0 > MSVC 2022 64-bit` | 必选 | 提供 Qt Core、Gui、Widgets 和对应编译库 |
| Qt Serial Port | 必选 | 提供 `Qt6::SerialPort`、`QSerialPort`、`QSerialPortInfo` |
| CMake | 建议勾选 | 与 Qt Kit 配套，当前安装版本为 3.30.5 |
| Ninja | 建议勾选 | Qt Creator 常用构建工具；当前项目切换 Ninja 前需先统一输出目录 |
| Qt Creator | 二选一 | 使用 Qt Creator 开发时勾选；若固定使用 VS Code/Visual Studio 可不装 |
| Qt Designer | 建议保留 | 编辑 `MainWindow.ui`；通常随 Qt Tools/Qt Creator 提供 |
| Sources | 可选 | 仅在需要单步进入 Qt 源码时安装 |
| Debug Symbols | 可选但推荐 | 排查 Qt 内部崩溃时使用，占用空间较大 |

当前项目不需要 Qt Quick、QML、Qt WebEngine、Qt Charts、Android、WebAssembly、Qt Design Studio。时序图和仪表盘目前通过 `QPainter` 自绘，不依赖 Qt Charts。

安装完成后应存在：

```text
D:/App/Qt/6.11.0/msvc2022_64/bin/qmake.exe
D:/App/Qt/6.11.0/msvc2022_64/bin/windeployqt.exe
D:/App/Qt/6.11.0/msvc2022_64/lib/cmake/Qt6/Qt6Config.cmake
D:/App/Qt/6.11.0/msvc2022_64/lib/cmake/Qt6SerialPort/
```

### 3.2 Visual Studio Installer

Windows 推荐安装“使用 C++ 的桌面开发”工作负载，并确认包含：

- 与 Qt `msvc2022_64` Kit 兼容的 MSVC x64/x86 C++ 生成工具；
- Windows 10 或 Windows 11 SDK；
- C++ CMake tools for Windows；
- Windows 调试工具，可选但推荐。

若使用不同版本的 Visual Studio，原则不是只看 IDE 名称，而是确认所用 MSVC 工具链能够消费 Qt 的 `msvc2022_64` 预编译库。团队发布机应固定同一套 Kit 和工具链，不要依赖个人机器上偶然可用的组合。

### 3.3 设备和系统依赖

#### Windows

- `Serial (Generic)`：使用系统串口驱动和 Qt SerialPort；
- `USB Raw (Windows)`：设备需要绑定 WinUSB 驱动，并提供设备接口 GUID、VID/PID 和端点配置；
- `CandleLight CANFD`：使用 WinUSB 和 gs_usb/CandleLight FD 协议；
- `setupapi`、`winusb`、`ole32` 来自 Windows SDK，不需要额外复制第三方 `.lib`。

驱动切换会影响系统中其他同类工具，执行前应记录设备原驱动。设备未绑定 WinUSB 时，应用可以正常构建，但对应物理插件无法打开设备。

#### Linux

Linux 构建至少需要 Qt 6 Widgets、Qt 6 SerialPort、CMake、C++17 编译器。启用 `USB Raw (Linux)` 还需要：

```bash
pkg-config
libusb-1.0 development package
```

CMake 未找到 libusb 时仍会生成该插件目标，但运行时会报告 USB Raw 不可用。Linux 和 macOS 目前不是本仓库的已验证发布基线，交付前必须补做平台构建和设备测试。

## 4. 环境配置与构建

### 4.1 CMake 查找 Qt 的推荐方式

推荐只设置 CMake 的 `CMAKE_PREFIX_PATH`，不要把大量 Qt 目录永久写入系统 `PATH`：

```text
CMAKE_PREFIX_PATH=D:/App/Qt/6.11.0/msvc2022_64
```

CMake 会据此找到 Qt 包配置、MOC、UIC、RCC 和 `windeployqt`。当前项目已经启用：

```cmake
CMAKE_AUTOMOC
CMAKE_AUTOUIC
CMAKE_AUTORCC
```

因此新增带 `Q_OBJECT` 的类、`.ui` 文件或 `.qrc` 文件后，只需把源文件加入对应 CMake target，不需要手工运行 `moc`、`uic` 或 `rcc`。

### 4.2 VS Code

仓库当前 `.vscode/settings.json` 已配置：

```json
{
  "qt-core.qtInstallationRoot": "D:\\App\\Qt",
  "qt-core.additionalQtPaths": [
    "D:\\App\\Qt\\6.11.0\\msvc2022_64\\bin\\qmake.exe"
  ],
  "qt-ui.customWidgetsDesignerExePath": "D:\\App\\Qt\\6.11.0\\msvc2022_64\\bin\\designer.exe",
  "cmake.configureSettings": {
    "CMAKE_PREFIX_PATH": "D:/App/Qt/6.11.0/msvc2022_64"
  }
}
```

建议安装 VS Code 的 C/C++、CMake Tools 和 Qt 相关扩展。首次打开项目后：

1. 选择 x64 的 Visual Studio Kit；
2. 执行 `CMake: Delete Cache and Reconfigure`；
3. 选择 `Release` 或 `Debug`；
4. 构建 target `mcd_app` 或 `all`；
5. 从对应配置目录启动应用。

若个人 Qt 安装路径不同，应只改本机 VS Code 用户设置或本地 CMake preset，不应把个人绝对路径继续提交到公共配置。后续建议增加 `CMakePresets.json` 和不入库的 `CMakeUserPresets.json`，把生成器/架构放入公共 preset，把 Qt 绝对路径留在用户 preset。

### 4.3 Qt Creator

1. 用 Qt Creator 打开根目录 `CMakeLists.txt`；
2. 选择 `Desktop Qt 6.11.0 MSVC 2022 64-bit` Kit；
3. 确认 CMake Configuration 中有正确的 `CMAKE_PREFIX_PATH`；
4. 建议先选择 Visual Studio 多配置生成器；
5. 构建并运行 `mcd_app`。

本工程是 CMake 项目，不要另建 `.pro` 文件，也不要用 qmake 维护第二套构建描述。

#### 4.3.1 可以直接用 Qt Creator/Designer 画界面

可以，而且主窗口已经采用这种方式：`src/app/MainWindow.ui` 负责静态布局，CMake 的 `AUTOUIC` 在构建时生成 `ui_MainWindow.h`，`MainWindow::buildUi()` 中的 `m_ui->setupUi(this)` 创建整棵控件树。

推荐的界面开发流程：

1. 在 Qt Creator 中双击 `.ui` 文件进入 Design 模式；
2. 拖入控件，并先设置 layout，再调整 stretch、size policy、minimum size；
3. 为需要在代码中访问的控件设置稳定且有意义的 `objectName`；
4. 静态文案、图标、初始属性和固定布局放在 `.ui` 中；
5. 数据绑定、业务判断、跨线程连接和运行时组件装配写在 C++ 中；
6. 构建 target，AUTOUIC 会自动重新生成代码，不要编辑 `ui_*.h`。

在当前主窗口中，`physicalCombo`、`protocolCombo`、`configTable`、`visualTabs`、`controlTabs`、`activityLog` 等已经在 Designer 文件里定义。`visualTabs` 和 `controlTabs` 是专门给运行时插件预留的容器：Designer 负责它们的位置、尺寸和布局，程序启动后再把插件页面放进去。

插件本身也可以使用 Designer。比如 `GaugePlugin` 可以增加 `GaugePlugin.ui`，类内部调用 `ui->setupUi(this)`，然后仍然通过 `Q_PLUGIN_METADATA` 作为动态插件加载。是否使用 `.ui` 与是否使用插件机制并不冲突。

#### 4.3.2 哪些界面仍然需要写 C++

“用 Designer”不等于完全不写 UI 代码。以下内容天然需要 C++：

- 运行时才知道数量和类型的页面，例如扫描插件后执行 `addTab()`；
- 根据配置 schema 动态生成的编辑器，例如不同 physical 插件有不同配置项；
- 自绘控件，例如当前 Time Chart 和 Gauge 的 `QPainter` 绘制；
- 数据 model、校验、状态机、设备连接和错误处理；
- 跨线程 queued connection、动态订阅、插件卸载和生命周期处理；
- 用户操作后才创建的临时对话框或上下文菜单。

固定对话框、固定工具栏和固定页面尽量改用 `.ui`，减少手写布局代码。当前 `DeviceConfigDialog` 是因为字段由插件配置动态决定，所以使用 C++ 创建表单是合理的；如果以后引入配置 schema，仍可以用 `.ui` 设计对话框外壳，只在预留的 `QFormLayout` 中动态插入字段控件。

#### 4.3.3 Qt Creator 能否定义信号和槽

能，但需要区分“声明”和“连接”：

- Designer 的 Signal/Slot Editor 可以连接 Qt 控件已有的信号和槽，连接会保存在 `.ui` 的 `<connections>` 中；
- Qt Creator 的“转到槽/Go to Slot”可以生成 `on_<objectName>_<signal>()` 槽函数，`setupUi()` 最后通过 `QMetaObject::connectSlotsByName()` 自动连接；
- 自定义信号和槽仍应在继承 `QObject` 的 C++ 类中声明，并包含 `Q_OBJECT`；
- 插件、业务 service、lambda、重载信号、跨线程和运行时对象通常使用 C++ 的类型安全 `connect()`；
- 连接只负责通信，不代替业务逻辑注册、对象创建和生命周期管理。

示例：

```cpp
class AgvControlPage : public QWidget {
    Q_OBJECT
public:
    explicit AgvControlPage(QWidget* parent = nullptr);

signals:
    void startRequested(int distanceMm);

private slots:
    void onStartClicked();
};

connect(page, &AgvControlPage::startRequested,
        agvService, &AgvTestService::startMove);
```

对于简单且稳定的“按钮关闭对话框”等连接，可以放在 Designer；项目内部的重要业务连接建议显式写成上面的类型安全 `connect()`，这样编译器可以检查参数类型，代码审查也能直接看到数据流。不要大量依赖 `on_objectName_signal()` 自动连接，因为重命名控件后容易静默失效。

当前 `MainWindow.ui` 的 `<connections/>` 为空，所有连接都集中写在 `MainWindow::buildUi()`。这不是功能缺失，而是当前选择了显式连接方式；后续可以把纯界面连接移进 Designer，但插件、核心和业务 service 的连接仍应保留在 C++ 装配层。

### 4.4 命令行构建

当前最稳妥的 Windows 构建方式是 Visual Studio 多配置生成器：

```powershell
$vsCmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

& $vsCmake -S . -B build `
  -G "Visual Studio 18 2026" `
  -A x64 `
  -DCMAKE_PREFIX_PATH=D:/App/Qt/6.11.0/msvc2022_64

& $vsCmake --build build --config Release --parallel
```

如果开发机安装的是其他 Visual Studio 版本，应替换 `$vsCmake` 路径，并把 `-G` 替换成该 CMake 的 `--help` 中实际存在的生成器名称。配置和构建必须使用同一个 CMake 可执行文件；不要先用系统 CMake 配置、再用 Visual Studio CMake 构建同一个目录，反之亦然。

当前 CMake 对多配置生成器的输出布局是：

```text
build/bin/Release/mcd_app.exe
build/bin/Release/plugins/physical/*.dll
build/bin/Release/plugins/protocol/*.dll
build/bin/Release/plugins/visual/*.dll
build/bin/Release/plugins/control/*.dll
```

`mcd_app` 从自身目录下的 `plugins` 递归扫描插件。Windows 每次构建主程序后，CMake 还会自动调用 `windeployqt`，把 Qt runtime 和 `platforms/qwindows.dll` 等部署到可执行文件旁边。

当前不建议直接把生成器改为 Ninja：主程序使用 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，插件目录又显式包含 `$<CONFIG>`，单配置生成器下可能导致主程序和插件不在同一目录树，进而让插件扫描失败。若后续统一了单配置/多配置输出规则，再把 Ninja 加入正式支持矩阵。

### 4.5 运行与自检

图形界面：

```powershell
./build/bin/Release/mcd_app.exe
```

插件扫描冒烟测试：

```powershell
./build/bin/Release/mcd_app.exe --smoke-test
```

成功标准是退出码为 0，且 physical、protocol、visual、control 四类插件数量都大于 0。

CAN FD 链路测试和 300 mm 运动测试会访问真实设备：

```powershell
./build/bin/Release/mcd_app.exe --canfd-link-test
./build/bin/Release/mcd_app.exe --distance-test-300
```

第二条命令会控制真实 AGV 运动，只能在已完成急停、防撞、架空或安全区域隔离的现场环境执行。它不是普通构建自检，禁止放入无人值守 CI。

### 4.6 常见环境问题

| 现象 | 优先检查 |
| --- | --- |
| `Could not find Qt6` | `CMAKE_PREFIX_PATH` 是否指向 Kit 根目录，而不是 `bin` 目录 |
| 找不到 `Qt6SerialPort` | Qt 安装器是否安装 Serial Port 组件，CMake cache 是否仍指向旧 Qt |
| 插件数量为 0 | 插件是否位于可执行文件旁的 `plugins/<type>`；构建配置是否一致 |
| `The specified module could not be found` | 插件依赖的 Qt DLL/系统 DLL 是否已部署；可用 `windeployqt` 重新部署 |
| `not a Qt plugin` 或加载失败 | Qt 版本、编译器 ABI、x64/x86、Debug/Release 是否混用 |
| 串口列表为空 | 驱动、设备管理器、权限以及端口是否被其他程序占用 |
| WinUSB 设备打不开 | 驱动绑定、接口 GUID、VID/PID、端点地址和权限 |
| CAN FD 有帧但错误持续增加 | 仲裁段/数据段波特率、采样点、BRS/FD 设置是否与固件完全一致 |

## 5. CMake target 与目录职责

### 5.1 现有目录

```text
src/
├─ sdk/                  插件公共接口和跨模块数据结构
├─ core/                 插件管理、数据中枢、缓冲池和分发
├─ app/                  QApplication、主窗口、配置对话框、AGV 测试入口
└─ plugins/
   ├─ physical/          字节输入输出和设备连接
   ├─ protocol/          字节流与 DataFrame/命令之间的转换
   ├─ visual/            只负责数据展示的 QWidget 插件
   └─ control/           只负责产生结构化命令的 QWidget 插件
```

### 5.2 构建目标

| target | 类型 | 职责 | 主要依赖 |
| --- | --- | --- | --- |
| `mcd_sdk` | STATIC | 公共接口、`DataFrame` 和元类型注册 | Qt Core、Widgets |
| `mcd_core` | STATIC | `DebugCore`、`PluginManager`、`ChannelHub`、`RingBufferPool` | `mcd_sdk`、Qt Core、Widgets |
| `mcd_app` | EXECUTABLE | GUI、配置、会话、历史、测试模式 | `mcd_core`、Qt Core、Widgets、SerialPort |
| 各插件 target | MODULE | 运行时动态插件 | `mcd_sdk` 及插件自身所需库 |

插件使用 `Q_PLUGIN_METADATA` 嵌入同目录 JSON 元数据。CMake 构建后会把插件 DLL 和 JSON 一起复制到输出目录。`PluginManager` 最终以 Qt 接口转换结果为准，JSON 的 `type` 和 `platforms` 是附加校验。

## 6. 当前数据流和线程模型

### 6.1 接收链路

```text
设备
  -> IPhysicalPlugin::dataReceived(QByteArray)
  -> IProtocolPlugin::feedBytes(...)
  -> IProtocolPlugin::frameParsed(DataFrame)
  -> DebugCore::publish(...)
       ├─ 合并通道元数据
       ├─ 数值样本写入 RingBufferPool
       ├─ ChannelHub 按订阅分发
       └─ 发出 framePublished/overflowOccurred
  -> IVisualPlugin::onChannelData(...)
```

### 6.2 发送链路

```text
IControlPlugin::commandGenerated(QVariantMap)
  或 AgvCanFdTestRunner 生成命令
  -> DebugCore::sendCommand(...)
  -> IProtocolPlugin::encodeCommand(...)
  -> IPhysicalPlugin::write(QByteArray)
  -> 发布 TX DataFrame，供 Raw Viewer 和计数器观察
```

### 6.3 线程职责

| 线程 | 当前对象/职责 | 禁止事项 |
| --- | --- | --- |
| UI 主线程 | `MainWindow`、所有 visual/control QWidget 插件 | 不做阻塞设备 I/O，不做长时间协议解析 |
| `mcd-ingest` | 激活的 physical/protocol 插件、设备读写、命令编码 | 不直接操作 QWidget |
| `mcd-dispatch` | `ChannelHub`、订阅关系、最新帧合并分发 | 不直接调用 QWidget 方法；使用 queued invoke 回 UI 线程 |

`RingBufferPool` 使用读写锁保护共享数据。分发器落后时只保留最新待分发帧，并通过 sequence/generation 记录跳帧或重置，不允许 UI 反压导致采集线程无限堆积。

## 7. 项目自定义类说明

### 7.1 SDK 数据类型

| 类型 | 职责 |
| --- | --- |
| `FrameDirection` | 标记 `Receive` 或 `Transmit`，让展示和统计区分 RX/TX |
| `ChannelSample` | 单个数值通道样本：索引、值、名称和单位；NaN 表示 raw-only |
| `DataFrame` | 插件间统一帧：时间戳、sequence、generation、通道、原始负载、方向和扩展属性 |
| `OverflowEvent` | 记录环形池或分发阶段的跳过、丢样本、丢帧信息 |
| `TimedSample` | 环形池内部使用的带时间戳、sequence 的数值点 |
| `PoolSnapshot` | 环形池当前最老/最新水位、generation、时间范围和容量统计 |
| `PoolWriteResult` | 单次写入分配的 sequence 和被裁剪样本数量 |
| `RingBuffer` | 单通道有限容量队列及范围查询 |

`DataFrame` 是当前架构最关键的数据契约。物理插件不创建业务帧；协议插件填充 `rawPayload`、`channels` 和 `attributes`；核心层补充缓存水位和用户通道元数据。

### 7.2 四类插件接口

| 类 | 基类 | 输入/输出 | 边界 |
| --- | --- | --- | --- |
| `IPhysicalPlugin` | `QObject` | 打开/关闭/写字节；发出原始接收字节和状态 | 只处理传输，不解析业务协议，不访问 UI |
| `IProtocolPlugin` | `QObject` | 原始字节解析成 `DataFrame`；结构化命令编码成字节 | 不打开设备，不直接更新 UI |
| `IVisualPlugin` | `QWidget` | 订阅通道并消费 `DataFrame` | 只在 UI 线程运行，不直接访问设备 |
| `IControlPlugin` | `QWidget` | 发出 `QVariantMap` 结构化命令 | 不直接调用 physical/protocol 插件 |

四个接口都有固定 IID `org.mcd.sdk.<Interface>/1.0`。以后破坏接口兼容性时必须升级 IID；只增加可选元数据时可以保留 IID，但要保证旧插件仍能加载。

### 7.3 核心类

| 类 | 职责 | 关键关系 |
| --- | --- | --- |
| `DebugCore` | 进程内单例数据中枢；初始化线程、连接数据链、发送命令、补元数据、发布错误和溢出 | 持有 `PluginManager`、`ChannelHub`、`RingBufferPool` |
| `PluginManager` | 递归扫描动态库，检查元数据和平台，分类插件，激活/停用 physical/protocol，迁移线程 | 使用 `QPluginLoader` 管理插件生命周期 |
| `ChannelHub` | 管理 visual 插件订阅、每消费者水位和异步 UI 分发 | 运行在 dispatch 线程，回调通过 queued invoke 返回 UI 线程 |
| `RingBufferPool` | 按通道保存有限的最新数值窗口，提供回放、快照、容量调整和文件持久化 | 与 UI/采集并发，通过 `QReadWriteLock` 保护 |

### 7.4 应用层类

| 类 | 职责 |
| --- | --- |
| `AppContext` | 最小应用装配点；统一初始化 `DebugCore`、确定插件目录并触发扫描，供 GUI、smoke test 和 AGV 测试共同使用 |
| `MainWindow` | 主界面装配、插件扫描和挂载、设备连接、会话/历史、通道表、串口扫描、统计和活动日志 |
| `DeviceConfigDialog` | 根据 physical 插件默认配置和当前配置动态生成键值编辑表单 |
| `AgvCanFdTestRunner` | `AgvCanFdTest.cpp` 内部的无界面测试状态机；管理心跳、超时、ACK、里程和安全状态 |
| `DirectionHighlighter` | Raw Viewer 内部语法高亮辅助类，用颜色区分方向/文本 |

`main.cpp` 根据命令行选择四种启动模式：正常 GUI、插件 smoke test、CAN FD link test、300 mm distance test。

### 7.5 已实现插件类

| 类 | 分类 | 当前作用 |
| --- | --- | --- |
| `SerialGenericPlugin` | physical | 基于 `QSerialPort` 的通用串口接入 |
| `UsbRawWinPlugin` | physical | Windows WinUSB bulk 接入 |
| `UsbRawLinuxPlugin` | physical | Linux libusb bulk 接入 |
| `CandleLightCanFdPlugin` | physical | Windows CandleLight/CANable CAN FD 接入 |
| `RawPassthroughPlugin` | protocol | 原始字节透传，并映射为可观察字节通道 |
| `SerialNumericPlugin` | protocol | 解析换行数值、键值和简单 JSON telemetry |
| `CanFramePlugin` | protocol | 解析/编码 `CA FD + CAN ID + DLC + payload` 工程帧 |
| `RawViewerPlugin` | visual | RX/TX 时间、HEX/ASCII、过滤、暂停、清空和详情 |
| `TimeChartPlugin` | visual | 数值通道轻量时序曲线 |
| `GaugePlugin` | visual | 显示选定数值通道的最新值 |
| `RawControlPlugin` | control | HEX 命令校验、历史、周期发送和快捷预设 |
| `SliderWidgetPlugin` | control | 滑块式数值命令和实时发送 |

### 7.6 主要 Qt 类及用途

| 类族 | 本项目用途 |
| --- | --- |
| `QObject`、信号/槽、`QMetaObject` | 模块解耦、跨线程 queued 调用、对象生命周期通信 |
| `QThread`、`QMutex`、`QReadWriteLock`、`QPointer` | 采集/分发线程、共享状态保护、异步对象安全引用 |
| `QPluginLoader`、`Q_DECLARE_INTERFACE`、`Q_PLUGIN_METADATA` | 动态插件发现、类型识别和元数据嵌入 |
| `QApplication`、`QMainWindow`、`QDialog`、`QWidget` | 应用事件循环、主窗口、配置对话框和插件页面 |
| `QTabWidget`、`QStackedWidget`、`QTableWidget`、`QTreeWidget` | 插件页、侧栏配置、通道表和插件树 |
| `QComboBox`、`QPushButton`、`QToolButton`、`QSpinBox`、`QSlider` | 用户配置与控制输入 |
| `QPlainTextEdit`、`QSyntaxHighlighter` | Raw Viewer、日志和方向高亮 |
| `QPainter`、`QPainterPath` | Time Chart 和 Gauge 自绘 |
| `QSerialPort`、`QSerialPortInfo` | 串口读写、端口枚举和波特率扫描 |
| `QByteArray`、`QString`、`QVariantMap`、Qt containers | 字节流、文本、插件配置、命令和扩展属性 |
| `QJsonDocument`、`QJsonObject`、`QFile`、`QDataStream` | 会话 JSON、历史文件和预设读写 |
| `QTimer`、`QElapsedTimer`、`QRandomGenerator` | UI 批刷新、周期命令、AGV 心跳/超时和 session 生成 |

## 8. `AgvCanFdTest.cpp` 当前职责

该文件不是一个普通 QWidget，而是通过命令行启动的设备联调程序，当前集中承担：

- AGV CAN ID、ACK 状态和命令 sequence 常量；
- 小端整数读写、CRC16-CCITT；
- 工程 CAN 帧、心跳、控制、急停和 300 mm 命令编码；
- 安全状态、命令 ACK、驱动状态、里程和电机状态解析；
- `WaitingSafety` 到 `Finished` 的状态机；
- 100 ms 心跳、50 ms watchdog、超时和控制台报告；
- 成功、失败和安全急停收尾。

它适合作为已经跑通硬件链路的验收入口，但不应继续无限增长。业务协议、会话状态机、输出格式和测试启动器需要逐步拆开，才能被 GUI、自动测试和未来其他 AGV 功能复用。

## 9. 后续组件化设计

### 9.1 先区分“组件”和“插件”

- 组件是具有明确职责、公开接口和测试边界的代码单元，可以是静态库、普通类或 model/service；
- 插件是需要运行时发现、按平台装载或允许替换的组件；
- 不要为了“组件化”把所有小类都做成 DLL。设备类型、协议类型、视图和控制面板适合插件；AGV CRC、状态模型、状态机等更适合普通库和类。

### 9.2 静态、动态和逻辑组件的三级接入模型

后续界面开发建议统一使用下面三条接入路径。一个组件只能有一个主要注册入口，避免既在 `.ui` 中创建、又在插件扫描时重复创建。

| 接入类型 | 适用对象 | 创建/注册方式 | 当前状态 |
| --- | --- | --- | --- |
| 静态 UI | 固定导航、固定面板、固定对话框外壳 | Designer `.ui` + `setupUi()` + 明确 `objectName` | 已具备，主窗口正在使用 |
| 动态 UI/设备能力 | physical、protocol、visual、control 插件 | CMake MODULE + IID + JSON + `QPluginLoader` + `PluginManager` | 已具备，四类插件可以扫描和装配 |
| 逻辑组件 | `DebugCore`，以及后续真实需要的 service/model | `AppContext` 创建或引用，构造函数注入，页面集中 `connect()` | 最小装配层已具备，不含通用注册框架 |

#### 静态 UI 注册

静态控件由 `.ui` 唯一创建。`MainWindow` 在 `setupUi()` 后获取控件指针并绑定逻辑，不应再 `new` 一个同名控件覆盖 Designer 生成对象。复杂固定页面可以分别建立 `.ui + .h + .cpp`，再通过 Designer 的 Promote to 功能把占位 `QWidget` 提升为自定义类。

Promote 适用于编译期已知的自定义控件。运行时插件不能靠 Promote 完成，因为 Designer 打开 `.ui` 时并不知道部署目录里将出现哪些 DLL。

#### 动态组件注册

当前动态链路已经完整：

```text
CMake mcd_add_plugin(...)
  -> Q_PLUGIN_METADATA + Q_INTERFACES
  -> 插件 JSON 描述 type/platform/version
  -> PluginManager::scanPlugins()
  -> qobject_cast 分类
  -> MainWindow::populatePluginUi()
       ├─ visual: addTab + ChannelHub::subscribe
       ├─ control: addTab + commandGenerated -> DebugCore::sendCommand
       └─ physical/protocol: 加入选择器，连接时激活
```

因此，新的 visual/control 插件不需要修改 `MainWindow.ui`。只要实现接口、加入 CMake 并提供 manifest，主程序就能发现和挂载。Designer 的责任只是提供 `QTabWidget`、stack 或 dock 等“插槽区域”。

后续可给插件元数据增加 `area`、`order`、`icon`、`singleton`、`requiredCapabilities`，让主窗口根据描述决定放入 tab、dock、侧栏还是工具栏，而不是继续增加插件名称判断。

#### 逻辑组件注册

逻辑组件不应该通过 `QPluginLoader` 全部动态化，也不建议让各页面到全局 service locator 中自行查找。当前已经增加一个最小、具体的 `AppContext`：

```cpp
class AppContext {
public:
    AppContext();

    DebugCore* debugCore() const;
    QString pluginRoot() const;
    void scanPlugins() const;

private:
    DebugCore* m_core = nullptr;
};
```

`main.cpp` 创建唯一 `AppContext`，正常 GUI、`--smoke-test`、`--canfd-link-test` 和 `--distance-test-300` 都从它取得同一个 `DebugCore` 和插件目录。`MainWindow(AppContext&)` 与 `runAgvCanFdTest(..., AppContext&, ...)` 使用构造参数显式注入，没有字符串查询、宏注册或隐藏的全局 service locator。

这一层目前只解决已经真实重复的三件事：

- 核心实例取得与初始化；
- 插件根目录选择；
- 插件扫描入口。

它同时明确：

- 谁创建组件；
- 谁拥有和销毁组件；
- 哪些连接在何时建立/断开；
- 核心初始化和插件扫描从哪里进入；
- GUI 与命令行如何复用同一业务逻辑。

不要为了形式完整预先添加空接口、工厂或 `QHash<QString, QObject*>`。以后只有在 `AgvTestService` 等逻辑类确实实现且有真实调用方时，才把具体成员和类型安全 getter 加入 `AppContext`，再通过构造函数传给页面。这样每增加一个逻辑组件都只有四步：实现具体类、在 `AppContext` 中创建、暴露类型安全 getter、在页面中连接信号槽。

#### 推荐的界面装配顺序

```text
main.cpp
  1. 创建 QApplication
  2. 创建 AppContext，并初始化 DebugCore
  3. 创建 MainWindow(context)
  4. MainWindow::setupUi() 创建静态界面
  5. 绑定静态控件与 service 信号槽
  6. 扫描并挂载动态插件
  7. 恢复会话/页面状态
  8. show() 并进入事件循环
```

这个顺序固定后，Qt Creator 负责界面设计，新组件只需选择正确的注册路径即可对接，不需要每次重新理解整个主窗口。

### 9.3 推荐依赖方向

```text
app/ui
  -> application service / domain controller
  -> core
  -> sdk contracts

physical plugin  -> sdk
protocol plugin  -> sdk
visual plugin    -> sdk
control plugin   -> sdk

禁止：plugin A -> plugin B
禁止：core -> MainWindow
禁止：domain -> QWidget
禁止：physical -> 业务协议解析
```

依赖只能向下。跨层交互通过接口、数据结构和 Qt 信号/槽完成，不通过全局变量、直接持有其他插件实例或访问 `MainWindow`。

### 9.4 推荐新增目录

在不一次性推翻现有目录的前提下，可以逐步演进为：

```text
src/
├─ sdk/                         稳定的插件 ABI 和公共数据契约
├─ core/                        通用运行时、插件管理、线程和缓冲
├─ domain/
│  └─ agv/
│     ├─ AgvCanIds.h            CAN ID、opcode、状态枚举
│     ├─ AgvCodec.h/.cpp        纯编码/解码、CRC、长度校验
│     ├─ AgvMessages.h          强类型消息和值对象
│     ├─ AgvSession.h/.cpp      session、sequence 和心跳规则
│     └─ AgvMotionStateMachine.h/.cpp
├─ application/
│  ├─ DeviceConnectionService.h/.cpp
│  └─ AgvTestService.h/.cpp     把 domain 与 DebugCore 连接起来
├─ app/
│  ├─ MainWindow.*
│  └─ models/                   QAbstractItemModel / view-model
├─ plugins/                     仍按四类插件组织
└─ tests/
   ├─ unit/
   ├─ integration/
   └─ hardware/
```

目录只是结果，真正重要的是每个组件的边界和依赖方向。

### 9.5 AGV 代码拆分顺序

#### 第一步：提取无状态 Codec

把 `byteAt`、`readLe16/32`、`putLe16/32`、CRC 和各种 `make...` 函数移入 `AgvCodec`。接口返回明确结果，不直接打印日志，不启动定时器，不访问 `DebugCore`。

建议使用强类型结果：

```cpp
struct DecodeError {
    enum Code { TooShort, InvalidVersion, InvalidCrc, UnsupportedMessage } code;
    QString detail;
};
```

这样可以针对正确帧、短帧、坏 CRC、边界值和大小端编写纯单元测试，不需要真实 CAN 设备。

#### 第二步：提取会话和状态机

把 session、sequence、heartbeat、deadline 和 `Stage` 转为 `AgvSession` 与 `AgvMotionStateMachine`。状态机只接收事件并产生动作：

```text
输入事件：SafetyUpdated / AckReceived / OdomUpdated / Timeout / StopRequested
输出动作：SendHeartbeat / SendStop / SendClearFault / SendGoto / Finish / Fail
```

状态机不直接调用 `QApplication::exec()`，因此 GUI、命令行工具和测试都可以复用。

#### 第三步：应用服务连接核心层

`AgvTestService` 负责订阅 `DebugCore::framePublished`、把 CAN 帧转换为状态机事件、把状态机动作转换为 `DebugCore::sendCommand`，并对外发出 progress/succeeded/failed 信号。

#### 第四步：UI 只绑定状态

GUI 页面只展示 telemetry、当前 stage、错误和可用操作。按钮调用 service；UI 不重新实现 CRC、sequence、超时和安全判断。

#### 第五步：保留薄命令行入口

`runAgvCanFdTest()` 最终只负责创建 service、连接控制台输出、启动测试和映射退出码。硬件联调行为保持不变，但核心逻辑已经可测试、可复用。

### 9.6 数据契约设计

当前 `QVariantMap` 很灵活，适合插件 ABI 的早期阶段，但字段拼写和类型只能运行时发现。后续建议：

- SDK 边界保留 `DataFrame` 和兼容的 `QVariantMap attributes`；
- domain 内部立即转换为强类型消息，不让字符串 key 传播到全部业务代码；
- 给常用 attribute key 定义唯一常量，避免各文件复制 `"can_id"`、`"bytes"` 等字面量；
- 每个协议写明字节序、长度、单位、量程、版本、CRC 范围和非法输入策略；
- `quint16 channel index` 只作为当前内部索引，后续引入稳定 `ChannelKey`，避免不同协议发生通道碰撞；
- 改动公共契约时给出兼容期、迁移方式和 IID/版本策略。

### 9.7 配置组件化

`DeviceConfigDialog` 当前把所有配置都作为文本编辑，简单但缺少类型、范围和敏感性信息。建议为 physical 插件增加配置 schema，例如：

```json
{
  "key": "nominal_bitrate",
  "type": "integer",
  "default": 1000000,
  "minimum": 10000,
  "maximum": 1000000,
  "label": "Nominal bitrate",
  "restartRequired": true
}
```

由通用配置组件根据 schema 生成 `QSpinBox`、`QComboBox`、`QCheckBox` 或设备选择器。配置分三层：插件默认值、用户持久化值、当前会话覆盖值。校验必须在 UI 和插件 `open()` 两端都执行，不能只相信界面。

### 9.8 UI 组件化规则

- `MainWindow` 只负责壳层和页面装配，业务状态进入 service/model；
- 列表和表格数据逐步迁移到 `QAbstractItemModel`，避免控制器里大量直接改单元格；
- visual 插件批量接收数据并定时刷新，不要每个采样点都 repaint；
- 所有 UI 文案继续使用 `tr()`；
- QWidget 只存在于 UI 线程；后台结果通过 queued signal 传回；
- 组件必须定义空状态、未连接、错误、暂停、溢出和销毁时的行为。

### 9.9 生命周期和所有权

- QObject 优先用 parent-child 管理；非 QObject 可使用值语义或 `std::unique_ptr`；
- `QPluginLoader` 的生命周期必须长于插件实例；卸载前先断开连接并停止后台 I/O；
- 定时器属于创建它的线程，不跨线程直接 start/stop；
- queued lambda 捕获 QObject 时使用 `QPointer`，防止事件到达前对象已销毁；
- 设备只允许一个明确 owner 执行 open/close/read/write；
- 退出顺序固定为：停止新命令 → 停止定时器 → 关闭设备 → 断开信号 → 回收线程 → 卸载插件。

### 9.10 错误与可观测性

统一区分：

- `status`：连接、配置、阶段等普通状态；
- `warning`：可恢复异常或数据不连续；
- `error`：操作失败，需要用户介入；
- `overflow`：为保持实时性主动跳过旧数据；
- `safety`：急停、防撞、命令拒绝等不可与普通 error 混淆的事件。

建议错误至少带 `component`、`code`、`message`、`context`、`timestamp`。日志不得只写“失败”，应能定位设备、插件、命令 sequence 和状态机 stage；同时避免把大块原始数据无限写入 UI 文本框。

## 10. 新组件开发流程

### 10.1 通用流程

1. 写清组件职责、输入、输出、线程和不负责事项；
2. 先定义数据契约与失败行为，再写 UI；
3. 在独立 target 中实现最小可测试核心；
4. 增加单元测试或最小 smoke test；
5. 接入 `DebugCore` 或相应插件接口；
6. 检查对象所有权、线程亲和性、断连和退出路径；
7. 更新 CMake、manifest、开发文档和测试记录；
8. 完成 Debug/Release 构建、插件扫描和目标平台验证。

### 10.2 新增 physical 插件

实现 `IPhysicalPlugin`，只提供 open/close/isOpen/write 和原始字节信号。必须测试设备不存在、配置错误、打开成功、拔出、写失败和关闭。不要在物理层解析业务字段。

### 10.3 新增 protocol 插件

实现 `IProtocolPlugin`，维护必要的流式接收缓存，正确处理半包、粘包、噪声、非法长度和重同步。解析输出 `DataFrame`，发送侧实现 `encodeCommand()`。不要直接访问串口、USB 或 QWidget。

### 10.4 新增 visual 插件

实现 `IVisualPlugin`，明确订阅通道和历史范围，限制内部数据窗口，采用批处理/定时刷新，处理 NaN、空数据、单位变化、generation 重置和 overflow。

### 10.5 新增 control 插件

实现 `IControlPlugin`，对输入做即时校验，只发结构化命令。周期发送必须可停止，断连后控件状态必须恢复，发送失败必须可见。

### 10.6 CMake 接入示例

```cmake
mcd_add_plugin(my_protocol protocol
    src/plugins/protocol/my_protocol/MyProtocolPlugin.cpp
    src/plugins/protocol/my_protocol/MyProtocolPlugin.h
    src/plugins/protocol/my_protocol/my_protocol.json
)
```

插件头文件还需要：

```cpp
Q_OBJECT
Q_PLUGIN_METADATA(IID IProtocolPlugin_iid FILE "my_protocol.json")
Q_INTERFACES(IProtocolPlugin)
```

manifest 至少写明 `type`、`name`、`version`、`interface`、`description` 和 `platforms`。类返回的 `name()` 应与 manifest 名称保持一致，避免配置和 UI 查找混乱。

## 11. 测试分层与完成标准

### 11.1 测试分层

| 层级 | 运行条件 | 主要内容 |
| --- | --- | --- |
| Unit | 无设备、无 GUI | Codec、CRC、环形池、状态机、配置校验 |
| Component | 可使用 Qt event loop | 插件接口、半包粘包、queued signal、model/service |
| Integration | 构建后的插件目录 | 插件扫描、激活、发送/接收闭环、会话和历史 |
| Hardware | 指定设备和安全场地 | 串口、WinUSB、CAN FD、AGV 安全与运动 |
| Packaging | 干净机器/虚拟机 | `windeployqt` 产物、驱动说明、启动和插件加载 |

### 11.2 Definition of Done

一个组件完成至少满足：

- 接口、线程、所有权和失败行为有文档；
- CMake Debug/Release 构建通过；
- 对应自动测试通过，硬件组件有日期化手工测试记录；
- 插件能扫描、平台过滤正确、错误配置有明确消息；
- 不存在 UI 线程阻塞 I/O、后台线程访问 QWidget、无上限缓存；
- 新增配置可保存/加载，默认值和范围清楚；
- 发布目录不依赖开发机的绝对路径；
- 相关 README、组件文档和变更记录同步更新。

## 12. 后续写组件文档的统一模板

以后每个组件可以复制以下提纲，文档放在 `docs/components/<component-name>.md`：

```markdown
# 组件名称

## 1. 目的和用户场景
说明为什么存在、解决什么问题。

## 2. 职责边界
负责什么；明确不负责什么。

## 3. 依赖关系
允许依赖谁；谁可以依赖它；禁止依赖谁。

## 4. 公共接口
类、方法、信号、数据类型、线程要求和版本。

## 5. 数据/协议契约
字段、类型、字节序、单位、范围、默认值、示例和兼容策略。

## 6. 状态与时序
状态机、正常流程、取消、超时、断连和退出流程。

## 7. 配置
key、类型、默认值、范围、是否需要重连/重启。

## 8. 错误处理与日志
错误码、用户提示、恢复策略和需要记录的上下文。

## 9. 线程与所有权
对象创建线程、调用线程、锁、队列和销毁顺序。

## 10. 测试
单元、组件、集成、硬件、边界和回归用例。

## 11. 构建与发布
CMake target、依赖、产物位置、平台限制和部署步骤。

## 12. 已知限制与演进计划
当前未覆盖范围、风险、兼容策略和后续任务。
```

文档中的代码名、配置 key、协议字段和测试命令必须能在仓库中找到对应实现；规划项应标记“建议/计划”，不能写成“已经支持”。

## 13. 建议实施路线

### 阶段 A：固定可重复构建

- 将 Qt 6.11.0、x64、生成器和构建命令作为团队基线；
- 增加 CMake presets，消除公共配置中的个人绝对路径；
- 统一单配置/多配置生成器的应用与插件输出路径；
- 在 CI 中执行构建和 `--smoke-test`。

### 阶段 B：先拆 AGV 纯逻辑

- 提取 `AgvCodec`、消息类型和 CRC 测试；
- 提取状态机并覆盖超时、拒绝、故障、取消和完成路径；
- 保持现有硬件测试入口输出兼容。

### 阶段 C：服务与 UI 解耦

- 增加 `AgvTestService`；
- `MainWindow` 只做页面装配；
- 引入 model/view，减少控件直接承载业务状态。

### 阶段 D：SDK 和配置成熟化

- 配置 schema、类型校验和敏感字段处理；
- 稳定 `ChannelKey` 和批量 UI 数据契约；
- 定义插件 API/IID 版本和兼容策略。

### 阶段 E：交付工程化

- 自动化单元/组件/集成测试；
- Windows 干净机部署验证；
- Linux 支持矩阵；
- 硬件测试记录、版本化协议文档和发布清单。

## 14. 关联文档

- `../README.md`：项目能力与快速构建入口；
- `../2026-06-09-mcu-debug-tool-design.md`：最初总体设计；
- `plugin-development-spec.md`：四类插件的开发规范；
- `realtime-pool-refactor-plan.md`：实时缓冲、线程和水位分发设计；
- `agv-canfd-integration-test-2026-08-11.md`：AGV CAN FD 已验证配置和现场联调记录。
