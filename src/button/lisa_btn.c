#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "lisa_log.h"
#include "lisa_thread.h"
#include "shell.h"

#include "battery.h"
#include "flexible_button.h"
#include "lisa_btn.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#define TAG "btn"

#define ENUM_TO_STR(e) (#e)

/* 按键引脚配置宏定义 */
#define BTN_GPIO_PORT_B     GPIOB()
#define POWER_BTN_PAD       CSK_IOMUX_PAD_B
#define POWER_BTN_PIN       4   // GPIOB_4 - Power按键

struct lisa_btn_ctx {
    void *user;
    lisa_btn_cb_t cb;
    bool is_init;
    flex_button_t btns[LISA_BTN_ID_MAX];
};

static struct lisa_btn_ctx btn_ctx = {0};

static uint8_t button_lvl_read(lisa_btn_id_t btn_id)
{
    /* 只支持Power按键 */
    if (btn_id != LISA_BTN_ID_POWER) {
        return true;  /* 其他按键返回未按下状态 */
    }
    
    uint32_t pin_mask = (1UL << POWER_BTN_PIN);
    uint32_t gpio_level = GPIO_PinRead(BTN_GPIO_PORT_B, pin_mask);
    
    return gpio_level;
}

static uint8_t common_btn_read(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;

    return button_lvl_read(btn->id);
}

const char *lisa_btn_evt_desc_get(lisa_btn_event_t evt)
{
    const char *evt_descs[] = {
        "down",       "click",   "double_click", "repeat_click", "short_start", "short_up",
        "long_start", "long_up", "long_hold",    "long_hold_up", "max",         "none",
    };

    if (evt >= LISA_BTN_PRESS_NONE) {
        return "unknown";
    }

    return evt_descs[evt];
}

static void common_btn_evt_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    LISA_LOGI(TAG, "btn id:%d, event:%s", btn->id, lisa_btn_evt_desc_get(btn->event));

    if (btn_ctx.cb) {
        btn_ctx.cb((lisa_btn_event_t)(btn->event), (void*)(intptr_t)btn->id);
    }
}

static void lisa_btn_task(void *arg)
{
    LISA_LOGI(TAG, "lisa_btn_task enter");
    lisa_thread_mdelay(2000);

    while (1) {
        flex_button_scan();
        lisa_thread_mdelay(20);
    }
}

void lisa_btn_init(lisa_btn_cb_t cb, void *user)
{
    int i;

    if (btn_ctx.is_init) {
        return;
    }

    /* 只初始化Power按键 */
    memset(&btn_ctx.btns[0], 0x0, sizeof(btn_ctx.btns));
    
    /* 只注册Power按键 */
    i = LISA_BTN_ID_POWER;  /* 现在POWER是第0个按键 */
    btn_ctx.btns[i].id = i;
    btn_ctx.btns[i].usr_button_read = common_btn_read;
    btn_ctx.btns[i].cb = common_btn_evt_cb;
    btn_ctx.btns[i].pressed_logic_level = 0;  /* Power按键按下为低电平 */
    btn_ctx.btns[i].short_press_start_tick = FLEX_MS_TO_SCAN_CNT(1500);
    btn_ctx.btns[i].long_press_start_tick = FLEX_MS_TO_SCAN_CNT(2300);
    btn_ctx.btns[i].long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(5000);

    LISA_LOGI(TAG, "Power btn[%d] init: pressed_logic_level=%d", i, btn_ctx.btns[i].pressed_logic_level);
    flex_button_register(&btn_ctx.btns[i]);

    btn_ctx.user = user;
    btn_ctx.cb = cb;

    /* Note: GPIOB should already be initialized by LED module or power manager */
    LISA_LOGI(TAG, "Configuring power button pin (GPIOB already initialized)");
    // GPIO_Initialize(BTN_GPIO_PORT_B, NULL, NULL);  // Removed to avoid resetting other GPIO configs
    
    /* 只配置Power按键引脚为输入模式，启用上拉电阻 */
    IOMuxManager_PinConfigure(POWER_BTN_PAD, POWER_BTN_PIN, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(BTN_GPIO_PORT_B, (1UL << POWER_BTN_PIN), CSK_GPIO_DIR_INPUT);

    lisa_thread_attr_t attr = {
        .name = "btn",
        .stack_size = 4096,
        .priority = 6,
    };
    lisa_thread_t *td = lisa_thread_create(&attr, lisa_btn_task, NULL);
    LISA_ASSERT(td, "lisa btn task create failed");

    btn_ctx.is_init = true;

    LISA_LOGI(TAG, "lisa btn init done");
}
