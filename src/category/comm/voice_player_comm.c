#include "voice_player_comm.h"
#include "app_player.h"

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

/**
 * @brief   PA 控制回调函数
 * @param   onoff 1 开启 PA，0 关闭 PA
 * @return  0 成功，其他失败
 */
#ifndef CONFIG_BOARD_ARCS_MINI3
#define PA_PIN_NUM PA_EN_PIN
#define PA_GPIO_DEVICE CONFIG_PA_DEVICE_NAME
#endif

#ifdef CONFIG_BOARD_ARCS_MINI3
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
#ifdef CONFIG_BOARD_ARCS_MINI3
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
 *          启动音量控制线程，并从 KV 恢复初始音量。
 *          各播放器的业务回调由对应 voice_player 子模块注册。
 */
int voice_player_platform_init(void)
{
    int ret = 0;

    LOGI("voice_player_platform_init...");

#ifndef CONFIG_BOARD_ARCS_MINI3
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
    /* PA mute 走 CH32 EXGPIO，外挂 MCU 未就绪时降级继续（pa_control_callback
     * 每次开关 PA 都会重试初始化），不能因此中断整个播放器平台初始化 */
    ret = pa_mute_hw_init();
    if (ret != 0) {
        LOGW("PA mute GPIO init failed, continue without PA mute control: %d", ret);
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
                .on_focus_lost = APP_PLAYER_FOCUS_LOSS_PAUSE,
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
