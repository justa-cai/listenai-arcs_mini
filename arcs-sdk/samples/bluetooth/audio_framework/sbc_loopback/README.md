# 蓝牙音频框架示例 - SBC 回环测试

本示例演示如何使用 `lisa_bt_audio_framework` 进行 SBC 编解码回环，实现麦克风到扬声器的音频路径。

## 功能说明

1. **SBC 编码**：从麦克风录制音频，使用 SBC 编码器进行编码
2. **SBC 解码**：将编码后的 SBC 数据解码并播放到扬声器
3. **实时回环**：编码和解码在同一设备上进行，形成音频回环

## 硬件连接

- 板卡需要有 `audio0` 设备，包括麦克风和扬声器

## 架构说明

```
[麦克风] -> [录音会话/SBC编码] -> [回环任务] -> [播放会话/SBC解码] -> [扬声器]
              (SBC Encoder)                        (SBC Decoder)
```

### 数据流程

1. **回环任务（Loopback Task）**
   - 从录音会话获取 SBC 编码数据
   - 直接将 SBC 数据喂给播放会话进行解码
   - 实现编码->解码的实时回环

## SBC 编解码器参数

- **采样率**：16 kHz
- **声道**：单声道（Mono）
- **位深**：16 位
- **比特率**：自适应
- **子带数**：8
- **块长度**：16
- **分配方法**：Loudness

## 配置说明

### prj.conf 配置

```properties
# 蓝牙音频框架
CONFIG_LISA_BT_AUDIO_FRAMEWORK=y
CONFIG_LISA_BT_AUDIO_INTERFACE=y
CONFIG_LISA_BT_AUDIO_MAX_SESSIONS=4

# SBC 编解码器
CONFIG_AUDIO_CODEC_SBC=y

# 编码数据缓冲区配置
CONFIG_LISA_BT_AUDIO_ENCODED_RINGBUF_SIZE=8
CONFIG_LISA_BT_AUDIO_ENCODED_PREFILL_SIZE=4

# PCM 环形缓冲区时间配置
CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_DOWNLINK=200
CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_UPLINK=200
CONFIG_LISA_BT_AUDIO_PCM_PREFILL_TIME_MS=60

# 工作缓冲区配置
CONFIG_LISA_BT_AUDIO_WORK_BUFFER_TIME_MS=64
CONFIG_LISA_BT_AUDIO_HW_BUFFER_COUNT=3

# 音频驱动
CONFIG_LISA_AUDIO_DEVICE=y
CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE=y
```

### 线程配置

- **回环任务**：优先级 10，栈大小 2048 字节
- **编码任务**：由框架自动创建和管理
- **解码任务**：由框架自动创建和管理

# 音频驱动
CONFIG_LISA_AUDIO_DEVICE=y
CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE=y
```

### 线程优先级

- **透传任务**：优先级 10，栈大小 2048 字节

## 示例内容

1. 初始化蓝牙音频框架
2. 创建录音会话（SBC 编码模式）
3. 创建播放会话（SBC 解码模式）
4. 启动回环任务，实时转发 SBC 编码数据
5. 运行 10 秒后自动停止并清理资源

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

程序启动后会：
1. 初始化蓝牙音频框架
2. 创建录音会话（SBC 编码模式）
3. 创建播放会话（SBC 解码模式）
4. 启动回环任务，实时转发 SBC 编码数据
5. 运行 10 秒后自动停止并清理资源

### 日志输出示例

```
I/bt_audio_sample [0:00:00.100] BT Audio Framework Sample - SBC Loopback
I/bt_audio_sample [0:00:00.101] =========================================
I/bt_audio_sample [0:00:00.105] BT audio framework initialized, version: 1.0.0
I/bt_audio_sample [0:00:00.150] Capture session created and started (SBC encode mode)
I/bt_audio_sample [0:00:00.200] Playback session created and started (SBC decode mode)
I/bt_audio_sample [0:00:00.201] Sample running... Press Ctrl+C to stop
I/bt_audio_sample [0:00:00.201] Recording -> SBC Encode -> SBC Decode -> Playback
I/bt_audio_sample [0:00:00.210] SBC loopback task started
...
I/bt_audio_sample [0:00:10.000] Cleaning up...
I/bt_audio_sample [0:00:10.201] SBC loopback task stopped
I/bt_audio_sample [0:00:10.500] Sample finished
```

## API 使用说明

### 1. 初始化框架

```c
bt_audio_error_t ret = bt_audio_framework_init();
```

### 2. 创建编码会话（录音）

```c
bt_audio_session_config_t capture_config = {
    .profile = BT_PROFILE_HFP,
    .codec_type = BT_CODEC_SBC,               // SBC 编解码器
    .direction = BT_AUDIO_DIR_CAPTURE,        // 录音方向
    .interface_name = "lisa_audio",
    .passthrough_mode = false,                // 编解码模式
    .event_callback = capture_event_callback,
    .user_data = NULL,
};

