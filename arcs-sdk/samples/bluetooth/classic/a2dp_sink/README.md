# 经典蓝牙 A2DP 音频接收示例

## 功能说明

本示例演示了如何使用 ARCS SDK 的蓝牙音频框架在 **Sink 模式**下工作，从远程设备（如手机）接收 A2DP 音频并通过 `lisa_audio` 播放。

示例实现了以下功能：
- 初始化 Lisa Shell、Lisa Bluetooth、BT Audio Framework 和 BT Sink
- 通过 Shell 命令 `bt_scan <0|1|2|3>` 控制经典蓝牙扫描模式
- 通过 Shell 命令 `bt_disconnect_addr <addr>` 按地址主动断开经典蓝牙连接
- 手机连接后自动完成经典蓝牙链路、HFP、AVRCP、A2DP 建链
- 收到 A2DP Start 后自动创建播放会话并开始 SBC 解码播放
- 通过 `AT` 透传命令直接下发底层 AT 指令

## 硬件连接

- ARCS 开发板（支持蓝牙经典功能）
- 音频播放设备（如喇叭、耳机、功放）
- 手机或其他蓝牙音频源设备

## 软件依赖

- FreeRTOS
- Lisa Bluetooth 组件
- Lisa Bluetooth Audio Framework
- Lisa Shell
- Lisa Audio Device

## 示例内容

1. 初始化 Shell 与文件系统
2. 初始化蓝牙协议栈和 BT Audio Framework
3. 初始化 BT Sink 音频适配层
4. 通过 `bt_scan` 打开 inquiry/page scan
5. 等待手机连接并自动完成 HFP、AVRCP、A2DP 建链
6. 手机侧开始播放音乐后，设备自动创建音频播放会话并输出 PCM

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
I/BT_AUDIO_SESSION Audio session manager initialized
I/BT_SINK          bt_sink initialized successfully with interface: lisa_audio
DBG:bt_stack_init,state:0
DBG:a2dp_init, init_type:1
DBG:bt_stack_init,state:1
DBG:bt reset cmp: 20:23:03:64:74:57
DBG:ble enable cmp, sta:0
DBG:bt classic enable cmp, status:0
DBG:a2dp en, role:1
DBG:hfp en, role:0,feats:0x31
INF:a2dp enable cmp!
DBG:hfp enable cmp,status:0x0
```

### 2. 打开扫描并连接手机

串口执行：

```bash
bt_scan 3
```

随后可以看到协议栈打开经典蓝牙扫描并接受手机连接：

```text
DBG:classic scan en:3
DBG:BT classic connected
DBG:classic scan en:0
DBG:hfp connect cmp,conidx:6, type:1, status:0x0
INF:avrcp connect cmp!,conidx:0x6, status:0x0
INF:a2dp connect cmp! conidx=6, status=0x0
```

### 3. 手机播放音乐

当手机侧开始播放音乐后，A2DP Start 会触发播放会话创建和音频输出：

```text
INF:a2dp start! codec:0, ch:2, sample_rate:44100
I/BT_AUDIO_ADAPTER AUD_OS_START_EVT: type=16, ch=2, sample=44100
I/BT_SINK          BT audio stream starting: type=16, ch=2, sample=44100
I/BT_AUDIO_SESSION Session created: direction=0, codec=2, passthrough=0
I/BT_AUDIO_SESSION Codec initialized: type=2, name=SBC, duration=2902 us, frame_size=77 bytes
I/LISA_AUDIO_INTERFACE Opening playback: 44100 Hz, 2 ch, 16 bits
I/BT_SINK          Playback session and interface opened successfully
I/BT_AUDIO_SESSION PCM prefilled, start playback
```

### 4. 停止播放

手机暂停或停止播放后，会话关闭：

```text
INF:a2dp stop!, conidx:6, sta:0x0
I/BT_AUDIO_ADAPTER AUD_OS_STOP_EVT: conidx=6, status=0x0
I/BT_SINK          BT audio stream stopping: conidx=6, status=0x0
I/BT_AUDIO_SESSION Session destroyed
```

## 使用方法

### 1. 启动设备

设备上电后会自动完成 Shell、蓝牙协议栈、BT Audio Framework 和 BT Sink 初始化。

### 2. 打开经典蓝牙扫描

在串口控制台输入：

```bash
bt_scan 3
```

参数说明：
- `0`：关闭扫描
- `1`：打开 inquiry scan
- `2`：打开 page scan
- `3`：同时打开 inquiry scan 和 page scan

通常建议使用 `bt_scan 3`，这样手机既能搜索到设备，也能主动连接设备。

### 3. 手机连接设备

在手机蓝牙设置中搜索并连接目标设备。连接建立后，示例会自动完成 HFP、AVRCP 和 A2DP 相关链路。

### 4. 手机开始播放音乐

连接成功后，不需要额外执行 `bt_audio_start` 之类命令。手机侧开始播放音乐时，设备会自动收到 A2DP Start 事件并开始解码播放。

### 5. 关闭扫描

如需关闭扫描，可执行：

```bash
bt_scan 0
```

### 6. 主动断开连接

如果需要从开发板侧主动断开某个已连接设备，可根据连接日志中的设备地址执行：

```bash
bt_disconnect_addr E0:5D:3B:CC:EE:E8
```

## 配置选项

### prj.conf 关键配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_BT_CLASSIC_ROLE_SINK` | 启用 Sink 模式 | `y` |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_A2DP_SINK` | 启用 A2DP Sink | `y` |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_HFP_HF` | 启用 HFP HF | `y` |
| `CONFIG_BT_AUDIO_CODEC_SBC` | 启用 SBC 编解码器 | `y` |
| `CONFIG_LISA_AUDIO_DEVICE` | 启用音频设备输出 | `y` |

