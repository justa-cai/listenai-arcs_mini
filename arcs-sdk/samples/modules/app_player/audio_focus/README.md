# 音频焦点管理示例

本示例演示如何使用 `app_player` 的音频焦点管理功能，实现多个播放器之间的智能焦点调度。

## 功能说明

本示例创建了 3 个播放器，模拟真实应用场景中的音频焦点管理：

- **tone** - 本地提示音播放器（优先级 0，最高）
- **tts** - 语音播报播放器（优先级 1）
- **music** - 音乐播放器（优先级 2，最低）

### 焦点抢占规则

| 播放器 | 优先级 | 可抢占的播放器 |
|--------|--------|----------------|
| tone   | 0      | tts |
| tts    | 1      | tone |
| music  | 2      | 无 |

### 焦点状态

每个播放器可以处于以下焦点状态之一：

- **FOREGROUND** - 前景焦点：正常播放
- **BACKGROUND** - 背景焦点：被暂停或停止（取决于配置的 behavior）
- **NONE** - 无焦点：被停止

### 焦点行为策略

每个播放器可以配置不同的焦点丢失行为：

- **APP_PLAYER_FOCUS_LOSS_PAUSE** - 暂停播放（焦点恢复后可继续）
- **APP_PLAYER_FOCUS_LOSS_STOP** - 停止播放（焦点恢复后不会自动继续）
- **APP_PLAYER_FOCUS_LOSS_DUCK** - 降低音量（保持播放）

## 演示场景

示例代码演示了以下场景：

### 场景 1：播放音乐
- music 播放器开始播放网络音频
- music 处于 FOREGROUND 状态

### 场景 2：播放本地提示音
- tone 播放器开始播放本地提示音
- tone 抢占 music，music 被暂停（BACKGROUND，因为配置了 PAUSE 策略）
- tone 处于 FOREGROUND 状态

### 场景 3：提示音播放完毕
- tone 播放完成并释放焦点
- music 不会立即恢复（通过焦点回调阻止自动恢复）
- 等待 3 秒后播放 TTS

### 场景 4：播放 TTS
- tts 播放器开始播放网络 TTS 音频
- tts 抢占 music，music 保持暂停状态
- tts 处于 FOREGROUND 状态

### 场景 5：TTS 播放完毕
- tts 播放完成并释放焦点
- music 自动恢复到 FOREGROUND，继续播放

## 关键代码说明

### 1. 定义焦点配置

```c
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
```

### 2. 初始化 app_player（带焦点管理）

```c
app_player_config_t app_config = {
    .pa_ctrl_callback = pa_control_callback,
    .focus_configs = focus_configs,
    .focus_config_count = 3
};

app_player_init(&app_config);
```

### 3. 创建播放器

```c
// 播放器名称需要与焦点配置中的 name 匹配
tone_player = app_player_create("tone");
tts_player = app_player_create("tts");
music_player = app_player_create("music");
```

### 4. 注册焦点变化回调

```c
bool focus_change_callback(app_player_t *player,
                          app_player_focus_state_t state,
                          app_player_t *by_which,
                          void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            LOGI("[%s] Got FOREGROUND focus (by player %p)", player_name, by_which);
            break;
        case APP_PLAYER_FOCUS_BACKGROUND:
            LOGI("[%s] Moved to BACKGROUND (by player %p)", player_name, by_which);
            break;
        case APP_PLAYER_FOCUS_NONE:
            LOGI("[%s] Lost focus (by player %p)", player_name, by_which);
            break;
    }

    // 返回 false，让 app_player 根据配置自动执行策略
    // 返回 true 可以阻止自动执行，完全自定义处理
    return false;
}

app_player_register_focus_cb(tone_player, focus_change_callback, "TONE");
app_player_register_focus_cb(tts_player, focus_change_callback, "TTS");
app_player_register_focus_cb(music_player, music_focus_change_callback, "MUSIC");
```

**关键特性**：music 播放器使用了特殊的焦点回调 `music_focus_change_callback`，用于实现自定义的焦点恢复逻辑：

