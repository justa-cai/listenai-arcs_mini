#pragma once

#include <stdbool.h>
#include <stdint.h>

struct wakeup_algo_res {
    uint8_t *addr;
    uint32_t size;
};

struct wakeup_algo_resources {
    struct wakeup_algo_res mlp;
    struct wakeup_algo_res wrap;
};

typedef enum{
    APP_WAKEUP_SENSITIVITY_LEVEL_0 = 0, /*极易唤醒*/
    APP_WAKEUP_SENSITIVITY_LEVEL_1,     /*易唤醒*/
    APP_WAKEUP_SENSITIVITY_LEVEL_2,      /*默认档位*/
    APP_WAKEUP_SENSITIVITY_LEVEL_3,      /*难唤醒*/
    APP_WAKEUP_SENSITIVITY_LEVEL_4,      /*极难唤醒*/
}app_wakeup_sensitivity_level_e;

int app_wakeup_init(struct wakeup_algo_resources *res);

#ifdef CONFIG_BOARD_ARCS_MINI
typedef struct {
    uint32_t wake_count;
    uint32_t record_events_since_wake;
    uint32_t input_submit_failures_since_wake;
    uint32_t output_buffers_since_wake;
    uint32_t output_bytes_since_wake;
    uint32_t cloud_frames_since_wake;
    uint32_t cloud_non_silent_frames_since_wake;
    uint32_t cloud_send_failures_since_wake;
    uint32_t last_cloud_peak;
    uint32_t suspend_count;
    uint32_t resume_count;
    uint32_t resume_failures;
    bool adc_low_power;
    bool reference_channel_off;
    bool playback_off;
} app_wakeup_audio_diag_t;

int app_wakeup_stop(void);

/**
 * @brief Keep recording active while lowering ADC bias and powering off idle playback.
 */
int app_wakeup_audio_standby_suspend(void);

/**
 * @brief Restore full ADC bias and rebuild the playback path before audio output.
 */
int app_wakeup_audio_standby_resume(void);

/** Snapshot counters retained across a battery-only hibernate/wake cycle. */
void app_wakeup_audio_diag_get(app_wakeup_audio_diag_t *diag);
#endif

/**
 * @brief 设置唤醒灵敏度级别
 * @param level 灵敏度级别: LEVEL_0(低), LEVEL_1(中), LEVEL_2(高)
 * @return 0: 成功, 其他值: 失败
 */
int app_wakeup_sensitivity_level_set(app_wakeup_sensitivity_level_e level);

/**
 * @brief 获取当前唤醒灵敏度级别
 * @return 当前灵敏度级别
 */
app_wakeup_sensitivity_level_e app_wakeup_sensitivity_level_get(void);
