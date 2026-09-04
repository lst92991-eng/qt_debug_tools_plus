# MCU Debug Tool 十阶段复现路线

> 本文件是课程大纲，不是零基础实操课。每一天只有在对应代码提交、完整逐步课件、验证记录和 Git 标签都存在后，才算可以交给初学者照做。当前已完成的零基础课是 Day 0 和 Day 1。

## 1. 复现方式

目标不是把 `dev` 的文件一次性复制回来，而是从当前 Day 0 骨架开始，按依赖方向逐层恢复完整上位机。每个阶段都必须满足：能编译、能说明、能测试、能单独提交。

完整版本是课程维护者核对行为的参考，不是学习者的复制来源。零基础学习者不需要自行进入 `dev` 查找或比较代码；每一课必须直接给出完整操作和代码。课程维护者应先写清接口和证据，再对照 `dev` 查漏。协议 ID、字节长度、端序、CRC、握手、超时和重试绝不能凭印象填写。

## 2. 最终依赖方向

```text
APP / UI
    |
    v
CORE：插件管理、连接编排、通道分发、缓冲池
    |
    v
SDK：DataFrame 与四类插件接口

physical plugin ----> SDK
protocol plugin ----> SDK
visual plugin ------> SDK
control plugin -----> SDK
```

运行时数据流：

```text
设备字节 -> physical -> protocol -> DataFrame -> ChannelHub -> visual
用户操作 -> control  -> command  -> protocol   -> physical   -> 设备
```

约束：

- UI 不直接操作串口、USB 或 CAN；
- physical 只收发原始字节，不解析业务协议；
- protocol 不访问设备和 QWidget；
- 插件彼此不直接依赖；
- 公共协议常量只定义一处；
- 后台线程不访问 QWidget。

## 3. 每个阶段的制作标准

下面是课程维护者的固定流程。正式课件必须把其中的占位内容替换为确切菜单、文件名、完整代码和预期输出，不能把本节原样交给初学者执行。

开始前：

```powershell
git switch teaching/from-zero
git status
```

在 Qt Creator 中：

1. 先读本阶段目标和完整版本对应文件；
2. 用 `File > New File` 创建类，不手工伪造 Qt 的 MOC/UIC 产物；
3. 把新文件加入对应 CMake target；
4. 运行 `Build > Run CMake`；
5. `Ctrl+B` 构建；
6. 运行本阶段最小测试；
7. 检查对象所有权、信号槽连接和退出路径；
8. 写本阶段教学笔记后再提交。

提交前：

```powershell
git diff --check
git status --short
git diff
git add <本阶段文件>
git diff --cached
git commit -m "feat(day01): build main window shell"
```

阶段完成后可打标签：

```powershell
git tag teaching-day-01
git push origin teaching/from-zero --tags
```

标签号和提交信息中的 day 号要与文档一致。

## 4. Day 1：主窗口和 Qt 基础

### 目标

把空白窗口做成调试工具的静态外壳，学习 QObject、信号槽、布局和 Designer，不接任何真实设备。

### 添加内容

- 菜单栏、状态栏；
- 物理插件和协议插件下拉框的占位 UI；
- 连接、断开、重新扫描按钮；
- 可视化区、控制区和活动日志区；
- 使用 layout 管理尺寸，禁止写死控件坐标。

### Qt Creator 操作

双击 `MainWindow.ui` 进入 Designer，先放容器和布局，再放控件。控件命名使用职责名，例如 `connectButton`、`physicalComboBox`，不用 `pushButton_3`。

### 验收

- 缩放窗口时布局正常；
- 按钮通过信号槽更新状态栏或活动日志；
- 没有串口、插件或协议代码；
- 提交建议：`feat(day01): build main window shell`。

## 5. Day 2：第一种 SDK 数据契约

### 目标

先恢复最底层、最容易单独验证的公共数据契约 `DataFrame`。四类插件接口不在同一天集中抄写，而是在出现第一个真实调用者时逐步引入。

### 添加内容

- `src/sdk/DataFrame.h/.cpp`；
- `FrameDirection`、`ChannelSample` 和 `DataFrame`；
- 自定义 Qt 元类型注册；
- 微秒级系统时间函数；
- 独立 `mcd_sdk` 静态库 target；
- 一个由 CTest 执行的无界面测试。

