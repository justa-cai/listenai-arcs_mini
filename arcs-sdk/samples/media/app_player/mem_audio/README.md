# app_player 内存音频播放示例

## 功能说明

演示如何使用 `app_player` 组件通过 `mem://` 协议播放存储在 Flash 内存中的音频文件，包括基本的播放、暂停、恢复和停止控制。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关

## 示例步骤

1. 将音频文件预先烧录到 Flash 指定地址（默认 `0x30100000`）
2. 初始化 `app_player` 模块并配置 PA 控制回调
3. 创建播放器实例并注册事件回调
4. 构造 `mem://` 协议 URL 指定音频数据地址和大小
5. 播放内存中的音频数据
6. 演示暂停、恢复、停止等控制操作

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
=== Memory Audio Playback Sample ===
Audio Flash Address: 0x30100000
Audio Data Size: 8192 bytes
PA OFF
Memory URL: mem://addr=806355570size=8192

=== Scenario 1: Play memory audio ===
Playing audio from memory...
PA ON
Player prepared
Player playing

=== Scenario 2: Pause playback ===
Audio paused
Player paused

=== Scenario 3: Resume playback ===
Audio resumed
Player playing

=== Scenario 4: Stop playback ===
Audio stopped
Player stopped
PA OFF

=== Sample completed ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `app_player_init()` | 初始化 app_player 模块 |
| `app_player_create()` | 创建播放器实例 |
| `app_player_register_callback()` | 注册播放事件回调函数 |
| `app_player_play()` | 播放指定 URL 的音频（支持 `mem://` 协议） |
| `app_player_pause()` | 暂停播放 |
| `app_player_resume()` | 恢复播放 |
| `app_player_stop()` | 停止播放（同步接口） |
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

```c
/* 构造 mem:// URL */
char mem_url[64];
snprintf(mem_url, sizeof(mem_url), "mem://addr=%usize=%u",
         AUDIO_FLASH_ADDR, AUDIO_DATA_SIZE);

/* 初始化播放器 */
app_player_config_t config = { .pa_ctrl_callback = pa_control_callback };
app_player_init(&config);

/* 创建播放器并播放 */
app_player_t *player = app_player_create("mem_demo");
app_player_register_callback(player, player_event_callback, NULL);
app_player_play(player, mem_url);
```

## 注意事项

1. **地址对齐**: Flash 地址 `AUDIO_FLASH_ADDR` 必须与烧录工具的地址一致
2. **文件大小**: `AUDIO_DATA_SIZE` 必须设置为实际文件大小，否则可能导致播放失败
3. **分区冲突**: 确保音频存储地址不与固件或其他数据的 Flash 分区冲突
4. **音频格式**: 支持 MP3、WAV、AAC、M4A、PCM 格式，推荐 16kHz、16bit、单声道
5. **PA 回调必填**: `app_player_init()` 时必须提供 `pa_ctrl_callback`，用于控制功放开关
