/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boot_display.h"
#include "boot_charging_img.h"
#include "boot_ota_img.h"
#include "boot_recovery_img.h"

#include "lisa_device.h"
#include "lisa_display.h"

#include "board.h"
#include "syslog.h"

#define BOOT_DISPLAY_WIDTH      240
#define BOOT_DISPLAY_HEIGHT     240
#define BOOT_DISPLAY_BRIGHTNESS 100

#ifndef CONFIG_BOOT_DISPLAY_MADCTL
#define CONFIG_BOOT_DISPLAY_MADCTL 0x60
#endif

#if CONFIG_BOOT_DISPLAY_MADCTL > 0xFF
#error "BOOT_DISPLAY_MADCTL must fit in 8 bits"
#endif

/* scatload_psram 会把 .psram.bss 清 0，main.c 后续还做了 PSRAM 的 DCache flush，
 * 所以这块缓冲 boot 到我们手里时就是可以直接给 DMA 用的全零数据。*/
#define BOOT_DISPLAY_CLEAR_ROWS 10
__attribute__((section(".psram.bss")))
static uint16_t s_clear_buf[BOOT_DISPLAY_WIDTH * BOOT_DISPLAY_CLEAR_ROWS];

#define BOOT_OTA_BAR_WIDTH  140
#define BOOT_OTA_BAR_HEIGHT 6
#define BOOT_OTA_BAR_X      ((BOOT_DISPLAY_WIDTH - BOOT_OTA_BAR_WIDTH) / 2)
#define BOOT_OTA_BAR_Y \
    (BOOT_DISPLAY_HEIGHT / 2 + BOOT_OTA_IMG_HEIGHT / 2 + 16)

__attribute__((section(".psram.bss")))
static uint16_t s_bar_buf[BOOT_OTA_BAR_WIDTH * BOOT_OTA_BAR_HEIGHT];

/* charging UI 上指向 POWER_KEY 的提示箭头：贴边位置和指向都用手机九宫格
 * 数字配置（1=左上 … 9=右下），见 Kconfig.public。
 * 正交箭头底边 = 2*深度 - 1 时斜边正好 45°，每行边界单调变化 1 像素；
 * 斜向箭头是直角顶点朝配置对角的等腰直角三角形，直角边取 15 让底宽和
 * 顶点深度跟正交箭头基本一致。*/
#define BOOT_POWER_KEY_HINT_DEPTH       11
#define BOOT_POWER_KEY_HINT_BASE        21
#define BOOT_POWER_KEY_HINT_DIAG_LEG    15
#define BOOT_POWER_KEY_HINT_EDGE_MARGIN 9

#ifndef CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION
#define CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION 7
#endif
#ifndef CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION
#define CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION 4
#endif

/* 九宫格数字拆成 -1/0/+1 的列、行分量 */
#define BOOT_HINT_GRID_COL(n) (((n) - 1) % 3 - 1)
#define BOOT_HINT_GRID_ROW(n) (((n) - 1) / 3 - 1)

#define BOOT_HINT_DIR_DX  BOOT_HINT_GRID_COL(CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION)
#define BOOT_HINT_DIR_DY  BOOT_HINT_GRID_ROW(CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION)
#define BOOT_HINT_POS_COL BOOT_HINT_GRID_COL(CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION)
#define BOOT_HINT_POS_ROW BOOT_HINT_GRID_ROW(CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION)

#if CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION < 1 || CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION > 9
#error "BOOT_DISPLAY_POWER_KEY_HINT_POSITION must be a keypad digit 1..9"
#endif
#if CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_POSITION == 5
#error "BOOT_DISPLAY_POWER_KEY_HINT_POSITION=5 (center) overlaps the centered charging image"
#endif
#if CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION < 1 || CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION > 9
#error "BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION must be a keypad digit 1..9"
#endif
#if CONFIG_BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION == 5
#error "BOOT_DISPLAY_POWER_KEY_HINT_DIRECTION=5 (center) has no pointing direction"
#endif

#if BOOT_HINT_DIR_DX != 0 && BOOT_HINT_DIR_DY != 0
#define BOOT_POWER_KEY_HINT_WIDTH  BOOT_POWER_KEY_HINT_DIAG_LEG
#define BOOT_POWER_KEY_HINT_HEIGHT BOOT_POWER_KEY_HINT_DIAG_LEG
#elif BOOT_HINT_DIR_DX != 0
#define BOOT_POWER_KEY_HINT_WIDTH  BOOT_POWER_KEY_HINT_DEPTH
#define BOOT_POWER_KEY_HINT_HEIGHT BOOT_POWER_KEY_HINT_BASE
#else
#define BOOT_POWER_KEY_HINT_WIDTH  BOOT_POWER_KEY_HINT_BASE
#define BOOT_POWER_KEY_HINT_HEIGHT BOOT_POWER_KEY_HINT_DEPTH
#endif

__attribute__((section(".psram.bss")))
static uint16_t s_power_key_hint_buf[BOOT_POWER_KEY_HINT_WIDTH * BOOT_POWER_KEY_HINT_HEIGHT];