## 代码架构

### 数据流程

```text
Shell 命令 bt_scan 3
    ↓
经典蓝牙扫描打开
    ↓
手机建立 BR/EDR 连接
    ↓
HFP / AVRCP / A2DP 建链
    ↓
A2DP Start 指示
    ↓
BT Audio Session 创建
    ↓
SBC 解码为 PCM
    ↓
lisa_audio 播放输出
```

### 关键点

- `main()`：完成 Shell、蓝牙、BT Audio Framework 和 BT Sink 初始化。
- `cmd_bt_scan()`：解析 `0..3` 参数并调用 `lisa_bt_scan()`。
- `cmd_bt_disconnect_by_addr()`：解析蓝牙地址并调用 `lisa_bluetooth_disconnect_by_addr()`。
- `SHELL_EXPORT_PASSTROUGH(AT, ...)`：提供 AT 透传入口，便于直接调试底层蓝牙 AT 指令。

## Shell 命令

- `bt_scan <0|1|2|3>`：设置经典蓝牙扫描模式。
- `bt_disconnect_addr <XX:XX:XX:XX:XX:XX>`：按蓝牙地址断开已连接设备。
- `AT`：进入 AT 透传模式，或直接执行单行 AT 指令。

## 故障排除

### 1. 手机搜索不到设备

- 确认已经执行 `bt_scan 3`
- 检查日志中是否出现 `DBG:classic scan en:3`
- 确认手机蓝牙已打开，且开发板没有被其他设备占用

### 2. 已连接但没有声音

- 确认手机侧已经开始实际播放音乐
- 检查日志中是否出现 `INF:a2dp start!` 和 `Playback session and interface opened successfully`
- 确认音频播放外设连接正常

### 3. 日志出现 `Unsupported sample rate: 44100`

从当前日志看，手机以 `44100 Hz` 发流，而本地播放设备最终按 `48000 Hz` 配置输出。该警告不一定导致播放失败，但如果后续出现音质或速率异常，需要继续检查底层音频设备配置。
