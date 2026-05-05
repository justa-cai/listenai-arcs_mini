#include <string.h>

#include "voice_player_comm.h"
#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"

#include "tone.h"
#include "app_player.h"
#include "lsc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_gpio.h"
#include "kv.h"
#include "lisa_kv.h"

#define TAG "voice_player_comm"
#include "lisa_log.h"
#include "board.h"

// 播放器实例
app_player_t *tone_player = NULL;
app_player_t *tts_player = NULL;
app_player_t *music_player = NULL;
app_player_t *alert_player = NULL;

// 播放列表管理
#define MAX_PLAYLIST_SIZE 100

typedef struct {
    char id[32];        // 音频ID
    char name[64];      // 音频名称
    uint8_t playable;   // 是否可播放
} playlist_item_t;

typedef struct {
    playlist_item_t items[MAX_PLAYLIST_SIZE];
    int count;          // 列表长度
    int current_index;  // 当前播放索引
} playlist_t;

static playlist_t g_playlist = {0};

// 系统音量（1-100）
static int g_system_volume = 50;

void voice_player_system_volume_init(void);

#define PA_PIN_NUM PA_EN_PIN
#define PA_GPIO_DEVICE CONFIG_PA_DEVICE_NAME

/**
 * @brief PA控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM, onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

/**
 * @brief 播放列表中指定索引的音频（自动跳过不可播放项）
 */
static int playlist_play_index(int index)
{
    if (index < 0 || index >= g_playlist.count) {
        LOGE("Invalid playlist index: %d (count: %d)", index, g_playlist.count);
        return -1;
    }

    // 从当前索引开始查找可播放的项
    int search_count = 0;
    int current_idx = index;

    while (search_count < g_playlist.count) {
        playlist_item_t *item = &g_playlist.items[current_idx];

        // 目前不检查该标记
        // if (!item->playable) {
        //     LOGW("Item %s is not playable, skipping to next", item->id);
        //     current_idx = (current_idx + 1) % g_playlist.count;
        //     search_count++;
        //     continue;
        // }

        // 从ID获取播放URL
        char url[256] = {0};
        int ret = lsc_music_request_url(item->id, url);

        if (ret != 0 || url[0] == '\0') {
            LOGE("Failed to get URL for item: %s (ret=%d), skipping to next", item->id, ret);
            current_idx = (current_idx + 1) % g_playlist.count;
            search_count++;
            continue;
        }

        LOGI("Playing playlist item [%d/%d]: %s - %s",
             current_idx + 1, g_playlist.count, item->name, item->id);
        LOGI("URL: %s", url);

        g_playlist.current_index = current_idx;
        ret = app_player_play(music_player, url);

        if (ret != APP_PLAYER_OK) {
            LOGE("Failed to play item: %s, skipping to next", item->id);
            current_idx = (current_idx + 1) % g_playlist.count;
            search_count++;
            continue;
        }

        return 0;
    }

    LOGE("No playable items in playlist");
    return -1;
}

/**
 * @brief 播放器事件回调
 */
static void player_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    const char *player_name = (const char *)user_data;

    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("[%s] Player prepared", player_name);
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("[%s] Player playing", player_name);
        break;
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("[%s] Player paused", player_name);
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        LOGI("[%s] Player completed", player_name);

        // 音乐播放完成后自动播放下一首
        if (player == music_player && g_playlist.count > 0) {
            int next_index = g_playlist.current_index + 1;
            if (next_index < g_playlist.count) {
                LOGI("Auto-advancing to next track");
                playlist_play_index(next_index);
            } else {
                LOGI("Playlist completed");
                g_playlist.current_index = -1;
            }
        }
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("[%s] Player error", player_name);

        // 播放错误时尝试播放下一首
        if (player == music_player && g_playlist.count > 0) {
            int next_index = g_playlist.current_index + 1;
            if (next_index < g_playlist.count) {
                LOGI("Error occurred, trying next track");
                playlist_play_index(next_index);
            }
        }
        break;
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("[%s] Player stopped", player_name);
        break;
    default:
        break;
    }
}

