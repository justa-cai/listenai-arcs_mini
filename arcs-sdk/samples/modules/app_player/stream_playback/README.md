# 流式播放示例

## 功能说明

本示例演示如何使用 `app_player` 组件的流式播放接口来播放 PCM 音频数据。

流式播放接口允许应用程序逐块写入音频数据，适用于以下场景：
- 实时音频合成（如 TTS 语音合成）
- 网络音频流播放
- 需要边生成边播放的应用

## 核心 API

```c
// 1. 开始流式播放（指定音频格式）
app_player_play_stream(player, sample_rate, channels, bits);

// 2. 写入 PCM 数据（可多次调用）
app_player_write_stream(player, data, size, timeout_ms);

// 3. 结束流式播放（通知数据写入完毕）
app_player_finish_stream(player);
```

## 完整流程

```c
// 初始化
app_player_config_t config = { .pa_ctrl_callback = pa_ctrl_callback };
app_player_init(&config);

// 创建播放器
app_player_t *player = app_player_create("stream_demo");
app_player_register_callback(player, event_callback, NULL);

// 流式播放
app_player_play_stream(player, 16000, 1, 16);   // 开始：16kHz, 单声道, 16位
app_player_write_stream(player, pcm_data, size, 1000);  // 写入数据
app_player_finish_stream(player);               // 结束

// 清理
app_player_destroy(player);
```

## 支持的音频格式

| 参数 | 支持值 |
|------|--------|
| 采样率 | 8000, 16000, 44100, 48000 Hz |
| 声道数 | 1（单声道）, 2（立体声） |
| 位深度 | 8, 16, 24, 32 bits |

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录运行

```{eval-rst}
.. include:: /sample_flash.rst
```

## 注意事项

1. 必须先调用 `app_player_play_stream()` 开启流式播放模式
2. 数据写入时需注意超时设置，避免阻塞过长
3. 调用 `app_player_finish_stream()` 后会自动播放完剩余缓冲区数据
4. 可通过事件回调监听 `APP_PLAYER_EVENT_COMPLETED` 获知播放完成
