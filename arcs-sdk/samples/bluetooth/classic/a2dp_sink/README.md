# Bluetooth Audio Sink Sample

## 功能说明

本示例演示了如何使用 ARCS SDK 的蓝牙音频框架在 **Sink 模式**下工作，从远程设备（如手机）接收音频数据并播放。

示例实现了以下功能：
- 蓝牙经典音频 Sink 模式初始化
- A2DP Sink 配置（音乐接收）
- HFP HF 配置（语音通话，可选）
- 虚拟声卡接口注册
- 音频数据接收与解码（SBC → PCM）
- Shell 命令控制音频接收

## 硬件连接

- ARCS 开发板（支持蓝牙经典功能）
- 音频播放设备（可选：扬声器、耳机等）
- 手机或其他蓝牙音频源设备

## 软件依赖

- FreeRTOS
- Lisa Bluetooth 组件
- Lisa Bluetooth Audio Framework
- Lisa Shell
- Lisa Audio Device（用于音频播放）

## 示例内容

1. 初始化蓝牙音频框架（Sink 模式）
2. 注册虚拟声卡接口
3. 初始化蓝牙并进入可发现和可连接状态
4. 等待蓝牙设备连接
5. 通过 Shell 命令启动音频接收
6. 接收并解码蓝牙音频数据

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

设备启动后控制台输出：

```
=== Bluetooth Audio Sink Sample ===
Initializing system components...
Bluetooth audio framework initialized (Sink mode)
Virtual soundcard interface registered
Bluetooth initialized successfully
Device name: ARCS_BT_SINK
Waiting for connection from audio source...
Use 'bt_audio_start' command to start audio receiving
```


## 使用方法

### 1. 启动设备

设备启动后会自动初始化蓝牙，并进入可发现和可连接状态。

控制台输出示例：
```
=== Bluetooth Audio Sink Sample ===
Initializing system components...
Bluetooth audio framework initialized (Sink mode)
Virtual soundcard interface registered
Bluetooth initialized successfully
Device name: ARCS_BT_SINK
Waiting for connection from audio source...
Use 'bt_audio_start' command to start audio receiving
```

### 2. 连接蓝牙设备

使用您的手机或其他蓝牙设备搜索并连接到 **"ARCS_BT_SINK"**。

### 3. 启动音频接收

连接成功后，在串口控制台输入以下命令启动音频接收：

```bash
bt_audio_start
```

输出示例：
```
Mode: Decode SBC to PCM
Virtual interface opened successfully
Waiting for audio data from remote device...
BT audio sink started successfully
Audio format: sample_rate=48000, channels=2, bits=16
```

现在您可以在手机上播放音乐，设备将接收并解码音频数据。

### 4. 查看状态

```bash
bt_audio_status
```

输出示例：
```
=== Bluetooth Audio Sink Status ===
Audio receiving: ENABLED
====================================
```

### 5. 停止音频接收

```bash
bt_audio_stop
```

## 配置选项

### prj.conf 关键配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_BT_CLASSIC_ROLE_SINK` | 启用 Sink 模式 | y |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_A2DP_SINK` | 启用 A2DP Sink | y |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_HFP_HF` | 启用 HFP HF（可选） | y |
| `CONFIG_BT_AUDIO_CODEC_SBC` | SBC 编解码器 | y |
| `CONFIG_LISA_BLUETOOTH_DEVICE_NAME` | 蓝牙设备名称 | "ARCS_BT_SINK" |
| `CONFIG_LISA_AUDIO_DEVICE` | 音频设备支持 | y |

### 关键差异：Source vs Sink

| 特性 | A2DP Source | A2DP Sink |
|------|-------------|-----------|
| 角色 | 音频发送端 | 音频接收端 |
| 典型设备 | 手机、电脑 | 蓝牙耳机、音箱 |
| 数据流向 | 发送音频 → 远程设备 | 接收音频 ← 远程设备 |
| Interface 类型 | `VINTF_PROFILE_PLAYBACK` | `VINTF_PROFILE_CAPTURE` |
| 编解码方向 | PCM → SBC | SBC → PCM |
| 配置宏 | `CONFIG_BT_CLASSIC_ROLE_SOURCE` | `CONFIG_BT_CLASSIC_ROLE_SINK` |