/* 复制 ST7789P3 内置初始化序列，MADCTL 由板级 boot 配置控制，
 * 让 boot recovery 图标方向跟板级 LCD 安装方向一致。*/
static const uint8_t s_panel_init_seq[] = {
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
    0x35, 1, 0x00,
    0x36, 1, (uint8_t)CONFIG_BOOT_DISPLAY_MADCTL,
    0x3A, 1, 0x05,
    0xB7, 1, 0x55,
    0xBB, 1, 0x16,
    0xC0, 1, 0x2C,
    0xC2, 1, 0x01,
    0xC3, 1, 0x13,
    0xC6, 1, 0x0F,
    0xD0, 3, 0xA7, 0xA4, 0xA1,
    0xD6, 1, 0xA1,
    0xE0, 14, 0xF0, 0x06, 0x0E, 0x08, 0x08, 0x04, 0x37, 0x43, 0x4C, 0x36, 0x12, 0x12, 0x2C, 0x34,
    0xE1, 14, 0xF0, 0x0D, 0x12, 0x0C, 0x0A, 0x16, 0x37, 0x43, 0x4C, 0x39, 0x14, 0x15, 0x2E, 0x36,
};

static lisa_device_t *s_display_device = NULL;

void boot_display_init(void)
{
    if (s_display_device != NULL) {
        return;
    }

    s_display_device = lisa_device_get("display");
    if (!lisa_device_ready(s_display_device)) {
        printk("display: device not ready\n");
        s_display_device = NULL;
        return;
    }

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config =
            {
                .spi_4wire =
                    {
                        .spi_dev = lisa_device_get("spi0"),
                        .cs_gpio = gpioa_dev,
                        .cs_pin = LCD_CS_PIN,
                        .dc_gpio = gpioa_dev,
                        .dc_pin = LCD_CD_PIN,
                        .spi_freq = 50 * 1000 * 1000,
                    },
            },
        .backlight =
            {
                .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                .config.pwm =
                    {
                        .channel = 1,
                        .dev = lisa_device_get("pwm0"),
                        .freq = 2000,
                    },
            },
        .rst_gpio = gpiob_dev,
        .rst_pin = LCD_RST_PIN,
        .panel_init_params = s_panel_init_seq,
        .panel_init_params_len = sizeof(s_panel_init_seq),
    };

    if (lisa_display_attach_bus(s_display_device, &display_config) != 0) {
        printk("display: attach bus failed\n");
        s_display_device = NULL;
        return;
    }

    /* DISPLAY_ON / 背光延迟到写完内容再开，避免闪花屏。*/
}

static void show_centered_image(const uint8_t *data, uint16_t w, uint16_t h, size_t bytes)
{
    if (s_display_device == NULL) {
        return;
    }

    lisa_display_buffer_desc_t desc = {
        .width = BOOT_DISPLAY_WIDTH,
        .height = BOOT_DISPLAY_CLEAR_ROWS,
        .pitch = BOOT_DISPLAY_WIDTH,
        .buf_size = sizeof(s_clear_buf),
    };

    for (uint16_t y = 0; y < BOOT_DISPLAY_HEIGHT; y += BOOT_DISPLAY_CLEAR_ROWS) {
        lisa_display_write(s_display_device, 0, y, &desc, s_clear_buf);
    }

    desc.width = w;
    desc.height = h;
    desc.pitch = w;
    desc.buf_size = bytes;

    uint16_t img_x = (BOOT_DISPLAY_WIDTH - w) / 2;
    uint16_t img_y = (BOOT_DISPLAY_HEIGHT - h) / 2;
    lisa_display_write(s_display_device, img_x, img_y, &desc, data);

    lisa_display_blanking_off(s_display_device);
    lisa_display_set_brightness(s_display_device, BOOT_DISPLAY_BRIGHTNESS);
}

void boot_display_show_recovery(void)
{
    show_centered_image(boot_recovery_img_data, BOOT_RECOVERY_IMG_WIDTH,
                        BOOT_RECOVERY_IMG_HEIGHT, sizeof(boot_recovery_img_data));
}

