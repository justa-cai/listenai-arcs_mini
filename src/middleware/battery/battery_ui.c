/*
 * @file battery_ui.c
 * @brief Battery UI layer - smoothing, debounce, and UI event publishing
 *
 * 职责：
 *   - 每秒从 battery.c 读取原始电量和状态
 *   - 对电量进行防抖/平滑处理（小抖动去抖、大跳变确认、UI 步进限速）
 *   - 处理插拔充电线时的 UI 冻结逻辑
 *   - 发布 VOICE_MSG_POWER_BATTERY_UPDATE 消息给 UI
 *
 * @version 0.1
 * @date 2025-04-16
 *
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "arcs_ap.h"
#include "lisa_log.h"
#include "lisa_timer.h"
#include "voice_msg.h"
#include "sysutils.h"

#include "battery.h"
#include "battery_ui.h"
#include "lisa_kv.h"

#define TAG "battery_ui"

#define VBAT_SAMPLE_PRIOD (1000)

/* 插拔充电线后，冻结百分比显示的时长（单位：采样次数，1次=1s） */
#define CHARGE_STATE_CHANGE_HOLD_TICKS (5)

/* 百分比去抖：小幅(1%)跳变需连续 N 次一致才更新，避免 UI 在阈值附近抖动 */
#define PERCENTAGE_DEBOUNCE_TICKS (3)

/* 大跳变确认：连续 N 秒都落在同一区间，才认为目标电量有效（用于换电池/瞬时突变） */
#define PERCENTAGE_JUMP_CONFIRM_TICKS   (10)
#define PERCENTAGE_JUMP_THRESHOLD       (8)
#define PERCENTAGE_JUMP_RANGE_TOLERANCE (2)

/* UI 平滑步进：每秒最多变化 1%（充电/放电统一） */
#define PERCENTAGE_MAX_STEP_PER_TICK (1)

/* 显示电量持久化：图标换挡时（每 10% 一档）写 KV，重启后恢复上次显示值 */
#define BATTERY_PCT_KV_KEY "bat_ui_pct"

static lisa_timer_t *battery_ui_timer __psram_bss__ = NULL;

/* 滤波器核心状态提升到模块级，以便 battery_ui_init() 从 KV 注入初值 */
static uint8_t s_output_pct __psram_data__ = 0xFF;
static uint8_t s_target_pct __psram_data__ = 0xFF;
static voice_msg_battery_info_t s_last_battery_info __psram_bss__ = {
    .level = 0,
    .status = VOICE_MSG_BATTERY_STATUS_UNKNOWN,
};
static bool s_last_battery_info_valid __psram_bss__ = false;

/**
 * @brief 百分比去抖/平滑滤波
 *
 * 策略：
 * 1) 小抖动（1%）需连续 N 次一致才更新目标值
 * 2) 大跳变（>=8%）需连续 10s 落在同一区间才切换目标值（换电池/瞬时突变保护）
 * 3) UI 每秒最多变化 1%，保证充放电都"慢慢变化"
 */
