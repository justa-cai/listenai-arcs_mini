# 经典蓝牙 HFP 音频源示例

## 功能说明

本示例演示了如何使用 ARCS SDK 的经典蓝牙 HFP 能力建立语音链路，并通过 BT Audio Framework 在本地与远端设备之间传输语音数据。

示例实现了以下功能：
- 初始化 Lisa Shell、Lisa Bluetooth 和 BT Audio Framework
- 注册经典蓝牙发现回调，支持扫描并按名称、索引或地址连接远端设备
- 通过 `bt_audio_start` 打开 HFP SCO 全双工音频链路
- 将本地预置 PCM 数据编码后发送到远端设备
- 将远端回传的语音数据写入本地 `lisa_audio` 播放设备
- 支持 `AT` 透传命令直接调试底层蓝牙 AT 指令

## 硬件连接

- ARCS 开发板（支持蓝牙经典功能）
- 支持 HFP 的手机或耳机设备
- 本地音频播放设备（用于播放接收到的语音数据）

## 软件依赖

- FreeRTOS
- Lisa Bluetooth 组件
- Lisa Bluetooth Audio Framework
- Lisa Shell
- Lisa Audio Device

## 示例内容

1. 初始化 Shell 与文件系统
2. 初始化蓝牙协议栈并注册发现回调
3. 获取本地音频播放设备 `audio0`
4. 通过 `bt_inquiry` 扫描目标设备
5. 通过 `bt_connect`、`bt_connect_index` 或 `bt_connect_addr` 发起经典蓝牙连接
6. 通过 `bt_audio_start` 建立 HFP 音频链路并启动音频任务

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

### 1. 启动日志

设备启动后可以看到类似如下日志：

```text
DBG:hci_aud_access_init,0
DBG:a2dp_init, init_type:0
DBG:bt_stack_init,state:0
DBG:a2dp_init, init_type:1
DBG:bt_stack_init,state:1
DBG:bt reset cmp: 20:23:03:64:74:57
DBG:ble enable cmp, sta:0
DBG:bt classic enable cmp, status:0
DBG:a2dp en, role:0
DBG:hfp en, role:1,feats:0x22
INF:a2dp enable cmp!
DBG:hfp enable cmp,status:0x0
I/BT_AUDIO_SESSION Audio session manager initialized
I/app-bt          Virtual BT interface initialized successfully
```

### 2. 扫描设备

串口执行：

```bash
bt_inquiry
```

命令会立即返回，扫描结果异步打印：

```text
letter:/$ bt_inquiry
Return: 1, 0x00000001
DBG:bt disc cre cmp,sta:0x00,0
DBG:gap start cmp,sta:0x00,type:4, actv:0,actv:0
I/app-bt Discovered device: E0:5D:3B:CC:EE:E8, RSSI=-46, Name=soundcore Liberty 4
I/app-bt Discovered device: 48:C5:24:BE:C7:60, RSSI=-69, Name=真我GT Neo5 SE
```

### 3. 连接设备

如果设备名包含空格，命令里需要加引号，例如：

```bash
bt_connect "soundcore Liberty 4"
```

连接日志示例：

```text
I/app-bt  Connecting to device: soundcore Liberty 4
I/LISA_BT Trying to connect to device: soundcore Liberty 4
I/LISA_BT Device list count: 5
I/LISA_BT Device[0]: soundcore Liberty 4
I/LISA_BT addr: E0:5D:3B:CC:EE:E8
I/LISA_BT Device found! Connecting...
DBG:BT classic connected
DBG:hfp connect cmp,conidx:6, type:0, status:0x0
INF:a2dp connect cmp! conidx=6, status=0x0
INF:avrcp connect cmp!,conidx:0x6, status:0x0
```

### 4. 启动 HFP 音频

连接成功后，执行：

```bash
bt_audio_start
```

日志示例：

```text
I/BT_VINTF        Profile opening: type=1, state=OPENING
I/BT_AUDIO_ADAPTER Starting audio stream for profile: 2 (conidx=6)
I/BT_AUDIO_ADAPTER HFP SCO connection requested
I/app-bt          Virtual interface opened successfully
DBG:hfp start! conidx:0x6,codec:0,status:0x0
I/BT_AUDIO_ADAPTER AUD_OS_START_EVT: type=17, ch=1, sample=16000
I/BT_AUDIO_SESSION Session created: direction=1, codec=4, passthrough=0
I/BT_AUDIO_SESSION Session created: direction=0, codec=4, passthrough=0
I/BT_AUDIO_SESSION Playback decode mode: created decode task
I/BT_VINTF        Downlink session created for HFP full-duplex
I/lisa_audio_play Play configured: rate=16000, gain=-10/-10 dB, buffers=3×256
I/lisa_audio_play Play started
I/app-bt          BT audio source started successfully
I/app-bt          Audio source task started
```

## 使用方法

### 1. 启动设备

设备上电后会自动初始化 Shell、蓝牙协议栈、虚拟音频接口和本地音频设备句柄。

