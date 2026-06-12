# Realtime Pool Refactor Plan

## 目标

本规划用于把当前上位机从“主线程分发 + 每通道长历史缓存”重构为更稳定的实时数据架构。

最终目标：

- 物理层数据上来后，在采集线程内直接完成协议解析。
- 解析后的数值数据直接写入唯一蓄水池 `RingBufferPool`。
- 蓄水池始终只保留最新一批数据，容量由前端配置，但受硬上限保护。
- 分发器在独立线程读取蓄水池，并记录每个消费者的分发水位。
- 如果分发水位落后于蓄水池最老水位，分发器直接追到蓄水池当前可读窗口，并记录跳过数据。
- UI 线程只做渲染，不参与设备读写、协议解析、历史缓存维护。

一句话原则：

```text
数据可以断，程序不能卡；蓄水池只保最新窗口，UI 落后就追尾。
```

## 当前问题

当前代码已经有插件分层，但数据路径仍有几个风险：

- USB/CAN 物理插件部分有工作线程，串口和协议解析仍主要在主线程。
- `RingBufferPool` 默认每通道 100 万点，Raw/CAN 字节展开后容易造成内存压力。
- `DebugCore::publish()` 同时做元数据合并、历史写入、UI 分发，职责偏重。
- 可视化插件直接消费每个 `DataFrame`，缺少统一的分发水位和落后处理。
- 当前通道 ID 是 `quint16`，对 CAN ID、raw byte、虚拟信号等复合场景不够稳。

## 目标架构

```text
                 UI Thread
        MainWindow / VisualPlugin / ControlPlugin
                 ▲
                 │ queued UiBatch
                 │
          DispatchThread
        ChannelDispatcher / BatchBuilder
                 ▲
                 │ read by seq watermark
                 │
          RingBufferPool
        newest window only, seq based
                 ▲
                 │ direct write
                 │
           IngestThread
        PhysicalPlugin + ProtocolPlugin
```

线程职责：

- `IngestThread`：设备打开、读取、写入、协议解析、写入蓄水池。
- `DispatchThread`：按订阅关系从蓄水池读取最新窗口，生成 UI 批量消息。
- `UI Thread`：渲染界面，接收用户命令，展示状态和 overflow 统计。

禁止事项：

- 不允许 `DispatchThread` 直接调用 `QWidget` 或可视化插件方法。
- 不允许 UI 线程执行阻塞设备 I/O。
- 不允许协议插件直接访问 UI。
- 不增加 RawChunkQueue、ParsedFrameQueue、UiFrameQueue 这类大中间缓存。

## 蓄水池模型

`RingBufferPool` 从“每通道无限倾向历史”改为“最新窗口池”。

核心状态：

```cpp
struct PoolSnapshot {
    quint64 oldestSeq = 0;
    quint64 newestSeq = 0;
    quint64 generation = 0;
    qint64 oldestTimestampUs = 0;
    qint64 newestTimestampUs = 0;
};
```

规则：

- 每写入一批解析结果，分配单调递增 `seq`。
- 蓄水池只保留最新 N 条、N 秒或 N MB 数据。
- UI 可以配置 N，但必须经过最小值、默认值、最大值校验。
- 容量缩小或清空时递增 `generation`。
- 分发器发现 `generation` 变化时，必须重置自己的水位。

推荐容量配置：

```text
minSamples      1000
defaultSamples 100000
maxSamples      1000000

minWindowMs      1000
defaultWindowMs 10000
maxWindowMs     120000

maxMemoryMB 256
```

第一阶段可以先使用 `maxSamples`，第二阶段再加 `maxWindowMs` 和 `maxMemoryMB`。

## 水位分发规则

每个消费者独立记录水位，不能只用全局水位。

```cpp
struct DispatchCursor {
    QString consumerId;
    QList<ChannelKey> channels;
    quint64 nextSeq = 0;
    quint64 observedGeneration = 0;
};
```

分发流程：

```text
snapshot = pool.snapshot()

if cursor.observedGeneration != snapshot.generation:
    cursor.nextSeq = snapshot.oldestSeq
    cursor.observedGeneration = snapshot.generation
    notify reset

if cursor.nextSeq < snapshot.oldestSeq:
    skipped = snapshot.oldestSeq - cursor.nextSeq
    cursor.nextSeq = snapshot.oldestSeq
    notify skipped

batch = pool.read(cursor.channels, cursor.nextSeq, snapshot.newestSeq)
cursor.nextSeq = snapshot.newestSeq + 1
emit uiBatchReady(batch)
```

落后策略：

- 分发器慢于蓄水池时，不阻塞写入。
- 分发器直接追到 `oldestSeq`。
- UI 收到 `skipped` 后应显示断点或提示，不假装数据连续。

## 数据类型调整

当前 `quint16 channel` 可以短期保留，但重构目标应引入稳定通道键。

建议：

```cpp
struct ChannelKey {
    QString source;    // physical/protocol/source name
    QString group;     // raw/can/serial/json
    QString name;      // temp, voltage, CAN123[0]
    quint32 numericId; // optional fast index
};
```

`RingBufferPool` 内部可继续用紧凑整数索引，但对外不要把 `quint16` 当全局身份。

数据分层：

- `RawPayload`：原始负载，用于 Raw Viewer 和调试，不默认进入数值蓄水池。
- `ChannelSample`：可画图、可缓存的数值样本。
- `UiBatch`：分发线程发给 UI 的批量结果。
- `OverflowEvent`：任何跳水位、清池、容量覆盖都要记录。

