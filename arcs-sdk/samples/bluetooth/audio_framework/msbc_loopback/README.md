# 蓝牙音频框架示例 - mSBC编解码

本示例演示如何使用 `lisa_bt_audio_framework` 进行音频的录音、编码、解码和播放。

## 功能说明

1. **录音与编码**：从麦克风录制音频，并使用 mSBC 编解码器进行编码
2. **解码与播放**：接收 mSBC 编码的数据，解码后通过扬声器播放
3. **环形缓冲区**：使用环形缓冲区在编码和解码会话之间传输数据

## 硬件连接

- 板卡需要有 `audio0` 设备，包括麦克风和扬声器

## 架构说明

```
[麦克风] -> [录音会话/编码] -> [环形缓冲区] -> [播放会话/解码] -> [扬声器]
              (mSBC Encoder)                      (mSBC Decoder)
```

### 数据流程

1. **录音线程（Encode Task）**
   - 从录音会话获取编码后的 mSBC 数据
   - 将编码数据写入环形缓冲区
   
2. **播放线程（Decode Task）**
   - 从环形缓冲区读取编码数据
   - 将数据喂给播放会话进行解码和播放

## mSBC 编解码器

- **采样率**：16 kHz
- **声道**：单声道
- **位深**：16 位
- **帧大小**：57 字节（编码后）
- **PCM 帧大小**：240 字节（120 个采样点）
- **比特率**：约 64 kbps
- **帧时长**：7.5 ms

## 配置说明

### prj.conf 配置

```properties
# 蓝牙音频框架
CONFIG_LISA_BT_AUDIO_FRAMEWORK=y
CONFIG_LISA_BT_AUDIO_INTERFACE=y

# mSBC 编解码器
CONFIG_AUDIO_CODEC_MSBC=y

# 音频驱动
CONFIG_LISA_AUDIO_DEVICE=y
```

### 线程优先级

- **编码任务**：优先级 10，栈大小 2048 字节
- **解码任务**：优先级 10，栈大小 2048 字节

## 示例内容

1. 初始化蓝牙音频框架
2. 创建录音会话（mSBC 编码模式）
3. 创建播放会话（mSBC 解码模式）
4. 启动编码和解码任务
5. 运行 60 秒后自动停止并清理资源

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
2. 创建录音会话（mSBC 编码）
3. 创建播放会话（mSBC 解码）
4. 启动编码和解码任务
5. 运行 60 秒后自动停止

### 日志输出示例

```
[bt_audio_sample] BT Audio Framework Sample - mSBC Codec
[bt_audio_sample] =====================================
[bt_audio_sample] BT audio framework initialized, version: 1.0.0
[bt_audio_sample] Audio device obtained: audio0
[bt_audio_sample] Capture session created and started
[bt_audio_sample] Playback session created and started
[bt_audio_sample] Encode task started
[bt_audio_sample] Decode task started
[bt_audio_sample] Sample running... Press Ctrl+C to stop
[bt_audio_sample] Recording audio -> mSBC encode -> mSBC decode -> Playback
[bt_audio_sample] Encoded 10240 bytes, buffer: 2048/16384
[bt_audio_sample] Decoded 10240 bytes
...
```

## API 使用说明

### 1. 初始化框架

```c
bt_audio_error_t ret = bt_audio_framework_init();
bt_audio_register_all_codecs();
bt_audio_register_all_interfaces();
```

### 2. 创建会话

```c
bt_audio_session_config_t config = {
    .profile = BT_PROFILE_HFP,
    .codec_type = BT_CODEC_MSBC,
    .direction = BT_AUDIO_DIR_CAPTURE,  // 或 BT_AUDIO_DIR_PLAYBACK
    .interface_name = "lisa_audio",
    .event_callback = event_callback,
    .user_data = NULL,
};

bt_audio_session_handle_t session;
bt_audio_session_create(&config, &session);
```

### 3. 启动会话

```c
bt_audio_codec_config_t codec_config = {
    .codec_type = BT_CODEC_MSBC,
    .format = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    },
};

bt_audio_session_start(session, &codec_config);
```

### 4. 数据传输

**编码方向（录音）**：
```c
// 获取编码后的数据
size_t read_size;
bt_audio_session_get_encoded_data(session, buffer, size, &read_size);
```

**解码方向（播放）**：
```c
// 喂入编码数据进行解码和播放
bt_audio_session_feed_encoded_data(session, data, size);
```

### 5. 清理资源（正确顺序）

```c
// 1. 先停止会话（停止硬件和编解码线程）
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
2. **编解码模式**：`passthrough_mode = false` 表示启用 mSBC 编解码
3. **帧对齐**：mSBC 数据必须按帧（57 字节）传输，不能截断
4. **清理顺序**：必须按照正确顺序清理资源，避免 ringbuf full 警告
5. **延迟**：系统引入一定的缓冲延迟（约 100-200ms），用于平滑数据流
6. **内存要求**：确保有足够的 RAM 用于编解码缓冲区

## 扩展建议

1. **蓝牙传输**：可将编码数据通过蓝牙发送到另一个设备
2. **文件录制**：将编码数据保存到文件系统
3. **音量控制**：使用 `bt_audio_session_set_volume()` 调整音量
4. **其他编解码器**：支持 SBC、LC3 等其他编解码器
5. **A2DP/HFP**：与实际的蓝牙协议栈集成

## 参考文档

- [lisa_bt_audio_framework 组件文档](../../../components/lisa_bt_audio_framework/README.md)
- [lisa_audio 驱动文档](../../../drivers/lisa_audio/README.md)
- [mSBC 编解码器规范](https://www.bluetooth.com/specifications/specs/hands-free-profile-1-8/)

## 故障排查

### 问题：没有音频输出

**可能原因**：
- 音频设备未正确初始化
- 麦克风或扬声器硬件问题
- mSBC 编解码器未正确配置

**解决方法**：
- 检查 `CONFIG_AUDIO_CODEC_MSBC=y` 是否已设置
- 查看日志中的错误信息
- 测试硬件是否正常

### 问题：音频断续或有噪音

**可能原因**：
- 缓冲区配置不足
- 系统负载过高
- 编解码错误或丢帧

**解决方法**：
- 增加 PCM ringbuf 时间配置
- 增加编码数据 ringbuf 大小
- 提高任务优先级
- 检查 mSBC 编解码器返回值
- 确保数据按帧边界对齐

### 问题：程序退出时有 ringbuf full 警告

**原因**：清理顺序不正确，会话停止前任务就已退出

**解决方法**：按照正确的清理顺序操作（见上文第 5 节）

### 问题：编译错误

**解决方法**：
- 确认 `prj.conf` 中已启用 mSBC 相关配置
- 检查 SDK 版本是否支持 `lisa_bt_audio_framework`
- 确认依赖组件已正确配置

## 许可证

Copyright (c) 2025, LISTENAI

SPDX-License-Identifier: Apache-2.0