### 2. 扫描设备

在串口控制台输入：

```bash
bt_inquiry
```

说明：
- `bt_inquiry` 的 shell 返回值不表示扫描结束，它只表示扫描命令已经成功下发。
- 设备发现结果会在后续异步打印。

### 3. 连接设备

可以按名称连接：

```bash
bt_connect My_Phone
```

如果设备名中包含空格，请使用引号：

```bash
bt_connect "soundcore Liberty 4"
```

也可以按扫描列表索引连接：

```bash
bt_connect_index 0
```

如果已知远端蓝牙地址，也可以不依赖扫描列表直接连接：

```bash
bt_connect_addr E0:5D:3B:CC:EE:E8
```

### 4. 启动 HFP 音频

连接成功后，执行：

```bash
bt_audio_start
```

当前实现会：
- 打开 `VINTF_PROFILE_CAPTURE`
- 请求建立 HFP SCO 链路
- 创建上行编码会话和下行解码会话
- 根据协商到的音频格式配置本地播放设备
- 创建音频发送任务，将本地 PCM 数据送入虚拟接口
- 通过 `capture_callback_handler()` 播放远端回传的语音数据

### 5. 停止 HFP 音频

```bash
bt_audio_stop
```

### 6. 断开蓝牙连接

可以按名称、扫描列表索引或地址断开连接：

```bash
bt_disconnect "soundcore Liberty 4"
bt_disconnect_index 0
bt_disconnect_addr E0:5D:3B:CC:EE:E8
```

## Shell 命令

- `bt_inquiry`：扫描周围经典蓝牙设备。
- `bt_connect <device_name>`：按设备名连接远端设备。
- `bt_connect_index <device_index>`：按扫描结果索引连接远端设备。
- `bt_connect_addr <XX:XX:XX:XX:XX:XX>`：按蓝牙地址直接连接远端设备。
- `bt_disconnect <device_name>`：按设备名断开远端设备。
- `bt_disconnect_index <device_index>`：按扫描结果索引断开远端设备。
- `bt_disconnect_addr <XX:XX:XX:XX:XX:XX>`：按蓝牙地址断开远端设备。
- `bt_audio_start`：打开 HFP 音频链路并启动本地音频任务。
- `bt_audio_stop`：停止本地音频任务并关闭虚拟接口。
- `AT`：进入 AT 透传模式，或直接执行单行 AT 命令。

## 配置选项

### 关键配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_BT_CLASSIC_ROLE_SOURCE` | 启用 Source 模式 | `y` |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_HFP_AG` | 启用 HFP AG | `y` |
| `CONFIG_BT_AUDIO_CODEC_MSBC` | 启用 mSBC 相关链路 | 依配置而定 |
| `CONFIG_LISA_AUDIO_DEVICE` | 启用本地音频播放设备 | `y` |

### 当前实现特点

- `FRAME_TIME_MS` 固定为 `7.5 ms`
- HFP 起流后当前协商日志显示为单声道 `16000 Hz`
- 上下行都使用 `mSBC` 编解码链路
- 本地播放设备按 `16000 Hz`、`-10/-10 dB` 增益启动

## 代码架构

### 数据流程

```text
bt_inquiry / bt_connect
    ↓
建立经典蓝牙连接
    ↓
bt_audio_start
    ↓
打开 VINTF_PROFILE_CAPTURE
    ↓
请求 HFP SCO 音频链路
    ↓
创建上行编码会话和下行解码会话
    ↓
open_complete_handler 配置本地 lisa_audio 播放设备
    ↓
audio_source_task 持续写入本地 PCM 数据到虚拟接口
    ↓
远端 HFP 语音数据通过 capture_callback_handler 回放到本地音频设备
```

### 关键点

- `cmd_bt_inquiry()`：调用 `lisa_bluetooth_inquiry_start()` 扫描目标设备。
- `cmd_bt_connect()` / `cmd_bt_connect_by_index()` / `cmd_bt_connect_by_addr()`：按名称、索引或地址连接远端设备。
- `cmd_bt_disconnect()` / `cmd_bt_disconnect_by_index()` / `cmd_bt_disconnect_by_addr()`：按名称、索引或地址断开连接。
- `cmd_bt_audio_start()`：打开 HFP 音频链路。
- `open_complete_handler()`：按协商结果配置本地播放设备并启动发送任务。
- `capture_callback_handler()`：播放接收到的远端语音数据。

## 故障排除

### 1. 扫描不到设备

- 确认对端设备蓝牙已打开并处于可发现状态
- 重新执行 `bt_inquiry`
- 检查发现回调日志是否有输出

### 2. 已连接但没有语音

- 确认已经执行 `bt_audio_start`
- 检查日志中是否出现 `HFP SCO connection requested` 和 `BT audio source started successfully`
- 检查本地 `audio0` 设备是否正常初始化

### 3. 本地没有播放声音

- 检查 `capture_callback_handler()` 是否被触发
- 检查 `lisa_audio_play_write failed` 日志
- 确认本地音频硬件连接正常
