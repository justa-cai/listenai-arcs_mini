#include <string.h>

#include "voice_player_comm.h"
#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"

#include "tone.h"
#include "app_player.h"
#include "lsc.h"
#include "voice_music_list.h"
#if CONFIG_LSFS
#include "lsfs.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lisa_gpio.h"
#include "kv.h"
#include "lisa_kv.h"

#define TAG "voice_player_comm"
#include "lisa_log.h"
#include "board.h"

/** 播放器实例 */
app_player_t *tone_player = NULL;
app_player_t *tts_player = NULL;
app_player_t *music_player = NULL;
app_player_t *alert_player = NULL;

/** 系统音量（1-100），Kconfig 配置默认值 */
static int g_system_volume = CONFIG_VOICE_PLAYER_DEFAULT_VOLUME;

/** 音量线程句柄 */
static TaskHandle_t g_volume_thread = NULL;
/** 音量线程信号量 */
static SemaphoreHandle_t g_volume_sem = NULL;

#define VOICE_PLAYER_SD_URL_PREFIX "/SD:/"

static bool voice_player_url_is_sd_file(const char *url)
{
    return url != NULL &&
           strncmp(url, VOICE_PLAYER_SD_URL_PREFIX,
                   sizeof(VOICE_PLAYER_SD_URL_PREFIX) - 1) == 0;
}

static int voice_player_check_sd_file_accessible(const char *url)
{
    if (!voice_player_url_is_sd_file(url)) {
        return APP_PLAYER_OK;
    }

#if CONFIG_LSFS
    struct lsfs_dirent file_info = {0};
    int ret = lsfs_stat(url, &file_info);
    if (ret != 0) {
        LOGW("Music SD file stat failed: url=%s ret=%d", url, ret);
        return APP_PLAYER_ERR_IO;
    }
    if (file_info.type != LSFS_DIR_ENTRY_FILE) {
        LOGW("Music SD path is not file: url=%s type=%d", url, file_info.type);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }
    return APP_PLAYER_OK;
#else
    LOGW("Music SD file check unsupported: url=%s", url);
    return APP_PLAYER_ERR_NOT_SUPPORTED;
#endif
}

static void voice_player_notify_sd_play_failed(const char *url)
{
    if (!voice_player_url_is_sd_file(url)) {
        return;
    }

    voice_msg_pub(VOICE_MSG_APP_SD_MUSIC_PLAY_FAILED, NULL, 0);
}

