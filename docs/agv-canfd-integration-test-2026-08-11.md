# AGV CAN FD 上位机联调记录（2026-08-11）

## 配置

- Physical：`CandleLight CANFD`
- Protocol：`CAN Frame`
- 仲裁段：1,000,000 bit/s，采样点 75%
- 数据段：4,000,000 bit/s，采样点 75%，BRS/FD 开启
- 设备：WeActStudio USB2CANFDV1，WinUSB + gs_usb/CandleLight FD
- 心跳：100 ms

采样点与电机板固件保持 75%。原默认仲裁段/数据段采样点为 87.5%/80%，持续遥测时会累积总线错误；两段均修正为 75% 后链路和 300 mm 运动测试通过。

## 标准操作顺序

1. 点击 `Connect`，确认状态为 `Connected` 且 `RX frames` 持续增长。
2. 选择 `Heartbeat 100ms`，点击 `Periodic`。每次启动心跳都会生成新的非零 Session，并重新计算所有 AGV 快捷命令 CRC。
3. 双击 `Stop`，再双击 `Clear Fault`。
4. 双击 `Move 300mm` 一次，保持心跳运行。
5. 等待约 6 秒及板端 COMPLETED 事件；结束后双击 `Stop`。
6. 关闭 `Periodic`，需要退出时再点击 `Disconnect`。

同一会话内不要连续重复发送固定序号的运动命令。需要重新测试时，先关闭再重新启动 Heartbeat，以建立新会话。

## 通过结果

- 图形上位机完整操作由用户现场确认通过。
- 命令行复核收到 STOP、CLEAR_FAULT、GOTO 的 RECEIVED/ACCEPTED，以及 GOTO COMPLETED。
- 里程计 X 从 0 mm 连续增长至 300 mm。
- 测试摘要：`rx=519`、`bad_crc=0`、`completed=1`、`odom_x_mm=300`。
- `0x201`、`0x211`、`0x221`、`0x231` 遥测均持续接收。

## 已知限制

- 曾观察到 `Failed to write all command bytes` 和 WinUSB 写超时，伴随事件日志刷新卡顿；重启上位机后本轮测试通过。长时间运行前仍需完成不少于 30 分钟压力测试。
- 出现写超时时不要重复发送运动命令：先关闭 `Periodic`，断开连接并重启上位机。
- `Raw Viewer` 高频自动滚动和大量事件日志会增加界面负担；正式整车联调优先保持 Gauge 页，按需短时查看原始帧。
- 当前只验证正向 300 mm；防撞条实体触发、带载 PID、倒车和转向尚未纳入本轮通过范围。
