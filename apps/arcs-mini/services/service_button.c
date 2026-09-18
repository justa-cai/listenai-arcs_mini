#include "stdint.h"
#include "stdbool.h"

#define TAG "service_button"

#include "FreeRTOS.h"
#include "timers.h"

#include "lisa_log.h"
#include "lisa_btn.h"

#include "Driver_GPADC.h"
#include "IOMuxManager.h"
#include "board.h"
#ifdef CONFIG_OTA
#include "ota_manager.h"
#endif

#include "voice_msg.h"
#include "service_button.h"
#include "miniapp.h"
#include "alarm_ring.h"
#include "service_sd_music.h"

/* ---- 配置 --------------------------------------------------------------- */

#define MINI_POWER_KEY_SHORT_PRESS_MS 800U
#define MINI_POWER_KEY_LONG_PRESS_MS  1600U
#define MINI_POWER_KEY_LONG_HOLD_MS   1600U
#define MINI_POWER_KEY_ID             0U

#ifdef CONFIG_MINIAPP
static TimerHandle_t s_miniapp_hold_timer;
/* Written by the button task and read by the hold timer. Ownership lasts
 * until release, even if the miniapp exits while the key is still held. */
static volatile bool s_miniapp_press_active;
static void miniapp_hold_timeout(TimerHandle_t timer)
{
    (void)timer;
    if (s_miniapp_press_active && miniapp_is_active() && !alarm_ring_is_active()) {
        (void)miniapp_exit();
    }
}
#endif

/* ---- 内部工具函数 ------------------------------------------------------- */

/* 电源键只向应用发布最终点击次数和长按关机事件。 */
static bool service_button_power_key_accepts_action(voice_msg_button_action_t action)
{
    switch (action) {
    case VOICE_MSG_BUTTON_ACTION_CLICK:
    case VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK:
    case VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK:
    case VOICE_MSG_BUTTON_ACTION_LONG_HOLD:
        return true;
    default:
        return false;
    }
}

static voice_msg_button_action_t map_lisa_btn_event(lisa_btn_event_t lisa_event)
{
    switch (lisa_event) {
    case LISA_BTN_PRESS_DOWN:
        return VOICE_MSG_BUTTON_ACTION_PRESS_DOWN;
    case LISA_BTN_PRESS_CLICK:
        return VOICE_MSG_BUTTON_ACTION_CLICK;
    case LISA_BTN_PRESS_DOUBLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_DOUBLE_CLICK;
    case LISA_BTN_PRESS_TRIPLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_TRIPLE_CLICK;
    case LISA_BTN_PRESS_QUADRUPLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_QUADRUPLE_CLICK;
    case LISA_BTN_PRESS_QUINTUPLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_QUINTUPLE_CLICK;
    case LISA_BTN_PRESS_SEXTUPLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_SEXTUPLE_CLICK;
    case LISA_BTN_PRESS_SEPTUPLE_CLICK:
        return VOICE_MSG_BUTTON_ACTION_SEPTUPLE_CLICK;
    case LISA_BTN_PRESS_REPEAT_CLICK:
        return VOICE_MSG_BUTTON_ACTION_REPEAT_CLICK;
    case LISA_BTN_PRESS_SHORT_START:
        return VOICE_MSG_BUTTON_ACTION_SHORT_START;
    case LISA_BTN_PRESS_SHORT_UP:
        return VOICE_MSG_BUTTON_ACTION_SHORT_UP;
    case LISA_BTN_PRESS_LONG_START:
        return VOICE_MSG_BUTTON_ACTION_LONG_START;
    case LISA_BTN_PRESS_LONG_UP:
        return VOICE_MSG_BUTTON_ACTION_LONG_UP;
    case LISA_BTN_PRESS_LONG_HOLD:
        return VOICE_MSG_BUTTON_ACTION_LONG_HOLD;
    case LISA_BTN_PRESS_LONG_HOLD_UP:
        return VOICE_MSG_BUTTON_ACTION_LONG_HOLD_UP;
    default:
        return VOICE_MSG_BUTTON_ACTION_UNKNOWN;
    }
}

/*
 * 将应用关心的最终按键动作发布到消息总线。
 *
 * 常规模式下，电源键只发布最终点击次数和长按关机事件。
 * 小应用模式下，每次短按释放立即转换为 CLICK，忽略硬件汇总的连击事件。
 * 其他按键 ID 保留完整事件，方便未来扩展按住说话等功能。
 * OTA 升级期间忽略所有按键事件，避免升级过程中意外触发语音交互。
 * OTA 包信息确认阶段仅放行 CLICK 和 LONG_HOLD 事件，用于确认或跳过升级。
 */