int voice_player_play_music_url(const char *url)
{
    int ret;

    if (!music_player || !url || url[0] == '\0') {
        LOGW("Music play rejected: player=%p url=%p", (void *)music_player, (const void *)url);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    ret = voice_player_check_sd_file_accessible(url);
    if (ret != APP_PLAYER_OK) {
        voice_player_notify_sd_play_failed(url);
        return ret;
    }

    ret = app_player_play(music_player, url);
    if (ret != APP_PLAYER_OK) {
        if (voice_player_url_is_sd_file(url)) {
            LOGW("Music SD play failed: url=%s ret=%d", url, ret);
        }
        voice_player_notify_sd_play_failed(url);
    }

    return ret;
}

/**
 * @brief   PA 控制回调函数
 * @param   onoff 1 开启 PA，0 关闭 PA
 * @return  0 成功，其他失败
 */
#ifndef CONFIG_BOARD_ARCS_MINI_V3
#define PA_PIN_NUM PA_EN_PIN
#define PA_GPIO_DEVICE CONFIG_PA_DEVICE_NAME
#endif

#ifdef CONFIG_BOARD_ARCS_MINI_V3
#ifndef PA_MUTE_ACTIVE_LEVEL
#define PA_MUTE_ACTIVE_LEVEL 1
#endif

static lisa_device_t *s_pa_mute_dev = NULL;
static bool s_pa_mute_configured = false;

static uint32_t pa_mute_level(bool mute)
{
    bool active_high = (PA_MUTE_ACTIVE_LEVEL != 0);

    if (mute) {
        return active_high ? LISA_GPIO_HIGH : LISA_GPIO_LOW;
    }

    return active_high ? LISA_GPIO_LOW : LISA_GPIO_HIGH;
}

static int pa_mute_hw_init(void)
{
    if (s_pa_mute_configured) {
        return 0;
    }

    s_pa_mute_dev = lisa_device_get(PA_MUTE_DEVICE_NAME);
    if (!lisa_device_ready(s_pa_mute_dev)) {
        LOGE("Error: %s device not ready", PA_MUTE_DEVICE_NAME);
        return -1;
    }

    uint32_t init_flag = pa_mute_level(true) ? LISA_GPIO_OUTPUT_INIT_HIGH : LISA_GPIO_OUTPUT_INIT_LOW;
    int ret = lisa_gpio_configure(s_pa_mute_dev, PA_MUTE_PIN, LISA_GPIO_OUTPUT | init_flag);
    if (ret != 0) {
        LOGE("PA mute GPIO configure failed: %d", ret);
        return -1;
    }

    s_pa_mute_configured = true;
    return 0;
}
#endif

static int pa_control_callback(int onoff)
{
#ifdef CONFIG_BOARD_ARCS_MINI_V3
    if (pa_mute_hw_init() != 0) {
        return -1;
    }

    LOGI("PA %s via exmcu mute", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(s_pa_mute_dev, PA_MUTE_PIN, pa_mute_level(onoff ? false : true));
#else
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM, onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
#endif
}

/**
 * @brief   播放器事件回调
 * @param   player    触发事件的播放器实例
 * @param   event     事件类型
 * @param   user_data 用户数据（播放器名称字符串）
 *
 * @note    音乐播放器在曲目播放完成后自动从 voice_music_list 获取下一首并播放，
 *          播放错误时也会尝试下一首。
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
        if (player == music_player) {
            music_item_t curr_track;
            if (voice_music_list_get_current(&curr_track) == 0 && curr_track.m_name[0]) {
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, curr_track.m_name, strlen(curr_track.m_name) + 1);
            }
            voice_msg_pub(VOICE_MSG_PLAYER_MUSIC_PLAYING, NULL, 0);
        }
        break;
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("[%s] Player paused", player_name);
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        LOGI("[%s] Player completed", player_name);

        /* 音乐播放完成后自动播放下一首 */
        if (player == music_player) {
            music_item_t next_track;
            if (voice_music_list_get_next(&next_track) == 0) {
                LOGI("Auto-advancing to next track");
                if (voice_player_play_music_url(next_track.m_url) == APP_PLAYER_OK &&
                    next_track.m_name[0]) {
                    voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, next_track.m_name, strlen(next_track.m_name) + 1);
                }
            } else {
                LOGI("Playlist completed");
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);
            }
        }
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("[%s] Player error", player_name);

        /* 播放错误时尝试播放下一首 */
        if (player == music_player) {
            music_item_t next_track;
            music_item_t curr_track;
            if (voice_music_list_get_current(&curr_track) == 0) {
                voice_player_notify_sd_play_failed(curr_track.m_url);
            }
            if (voice_music_list_get_next(&next_track) == 0) {
                LOGI("Error occurred, trying next track");
                if (voice_player_play_music_url(next_track.m_url) == APP_PLAYER_OK &&
                    next_track.m_name[0]) {
                    voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, next_track.m_name, strlen(next_track.m_name) + 1);
                }
            } else {
                voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);
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
 * @brief   焦点变化回调 - 通用版本
 * @param   player     触发回调的播放器实例
 * @param   state      焦点状态变化
 * @param   by_which   触发变化的对方播放器
 * @param   user_data  用户数据（播放器名称字符串）
 *
 * @note    此回调仅记录日志，不干预焦点策略。
 * @return  false 表示让 app_player 根据配置自动执行策略
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

/**
 * @brief   音量控制线程
 * @param   arg 未使用
 *
 * @note    等待信号量，收到信号后将 g_system_volume 应用到所有已创建的播放器。
 *          异步设计避免在 TTS 播报期间阻塞调用者或与播放器内部状态冲突。
 */
