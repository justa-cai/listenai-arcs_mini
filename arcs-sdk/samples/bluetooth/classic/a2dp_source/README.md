# Bluetooth Audio Source Sample

## 功能说明

本示例演示了如何使用 ARCS SDK 的蓝牙音频框架在 **Source 模式**下工作，将本地音频数据通过经典蓝牙发送到远程 A2DP Sink 设备，如蓝牙耳机或音箱。

示例实现了以下功能：
- 初始化 Lisa Shell、Lisa Bluetooth 和 BT Audio Framework
- 注册经典蓝牙发现回调，支持查看扫描到的设备名称和地址
- 通过 Shell 命令执行设备扫描、按名称或索引连接远端设备
- 通过虚拟音频接口发送本地音频数据
- 在编码模式下将 PCM 数据编码为 SBC 后发送
- 支持 `AT` 透传命令直接调试底层蓝牙 AT 指令

## 硬件连接

- ARCS 开发板（支持蓝牙经典功能）
- 蓝牙耳机或蓝牙音箱（支持 A2DP Sink）

## 软件依赖

- FreeRTOS
- Lisa Bluetooth 组件
- Lisa Bluetooth Audio Framework
- Lisa Shell
- 预置音频数据资源

## 示例内容

1. 初始化 Shell 与文件系统
2. 初始化蓝牙协议栈并注册发现回调
3. 初始化虚拟蓝牙音频接口
4. 通过 `bt_inquiry` 扫描可连接设备
5. 通过 `bt_connect` 或 `bt_connect_index` 连接目标设备
6. 通过 `bt_audio_start` 启动音频发送任务

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
DBG:hci_aud_access_init,1
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
I/app-bt Discovered device: 6A:05:BF:9F:D0:EC, RSSI=-74, Name=Saw的MIX 2
I/app-bt Discovered device: E0:5D:3B:CC:EE:E8, RSSI=-52, Name=soundcore Liberty 4
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
I/LISA_BT Device list count: 6
I/LISA_BT Device[5]: soundcore Liberty 4
I/LISA_BT addr: E0:5D:3B:CC:EE:E8
I/LISA_BT Device found! Connecting...
DBG:BT classic connected
DBG:hfp connect cmp,conidx:6, type:0, status:0x0
INF:a2dp connect cmp! conidx=6, status=0x0
INF:avrcp connect cmp!,conidx:0x6, status:0x0
```

### 4. 启动音频发送

连接成功后，执行：

```bash
bt_audio_start
```

日志示例：

```text
I/app-bt   Mode: Encode PCM to SBC
I/BT_VINTF Profile opening: type=0, state=OPENING
I/BT_AUDIO_ADAPTER A2DP stream start requested
I/app-bt   Virtual interface opened successfully
INF:a2dp start! codec:0, ch:2, sample_rate:44100
I/BT_AUDIO_SESSION Session created: direction=1, codec=2, passthrough=0
I/SBC_CODEC Encoder initialized: 44100Hz 2ch, codesize=512, frame_length=77
I/BT_VINTF Audio streaming started for profile=1, conidx=6
I/app-bt   BT audio source started successfully
I/app-bt   Audio source task started (PCM encode mode)
I/app-bt   PCM data size: 3527680 bytes, frame size: 1764 bytes
```

## 使用方法

### 1. 启动设备

设备上电后会自动完成 Shell、蓝牙协议栈和虚拟蓝牙音频接口初始化。初始化成功后，日志中会出现 `Virtual BT interface initialized successfully`。

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
bt_connect My_Headset
```

如果设备名中包含空格，请使用引号：

```bash
bt_connect "soundcore Liberty 4"
```

也可以按扫描列表索引连接：

```bash
bt_connect_index 0
```

### 4. 启动音频发送

连接成功后，执行：

```bash
bt_audio_start
```

当前代码默认使用编码模式发送 PCM，并将其编码为 SBC 后通过 A2DP 发出。

### 5. 调整音量

```bash
bt_audio_volume 60
```

### 6. 停止音频发送

```bash
bt_audio_stop
```

## Shell 命令

- `bt_inquiry`：扫描周围经典蓝牙设备。
- `bt_connect <device_name>`：按设备名连接远端设备。
- `bt_connect_index <device_index>`：按扫描结果索引连接远端设备。
- `bt_audio_start`：打开虚拟音频接口并启动音频发送任务。
- `bt_audio_stop`：停止音频发送任务并关闭虚拟接口。
- `bt_audio_volume <0-100>`：设置发送音量。
- `AT`：进入 AT 透传模式，或直接执行单行 AT 命令。

## 配置选项

### 关键配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_BT_CLASSIC_ROLE_SOURCE` | 启用 Source 模式 | `y` |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_A2DP_SOURCE` | 启用 A2DP Source | `y` |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_HFP_AG` | 启用 HFP AG | `y` |
| `CONFIG_BT_AUDIO_CODEC_SBC` | 启用 SBC 编解码器 | `y` |

### 当前音频参数

当前 `main.c` 中使用的主要参数如下：

```c
#define SAMPLE_RATE      44100
#define CHANNELS         2
#define BITS_PER_SAMPLE  16
#define USE_PCM_ENCODE_MODE 1
```

## 代码架构

### 数据流程

```text
bt_inquiry / bt_connect
    ↓
建立经典蓝牙连接
    ↓
bt_audio_start
    ↓
打开虚拟蓝牙音频接口
    ↓
audio_source_task 持续读取本地音频资源
    ↓
vintf_playback_write
    ↓
BT Audio Session 编码为 SBC
    ↓
A2DP 发送到远端设备
```

### 关键点

- `cmd_bt_inquiry()`：调用 `lisa_bluetooth_inquiry_start()` 扫描附近设备。
- `cmd_bt_connect()` / `cmd_bt_connect_by_index()`：按名称或索引发起连接。
- `cmd_bt_audio_start()`：设置编码模式并打开 `VINTF_PROFILE_PLAYBACK`。
- `audio_source_task()`：循环发送预置 PCM 数据。

## 故障排除

### 1. 扫描不到设备

- 确认目标耳机或音箱已进入可配对状态
- 检查串口日志是否持续输出发现结果
- 必要时重新执行 `bt_inquiry`

### 2. 连接成功但没有声音

- 确认已经执行 `bt_audio_start`
- 检查日志中是否出现 `INF:a2dp start!` 和 `BT audio source started successfully`
- 确认远端设备支持 SBC/A2DP Sink

### 3. 音频任务启动失败

- 检查是否有 `Failed to create audio source task`
- 检查系统堆内存是否充足