```c
static bool music_focus_change_callback(app_player_t *player,
                                        app_player_focus_state_t state,
                                        app_player_t *by_which,
                                        void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            LOGI("[%s] Got FOREGROUND focus (by player %p)", player_name, by_which);

            // 如果是 tone 完成后恢复焦点，先不做任何操作
            if (by_which == tone_player) {
                LOGI("[%s] Tone completed, but waiting for TTS...", player_name);
                // 返回 true 阻止自动恢复播放
                return true;
            }
            break;

        case APP_PLAYER_FOCUS_BACKGROUND:
            LOGI("[%s] Moved to BACKGROUND (by player %p)", player_name, by_which);
            break;

        case APP_PLAYER_FOCUS_NONE:
            LOGI("[%s] Lost focus (by player %p)", player_name, by_which);
            break;
    }

    // 返回 false，让 app_player 根据配置自动执行策略
    return false;
}
```

**说明**：
- 当 tone 播放完成释放焦点时，music 会收到 `FOREGROUND` 状态回调
- 通过检查 `by_which == tone_player`，判断是 tone 释放的焦点
- 返回 `true` 阻止 music 自动恢复播放，等待后续的 TTS 播放
- 当 TTS 播放完成释放焦点时，`by_which` 不是 `tone_player`，返回 `false` 允许 music 自动恢复

这展示了如何通过焦点回调的返回值来实现复杂的焦点管理逻辑。

### 5. 播放时自动申请焦点

```c
// 播放网络音频
app_player_play(music_player, "https://example.com/music.mp3");

// 播放本地提示音
const char *tone_url = app_tone_get_url(TONE_ID_0);
app_player_play(tone_player, tone_url);

// 播放时会自动申请焦点，如果该播放器可以抢占当前播放的其他播放器，
// 则其他播放器会收到焦点变化回调，并根据配置的 behavior 自动执行相应策略
```

## 编译和运行

### 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 运行

烧录后，程序会自动运行并演示各种焦点场景，查看串口输出日志了解焦点切换过程。

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

## 注意事项

1. **焦点配置必须在 `app_player_init` 时提供**，创建播放器后无法动态修改
2. **播放器名称必须与焦点配置的 name 匹配**，否则该播放器不会参与焦点管理
3. **焦点回调函数应快速返回**，避免阻塞焦点管理器，耗时操作应提交到任务队列
4. **焦点回调返回值的含义**：
   - 返回 `false`：让 app_player 根据配置的 behavior 自动执行策略（推荐）
   - 返回 `true`：阻止自动执行，完全由应用程序自定义处理
5. **behavior 配置说明**：
   - `on_background`：被抢占焦点时的行为（进入 BACKGROUND 状态）
   - `on_focus_lost`：完全失去焦点时的行为（进入 NONE 状态）
6. **示例使用本地提示音和网络音频**，需要先烧录 tone.bin 并确保网络连接正常

## 扩展使用

### 自定义焦点处理策略

你可以根据应用需求定制焦点变化的处理策略：

```c
bool custom_focus_callback(app_player_t *player,
                          app_player_focus_state_t state,
                          app_player_t *by_which,
                          void *user_data)
{
    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            // 策略 1：自定义恢复逻辑（例如根据条件决定是否恢复）
            if (should_resume()) {
                return false;  // 让系统自动恢复
            } else {
                return true;   // 阻止自动恢复
            }
            break;

        case APP_PLAYER_FOCUS_BACKGROUND:
            // 策略 2：完全自定义处理（例如降低音量而不是暂停）
            app_player_set_volume(player, 30);
            return true;  // 阻止系统执行默认策略

        case APP_PLAYER_FOCUS_NONE:
            // 策略 3：记录位置后停止
            uint32_t position;
            app_player_get_position(player, &position);
            save_playback_position(position);
            return false;  // 让系统执行配置的策略
    }
    return false;
}
```

## 常见问题

### WiFi 连接失败

**问题描述**：示例程序无法连接到 WiFi 网络，或者网络连接超时。

**解决方法**：

本示例需要联网才能播放网络音频和在线 TTS，请按以下步骤配置 WiFi：

1. 打开 [src/net/net_connect.c](src/net/net_connect.c#L28-L29) 文件

2. 修改WiFi配置为你的实际网络信息：

```c
#define TARGET_WIFI_SSID "your_wifi_ssid"      // 替换为你的 WiFi 名称
#define TARGET_WIFI_PWD "your_wifi_password"   // 替换为你的 WiFi 密码
```