/**
 * @brief 焦点变化回调 - 音乐播放器专用
 *
 * @note 此回调仅用于监听焦点变化事件，不需要手动执行 pause/resume/stop
 *       app_player 会根据配置的 behavior 自动执行相应策略
 * @return false 表示让 app_player 执行默认策略，true 表示完全接管
 */
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

/**
 * @brief 焦点变化回调 - 通用版本
 */
static bool focus_change_callback(app_player_t *player,
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
    return false;
}

int voice_player_platform_init(void)
{
    int ret = 0;

    LOGI("voice_player_platform_init...");

    /* 初始化PA控制GPIO */
    lisa_device_t *gpio_dev = lisa_device_get(PA_GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        LOGE("Error: %s device not ready", PA_GPIO_DEVICE);
        return -1;
    }
    ret = lisa_gpio_configure(gpio_dev, PA_PIN_NUM, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        LOGE("GPIO configure failed: %d", ret);
        return -1;
    }

    /* 定义焦点通道配置 */
    app_player_focus_channel_config_t focus_configs[] = {
        {
            .name = "tone",
            .priority = 0,  // 最高优先级（本地提示音）
            .capture_names = (const char *[]){"tts"},
            .capture_count = 1,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        },
        {
            .name = "tts",
            .priority = 10,  // 中等优先级
            .capture_names = (const char *[]){"alert"},
            .capture_count = 1,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_STOP,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        },
        {
            .name = "music",
            .priority = 50,  // 最低优先级
            .capture_names = NULL,
            .capture_count = 0,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        },
        {
            .name = "alert",
            .priority = 40,  // 最低优先级
            .capture_names = (const char *[]){"tts", "tone"},
            .capture_count = 2,
            .behavior = {
                .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
            }
        }
    };

    /* 初始化 app_player 模块（带焦点管理） */
    app_player_config_t app_config = {
        .pa_ctrl_callback = pa_control_callback,
        .focus_configs = focus_configs,
        .focus_config_count = 4
    };

    ret = app_player_init(&app_config);
    if (ret != APP_PLAYER_OK) {
        LOGE("App player init failed: %d", ret);
        return -1;
    }

    tone_player = app_player_create("tone");
    if (tone_player == NULL) {
        LOGE("Failed to create tone player");
        return -1;
    }

    tts_player = app_player_create("tts");
    if (tts_player == NULL) {
        LOGE("Failed to create tts player");
        return -1;
    }

    music_player = app_player_create("music");
    if (music_player == NULL) {
        LOGE("Failed to create music player");
        return -1;
    }

    alert_player = app_player_create("alert");
    if (alert_player == NULL) {
        LOGE("Failed to create alert player");
        return -1;
    }

    /* 注册事件回调 */
    app_player_register_callback(music_player, player_event_callback, "MUSIC");

    /* 注册焦点变化回调 */
    app_player_register_focus_cb(music_player, music_focus_change_callback, "MUSIC");

    voice_player_system_volume_init();
    return 0;
}

int voice_player_play_array(const struct voice_msg_audio_item *items, int count)
{
    if (items == NULL || count <= 0) {
        LOGE("Invalid parameters");
        return -1;
    }

    if (count > MAX_PLAYLIST_SIZE) {
        LOGW("Playlist size %d exceeds max %d, truncating", count, MAX_PLAYLIST_SIZE);
        count = MAX_PLAYLIST_SIZE;
    }

    // 停止当前播放
    app_player_stop(music_player);

    // 清空并重建播放列表
    memset(&g_playlist, 0, sizeof(g_playlist));
    g_playlist.count = count;
    g_playlist.current_index = -1;

    // 复制音频项到播放列表
    for (int i = 0; i < count; i++) {
        memcpy(g_playlist.items[i].id, items[i].id, sizeof(g_playlist.items[i].id));
        memcpy(g_playlist.items[i].name, items[i].name, sizeof(g_playlist.items[i].name));
        g_playlist.items[i].playable = items[i].playable;

        LOGI("Playlist[%d]: %s - %s (playable: %d)",
             i, g_playlist.items[i].name, g_playlist.items[i].id, g_playlist.items[i].playable);
    }

    // 播放第一首
    return playlist_play_index(0);
}

