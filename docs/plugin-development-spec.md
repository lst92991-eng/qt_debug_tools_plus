# Plugin Development Spec

## 目标

本文定义 MCU Debug Tool 插件的完整开发流程和设计规范。

适用插件类型：

- physical：物理连接插件，如串口、USB Raw、CandleLight CANFD。
- protocol：协议解析和命令编码插件，如 Raw、Serial Numeric、CAN Frame。
- visual：可视化插件，如 Raw Viewer、Time Chart、Gauge。
- control：控制插件，如 Raw Control、Slider Widget。

核心原则：

```text
物理层只处理字节进出。
协议层只处理编码解码。
核心层只处理装配、水位和分发。
UI 插件只在 UI 线程渲染和产生用户命令。
```

## 插件目录规范

源码目录：

```text
src/plugins/<type>/<plugin_id>/
  PluginClass.h
  PluginClass.cpp
  <plugin_id>.json
```

构建输出：

```text
build/bin/<config>/plugins/<type>/
  <plugin_id>.dll
  <plugin_id>.json
```

命名规则：

- 目录使用小写 snake_case。
- C++ 类使用 PascalCase，并以 `Plugin` 结尾。
- JSON 文件名与目录名一致。
- 插件显示名可以使用空格，但必须稳定。

示例：

```text
src/plugins/protocol/can_frame/
  CanFramePlugin.h
  CanFramePlugin.cpp
  can_frame.json
```

## Manifest 规范

每个插件必须提供 JSON manifest。

最小字段：

```json
{
  "type": "protocol",
  "id": "can_frame",
  "name": "CAN Frame",
  "version": "1.0.0",
  "interface": "IProtocolPlugin",
  "description": "Parse CA FD framed CAN payloads",
  "platforms": ["all"]
}
```

字段说明：

- `type`：`physical`、`protocol`、`visual`、`control`。
- `id`：稳定机器名，不随显示语言变化。
- `name`：用户可见名称。
- `version`：语义化版本。
- `interface`：实现的 SDK 接口。
- `platforms`：`all`、`windows`、`linux`、`macos`。
- `description`：一句话说明插件用途。

建议字段：

```json
{
  "author": "project",
  "priority": 10,
  "capabilities": ["canfd", "bulk-usb"],
  "config_schema": []
}
```

## 线程契约

### Physical 插件

目标线程：

- 运行在 `IngestThread`。
- 阻塞读写不能发生在 UI 线程。

规则：

- `open/close/write` 只由采集线程调用。
- `close` 必须可控退出，不能无限等待。
- 所有设备句柄只在采集线程或插件内部受控 worker 中访问。
- 设备错误通过 `errorOccurred` 或状态事件上报。
- 不允许直接访问 `DebugCore`、`MainWindow` 或可视化插件。

必须处理：

- 设备不存在。
- 设备被占用。
- 设备运行中拔出。
- 写入部分成功。
- 关闭时读线程仍阻塞。

### Protocol 插件

目标线程：

- 运行在 `IngestThread`。

规则：

- 只接收 raw bytes 或 raw chunk。
- 只输出解析后的帧或数值样本。
- 不做设备 I/O。
- 不做 UI 更新。
- 保持协议状态机可 reset。

必须处理：

- 粘包。
- 半包。
- 噪声前缀。
- 非法长度。
- 校验失败。
- raw 队列或蓄水池发生覆盖后，协议状态重同步。

### Visual 插件

目标线程：

- 只运行在 UI 线程。

规则：

- 继承 `QWidget` 的插件不得移动到后台线程。
- 不得直接读取设备。
- 不得执行长时间文件 I/O。
- 不得在每个样本到来时强制 repaint。
- 使用批量数据和定时刷新。

### Control 插件

目标线程：

- 只运行在 UI 线程。

规则：

- 只产生用户命令。
- 不直接调用物理插件。
- 命令通过核心层进入采集线程。
- 命令失败必须能反馈给用户。

## 数据契约

### Raw 和 Numeric 分流

Raw 数据：

- 用于 Raw Viewer、调试日志、TX/RX 展示。
- 默认不进入数值蓄水池。

Numeric 数据：

- 用于曲线、仪表、历史窗口。
- 必须有稳定通道身份。
- 进入 `RingBufferPool`。

### 通道身份

新插件不应依赖 `quint16` 作为全局通道身份。

推荐使用：

```cpp
struct ChannelKey {
    QString source;
    QString group;
    QString name;
    quint32 numericId;
};
```

短期兼容：

- 旧插件可以继续填 `ChannelSample::index`。
- 新协议应同时提供 name/unit/attributes，便于后续迁移。

## 蓄水池协作规范

插件不直接管理蓄水池。

写入路径：

```text
PhysicalPlugin -> ProtocolPlugin -> IngestWorker -> RingBufferPool
```

