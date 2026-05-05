# USB Host Video + Serial + Audio 示例

## 功能说明

该示例在同一份固件中同时启用三类 USB Host 功能：

- UVC 摄像头采集与 LCD 显示
- CDC ACM / vendor serial 串口设备持续回环测试（轮次间隔 500 ms）
- UAC 1.0 音频设备播放模式（默认 playback only）

示例基于 CherryUSB 协议栈实现：

- UVC 部分沿用 `cherryusb_video` 的显示与缓冲机制
- CDC 部分沿用 `cherryusb_serial` 的持续回环验证流程，默认发送长度已配置为 `10240` 字节，优先尝试 `CDC ACM`，同时支持已启用的 vendor serial 驱动
- UAC 部分沿用 `cherryusb_audio` 的播放路径，当前 sample 默认仅启动 speaker playback，以降低三设备并行时的 USB pipe 压力

## 特性

- 最多同时支持 2 台 UVC 摄像头显示在 LCD 左右半屏
- 支持 CDC ACM / vendor serial 串口设备持续回环测试（轮次间隔 500 ms）
- 默认支持 UAC 1.0 speaker playback 模式
- UVC、CDC、UAC 在同一镜像中并行工作；默认配置优先保证三类设备可同时拉起
- 串口 10KB 回环路径已针对并发场景做专项调优：`CONFIG_USBHOST_SERIAL_RX_SIZE=16384`、接收空闲超时 `15000 ms`、进度日志按 `1024` 字节节流
- `chryusb` 代码重定位到 PSRAM，降低 SRAM 压力

## 当前验证情况

### 已验证设备

- UVC：`TinyUSB Device`（`VID:PID = cafe:4020`）
  - 2 接口 UVC 设备
  - Bulk 模式
  - `UNCOMPRESSED 640x480 @ 15fps`
  - 可正常打开、取帧、打印帧统计并刷新 LCD
- UAC：`GeneralPlus USB Audio Device`（`VID:PID = 1b3f:2008`）
  - speaker：`2ch / 16bit / 48kHz`
  - mic：`1ch / 16bit / 48kHz`
  - 默认以 playback only 方式运行

- CDC：`FT232R USB UART`（`VID:PID = 0403:6001`）
  - 通过 `FTDI` host class 识别，设备节点 `/dev/ttyUSB0`
  - 串口参数：`115200 8N1`
  - 持续 `1024` 字节回环测试已实测连续成功
  - 默认代码参数已切换到持续 `10240` 字节回环压测，并增大 RX ringbuffer 以降低并发场景下的数据丢失概率；该长度仍以最新实机回归结果为准

### 三设备并发联调结果

- 联调拓扑：两级 USB 2.0 Hub，下挂 `FT232R USB UART` + `TinyUSB Device` UVC 摄像头 + `GeneralPlus USB Audio Device`
- 当前结果：`FTDI + UVC + UAC speaker playback` 可同时拉起；串口回环在最新日志中连续完成 `#1 ~ #9` 轮成功，UVC 持续打印帧统计，UAC 正常进入 `Active: playback`
- 兼容现象：同一拓扑下如果还接入 ASIX 网卡、USB Storage、Type-C Digital AV Adapter 等非目标类设备，仍会打印 `Do not support Class...`，但不会影响目标三类设备继续运行

### 当前支持范围与限制

- CDC 串口设备
  - 优先支持标准 `CDC ACM` 设备（`/dev/ttyACM0`），也支持已启用的 vendor serial 设备（`/dev/ttyUSB0`）
  - 同时启用 `CH34X`、`FTDI`、`CP210X` 三类 vendor serial host driver，设备节点为 `/dev/ttyUSB0`
  - 当前已实测 `FT232R USB UART (0403:6001)` 可稳定进入本 sample 的持续回环路径，并已完成与 `UVC + UAC playback` 的并发联调
  - `CH34X`、`CP210X` 驱动已编入，但尚未逐一实机验证
- 非目标类设备
  - 例如 MSC、HID、网卡、转接器等设备会被枚举，但只打印 `Do not support Class...` 提示，不影响已支持设备继续工作

## 硬件连接

### 开发板

- ARCS EVB 开发板（Host 端）× 1

### USB Host 硬件修改

- `USB_ARCS` Type-C 接口需直接供电 5V
- `CC1 / CC2` 需各接 22K 上拉到 VBUS

