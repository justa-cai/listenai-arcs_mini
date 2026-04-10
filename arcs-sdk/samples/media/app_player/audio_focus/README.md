# app_player 音频焦点管理示例

## 功能说明

演示如何使用 `app_player` 的音频焦点管理功能，实现多个播放器之间的智能焦点调度，支持焦点抢占、暂停恢复和自定义回调处理。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关
- **WiFi**: 需要设备支持 WiFi 并连接到互联网（用于播放网络音频）

## 示例步骤

1. 配置焦点通道（tone/tts/music 三个优先级）并初始化 `app_player`
2. 创建三个播放器实例并注册焦点变化回调
3. 播放音乐（music 获得前景焦点）
4. 播放提示音（tone 抢占 music，music 被暂停）
5. 提示音播放完成后等待 TTS 播放
6. TTS 播放完成后 music 自动恢复

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
=== Audio Focus Management Sample ===
Creating players...
All players created and registered

=== Scenario 1: Playing music ===
Music is playing...
[MUSIC] Player prepared
[MUSIC] Player playing

=== Scenario 2: Playing tone (music will pause) ===
[MUSIC] Moved to BACKGROUND (by player 0x...)
[MUSIC] Player paused
[TONE] Player prepared
[TONE] Player playing
[TONE] Player completed

=== Scenario 3: Tone completed, waiting 3s before TTS ===
Music should NOT resume yet (by_which=tone_player, blocked in callback)

=== Scenario 4: Playing TTS (music stays paused) ===
TTS is playing...
[TTS] Player prepared
[TTS] Player playing
[TTS] Player completed

=== Scenario 5: TTS completed, music should auto-resume ===
[MUSIC] Got FOREGROUND focus (by player 0x...)
[MUSIC] Player playing

=== Stopping all players ===
[MUSIC] Player stopped

=== Demo completed ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `app_player_init()` | 初始化 app_player 模块（含焦点配置） |
| `app_player_create()` | 创建播放器实例（名称需匹配焦点配置） |
| `app_player_register_focus_cb()` | 注册焦点变化回调函数 |
| `app_player_register_callback()` | 注册播放事件回调函数 |
| `app_player_play()` | 播放音频（自动申请焦点） |
| `app_player_pause()` | 暂停播放 |
| `app_player_resume()` | 恢复播放 |
| `app_player_stop()` | 停止播放 |
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

```c
/* 定义焦点配置 */
app_player_focus_channel_config_t focus_configs[] = {
    {
        .name = "tone",
        .priority = 0,  // 最高优先级
        .capture_names = (const char *[]){"tts"},
        .capture_count = 1,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "tts",
        .priority = 1,
        .capture_names = (const char *[]){"tone"},
        .capture_count = 1,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "music",
        .priority = 2,  // 最低优先级
        .capture_names = NULL,
        .capture_count = 0,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    }
};

/* 初始化 app_player（带焦点管理） */
app_player_config_t app_config = {
    .pa_ctrl_callback = pa_control_callback,
    .focus_configs = focus_configs,
    .focus_config_count = 3
};
app_player_init(&app_config);

/* 创建播放器并注册焦点回调 */
tone_player = app_player_create("tone");
app_player_register_focus_cb(tone_player, focus_change_callback, "TONE");
```

## 注意事项

1. **焦点配置时机**: 焦点配置必须在 `app_player_init()` 时提供，创建播放器后无法动态修改
2. **名称匹配**: 播放器名称必须与焦点配置的 `name` 匹配，否则该播放器不会参与焦点管理
3. **回调返回值**: 返回 `false` 让系统自动执行策略，返回 `true` 阻止自动执行
4. **音频文件准备**: 运行前需将 tone 音频文件烧录到 Flash 指定地址，并确保网络连接正常
5. **回调执行速度**: 焦点回调函数应快速返回，避免阻塞焦点管理器