static void draw_power_key_hint(void)
{
    if (s_display_device == NULL) {
        return;
    }

    const int w = BOOT_POWER_KEY_HINT_WIDTH;
    const int h = BOOT_POWER_KEY_HINT_HEIGHT;
    const int dx = BOOT_HINT_DIR_DX;
    const int dy = BOOT_HINT_DIR_DY;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool filled;
            if (dx != 0 && dy != 0) {
                /* 斜向：把坐标翻转到直角顶点所在角，填充 u + v <= 直角边 - 1
                 * （此形态下 w == h == DIAG_LEG，用 w 保证与缓冲宏解耦） */
                int u = dx < 0 ? x : w - 1 - x;
                int v = dy < 0 ? y : h - 1 - y;
                filled = u + v <= w - 1;
            } else {
                /* 正交：along 是朝指向顶点的深度，dist2 用 2*cross - (底边-1)
                 * 度量到底边中线的距离，奇偶底边都能得到对称三角形。*/
                int along = dx < 0 ? x : dx > 0 ? w - 1 - x : dy < 0 ? y : h - 1 - y;
                int cross = dx != 0 ? y : x;
                int base = dx != 0 ? h : w;
                int depth = dx != 0 ? w : h;
                int dist2 = cross * 2 - (base - 1);
                if (dist2 < 0) {
                    dist2 = -dist2;
                }
                filled = along >= dist2 * (depth - 1) / (base - 1);
            }
            s_power_key_hint_buf[y * w + x] = filled ? 0xFFFFu : 0x0000u;
        }
    }

    /* boot 编译时未定义 CONFIG_DCACHE_ENABLE，SDK 里 lisa_display / lisa_spi
     * 的 cache flush 全被预处理掉，DCache 又是 non-coherent，所以这里要手动
     * 把 buffer flush 到 PSRAM，否则 SPI DMA 读到的还是 scatload 后的全零。*/
    extern void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t dsize);
    HAL_FlushDCache_by_Addr((uint32_t *)s_power_key_hint_buf, sizeof(s_power_key_hint_buf));

    lisa_display_buffer_desc_t desc = {
        .width = BOOT_POWER_KEY_HINT_WIDTH,
        .height = BOOT_POWER_KEY_HINT_HEIGHT,
        .pitch = BOOT_POWER_KEY_HINT_WIDTH,
        .buf_size = sizeof(s_power_key_hint_buf),
    };

    /* 贴边规则：箭头指向的屏幕边贴死（0 留白），其余靠边方向留 EDGE_MARGIN */
    const int col = BOOT_HINT_POS_COL;
    const int row = BOOT_HINT_POS_ROW;
    uint16_t hint_x = col < 0   ? (dx < 0 ? 0 : BOOT_POWER_KEY_HINT_EDGE_MARGIN)
                      : col > 0 ? BOOT_DISPLAY_WIDTH - w -
                                      (dx > 0 ? 0 : BOOT_POWER_KEY_HINT_EDGE_MARGIN)
                                : (BOOT_DISPLAY_WIDTH - w) / 2;
    uint16_t hint_y = row < 0   ? (dy < 0 ? 0 : BOOT_POWER_KEY_HINT_EDGE_MARGIN)
                      : row > 0 ? BOOT_DISPLAY_HEIGHT - h -
                                      (dy > 0 ? 0 : BOOT_POWER_KEY_HINT_EDGE_MARGIN)
                                : (BOOT_DISPLAY_HEIGHT - h) / 2;
    lisa_display_write(s_display_device, hint_x, hint_y, &desc, s_power_key_hint_buf);
}

void boot_display_show_charging(void)
{
    show_centered_image(boot_charging_img_data, BOOT_CHARGING_IMG_WIDTH,
                        BOOT_CHARGING_IMG_HEIGHT, sizeof(boot_charging_img_data));
    draw_power_key_hint();
}

void boot_display_show_ota(void)
{
    show_centered_image(boot_ota_img_data, BOOT_OTA_IMG_WIDTH, BOOT_OTA_IMG_HEIGHT,
                        sizeof(boot_ota_img_data));

    for (size_t i = 0; i < BOOT_OTA_BAR_WIDTH * BOOT_OTA_BAR_HEIGHT; i++) {
        s_bar_buf[i] = 0xFFFFu;
    }

    /* 同 draw_power_key_hint：boot 未开 DCache，CPU 填充后必须手动 flush
     * 才能保证 SPI DMA 读到进度条像素而不是 scatload 后的全零。*/
    extern void HAL_FlushDCache_by_Addr(uint32_t *addr, uint32_t dsize);
    HAL_FlushDCache_by_Addr((uint32_t *)s_bar_buf, sizeof(s_bar_buf));
}

void boot_display_ota_progress(uint8_t percent)
{
    static uint8_t last_pct = 0xFFu;

    if (s_display_device == NULL) {
        return;
    }
    if (percent > 100u) {
        percent = 100u;
    }
    if (percent == last_pct) {
        return;
    }
    last_pct = percent;

    uint16_t fill_w = (uint16_t)((uint32_t)BOOT_OTA_BAR_WIDTH * percent / 100u);
    if (fill_w == 0u) {
        return;
    }

    /* 进度条整体是全白 buffer，发 fill_w × BAR_H 大小过去就是一块
     * 填充的白矩形，驱动按 width*height 个像素线性取值，不关心我们
     * 内存里怎么排布——全白的话怎么切都是白的。*/
    lisa_display_buffer_desc_t desc = {
        .width = fill_w,
        .height = BOOT_OTA_BAR_HEIGHT,
        .pitch = fill_w,
        .buf_size = (uint32_t)fill_w * BOOT_OTA_BAR_HEIGHT * sizeof(uint16_t),
    };
    lisa_display_write(s_display_device, BOOT_OTA_BAR_X, BOOT_OTA_BAR_Y, &desc, s_bar_buf);
}