int voice_player_play_next(void)
{
    if (g_playlist.count == 0) {
        LOGW("Playlist is empty");
        return -1;
    }

    int next_index = g_playlist.current_index + 1;
    if (next_index >= g_playlist.count) {
        LOGI("Already at last track, wrapping to first");
        next_index = 0;
    }

    return playlist_play_index(next_index);
}

int voice_player_play_prev(void)
{
    if (g_playlist.count == 0) {
        LOGW("Playlist is empty");
        return -1;
    }

    int prev_index = g_playlist.current_index - 1;
    if (prev_index < 0) {
        LOGI("Already at first track, wrapping to last");
        prev_index = g_playlist.count - 1;
    }

    return playlist_play_index(prev_index);
}

int voice_player_replay_current(void)
{
    if (g_playlist.count == 0) {
        LOGW("Playlist is empty");
        return -1;
    }

    if (g_playlist.current_index < 0) {
        LOGW("No current track");
        return -1;
    }

    return playlist_play_index(g_playlist.current_index);
}

static bool voice_player_state_is_audio_active(app_player_t *player)
{
    if (player == NULL) {
        return false;
    }

    switch (app_player_get_state(player)) {
    case APP_PLAYER_STATE_PREPARING:
    case APP_PLAYER_STATE_PREPARED:
    case APP_PLAYER_STATE_PLAYING:
        return true;
    default:
        return false;
    }
}

bool voice_player_is_music_active(void)
{
    return voice_player_state_is_audio_active(music_player);
}

bool voice_player_is_audio_active(void)
{
    return voice_player_state_is_audio_active(tone_player) ||
           voice_player_state_is_audio_active(tts_player) ||
           voice_player_state_is_audio_active(music_player) ||
           voice_player_state_is_audio_active(alert_player);
}

int voice_player_set_system_volume(int volume)
{
    // 参数校验
    if (volume < 0 || volume > 100) {
        LOGE("Invalid volume: %d (valid range: 0-100)", volume);
        return -1;
    }

    LOGI("Setting system volume to %d", volume);

    // 保存系统音量
    g_system_volume = volume;

    int ret = 0;
    int failed_count = 0;

    // 设置所有已创建的播放器音量
    if (tone_player != NULL) {
        int r = app_player_set_volume(tone_player, volume);
        if (r != APP_PLAYER_OK) {
            LOGE("Failed to set tone player volume: %d", r);
            failed_count++;
        }
    }

    if (tts_player != NULL) {
        int r = app_player_set_volume(tts_player, volume);
        if (r != APP_PLAYER_OK) {
            LOGE("Failed to set tts player volume: %d", r);
            failed_count++;
        }
    }

    if (music_player != NULL) {
        int r = app_player_set_volume(music_player, volume);
        if (r != APP_PLAYER_OK) {
            LOGE("Failed to set music player volume: %d", r);
            failed_count++;
        }
    }

    if (alert_player != NULL) {
        int r = app_player_set_volume(alert_player, volume);
        if (r != APP_PLAYER_OK) {
            LOGE("Failed to set alert player volume: %d", r);
            failed_count++;
        }
    }

    if (failed_count > 0) {
        LOGW("Failed to set volume for %d player(s)", failed_count);
        return -1;
    }

    lisa_kv_set_int(KV_KEY_USER_VOLUME, g_system_volume);

    LOGI("System volume set to %d successfully", volume);
    return 0;
}

int voice_player_get_system_volume(void)
{
    return g_system_volume;
}

void voice_player_system_volume_init(void)
{
    int r;

	r = lisa_kv_get_int(KV_KEY_USER_VOLUME, &g_system_volume);
	if (r != 0) {
        LOGW("Failed to get system volume from kv, use default value 70");
		g_system_volume = 70;
	} else {
		if (g_system_volume > 100) {
			g_system_volume = 100;
		}

		if (g_system_volume < 0) {
			g_system_volume = 0;
		}
	}

    voice_player_set_system_volume(g_system_volume);
}
