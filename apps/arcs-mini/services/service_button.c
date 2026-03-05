#include <stdint.h>
#include <stdbool.h>
#include "lisa_log.h"
#include "lisa_btn.h"
#include "voice_msg.h"
#include "service_button.h"
#include "Driver_GPADC.h"
#include "IOMuxManager.h"
#include "board.h"

#define TAG "service_button"

extern void factory_reset(void);

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

static void publish_button_event(uint8_t btn_id, lisa_btn_event_t action)
{
    voice_msg_button_evt_t evt = {
        .button_id = btn_id,
        .action = map_lisa_btn_event(action),
    };

    LISA_LOGI(TAG, "[Button %d] action=%d", btn_id, action);
    voice_msg_pub(VOICE_MSG_BUTTON_CHANGE, &evt, sizeof(evt));
}

static void evb_button_event_callback(lisa_btn_event_t event, uint8_t btn_id, void *user)
{
    switch (event) {
        case LISA_BTN_PRESS_DOWN:
            publish_button_event(btn_id, LISA_BTN_PRESS_DOWN);
            break;

        case LISA_BTN_PRESS_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_CLICK);
            break;

        case LISA_BTN_PRESS_DOUBLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_DOUBLE_CLICK);
            break;

        case LISA_BTN_PRESS_TRIPLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_TRIPLE_CLICK);
            break;

        case LISA_BTN_PRESS_QUADRUPLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_QUADRUPLE_CLICK);
            break;

        case LISA_BTN_PRESS_QUINTUPLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_QUINTUPLE_CLICK);
            break;

        case LISA_BTN_PRESS_SEXTUPLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_SEXTUPLE_CLICK);
            break;

        case LISA_BTN_PRESS_SEPTUPLE_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_SEPTUPLE_CLICK);
            break;

        case LISA_BTN_PRESS_REPEAT_CLICK:
            publish_button_event(btn_id, LISA_BTN_PRESS_REPEAT_CLICK);
            break;

        case LISA_BTN_PRESS_SHORT_START:
            publish_button_event(btn_id, LISA_BTN_PRESS_SHORT_START);
            break;

        case LISA_BTN_PRESS_SHORT_UP:
            publish_button_event(btn_id, LISA_BTN_PRESS_SHORT_UP);
            break;

        case LISA_BTN_PRESS_LONG_START:
            publish_button_event(btn_id, LISA_BTN_PRESS_LONG_START);
            break;

        case LISA_BTN_PRESS_LONG_UP:
            publish_button_event(btn_id, LISA_BTN_PRESS_LONG_UP);
            break;

        case LISA_BTN_PRESS_LONG_HOLD:
            publish_button_event(btn_id, LISA_BTN_PRESS_LONG_HOLD);
            break;

        case LISA_BTN_PRESS_LONG_HOLD_UP:
            publish_button_event(btn_id, LISA_BTN_PRESS_LONG_HOLD_UP);
            break;

        default:
            break;
    }
}

int service_button_init(void)
{
    static const lisa_btn_gpio_item_t mini_power_key_gpio = {
        .pin_num = POWER_KEY_PIN,
        .iomux_func = CSK_AON_IOMUX_FUNC_DEFAULT,
        .active_level = 0,
        .pull_enable = false,
    };

    static const lisa_btn_gpio_config_t mini_power_key_cfg = {
        .gpio_dev_name = "gpiob",
        .button_count = 1,
        .buttons = &mini_power_key_gpio,
        .time_config =
            {
                .short_press_time = 1500,
                .long_press_time = 3000,
                .long_hold_time = 3000,
                .scan_period = 20,
            },
        .callback = evb_button_event_callback,
        .user_data = NULL,
    };

    int ret = lisa_btn_gpio_init(&mini_power_key_cfg);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to init GPIO button: %d", ret);
        return ret;
    }

    return 0;
}