### 设备连接

- UVC：单摄可直连；多设备并行时建议使用 USB 2.0 Hub
- CDC：推荐标准 CDC ACM 或已支持的 vendor serial 设备，回环测试时短接 TX / RX
- UAC：标准 USB Audio Class 1.0 设备，可为 speaker、mic 或复合声卡

## 示例内容

1. 初始化 LCD / LVGL 显示子系统
2. 初始化 USB Host 控制器与 CherryUSB Host 协议栈
3. 摄像头插入后自动协商格式并开始视频流显示
4. CDC ACM / vendor serial 设备插入后自动启动持续回环测试（当前默认发送长度 `10240` 字节）
5. UAC 设备插入后默认选择 speaker 流并启动 playback
6. 设备断开后分别清理各自资源

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

### UVC 正常工作日志示例

以下日志来自已验证的 `TinyUSB Device (cafe:4020)`：

```text
[I/usbh_core] New device found,idVendor:cafe,idProduct:4020,bcdDevice:0100
[I/usbh_core] The device has 2 interfaces
[I/usbh_core] Manufacturer: TinyUSB
[I/usbh_core] Product: TinyUSB Device
[I/usbh_core] SerialNumber: 123456
[I/usbh_core] Enumeration success, start loading class driver
[I/usbh_core] Loading video_ctrl class driver on interface 0
[I/usbh_video] Register Video Class:/dev/video0
[VIDEO0] Camera connected: /dev/video0
[I/usbh_core] Loading video_streaming class driver on interface 1
[VIDEO0] Opening device (format=UNCOMPRESSED 640x480)...
[I/usbh_video] Open video and select formatidx:1, frameidx:1, altsetting:0
[VIDEO0] frame_buf allocated: 614400 B (640x480)
[VIDEO0] chunk_buf allocated: 4096 B
[VIDEO0] Starting bulk streaming...
[VIDEO0] Streaming started
[VIDEO0] first frame header: 0F 6B 10 A8
[VIDEO0] Running: 23 frames, 13800 KB total
[VIDEO0] frame #30  size=614400 B  total=18000 KB
```

### 三设备并发正常工作日志示例

以下日志来自最新一次 `FT232R + UVC + UAC playback` 联调。该轮实测使用的是 `1024` 字节回环长度：

```text
[I/usbh_core] Loading ftdi class driver on interface 0
[I/usbh_serial] Register Serial Class: /dev/ttyUSB0 (ftdi)
[SERIAL] Serial device opened successfully: /dev/ttyUSB0
[VIDEO0] Streaming started
[AUDIO] Mic stream detected but disabled by default (playback only mode)
[AUDIO] Speaker: speaker 2ch/16bit/48000Hz
[I/usbh_audio] Audio ISO playback started (mps=192, packets=32)
[AUDIO] Active: playback
[SERIAL] Start serial loopback test #4, len: 1024
[SERIAL] All 1024 bytes verified correctly!
[VIDEO0] Running: 83 frames, 13510 KB total
```

### 10KB 串口压测调优说明

针对 `UVC + CDC + UAC playback` 并发场景下将串口回环长度提升到 `10240` 字节时出现的接收超时问题，当前 sample 已增加如下调优：

- `CONFIG_USBHOST_SERIAL_RX_SIZE=16384`，扩大 host serial ringbuffer，避免 `10240` 字节连续回环时因用户线程消费不及时而丢包
- 串口发送/接收进度日志按每 `1024` 字节打印一次，减少频繁日志对串口接收线程调度的干扰
- 串口接收超时改为“空闲超时”语义，默认 `15000 ms`，更适合大包在 `115200 8N1` 下与 UVC/UAC 并发运行

若后续实机日志出现：

```text
[SERIAL][TX] Progress: 1024/10240
[SERIAL][RX] Progress: 2048/10240
[SERIAL] Serial read timeout after 15000 ms idle (received xxxx/10240 bytes)
```

可据此继续判断是纯粹超时、ringbuffer 丢包，还是三设备并发下总线调度仍然不足。

### UAC 正常工作日志示例

以下日志来自已验证的 `GeneralPlus USB Audio Device (1b3f:2008)`：