static uint8_t percentage_debounce_filter(uint8_t percentage)
{
    /* output/target 已提升为模块级变量 s_output_pct / s_target_pct */
    static uint8_t candidate_percentage __psram_data__ = 0xFF;
    static uint8_t candidate_count      __psram_bss__  = 0;

    static uint8_t jump_candidate_center __psram_data__ = 0xFF;
    static uint8_t jump_candidate_count  __psram_bss__  = 0;

    if (s_output_pct == 0xFF) {
        s_output_pct = percentage;
        s_target_pct = percentage;
        return s_output_pct;
    }

    int diff_to_target = abs((int)percentage - (int)s_target_pct);

    /* 大跳变场景：要求连续 10s 在同一区间内，才切换目标值 */
    if (diff_to_target >= PERCENTAGE_JUMP_THRESHOLD) {
        if (jump_candidate_center == 0xFF ||
            abs((int)percentage - (int)jump_candidate_center) > PERCENTAGE_JUMP_RANGE_TOLERANCE) {
            jump_candidate_center = percentage;
            jump_candidate_count  = 1;
        } else if (jump_candidate_count < PERCENTAGE_JUMP_CONFIRM_TICKS) {
            jump_candidate_count++;
        }

        if (jump_candidate_count >= PERCENTAGE_JUMP_CONFIRM_TICKS) {
            s_target_pct          = jump_candidate_center;
            jump_candidate_center = 0xFF;
            jump_candidate_count  = 0;
        }

    } else {
        /* 退出大跳变确认态 */
        jump_candidate_center = 0xFF;
        jump_candidate_count  = 0;

        /* 常规更新：对 1% 抖动做去抖，其它变化直接作为目标 */
        if (percentage == s_target_pct) {
            candidate_percentage = 0xFF;
            candidate_count      = 0;
        } else if (abs((int)percentage - (int)s_target_pct) == 1) {
            if (percentage != candidate_percentage) {
                candidate_percentage = percentage;
                candidate_count      = 1;
            } else if (candidate_count < PERCENTAGE_DEBOUNCE_TICKS) {
                candidate_count++;
            }

            if (candidate_count >= PERCENTAGE_DEBOUNCE_TICKS) {
                s_target_pct         = candidate_percentage;
                candidate_percentage = 0xFF;
                candidate_count      = 0;
            }
        } else {
            s_target_pct         = percentage;
            candidate_percentage = 0xFF;
            candidate_count      = 0;
        }
    }

    /* UI 平滑步进（充电/放电统一） */
    if (s_output_pct < s_target_pct) {
        uint8_t rise = s_target_pct - s_output_pct;
        s_output_pct += (rise > PERCENTAGE_MAX_STEP_PER_TICK) ? PERCENTAGE_MAX_STEP_PER_TICK : rise;
    } else if (s_output_pct > s_target_pct) {
        uint8_t fall = s_output_pct - s_target_pct;
        s_output_pct -= (fall > PERCENTAGE_MAX_STEP_PER_TICK) ? PERCENTAGE_MAX_STEP_PER_TICK : fall;
    }

    return s_output_pct;
}

static voice_msg_battery_status_t battery_status_to_msg(battery_status_t status)
{
    switch (status) {
    case BATTERY_STATUS_NO_BATTERY:
        return VOICE_MSG_BATTERY_STATUS_NO_BATTERY;
    case BATTERY_STATUS_NOT_CONNECT:
        return VOICE_MSG_BATTERY_STATUS_NOT_CONNECT;
    case BATTERY_STATUS_CHARGING:
        return VOICE_MSG_BATTERY_STATUS_CHARGING;
    case BATTERY_STATUS_CHARGE_DONE:
        return VOICE_MSG_BATTERY_STATUS_CHARGE_DONE;
    case BATTERY_STATUS_UNKNOWN:
    default:
        return VOICE_MSG_BATTERY_STATUS_UNKNOWN;
    }
}

bool battery_ui_get_info(voice_msg_battery_info_t *info)
{
    if (!info) {
        return false;
    }

    if (s_last_battery_info_valid) {
        *info = s_last_battery_info;
        return true;
    }

    battery_status_t status = battery_get_status();
    bool plug_in_pending = battery_usb_plugged_stable_get() && status == BATTERY_STATUS_NOT_CONNECT;

    info->level = (s_output_pct != 0xFF) ? s_output_pct : battery_get_pct_raw();
    info->status = battery_status_to_msg(plug_in_pending ? BATTERY_STATUS_CHARGING : status);

    return true;
}