bt_audio_session_handle_t capture_session;
bt_audio_session_create(&capture_config, &capture_session);
```

### 3. 启动编码会话

```c
bt_audio_codec_config_t codec_config = {
    .codec_type = BT_CODEC_SBC,
    .format = {
        .sample_rate = 16000,     // 16kHz
        .channels = 1,            // 单声道
        .bits_per_sample = 16,
    },
    .bitrate = 0,                 // 自适应比特率
};

bt_audio_session_start(capture_session, &codec_config);
```

### 4. 创建并启动解码会话（播放）

```c
// 配置和启动类似编码会话，但 direction = BT_AUDIO_DIR_PLAYBACK
```

### 5. SBC 数据传输

**从录音会话获取 SBC 编码数据**：
```c
uint8_t buffer[512];
size_t read_size;
bt_audio_session_get_encoded_data(capture_session, buffer, sizeof(buffer), &read_size);
```

**向播放会话喂入 SBC 编码数据**：
```c
bt_audio_session_feed_encoded_data(playback_session, buffer, read_size);
```

### 6. 清理资源（正确顺序）

```c
// 1. 先停止会话（停止硬件和线程）
bt_audio_session_stop(capture_session);
bt_audio_session_stop(playback_session);

// 2. 通知回环任务退出
g_running = false;

// 3. 等待任务完全退出
vTaskDelay(pdMS_TO_TICKS(200));

// 4. 销毁会话
bt_audio_session_destroy(capture_session);
bt_audio_session_destroy(playback_session);

// 5. 反初始化框架
bt_audio_framework_deinit();
```

## 注意事项

1. **音频设备**：确保板卡有 `audio0` 设备，包括麦克风和扬声器
2. **编解码模式**：`passthrough_mode = false` 表示启用 SBC 编解码
3. **缓冲区配置**：根据实际需求调整 ringbuf 大小
4. **清理顺序**：必须按照正确顺序清理资源，避免 ringbuf full 警告
5. **延迟**：SBC 编解码会引入一定延迟（约 100-200ms）

## SBC vs mSBC

| 特性 | SBC | mSBC |
|------|-----|------|
| 采样率 | 灵活（16/32/44.1/48 kHz） | 固定 16 kHz |
| 比特率 | 可变 | 固定（约 64 kbps） |
| 用途 | A2DP 音乐传输 | HFP 语音通话 |
| 帧时长 | 可变 | 固定 7.5ms |
| 子带数 | 4 或 8 | 固定 8 |
| 块长度 | 4/8/12/16 | 固定 15 |

## 扩展建议

1. **不同参数**：尝试不同的采样率、比特率和声道配置
2. **性能测试**：测量编解码延迟和 CPU 占用
3. **音质对比**：对比 SBC、mSBC 和 PCM 透传的音质差异
4. **多会话**：同时运行多个编解码会话
5. **文件录制**：将编码后的 SBC 数据保存到文件

## 参考文档

- [lisa_bt_audio_framework 组件文档](../../../components/lisa_bt_audio_framework/README.md)
- [SBC 编解码器文档](../../../soc/arcs/hal/modules/codec/sbc/README.md)
- [SBC 规范](https://www.bluetooth.com/specifications/specs/a2dp-1-3-2/)

## 故障排查

### 问题：没有音频输出

**可能原因**：
- 音频设备未正确初始化
- 麦克风或扬声器硬件问题
- SBC 编解码器未正确配置

**解决方法**：
- 检查 `CONFIG_AUDIO_CODEC_SBC=y` 是否已设置
- 查看日志中的错误信息
- 测试硬件是否正常

### 问题：音频断续或有噪音

**可能原因**：
- 缓冲区配置不足
- 系统负载过高
- 编解码错误

**解决方法**：
- 增加 PCM ringbuf 时间配置
- 增加编码数据 ringbuf 大小
- 提高任务优先级
- 检查 SBC 编解码器返回值

### 问题：程序退出时有 ringbuf full 警告

**原因**：清理顺序不正确，会话停止前任务就已退出

**解决方法**：按照正确的清理顺序操作（见上文第 6 节）

### 问题：编译错误

**解决方法**：
- 确认 `prj.conf` 中已启用 SBC 相关配置
- 检查 SDK 版本
- 确认依赖组件已正确配置

## 许可证

Copyright (c) 2025, LISTENAI

SPDX-License-Identifier: Apache-2.0
