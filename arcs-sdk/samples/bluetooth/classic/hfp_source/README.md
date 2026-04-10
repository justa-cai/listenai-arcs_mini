# Bluetooth Audio Source Sample

## 功能说明

本示例演示了如何使用 ARCS SDK 的蓝牙音频框架在 **Source 模式**下工作，将音频数据通过蓝牙发送到远程设备（如蓝牙耳机、音箱）。

示例实现了以下功能：
- 蓝牙经典音频 Source 模式初始化
- A2DP Source 配置（音乐传输）
- HFP AG 配置（语音通话，可选）
- 虚拟声卡接口注册
- 立体声测试音生成（左声道 440Hz，右声道 523.25Hz）
- 音频数据编码和蓝牙传输
- Shell 命令控制音频发送

## 硬件连接

- ARCS 开发板（支持蓝牙经典功能）
- 蓝牙耳机或音箱（支持 A2DP Sink）

## 软件依赖

- FreeRTOS
- Lisa Bluetooth 组件
- Lisa Bluetooth Audio Framework
- Lisa Shell
- 数学库（用于正弦波生成）

## 示例内容

1. 初始化蓝牙音频框架（Source 模式）
2. 注册虚拟声卡接口
3. 初始化蓝牙并进入可发现和可连接状态
4. 等待蓝牙设备连接
5. 通过 Shell 命令启动音频发送
6. 生成立体声测试音并通过蓝牙传输

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
=== Bluetooth Audio Source Sample ===
Initializing system components...
Bluetooth audio framework initialized (Source mode)
Virtual soundcard interface registered
Bluetooth initialized successfully
Device name: ARCS_BT_SOURCE
System initialization complete
Use 'bt_audio_start' command to start audio transmission
```


## 使用方法

### 1. 启动设备

设备启动后会自动初始化蓝牙，并进入可发现和可连接状态。

控制台输出示例：
```
=== Bluetooth Audio Source Sample ===
Initializing system components...
Bluetooth audio framework initialized (Source mode)
Virtual soundcard interface registered
Bluetooth initialized successfully
Device name: ARCS_BT_SOURCE
System initialization complete
Use 'bt_audio_start' command to start audio transmission
```

### 2. 连接蓝牙设备

使用您的蓝牙耳机或音箱搜索并连接到设备 **"ARCS_BT_SOURCE"**。

### 3. 启动音频发送

连接成功后，在串口控制台输入以下命令启动音频发送：

```bash
bt_audio_start
```

您应该能听到立体声测试音：
- **左声道**: 440Hz（A4 音符）
- **右声道**: 523.25Hz（C5 音符）

### 4. 查看状态

```bash
bt_audio_status
```

输出示例：
```
=== Bluetooth Audio Source Status ===
Audio sending: ENABLED
Task running: YES
Sample rate: 48000 Hz
Channels: 2 (Stereo)
Bits per sample: 16
Frame size: 480 samples (1920 bytes)
Test tone frequencies: L=440.00Hz, R=523.25Hz
====================================
```

### 5. 停止音频发送

```bash
bt_audio_stop
```

## 配置选项

### prj.conf 关键配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_BT_CLASSIC_ROLE_SOURCE` | 启用 Source 模式 | y |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_A2DP_SOURCE` | 启用 A2DP Source | y |
| `CONFIG_LISA_BLUETOOTH_CLASSIC_HFP_AG` | 启用 HFP AG（可选） | y |
| `CONFIG_BT_AUDIO_CODEC_SBC` | SBC 编解码器 | y |
| `CONFIG_LISA_BLUETOOTH_DEVICE_NAME` | 蓝牙设备名称 | "ARCS_BT_SOURCE" |

### 音频参数配置（main.c）

```c
#define SAMPLE_RATE         48000   // 采样率
#define CHANNELS            2       // 声道数
#define BITS_PER_SAMPLE     16      // 采样位深
#define TEST_TONE_FREQ_LEFT  440.0  // 左声道频率
#define TEST_TONE_FREQ_RIGHT 523.25 // 右声道频率
```

## 代码架构

### 数据流程

```
音频生成任务 (audio_source_task)
    ↓
生成 PCM 数据 (generate_stereo_sine_wave)
    ↓