```text
[I/usbh_core] New device found,idVendor:1b3f,idProduct:2008,bcdDevice:0100
[I/usbh_core] Manufacturer: GeneralPlus
[I/usbh_core] Product: USB Audio Device
[I/usbh_audio] Register Audio Class:/dev/audio0
[AUDIO] Audio device connected
[AUDIO] Speaker: speaker 2ch/16bit/48000Hz
[I/usbh_audio] Ep=05 Attr=01 Mps=192 Interval=01 Mult=00
[I/usbh_audio] Open audio stream :speaker, altsetting: 1
[AUDIO] speaker unmute: 9
[AUDIO] speaker volume 0dB: 10
[AUDIO] Starting 1kHz sine wave playback...
[I/usbh_audio] Audio ISO playback started (mps=192, packets=32)
[AUDIO] Active: playback
```

### 非目标类设备的典型日志

接入非本 sample 目标类的设备时，会先完成 USB 枚举，再打印接口不支持信息，例如：

```text
[E/usbh_core] Do not support Class:0xff, Subclass:0xff, Protocl:0xff on interface 0
[E/usbh_core] Do not support Class:0x08, Subclass:0x06, Protocl:0x50 on interface 0
[E/usbh_core] Do not support Class:0x03, Subclass:0x00, Protocl:0x00 on interface 3
```

这类日志表示该设备或该接口不在本 sample 的处理范围内，并不代表 USB Host 框架整体异常。

## 日志说明

### 事件号说明

当前 sample 的 `usbh_event_handler()` 仅对少量事件号做了字符串映射，因此部分内部事件会显示为 `INVALID`。这并不代表事件非法。

结合 CherryUSB 当前事件定义，可按下述方式理解：

- `event=5`：`USBH_EVENT_DEVICE_CONFIGURED`
- `event=9`：`USBH_EVENT_INTERFACE_UNSUPPORTED`
- `event=10`：`USBH_EVENT_INTERFACE_START`
- `event=12`：`USBH_EVENT_INIT`

因此日志中的：

- `event=10 (INVALID)` 往往表示接口类驱动开始加载，是正常过程
- `event=9 (INVALID)` 往往表示某个接口当前 sample 不支持，不一定是故障

### 返回值说明

UAC 路径中的：

- `speaker unmute: 9`
- `speaker volume 0dB: 10`

是底层控制传输返回值的直接打印，当前实测设备在该路径下可以继续正常启动 speaker playback，可作为成功联调样例参考。

## 注意事项

- CDC 路径优先使用标准 `CDC ACM`，同时已启用 `/dev/ttyUSB0` 的 `CH34X` / `FTDI` / `CP210X` vendor-serial 驱动；当前已实测 FT232 可进入持续回环测试，其他两类设备未逐一验证
- CDC 回环测试默认要求物理短接 TX / RX；当前默认每 500 ms 发起一轮 `10240` 字节回环，若未短接，或并发压力下接收长期无新数据，会在 `15000 ms` 空闲超时后打印提示
- UVC 当前优先按 `MJPEG` 打开，设备不支持时会自动退回到第一个可用格式；当前实测通过的是 `UNCOMPRESSED 640x480 @ 15fps`
- BK 平台 `CONFIG_USB_MUSB_PIPE_NUM` 固定为 `8`，两级 Hub + `CDC ACM` + `UVC` + `UAC playback+recording` 会超过可用 pipe 预算，因此本 sample 默认采用 playback only 配置
- UAC 部分当前默认只启动 speaker playback；mic 接口仍会被枚举识别，但不会自动开启录音流
- 当前版本已在 Host Core 中增加总线级 EP0 control transfer 串行化，用于避免多 Hub/多设备并发时不同 hport 同时抢占同一个硬件控制通道
- 当前 `10240` 字节回环压测仍建议优先使用已验证的 `FT232R` 设备，并保留最新日志用于确认是否仍存在部分接收超时
- 如果某些 UVC 设备在 Hub 后出现 `Failed to get device descriptor,errorcode:-14`，说明失败发生在 USB 枚举阶段，而不是 `usbh_video` 类驱动阶段。建议优先排查供电、线材、Hub 级联和设备兼容性，并先做直连验证
- 设备较多时，日志中出现若干非目标类接口的 `Do not support Class...` 提示属于预期现象，只要目标 UVC/UAC/CDC 设备仍能正常运行即可