### 学习重点

- `enum class`、`struct` 和默认值；
- 原始字节、数值通道和扩展属性为什么放在同一帧中；
- Qt 自定义类型注册为什么是后续跨线程信号槽的前提；
- CMake 如何把源码组织为静态库并挂入 CTest。

### 验收

- 主程序链接 `mcd_sdk` 后仍能运行；
- `DataFrame` 不依赖设备 API 和页面逻辑；
- 默认方向、NaN、时间戳、原始负载和通道字段测试通过；
- 提交建议：`feat(day02): add shared data frame`。

## 6. Day 3：插件装载

### 目标

让程序从目录扫描 Qt 动态插件，理解“接口、实现、manifest、构建产物”四者关系。

### 添加内容

- `PluginManager`；
- `mcd_add_plugin()` CMake 函数；
- 第一个 `IProtocolPlugin` 接口；
- 一个最小 `raw_passthrough` 协议插件；
- 插件 JSON manifest；
- 主窗口“扫描插件”动作。

### 学习重点

- `QPluginLoader` 生命周期必须长于插件实例；
- `Q_PLUGIN_METADATA` 与 JSON 文件名匹配；
- 插件输出到 `bin/plugins/<type>`；
- 扫描失败要显示文件名和错误原因。

### 验收

- 空目录扫描不崩溃；
- 能发现一个协议插件；
- 非法 manifest 给出明确错误；
- Debug 主程序不加载 Release 插件；
- 提交建议：`feat(day03): load runtime plugins`。

## 7. Day 4：核心分发和有界缓冲

### 目标

恢复 `ChannelHub`、`RingBufferPool` 和 `DebugCore`，建立单向数据流。

### 添加内容

- 物理层原始数据入口；
- 协议解析后的 `DataFrame` 发布；
- 通道元数据和订阅；
- 固定容量环形缓冲；
- 溢出统计和批量 UI 分发。

### 学习重点

- 采集线程与 UI 线程的边界；
- queued signal；
- 高速输入时为什么必须有界并允许丢旧数据；
- `DebugCore` 负责编排，不把所有逻辑塞进一个巨型类。

### 验收

- 使用假数据源持续发送数据；
- UI 不冻结；
- 达到容量后内存不继续增长；
- 溢出数可观察；
- 提交建议：`feat(day04): route frames through bounded pools`。

## 8. Day 5：通用串口物理插件

### 目标

接入第一种真实设备通道，只解决串口打开、关闭、收发和错误报告。

### 添加内容

- CMake 增加 `Qt6::SerialPort`；
- 引入 `IPhysicalPlugin` 接口；
- `SerialGenericPlugin`；
- `DeviceConfigDialog` 的串口参数；
- 端口、波特率、数据位、校验、停止位和流控；
- 拔出、重连和关闭流程。

### 学习重点

- `QSerialPortInfo` 枚举；
- 串口对象的线程归属；
- `readyRead` 只读取字节，不解析协议；
- 设备不存在、占用和配置错误的提示。

### 验收

- 无串口时仍能启动；
- 错误端口不崩溃；
- 串口回环可收发；
- 断开后定时发送停止；
- 提交建议：`feat(day05): add generic serial transport`。

## 9. Day 6：原始数据闭环

### 目标

用最简单协议完成第一条端到端链路。

### 添加内容

- 完善 `RawPassthroughPlugin`；
- 引入 `IVisualPlugin` 和 `IControlPlugin` 接口；
- `RawViewerPlugin`：HEX/ASCII、时间戳、过滤、暂停、清空；
- `RawControlPlugin`：HEX 校验、发送历史、周期发送；
- 主窗口动态挂载 visual/control 插件。

### 学习重点

- 接收路径和发送路径对称；
- 视图只订阅数据，不持有串口；
- 周期发送必须能停止；
- 文本显示要限长，不能无限追加。

### 验收

- 串口回环中发送 `01 02 03`，Raw Viewer 能看到相同字节；
- 非法 HEX 被 UI 拒绝；
- 暂停只影响显示，不破坏底层收包；
- 提交建议：`feat(day06): complete raw data loop`。

## 10. Day 7：数值协议和可视化

### 目标

从字节升级为带通道和数值的 `DataFrame`，恢复曲线、仪表和滑块控制。

