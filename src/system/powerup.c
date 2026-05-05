#include "FreeRTOS.h"
#include "task.h"

#include "sys_init.h"
#include "board.h"
#include "lisa_gpio.h"
#include "lisa_display.h"
#include "power/power_manager.h"
#include "uboot_features_api.h"

#define TAG "powerup"
#include "lisa_log.h"

static void shutdown(void)
{
    LISA_LOGI(TAG, "System shutting down...");

    pa_manager_control(0, 0);

    lisa_device_t *disp = lisa_device_get("display");
    lisa_display_blanking_on(disp);
    lisa_display_set_brightness(disp, 0);

    vTaskDelay(pdMS_TO_TICKS(500));
}

static int power_up_guard(void)
{
    power_config_t power_cfg = {
        .on_shutdown = shutdown,
    };
    power_init(&power_cfg);

    /* 新 boot 已在 stage0 做长按守护 + 驱动指示 LED，app 只在老 boot 下兜底 */
    if (!uboot_features_has(UBOOT_FEATURE_POWER_GUARD)) {
        if (!power_wait_settle()) {
            power_shutdown();
            return 0;
        }

        // 点亮LED，表示系统已上电
        struct lisa_device *led_dev = lisa_device_get("gpiob");
        lisa_gpio_configure(led_dev, LED_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    }

    return 0;
}

SYS_INIT(power_up_guard, SYS_INIT_LEVEL_PRE_APPLICATION, 0);
