/* Zig Display Test
 *
 * C 端: 完成板级 Display 初始化 (attach_bus 配置 SPI/GPIO/背光)
 * Zig 端: 填充红绿蓝三色测试
 */

#define LOG_TAG "disp_test"
#include <lisa_log.h>
#include <lisa_device.h>
#include <lisa_display.h>
#include "IOMuxManager.h"
#include "board.h"

/* Zig 导出的显示测试函数 */
extern int zig_display_test(void);

/* arcs_evb 板级引脚定义 */
#define LCD_CS_PIN       5
#define LCD_SPI_CLK_PIN  3
#define LCD_SPI_DATA_PIN 1

/* pinmux 配置 (arcs_evb) */
void lisa_gpioa_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
}

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

int main(int argc, char **argv)
{
    LOGI("=== Zig Display Test ===");

    /* 1. 获取 display 设备 */
    lisa_device_t *display_device = lisa_device_get("display");
    if (!display_device) {
        LOGE("Failed to get display device");
        return -1;
    }
    LOGI("Display device found");

    /* 2. 配置 display 总线 (SPI 4-wire + PWM 背光) */
    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .panel_name = "st7789p3",
        .bus_config = {.spi_4wire = {
            .spi_dev = lisa_device_get("spi1"),
            .cs_gpio = lisa_device_get("gpiob"),
            .cs_pin = LCD_CS_PIN,
            .dc_gpio = lisa_device_get("gpiob"),
            .dc_pin = LCD_CD_PIN,
            .spi_freq = 50 * 1000 * 1000,
        }},
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .channel = 0,
                .dev = lisa_device_get("pwm0"),
                .freq = 2000
            }
        },
        .rst_gpio = lisa_device_get("gpioa"),
        .rst_pin = LCD_RST_PIN,
    };

    int ret = lisa_display_attach_bus(display_device, &display_config);
    if (ret != 0) {
        LOGE("attach_bus failed: %d", ret);
        return -2;
    }
    LOGI("Display bus attached (SPI 4-wire, ST7789P3)");

    /* 3. 调用 Zig 显示测试 */
    LOGI("Calling zig_display_test()...");
    ret = zig_display_test();

    if (ret == 0) {
        LOGI("=== Zig Display Test PASSED ===");
    } else {
        LOGE("=== Zig Display Test FAILED (ret=%d) ===", ret);
    }

    return ret;
}