### 添加内容

- `SerialNumericPlugin`；
- `TimeChartPlugin`；
- `GaugePlugin`；
- `SliderWidgetPlugin`；
- 通道选择、单位和历史范围。

### 学习重点

- 半包、粘包、噪声和重同步；
- `QPainter` 坐标变换；
- 批量刷新而不是每个采样点 repaint；
- NaN、空数据、量程变化和 overflow。

### 验收

- 能解析单值、`key=value` 和约定的 JSON 遥测；
- 高频假数据下 UI 可操作；
- 曲线历史有容量上限；
- 提交建议：`feat(day07): visualize numeric channels`。

## 11. Day 8：USB 物理插件

### 目标

在保持上层不变的前提下增加 Windows WinUSB 和 Linux libusb 通道。

### 添加内容

- `UsbRawWinPlugin`；
- `UsbRawLinuxPlugin`；
- CMake 平台条件；
- GUID、VID/PID、端点和超时配置；
- 设备拔出与资源回收。

### 学习重点

- 平台 API 只留在 physical 插件；
- Windows 句柄、overlapped I/O 和取消顺序；
- Linux 找不到 libusb 时仍能构建，但运行时明确报告不可用；
- 驱动切换前记录设备原驱动。

### 验收

- 无设备启动和扫描正常；
- 错误 GUID/VID/PID 有清楚提示；
- 关闭顺序不会留下后台线程；
- 提交建议：`feat(day08): add raw usb transports`。

## 12. Day 9：CAN Frame 与 CandleLight CAN FD

### 目标

恢复 CAN 帧协议和 gs_usb/CandleLight FD 物理通道，并先完成只读链路验证。

### 添加内容

- `CanFramePlugin`；
- `CandleLightCanFdPlugin`；
- CAN ID、DLC、FD/BRS 标志和 payload 映射；
- nominal/data bitrate 配置；
- `--canfd-link-test` 无界面链路测试。

### 学习重点

- CAN 字段、USB 控制请求和字节长度必须来自 `dev` 中的已验证实现或固件资料；
- 物理层 gs_usb 包与上层 CAN `DataFrame` 分离；
- 总线配置、启动、停止和设备关闭有明确状态；
- 未通过链路测试前，不发送运动命令。

### 验收

- 没有 CAN 设备时给出可理解错误；
- 插件扫描数量正确；
- 指定安全台架上完成收帧/发帧链路测试；
- 提交建议：`feat(day09): add candlelight can fd link`。

## 13. Day 10：应用集成、持久化和 AGV 测试

### 目标

完成会话、重连、日志、AGV 测试和发布收尾，但不让业务逻辑重新耦合回 `MainWindow`。

### 添加内容

- `AppContext` 作为应用组合根；
- 会话保存/加载和通道映射；
- 重连、错误分级和退出清理；
- AGV codec、session、运动状态机和 application service；
- `--distance-test-300` 安全台架测试；
- `windeployqt` 发布步骤和最终文档。

### 学习重点

- AGV 编码/解码、状态机、设备发送和 UI 分开；
- safety 事件不与普通 error 混淆；
- 测试先覆盖 codec/CRC/超时/拒绝，再做硬件运动；
- 退出顺序：停止新命令、停止定时器、关闭设备、断开信号、回收线程、卸载插件。

### 验收

- Release 全量构建通过；
- `--smoke-test` 插件数量正确；
- 单元、组件、集成测试通过；
- 硬件测试有日期、设备和安全条件记录；
- 干净机器能启动发布目录；
- 提交建议：`feat(day10): complete integrated debug tool`。

## 14. 每日教学笔记模板

每完成一个模块，在 `docs/modules/` 增加编号文档：

```markdown
# 01 模块名

## 目标
## 输入
## 设计
## 文件
## 替换影响
## 验证
## 未决问题
```

只写已经实现并验证的事实。规划项明确标记“计划”，协议不确定项记录证据来源和假设，不能写成已经支持。

## 15. 现在应该做什么

当前只做 Day 0：安装环境、打开工程、选择正确 Kit、构建并运行骨架。你确认看到窗口后，再开始 Day 1 的主界面静态布局。一次只恢复一层，这样每个错误都有很小的排查范围。
