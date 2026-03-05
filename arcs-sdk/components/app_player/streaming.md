# 流式播放使用指南

流式播放功能支持实时写入 PCM 音频数据并播放，特别适用于以下场景：
- **实时 TTS 合成**：边合成边播放，降低延迟
- **网络音频流**：实时接收并播放网络音频数据
- **实时音频处理**：处理后的音频数据实时播放

## 使用流程

### 1. 开始流式播放

使用 `app_player_play_stream` 初始化流式播放：

```c
// 开始流式播放(16kHz, 单声道, 16位)
int ret = app_player_play_stream(player, 16000, 1, 16);
if (ret != APP_PLAYER_OK) {
    // 处理错误
}
```

**参数说明：**
- `sample_rate`: 采样率(Hz)，如 8000, 16000, 48000
- `channels`: 声道数（当前仅支持 1 = 单声道）
- `bits`: 位深度（当前仅支持 16 位）

### 2. 写入音频数据

使用 `app_player_write_stream` 逐块写入 PCM 数据：

```c
uint8_t pcm_buffer[1024];
int size = 1024;

// 写入PCM数据(超时时间1000ms)
int written = app_player_write_stream(player, pcm_buffer, size, 1000);
if (written < 0) {
    // 写入失败
} else {
    // written 为实际写入的字节数
}
```

**使用说明：**
- 可以多次调用以逐块写入数据
- `timeout_ms` 指定写入超时时间(毫秒)
- 返回值为实际写入的字节数，负数表示错误

### 3. 结束流式播放

数据写入完成后，调用 `app_player_finish_stream` 通知播放器：

```c
// 通知播放器数据已全部写入
int ret = app_player_finish_stream(player);
if (ret != APP_PLAYER_OK) {
    // 处理错误
}

// 播放完成后会收到 APP_PLAYER_EVENT_COMPLETED 事件
```

## 使用限制（重要）

**流式播放仅支持以下操作流程：**

```
app_player_play_stream -> app_player_write_stream -> app_player_finish_stream
```

**流式播放模式下不支持以下操作：**
- ❌ `app_player_seek()` - 跳转操作
- ❌ `app_player_pause()` - 暂停操作
- ❌ `app_player_stop()` / `app_player_stop_sync()` - 停止操作
- ❌ `app_player_resume()` / `app_player_resume_sync()` - 恢复操作

如果在流式播放模式下调用上述接口，会返回 `APP_PLAYER_ERR_NOT_SUPPORTED` 错误。

## 注意事项

1. **音频格式限制**：
   - 当前仅支持单声道（channels = 1）
   - 当前仅支持 16 位深度（bits = 16）
   - 采样率可自定义（推荐 16000 Hz）

2. **写入顺序**：
   - 必须先调用 `app_player_play_stream` 初始化
   - 然后可多次调用 `app_player_write_stream` 写入数据
   - 最后调用 `app_player_finish_stream` 结束

3. **错误处理**：
   - 如果未调用 `play_stream` 就调用 `write_stream`，会返回错误
   - 写入超时会返回错误，需检查返回值
   - 在流式模式下调用不支持的操作会返回 `APP_PLAYER_ERR_NOT_SUPPORTED`

4. **与普通播放的区别**：
   - 流式播放不能与 URL 播放同时使用
   - 流式播放模式下不支持 seek/pause/stop/resume 等控制操作
   - 适合实时数据，不适合已知完整文件的场景
   - 如需中断流式播放，应调用 `app_player_finish_stream()` 正常结束

## 完整示例

```c
#include "app_player.h"

void stream_playback_example(app_player_t *player) {
    // 1. 开始流式播放
    int ret = app_player_play_stream(player, 16000, 1, 16);
    if (ret != APP_PLAYER_OK) {
        printf("Failed to start stream playback\n");
        return;
    }

    // 2. 逐块写入 PCM 数据
    uint8_t pcm_buffer[1024];
    int total_bytes = 0;

    while (has_more_data()) {  // 假设有数据源
        int size = get_pcm_data(pcm_buffer, sizeof(pcm_buffer));

        int written = app_player_write_stream(player, pcm_buffer, size, 1000);
        if (written < 0) {
            printf("Failed to write stream data\n");
            break;
        }

        total_bytes += written;
        printf("Written %d bytes, total %d\n", written, total_bytes);
    }

    // 3. 结束流式播放
    ret = app_player_finish_stream(player);
    if (ret != APP_PLAYER_OK) {
        printf("Failed to finish stream playback\n");
        return;
    }

    printf("Stream playback completed\n");
}
```