static void publish_button_event(uint8_t btn_id, lisa_btn_event_t action, bool from_gpio)
{
#ifndef CONFIG_MINIAPP
    (void)from_gpio;
#endif
#ifdef CONFIG_MINIAPP
    if (btn_id == MINI_POWER_KEY_ID && s_miniapp_hold_timer) {
        bool was_miniapp_press = from_gpio && s_miniapp_press_active;
        if (from_gpio && action == LISA_BTN_PRESS_DOWN) {
            s_miniapp_press_active = miniapp_is_active() && !alarm_ring_is_active();
        }
        if (action == LISA_BTN_PRESS_SHORT_UP || action == LISA_BTN_PRESS_LONG_UP ||
            action == LISA_BTN_PRESS_LONG_HOLD_UP) {
            (void)xTimerStop(s_miniapp_hold_timer, 0);
            if (from_gpio) {
                s_miniapp_press_active = false;
            }
        }
        if (miniapp_is_active() && !alarm_ring_is_active()) {
            if (action == LISA_BTN_PRESS_DOWN) {
                (void)xTimerReset(s_miniapp_hold_timer, 0);
            }
            if (action == LISA_BTN_PRESS_SHORT_UP) {
                action = LISA_BTN_PRESS_CLICK;
            } else if (from_gpio || action != LISA_BTN_PRESS_CLICK) {
                /* GPIO 的 CLICK 是延迟汇总事件；鼠标的 CLICK 已是独立点击。 */
                return;
            }
        } else if (was_miniapp_press) {
            /* Do not reinterpret the rest of this press as a desktop action. */
            return;
        }
    }
#endif
    voice_msg_button_action_t mapped_action = map_lisa_btn_event(action);

    if (mapped_action == VOICE_MSG_BUTTON_ACTION_UNKNOWN) {
        return;
    }

    if (btn_id == MINI_POWER_KEY_ID &&
        !service_button_power_key_accepts_action(mapped_action)) {
        return;
    }

#ifdef CONFIG_OTA
    ota_state_e ota_state = ota_manager_get_state();
    if (ota_state == OTA_STATE_CHECKING || ota_state == OTA_STATE_UPDATING) {
        LISA_LOGI(TAG, "Ignore button %d action=%d during OTA state=%d",
                  btn_id, action, ota_state);
        return;
    }

    if (ota_state == OTA_STATE_PACKAGE_INFO) {
        if (action != LISA_BTN_PRESS_CLICK &&
            action != LISA_BTN_PRESS_LONG_HOLD) {
            LISA_LOGI(TAG, "Filter button %d action=%d during OTA package info, only CLICK/LONG_HOLD allowed",
                      btn_id, action);
            return;
        }
    }
#endif

    if (service_sd_music_is_syncing()) {
        LISA_LOGI(TAG, "Ignore button %d action=%d during SD music sync",
                  btn_id, action);
        return;
    }

    service_image_waiting_cancel();

    voice_msg_button_evt_t evt = {
        .button_id = btn_id,
        .action = mapped_action,
    };
    LISA_LOGI(TAG, "[Button %d] action=%d", btn_id, action);
    voice_msg_pub(VOICE_MSG_BUTTON_CHANGE, &evt, sizeof(evt));
}

/*
 * lisa_btn 硬件层回调，将事件交给 publish_button_event 统一筛选和发布。
 *
 * 不再展开 switch-case —— lisa_btn_event_t 到业务事件的映射已由
 * map_lisa_btn_event() 集中完成，回调只负责过滤和转发。
 */
static void btn_event_callback(lisa_btn_event_t event, uint8_t btn_id, void *user)
{
    (void)user;
    publish_button_event(btn_id, event, true);
}

static void service_button_publish_mouse_click(void *arg, uint32_t value)
{
    (void)arg;
    (void)value;
    publish_button_event(MINI_POWER_KEY_ID, LISA_BTN_PRESS_CLICK, false);
}

void app_usb_hid_mouse_left_click_from_isr(void)
{
    BaseType_t yield = pdFALSE;

    if (xTimerPendFunctionCallFromISR(service_button_publish_mouse_click,
                                      NULL, 0U, &yield) != pdPASS) {
        LISA_LOGW(TAG, "Failed to defer USB mouse click");
        return;
    }
    portYIELD_FROM_ISR(yield);
}

/* ---- 公开 API ----------------------------------------------------------- */

int service_button_init(void)
{
#ifdef CONFIG_MINIAPP
    s_miniapp_hold_timer = xTimerCreate("miniapp.hold", pdMS_TO_TICKS(3000),
                                      pdFALSE, NULL, miniapp_hold_timeout);
    if (!s_miniapp_hold_timer) {
        return -1;
    }
#endif
    static const lisa_btn_gpio_item_t mini_power_key_gpio = {
        .pin_num      = POWER_KEY_PIN,
        .iomux_func   = CSK_AON_IOMUX_FUNC_DEFAULT,
        .active_level = 0,
        .pull_enable  = false,
    };

    static const lisa_btn_gpio_config_t mini_power_key_cfg = {
        .gpio_dev_name = "gpiob",
        .button_count  = 1,
        .buttons       = &mini_power_key_gpio,
        .time_config   = {
            .short_press_time = MINI_POWER_KEY_SHORT_PRESS_MS,
            .long_press_time  = MINI_POWER_KEY_LONG_PRESS_MS,
            .long_hold_time   = MINI_POWER_KEY_LONG_HOLD_MS,
            .scan_period      = 20,
        },
        .callback   = btn_event_callback,
        .user_data  = NULL,
    };

    int ret = lisa_btn_gpio_init(&mini_power_key_cfg);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to init GPIO button: %d", ret);
        return ret;
    }

    return 0;
}
