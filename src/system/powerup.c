#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

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

#ifdef CONFIG_BOARD_ARCS_MINI3
static void powerup_force_camera_pwdn_low(void)
{
    struct lisa_device *pwdn_dev = lisa_device_get(CAM_PWDN_DEVICE_NAME);
    if (!pwdn_dev || !lisa_device_ready(pwdn_dev)) {
        LISA_LOGW(TAG, "Camera PWDN device %s not ready", CAM_PWDN_DEVICE_NAME);
        return;
    }

    int ret = lisa_gpio_configure(pwdn_dev, CAM_PWDN_PIN, LISA_GPIO_CONFIG_OUTPUT_LOW);
    if (ret != 0) {
        LISA_LOGW(TAG, "Failed to force camera PWDN low: pin=%d ret=%d", CAM_PWDN_PIN, ret);
        return;
    }

    LISA_LOGI(TAG, "Camera PWDN forced low at startup");
}

static void powerup_init_charge_en(void)
{
    struct lisa_device *charge_en_dev = lisa_device_get(CHARGE_EN_DEVICE_NAME);
    if (!charge_en_dev || !lisa_device_ready(charge_en_dev)) {
        LISA_LOGW(TAG, "Charge EN device %s not ready", CHARGE_EN_DEVICE_NAME);
        return;
    }

    int ret = lisa_gpio_configure(charge_en_dev, CHARGE_EN_PIN, LISA_GPIO_INPUT);
    if (ret != 0) {
        LISA_LOGW(TAG, "Failed to init charge EN: pin=%d ret=%d", CHARGE_EN_PIN, ret);
        return;
    }

    LISA_LOGI(TAG, "Charge EN initialized as floating input");
}

static void powerup_log_4g_det_state(void)
{
    struct lisa_device *det_dev = lisa_device_get(CH_4G_DET_DEVICE_NAME);
    if (!det_dev || !lisa_device_ready(det_dev)) {
        LISA_LOGW(TAG, "4G DET device %s not ready", CH_4G_DET_DEVICE_NAME);
        return;
    }

    int ret = lisa_gpio_configure(det_dev, CH_4G_DET_PIN, LISA_GPIO_INPUT);
    if (ret != 0) {
        LISA_LOGW(TAG, "Failed to configure 4G DET: device=%s pin=%d ret=%d",
                  CH_4G_DET_DEVICE_NAME, CH_4G_DET_PIN, ret);
        return;
    }

    int level = lisa_gpio_read_pin(det_dev, CH_4G_DET_PIN);
    if (level < 0) {
        LISA_LOGW(TAG, "Failed to read 4G DET: device=%s pin=%d ret=%d",
                  CH_4G_DET_DEVICE_NAME, CH_4G_DET_PIN, level);
        return;
    }

    bool active = level == (CH_4G_DET_ACTIVE_LEVEL ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
    LISA_LOGI(TAG, "4G DET state: device=%s pin=%d level=%d active_level=%d active=%d",
              CH_4G_DET_DEVICE_NAME, CH_4G_DET_PIN, level, CH_4G_DET_ACTIVE_LEVEL, active);
}
#endif

static int power_up_guard(void)
{
    power_config_t power_cfg = {
        .on_shutdown = shutdown,
    };
    power_init(&power_cfg);

#ifdef CONFIG_BOARD_ARCS_MINI3
    powerup_force_camera_pwdn_low();
    powerup_init_charge_en();
    /* 配置 DET 为输入并打印一次上电状态 */
    powerup_log_4g_det_state();
#endif

    /* 新 boot 已在 stage0 做长按守护 + 驱动指示 LED，app 只在老 boot 下兜底 */
    if (!uboot_features_has(UBOOT_FEATURE_POWER_GUARD)) {
        if (!power_wait_settle()) {
            power_shutdown();
            return 0;
        }

        // 点亮LED，表示系统已上电
#ifndef CONFIG_BOARD_ARCS_MINI3
        struct lisa_device *led_dev = lisa_device_get("gpiob");
        lisa_gpio_configure(led_dev, LED_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
#endif
    }

    return 0;
}

SYS_INIT(power_up_guard, SYS_INIT_LEVEL_PRE_APPLICATION, 0);