## 代码架构

### 数据流程

```
远程设备（手机）
    ↓
蓝牙栈接收 A2DP 数据
    ↓
适配器接收
    ↓
会话解码 (SBC → PCM)
    ↓
虚拟接口处理 (bt_audio_interface_virtual)
    ↓
音频数据回调 (audio_data_callback)
    ↓
音频播放设备 (DAC/I2S)
```

### 关键函数

#### 1. 音频数据回调

```c
static void audio_data_callback(const uint8_t *data, size_t len, void *user_data)
```

当从蓝牙接收并解码音频数据后，该回调函数会被调用。接收到的是 PCM 数据，可以直接发送到音频播放设备。

#### 2. 打开完成回调

```c
static void open_complete_handler(const bt_audio_format_t format)
```

当虚拟接口打开完成时被调用，可以获取音频格式信息（采样率、声道数等）。

#### 3. Shell 命令

- `bt_inquiry`: 扫描周围的蓝牙设备
- `bt_connect <device_name>`: 连接到指定设备
- `bt_audio_start`: 启动音频接收
- `bt_audio_stop`: 停止音频接收
- `bt_audio_status`: 显示当前状态

## 扩展开发

### 1. 添加音频播放支持

在 `audio_data_callback` 中添加音频播放代码：

```c
static void audio_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    // 将 PCM 数据发送到音频设备
    lisa_audio_play_write(data, len);
}
```

### 2. 音频处理

在播放前对音频数据进行处理：

```c
static void audio_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    // 音量调节
    adjust_volume((int16_t*)data, len / 2, volume_level);
    
    // 音效处理
    apply_equalizer((int16_t*)data, len / 2);
    
    // 播放
    lisa_audio_play_write(data, len);
}
```

### 3. 保存接收的音频

将接收到的音频保存到文件：

```c
static FILE *audio_file = NULL;

static void audio_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    if (audio_file) {
        fwrite(data, 1, len, audio_file);
    }
    
    // 同时播放
    lisa_audio_play_write(data, len);
}
```

### 4. 支持多种音频格式

示例默认支持 SBC 编解码，可以通过配置添加更多格式：

- **AAC**: 更高的音质
- **LC3**: 低延迟编解码
- **AptX/AptX HD**: 高质量编解码（需要授权）

## 故障排除

### 1. 无法连接

- 确认设备名称配置正确
- 检查蓝牙是否已初始化
- 查看是否有配对信息冲突

### 2. 没有音频输出

- 检查 `audio_data_callback` 是否被调用
- 确认音频播放设备已正确初始化
- 检查音频格式是否匹配

### 3. 音频断续

- 增加音频缓冲区大小
- 检查系统负载
- 优化任务优先级

## 性能优化

### 1. 降低延迟

- 使用低延迟编解码器（LC3）
- 减小音频缓冲区
- 提高音频任务优先级

### 2. 提高音质

- 使用高质量编解码器（AAC、AptX）
- 增加编码比特率
- 减少音频处理环节

### 3. 降低功耗

- 在空闲时关闭音频接收
- 使用低功耗蓝牙编解码器
- 优化音频处理算法

## 相关示例

- **a2dp_source**: A2DP Source 音频发送示例
- **hfp_hf**: HFP Hands-Free 语音通话示例
- **audio_player**: 音频播放器示例

## 参考文档

- [ARCS SDK 蓝牙音频框架文档](../../docs/bluetooth/audio_framework.md)
- [A2DP 协议规范](https://www.bluetooth.com/specifications/specs/a2dp/)
- [蓝牙音频编解码器对比](../../docs/bluetooth/codecs.md)

## 许可证

Copyright (c) 2025, LISTENAI

SPDX-License-Identifier: Apache-2.0