读取路径：

```text
RingBufferPool -> ChannelDispatcher -> UI
```

插件必须接受这些事实：

- 蓄水池只保留最新窗口。
- 历史可能断裂。
- 分发器可能跳过旧数据。
- 可视化插件必须能处理 skipped/reset 事件。

## 配置 Schema

插件应声明配置项，而不是只返回自由 `QVariantMap`。

建议 schema：

```json
{
  "key": "baud",
  "label": "Baud Rate",
  "type": "int",
  "default": 115200,
  "min": 1200,
  "max": 4000000,
  "required": true
}
```

支持类型：

- `string`
- `int`
- `double`
- `bool`
- `enum`
- `hex`
- `path`

规则：

- UI 根据 schema 生成控件。
- 插件仍需在 `open` 前做最终校验。
- 缺失字段使用默认值。
- 非法字段返回明确错误。

## Physical 插件开发流程

1. 确认设备打开方式和驱动要求。
2. 定义配置 schema。
3. 实现 `defaultConfig`。
4. 实现 `open`，只做必要初始化和句柄获取。
5. 实现读循环，确保可取消。
6. 实现 `write`，处理部分写入和错误。
7. 实现 `close`，保证重复调用安全。
8. 上报连接状态、错误和断开事件。
9. 写最小 smoke test 或手动测试步骤。

检查项：

- 设备拔出不会卡死。
- `close` 可重复调用。
- 写失败不会崩溃。
- 线程退出后再释放句柄。
- 不直接依赖 UI。

## Protocol 插件开发流程

1. 写清协议帧格式。
2. 中央化协议常量。
3. 实现 parser 状态机。
4. 实现 encoder。
5. 定义 parse error。
6. 支持 reset。
7. 对半包、粘包、噪声做测试。
8. 输出 raw payload 和 numeric samples。

检查项：

- 不发明未验证的字段。
- 非法长度不会越界。
- 丢数据后可以重新同步。
- 编码和解码使用同一份常量。
- 不访问物理设备。

## Visual 插件开发流程

1. 明确订阅哪些通道或事件。
2. 接收批量 UI 数据。
3. 内部维护有限窗口。
4. 使用定时刷新。
5. 支持 reset/skipped 提示。
6. 对空数据、NaN、单位变化做处理。

检查项：

- 不逐样本 repaint。
- 不在 UI 线程做重计算。
- 不持有后台线程对象。
- 文本和图表有最大容量。
- 清空和暂停行为明确。

## Control 插件开发流程

1. 定义用户输入控件。
2. 实时校验输入。
3. 生成结构化命令。
4. 通过核心层发送命令。
5. 显示发送失败或 busy 状态。
6. 不直接调用 physical/protocol 插件。

检查项：

- 命令不会静默丢失。
- 周期发送可以停止。
- 断开连接时按钮状态正确。
- 输入历史和预设有容量上限。

## 错误和 Overflow 规范

所有插件和核心模块应区分：

- `error`：需要用户知道的问题，如设备打开失败。
- `warning`：可恢复异常，如协议丢包。
- `overflow`：系统主动丢弃旧数据以保持实时性。
- `status`：连接状态、当前配置、统计信息。

Overflow 至少包含：

```cpp
struct OverflowEvent {
    QString stage;
    quint64 skippedSeq = 0;
    quint64 droppedBytes = 0;
    quint64 droppedFrames = 0;
    qint64 timestamp_us = 0;
};
```

UI 规则：

- Raw Viewer 插入一行提示。
- Chart 断线或标记跳点。
- 状态栏显示累计次数。

## 测试要求

每个新插件至少提供：

- 构建通过。
- 插件可被扫描。
- 默认配置可展示。
- 错误配置有错误消息。
- 基本输入输出 smoke test。

协议插件还应提供：

- 半包测试。
- 粘包测试。
- 噪声重同步测试。
- 非法长度测试。
- 编码回归测试。

物理插件还应提供手动测试记录：

- 设备不存在。
- 设备打开成功。
- 设备拔出。
- 发送成功。
- 关闭成功。

## 发布检查

发布前检查：

- manifest 字段完整。
- 插件 DLL/SO 和 JSON 同目录。
- 平台限制正确。
- Windows Release 包包含 Qt runtime 和 platform plugins。
- smoke test 通过。
- README 写明硬件和驱动要求。
- 新协议格式写入文档。

## 禁止事项

- 禁止插件直接调用其他插件。
- 禁止插件直接访问 `MainWindow`。
- 禁止 QWidget 进入后台线程。
- 禁止在协议插件中打开设备。
- 禁止在物理插件中解析业务协议。
- 禁止无上限缓存。
- 禁止静默吞掉命令。
- 禁止复制粘贴协议常量到多个模块。