static void volume_thread_func(void *arg)
{
    (void)arg;

    while (1) {
        /* 等待音量变更信号（阻塞，不消耗 CPU） */
        if (xSemaphoreTake(g_volume_sem, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        int volume = g_system_volume;
        LOGI("Volume thread applying: %d", volume);

        if (tone_player != NULL) {
            app_player_set_volume(tone_player, volume);
        }
        if (tts_player != NULL) {
            app_player_set_volume(tts_player, volume);
        }
        if (music_player != NULL) {
            app_player_set_volume(music_player, volume);
        }
        if (alert_player != NULL) {
            app_player_set_volume(alert_player, volume);
        }
    }
}

/**
 * @brief   从 KV 存储恢复系统音量并触发首次应用
 *
 * @note    在 voice_player_platform_init 末尾调用，
 *          此时所有播放器和音量线程已就绪。
 */
static void voice_player_system_volume_init(void)
{
    int r;

    r = lisa_kv_get_int(KV_KEY_USER_VOLUME, &g_system_volume);
    if (r != 0) {
        LOGW("Failed to get system volume from kv, use default %d",
             CONFIG_VOICE_PLAYER_DEFAULT_VOLUME);
        g_system_volume = CONFIG_VOICE_PLAYER_DEFAULT_VOLUME;
    } else {
        if (g_system_volume > 100) {
            g_system_volume = 100;
        }
        if (g_system_volume < 0) {
            g_system_volume = 0;
        }
    }

    if (g_volume_sem != NULL) {
        xSemaphoreGive(g_volume_sem);
    }
}


/**
 * @brief   初始化语音播放器平台
 * @return  0 成功，-1 失败
 *
 * @note    创建 tone/tts/music/alert 四个播放器实例，配置音频焦点通道，
 *          注册事件回调，启动音量控制线程，并从 KV 恢复初始音量。
 */
int voice_player_platform_init(void)
{
    int ret = 0;

    LOGI("voice_player_platform_init...");

#ifndef CONFIG_BOARD_ARCS_MINI_V3
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
#else
    ret = pa_mute_hw_init();
    if (ret != 0) {
        return -1;
    }
#endif

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
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_PAUSE,
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

    /* 创建音量控制线程 */
    g_volume_sem = xSemaphoreCreateBinary();
    if (g_volume_sem == NULL) {
        LOGE("Failed to create volume semaphore");
        return -1;
    }
    xTaskCreate(volume_thread_func, "volctrl", 1024, NULL, 3, &g_volume_thread);
    if (g_volume_thread == NULL) {
        LOGE("Failed to create volume thread");
        vSemaphoreDelete(g_volume_sem);
        g_volume_sem = NULL;
        return -1;
    }

    /* 音量初始化 */
    voice_player_system_volume_init();
    
    /* 初始化统一音乐列表 */
    voice_music_list_init();

    return 0;
}

/**
 * @brief   判断指定播放器是否处于活跃音频播放管线中
 * @param   player 播放器实例
 * @return  true 活跃（准备中/已准备/播放中），false 非活跃或 player 为 NULL
 */
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

/**
 * @brief   判断音乐播放器是否处于活跃音频播放管线中
 * @return  true 活跃（准备中/已准备/播放中），false 非活跃
 */
bool voice_player_is_music_active(void)
{
    return voice_player_state_is_audio_active(music_player);
}

/**
 * @brief   判断任一播放器是否处于活跃音频播放管线中
 * @return  true 有任一播放器活跃，false 全部空闲
 */
bool voice_player_is_audio_active(void)
{
    return voice_player_state_is_audio_active(tone_player) ||
           voice_player_state_is_audio_active(tts_player) ||
           voice_player_state_is_audio_active(music_player) ||
           voice_player_state_is_audio_active(alert_player);
}



/**
 * @brief   设置系统音量（异步）
 * @param   volume 音量值（0-100）
 * @return  0 成功，-1 参数无效
 *
 * @note    异步接口：保存音量值并通知后台线程应用，立即返回。
 *          不持久化 KV——KV 管理由 service_volume 层负责。
 *          0-100 为硬件安全边界，业务策略（如 min_volume）由上层处理。
 */
int voice_player_set_system_volume(int volume)
{
    if (volume < 0 || volume > 100) {
        LOGE("Invalid volume: %d (valid range: 0-100)", volume);
        return -1;
    }

    LOGI("Setting system volume to %d", volume);
    g_system_volume = volume;

    if (g_volume_sem != NULL) {
        xSemaphoreGive(g_volume_sem);
    }

    return 0;
}

/**
 * @brief   获取当前系统音量
 * @return  音量值（0-100）
 */
int voice_player_get_system_volume(void)
{
    return g_system_volume;
}
