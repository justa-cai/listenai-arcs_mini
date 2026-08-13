#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TAG "brightness"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "lisa_display.h"

#include "kv.h"
#include "IOMuxManager.h"
#include "Driver_I2C.h"
#include "board.h"

#include "service_brightness.h"

/* ---- 配置 --------------------------------------------------------------- */

#define DEFAULT_BRIGHTNESS 70

/* ---- 前向声明 ----------------------------------------------------------- */

static int service_brightness_clamp(int brightness);
static int service_brightness_to_hw(int brightness);
static void service_brightness_apply(int brightness);
static int service_brightness_build_display_config(lisa_display_config_t *cfg,
                                                    lisa_device_t *spi_dev,
                                                    lisa_device_t *gpioa_dev,
                                                    lisa_device_t *gpiob_dev);

/* ---- 模块级状态 --------------------------------------------------------- */

static lisa_device_t *s_display_device = NULL;

/* ---- 内部工具函数 ------------------------------------------------------- */

static int service_brightness_clamp(int brightness)
{
    if (brightness < 0) {
        return 0;
    }
    if (brightness > 100) {
        return 100;
    }
    return brightness;
}

/*
 * 将用户亮度（0–100）映射为硬件亮度。
 *
 * 人眼对亮度的感知是非线性的（近似对数 / 幂律关系），直接使用线性值会
 * 导致低亮度区间调节不敏感。因此对中间值采用二次曲线映射以补偿感知
 * 非线性；端点 0 和 100 保持不变，保证"全暗"和"全亮"行为与用户预期一致。
 */
static int service_brightness_to_hw(int brightness)
{
    brightness = service_brightness_clamp(brightness);
    if (brightness == 0 || brightness == 100) {
        return brightness;
    }

    /* brightness² / 100，+99 向上取整 */
    return (brightness * brightness + 99) / 100;
}

static void service_brightness_apply(int brightness)
{
    brightness = service_brightness_clamp(brightness);

    if (!s_display_device) {
        LOGW("Display device not available");
        return;
    }

    int hw_brightness = service_brightness_to_hw(brightness);
    if (lisa_display_set_brightness(s_display_device, hw_brightness) != 0) {
        LOGE("Failed to set brightness to %d (hw: %d)", brightness, hw_brightness);
    } else {
        LOGI("Brightness set to %d (hw: %d)", brightness, hw_brightness);
    }
}

/*
 * 构建 lisa_display_attach_bus() 所需的配置。
 *
 * gpioa_dev / gpiob_dev 必须已经通过 lisa_device_get()+ready 校验，
 * 否则 config 中的 GPIO 字段将携带无效指针，导致后续硬件访问崩溃。
 */
static int service_brightness_build_display_config(lisa_display_config_t *cfg,
                                                    lisa_device_t *spi_dev,
                                                    lisa_device_t *gpioa_dev,
                                                    lisa_device_t *gpiob_dev)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->bus_type = LISA_DISPLAY_BUS_SPI_4WIRE;

    cfg->bus_config.spi_4wire.spi_dev  = spi_dev;
    cfg->bus_config.spi_4wire.spi_freq = 50 * 1000 * 1000;
#ifndef CONFIG_LISA_DISPLAY_COMPOSITE
    cfg->bus_config.spi_4wire.cs_gpio  = gpioa_dev;
    cfg->bus_config.spi_4wire.cs_pin   = LCD_CS_PIN;
#endif
    cfg->bus_config.spi_4wire.dc_gpio  = gpioa_dev;
    cfg->bus_config.spi_4wire.dc_pin   = LCD_CD_PIN;

#ifdef CONFIG_LISA_DISPLAY_TE_SYNC
    cfg->te_gpio = gpioa_dev;
    cfg->te_pin  = LCD_TE_PIN;
#endif

    cfg->backlight.type                     = LISA_DISPLAY_BACKLIGHT_TYPE_PWM;
    cfg->backlight.blacklight_polarity      = LISA_DISPLAY_BLACKLIGHT_POLARITY_HIGH;
    cfg->backlight.config.pwm.dev           = lisa_device_get("pwm0");
    cfg->backlight.config.pwm.channel       = 1;
    cfg->backlight.config.pwm.freq          = 2000;

    cfg->rst_gpio = gpiob_dev;
    cfg->rst_pin  = LCD_RST_PIN;

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    cfg->composite_activate   = board_display_composite_activate;
    cfg->composite_deactivate = board_display_composite_deactivate;
#endif

    return 0;
}

/* ---- 公开 API ----------------------------------------------------------- */

void service_brightness_init(void)
{
    lisa_device_t *spi_dev;
    lisa_device_t *gpioa_dev;
    lisa_device_t *gpiob_dev;
    lisa_display_config_t display_config;

    s_display_device = lisa_device_get("display");
    if (!lisa_device_ready(s_display_device)) {
        LOGE("Display device not ready");
        s_display_device = NULL;
        return;
    }

    /* 获取 SPI / GPIO 设备并校验，避免将空指针传给底层驱动 */
    spi_dev = lisa_device_get("spi0");
    if (!spi_dev || !lisa_device_ready(spi_dev)) {
        LOGE("SPI0 device not ready");
        s_display_device = NULL;
        return;
    }

    gpioa_dev = lisa_device_get("gpioa");
    if (!gpioa_dev || !lisa_device_ready(gpioa_dev)) {
        LOGE("GPIOA device not ready");
        s_display_device = NULL;
        return;
    }

    gpiob_dev = lisa_device_get("gpiob");
    if (!gpiob_dev || !lisa_device_ready(gpiob_dev)) {
        LOGE("GPIOB device not ready");
        s_display_device = NULL;
        return;
    }

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    board_display_composite_init();
#endif

    service_brightness_build_display_config(&display_config,
                                             spi_dev, gpioa_dev, gpiob_dev);

    if (lisa_display_attach_bus(s_display_device, &display_config) != 0) {
        LOGE("Failed to attach display bus");
        s_display_device = NULL;
        return;
    }

    if (lisa_display_blanking_on(s_display_device) != 0) {
        LOGE("Failed to turn on display blanking");
        /* 屏幕可能仍然可用，不设为致命错误，继续设置亮度 */
    }

    int brightness = DEFAULT_BRIGHTNESS;
    if (lisa_kv_get_int(KV_KEY_USER_BRIGHTNESS, &brightness) != 0) {
        brightness = DEFAULT_BRIGHTNESS;
    }

    service_brightness_set_temp(brightness);

    LOGI("Brightness service initialized");
}

void service_brightness_set(int brightness)
{
    brightness = service_brightness_clamp(brightness);

    lisa_kv_set_int(KV_KEY_USER_BRIGHTNESS, brightness);
    service_brightness_apply(brightness);
}

void service_brightness_set_temp(int brightness)
{
    brightness = service_brightness_clamp(brightness);
    service_brightness_apply(brightness);
}

int service_brightness_get(void)
{
    int brightness = DEFAULT_BRIGHTNESS;

    if (lisa_kv_get_int(KV_KEY_USER_BRIGHTNESS, &brightness) != 0) {
        brightness = DEFAULT_BRIGHTNESS;
    }

    return brightness;
}