static void battery_voltage_sample_cb(struct lisa_timer *timer)
{
    uint8_t sampled_percentage = battery_get_pct_raw();
    battery_status_t status    = battery_get_status();
#ifdef CONFIG_LISA_CH32V003_ADC
    (void)battery_get_temp_adc_raw();
#endif
    bool is_charging           = (status == BATTERY_STATUS_CHARGING || status == BATTERY_STATUS_CHARGE_DONE);
    uint8_t raw_percentage     = percentage_debounce_filter(sampled_percentage);

    static uint8_t last_percentage     __psram_data__ = 0xFF;
    static uint8_t last_logic_status   __psram_data__ = 0xFF;
    static uint8_t last_publish_status __psram_data__ = 0xFF;

    bool usb_plugged_stable = battery_usb_plugged_stable_get();

    /* 插拔检测：仅关注「充电中/充满」与「未充电」之间的切换 */
    static uint8_t frozen_percentage __psram_bss__ = 0;
    static uint8_t charge_hold_ticks __psram_bss__ = 0;

    bool was_charging    = (last_logic_status == BATTERY_STATUS_CHARGING ||
                            last_logic_status == BATTERY_STATUS_CHARGE_DONE);
    bool plug_in_pending = (usb_plugged_stable && status == BATTERY_STATUS_NOT_CONNECT);

    if (last_logic_status != 0xFF && is_charging != was_charging) {
        /* 充放电状态切换：
         * 1) 进入充电：图标与电量同一时刻切换（不再额外保持）
         * 2) 退出充电：保持 5s，避免拔线瞬间抖动 */
        frozen_percentage = (last_percentage != 0xFF) ? last_percentage : raw_percentage;
        charge_hold_ticks = is_charging ? 0 : CHARGE_STATE_CHANGE_HOLD_TICKS;
    }

    uint8_t display_percentage;
    if (plug_in_pending && last_percentage != 0xFF) {
        /* USB 已插入但状态尚未确认到 CHARGING，先保持旧电量，避免图标切换前先跳电量 */
        display_percentage = last_percentage;
    } else if (charge_hold_ticks > 0) {
        charge_hold_ticks--;
        display_percentage = frozen_percentage;
    } else {
        display_percentage = raw_percentage;
    }

    battery_status_t display_status = plug_in_pending ? BATTERY_STATUS_CHARGING : status;

    voice_msg_battery_info_t msg = {
        .level  = display_percentage,
        .status = battery_status_to_msg(display_status),
    };

    s_last_battery_info = msg;
    s_last_battery_info_valid = true;

    if (display_percentage != last_percentage || display_status != last_publish_status) {
        voice_msg_pub(VOICE_MSG_POWER_BATTERY_UPDATE, &msg, sizeof(msg));
        last_percentage      = display_percentage;
        last_publish_status  = display_status;
    }

    /* 仅当图标换挡时（每 10% 一档）才写 KV，整个充放电周期最多写 9 次 */
    static uint8_t s_saved_icon_idx __psram_data__ = 0xFF;
    if (last_percentage != 0xFF) {
        uint8_t icon_idx = last_percentage / 10;
        if (icon_idx != s_saved_icon_idx) {
            lisa_kv_set_int(BATTERY_PCT_KV_KEY, last_percentage);
            s_saved_icon_idx = icon_idx;
        }
    }

    last_logic_status = status;

    LISA_LOGD(TAG, "Battery: sampled=%d%%, filtered=%d%%, display=%d%%, status=%d, ui_status=%d, hold=%d",
              sampled_percentage, raw_percentage, display_percentage, status, display_status, charge_hold_ticks);

    lisa_timer_start(battery_ui_timer);
}

void battery_ui_init(void)
{
    static bool init_flag __psram_bss__ = false;

    if (init_flag) {
        return;
    }

    /* 从 KV 恢复上次的显示电量，作为滤波器初始输出值和目标值，
     * 重启后 UI 从上次位置继续平滑步进，而不是直接跳变到原始测量值 */
    int saved_pct;
    if (lisa_kv_get_int(BATTERY_PCT_KV_KEY, &saved_pct) == 0 &&
        saved_pct >= 0 && saved_pct <= 100) {
        s_output_pct = (uint8_t)saved_pct;
        s_target_pct = (uint8_t)saved_pct;
        LISA_LOGI(TAG, "Restored display pct from KV: %d%%", saved_pct);
    }

    battery_ui_timer = lisa_timer_create(VBAT_SAMPLE_PRIOD, battery_voltage_sample_cb, NULL);
    if (battery_ui_timer) {
        lisa_timer_start(battery_ui_timer);
    } else {
        LISA_LOGE(TAG, "Failed to create battery UI timer");
    }

    init_flag = true;
}