## 满池策略

本项目采用“保持最新窗口”策略，不做无限排队。

规则：

- 写入永不等待 UI。
- 新数据到来时，旧数据被覆盖或裁剪。
- 如果覆盖导致某个分发器落后，分发器下次读取时自动追到 `oldestSeq`。
- 清池、resize、配置变化都递增 `generation`。
- 控制命令不走这个丢弃策略，命令满载时返回 busy 或错误。

## 命令路径

命令路径不使用蓄水池。

```text
UI Thread
  -> DebugCore::sendCommand
  -> queued invoke IngestWorker::sendCommand
  -> ProtocolPlugin::encodeCommand
  -> PhysicalPlugin::write
```

规则：

- 物理设备对象只在 `IngestThread` 访问。
- 命令不能静默丢弃。
- 连接关闭、停止中、队列忙时要返回明确错误。
- TX 仍可发布一条轻量事件给 UI，用于 Raw Viewer 和计数。

## 类职责重构

### DebugCore

重构后职责：

- 管理线程对象生命周期。
- 管理 active physical/protocol。
- 提供 UI 调用入口。
- 转发状态、错误、overflow、UI batch。

不再直接做：

- 高频协议解析。
- 高频逐帧 UI 分发。
- 物理设备直接读写。

### IngestWorker

职责：

- 拥有或访问当前物理插件和协议插件。
- 在同一线程内完成 `read -> feedBytes -> parsed frame -> pool.write`。
- 处理 `open/close/writeCommand`。
- 发生协议重置、设备错误、写失败时发状态事件。

### RingBufferPool

职责：

- 线程安全保存最新窗口。
- 提供 `snapshot()`。
- 提供 `readRange(channels, fromSeq, toSeq)`。
- 提供 `configureCapacity(...)`。
- 提供 overflow/reset 统计。

### ChannelDispatcher

职责：

- 保存订阅关系。
- 保存每个消费者的 `DispatchCursor`。
- 定时或收到 `dataCommitted` 后读取蓄水池。
- 生成 `UiBatch`，通过 queued signal 发到 UI。

### VisualPlugin

职责：

- 只在 UI 线程运行。
- 消费 `UiBatch` 或 `DataFrameBatch`。
- 使用自己的定时刷新策略，不逐点 repaint。

## 分阶段计划

### Phase 1: 固定最新窗口

目标：

- `RingBufferPool` 增加 `seq/generation/snapshot`。
- 增加容量配置接口。
- 满池只保留最新窗口。
- 保留当前线程模型，先不移动协议线程。

验证：

- 高频写入不会无限涨内存。
- 缩小容量后旧数据被裁剪。
- `snapshot.oldestSeq/newestSeq` 单调正确。

### Phase 2: 水位分发器

目标：

- `ChannelHub` 升级为分发器或新增 `ChannelDispatcher`。
- 每个可视化插件维护独立水位。
- 落后时追到 `oldestSeq` 并产生 `OverflowEvent`。
- UI 显示跳过计数。

验证：

- 模拟慢消费者，不阻塞写入。
- 慢消费者恢复后从最新窗口继续。
- 快消费者不受慢消费者影响。

### Phase 3: IngestThread

目标：

- 新增 `IngestWorker`。
- 物理插件和协议插件在采集线程内协作。
- 串口也迁移到采集线程。
- 设备 write 只在采集线程执行。

验证：

- UI 主线程不执行协议解析。
- 断开设备不会长时间卡 UI。
- 串口、USB、CandleLight 路径行为一致。

### Phase 4: SDK 数据模型升级

目标：

- 引入 `ChannelKey` 或等价稳定通道身份。
- 协议插件批量输出解析结果。
- Raw 数据和 numeric 数据明确分流。

验证：

- CAN ID、raw byte、serial JSON 字段不再发生通道碰撞。
- Raw Viewer 仍能显示原始双向负载。
- Time Chart 只消费 numeric samples。

### Phase 5: 配置与部署完善

目标：

- 前端增加蓄水池容量配置。
- 插件配置 schema 化。
- Windows 使用 `windeployqt` 或 CMake deployment。
- smoke test 改成不依赖 GUI 平台插件。

验证：

- 新机器可直接运行 Release 包。
- smoke test 可在 CI 或无桌面环境运行。

## 验证清单

每个阶段至少验证：

- 构建通过。
- 插件扫描通过。
- 模拟高频输入不会爆内存。
- UI 可以响应窗口拖动、按钮点击。
- 设备断开或关闭不会卡死。
- overflow/skipped 计数准确。
- 旧插件迁移路径明确。

## 风险与取舍

取舍：

- 本架构牺牲完整历史，换实时性和稳定性。
- 如果用户需要完整录制，应单独设计 raw capture file，不应挤进实时蓄水池。
- 分发线程不是数据拥有者，只负责按水位读取和生成批量 UI 消息。

主要风险：

- `RingBufferPool` 改动会影响所有可视化插件。
- `quint16` 通道迁移需要兼容旧插件。
- 物理插件迁移到采集线程后，QObject 线程亲和性必须严格处理。

迁移建议：

- 先兼容旧 `DataFrame`，新增批量接口。
- 新旧接口并行一个版本。
- 插件逐个迁移，最后移除旧逐帧分发。