虚拟声卡写入 (bt_vsnd_play_write)
    ↓
虚拟接口处理 (bt_audio_interface_virtual)
    ↓
会话编码 (bt_audio_session - SBC/LC3/mSBC)
    ↓
适配器发送
    ↓
蓝牙栈传输 (bt_stack_a2dp_send_data / bt_stack_hfp_send_sco_data)
    ↓
远程设备接收
```

### 关键函数

#### 1. 音频生成

```c
static void generate_stereo_sine_wave(int16_t *buffer, size_t frame_count, 
                                      uint32_t sample_rate,
                                      float *phase_left, float *phase_right)
```

生成双声道正弦波测试音。

#### 2. 音频发送任务

```c
static void audio_source_task(void *pvParameters)
```

持续生成并发送音频数据的 FreeRTOS 任务。

#### 3. Shell 命令

- `bt_audio_start`: 启动音频发送
- `bt_audio_stop`: 停止音频发送
- `bt_audio_status`: 显示当前状态

## 扩展开发

### 1. 使用真实音频源

替换 `generate_stereo_sine_wave()` 函数，从以下源读取音频数据：
- SD 卡上的 WAV/MP3 文件
- 麦克风采集
- 网络音频流
- I2S 音频输入

示例：
```c
// 从文件读取 PCM 数据
size_t bytes_read = read_audio_file(audio_buffer, AUDIO_BUFFER_SIZE);
if (bytes_read > 0) {
    bt_vsnd_play_write((uint8_t *)audio_buffer, bytes_read);
}
```

### 2. 添加音频处理

在发送前对音频数据进行处理：
- 音量控制
- 均衡器
- 降噪
- 回声消除

### 3. 支持多种编解码器

在 `prj.conf` 中启用更多编解码器：

```properties
CONFIG_BT_AUDIO_CODEC_SBC=y
CONFIG_BT_AUDIO_CODEC_LC3=y
CONFIG_BT_AUDIO_CODEC_MSBC=y
```

### 4. 动态采样率适配

根据蓝牙连接协商的参数动态调整：

```c
bt_audio_framework_config_t audio_config = {
    .mode = BT_AUDIO_MODE_SOURCE,
    .sample_rate = negotiated_sample_rate,  // 根据协商结果
    .channels = negotiated_channels,
    .bits_per_sample = negotiated_bits,
};
```

## 故障排查

### 问题 1: 无法连接蓝牙设备

**可能原因**:
- 设备不在可发现状态
- 蓝牙初始化失败

**解决方法**:
1. 检查日志确认蓝牙初始化成功
2. 确认设备名称 "ARCS_BT_SOURCE" 出现在扫描列表中
3. 尝试重启设备

### 问题 2: 连接成功但听不到声音

**可能原因**:
- 未启动音频发送
- Source 模式未启用
- 编解码器不匹配

**解决方法**:
1. 执行 `bt_audio_start` 命令
2. 确认 `prj.conf` 中 `CONFIG_BT_CLASSIC_ROLE_SOURCE=y`
3. 检查日志中的编解码器协商信息

### 问题 3: 音频卡顿或断续

**可能原因**:
- 发送速率不匹配
- 缓冲区配置不当
- 系统负载过高

**解决方法**:
1. 调整 `AUDIO_FRAME_SIZE` 和发送延时
2. 增加 PSRAM 堆大小
3. 提高音频任务优先级

### 问题 4: 编译错误

**常见错误**:
```
undefined reference to `bt_audio_interface_virtual_register`
```

**解决方法**:
确保 `bt_audio_interface_virtual.c` 已添加到构建系统中。

## 参考文档

- [Bluetooth Audio Framework 架构文档](../../components/lisa_bt_audio_framework/docs/ARCHITECTURE_OVERVIEW.md)
- [Source Mode 实现指南](../../components/lisa_bt_audio_framework/docs/SOURCE_MODE_ARCHITECTURE.md)
- [快速开始指南](../../components/lisa_bt_audio_framework/docs/SOURCE_MODE_QUICK_START.md)

## 许可证

Copyright (c) 2024 listenai.com
SPDX-License-Identifier: Apache-2.0
