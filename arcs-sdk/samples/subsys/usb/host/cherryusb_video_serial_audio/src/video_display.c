/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_device.h"
#include "lisa_display.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "IOMuxManager.h"
#include "pinmux.h"
#include "sysheap.h"
#include "workqueue.h"

#include "video_display.h"

/* TJpgDec：通过 LVGL8 sjpg 模块提供的 Tiny JPEG 解码器 */
#if defined(CONFIG_LV_USE_SJPG) && CONFIG_LV_USE_SJPG
#include "extra/libs/sjpg/tjpgd.h"
#endif

/* arcs_evb LCD SPI CS 引脚（pinmux.h 中未定义，参考 face_detect/display.c） */
#ifndef LCD_CS_PIN
#define LCD_CS_PIN 5
#endif

/* ------------------------------------------------------------------ */
/*  显示尺寸                                                            */
/*  ST7789P3 物理分辨率 240×320，LVGL 旋转 270° 后逻辑分辨率 320×240   */
/* ------------------------------------------------------------------ */
#define PREVIEW_W 320
#define PREVIEW_H 240

/* 每路摄像头占半屏宽度 */
#define HALF_W (PREVIEW_W / 2)   /* 160 */
#define DISPLAY_SLOTS_PER_DEV 2

/* ------------------------------------------------------------------ */
/*  预览帧缓冲（RGB565，放 PSRAM 避免占用 SRAM）                        */
/* ------------------------------------------------------------------ */
static __attribute__((section(".psram.bss"))) uint16_t preview_buf[PREVIEW_W * PREVIEW_H];

/* ------------------------------------------------------------------ */
/*  LVGL 对象                                                           */
/* ------------------------------------------------------------------ */
static lv_obj_t     *preview_obj;
static lv_img_dsc_t  preview_img_dsc;
static lv_obj_t     *info_label[2];   /* 每路摄像头各一个状态标签 */

/* ------------------------------------------------------------------ */
/*  显示更新 workqueue                                                  */
/* ------------------------------------------------------------------ */
static workqueue_t *display_wq;

typedef struct {
    uint8_t   dev_idx;
    uint8_t  *frame_buf;
    uint32_t  frame_size;
    uint32_t  frame_capacity;
    uint16_t  width;
    uint16_t  height;
    uint8_t   format;
    volatile uint32_t busy;
} display_slot_t;

static display_slot_t display_slots[2][DISPLAY_SLOTS_PER_DEV];

/* ------------------------------------------------------------------ */
/*  YUY2 → RGB565 内联转换（BT.601）                                   */
/* ------------------------------------------------------------------ */
static inline uint16_t yuyv_to_rgb565(uint8_t y, uint8_t u, uint8_t v)
{
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/* ------------------------------------------------------------------ */
/*  YUYV422 → RGB565，缩放并写入 preview_buf 的指定半屏（x_off 列起始） */
/* ------------------------------------------------------------------ */
static void yuyv_to_preview_half(uint8_t *yuyv, uint16_t src_w, uint16_t src_h, int x_off)
{
    for (int dy = 0; dy < PREVIEW_H; dy++) {
        int sy = dy * src_h / PREVIEW_H;
        for (int dx = 0; dx < HALF_W; dx++) {
            int sx = dx * src_w / HALF_W;
            /* YUYV 每 2 个像素共用一对 U/V，以 4 字节为一组：Y0 U0 Y1 V0 */
            int pair_base = (sy * src_w + (sx & ~1)) * 2;
            uint8_t Y = yuyv[pair_base + (sx & 1) * 2];
            uint8_t U = yuyv[pair_base + 1];
            uint8_t V = yuyv[pair_base + 3];
            preview_buf[dy * PREVIEW_W + x_off + dx] = yuyv_to_rgb565(Y, U, V);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  MJPEG → RGB565 解码并写入 preview_buf 半屏（TJpgDec，JD_FORMAT=0） */
/* ------------------------------------------------------------------ */
#if defined(CONFIG_LV_USE_SJPG) && CONFIG_LV_USE_SJPG

/* JPEG 解码工作内存池大小（字节）：包含 Huffman 表、量化表、MCU 工作缓冲 */
#define JPEG_WORK_POOL_SIZE 4096

typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint32_t       pos;
    int            x_off;
    uint16_t       src_w;
    uint16_t       src_h;
} jpeg_dec_ctx_t;

static size_t jpeg_infunc(JDEC *jd, uint8_t *buf, size_t nbyte)
{
    jpeg_dec_ctx_t *ctx = (jpeg_dec_ctx_t *)jd->device;
    uint32_t remain = ctx->size - ctx->pos;
    if ((uint32_t)nbyte > remain) {
        nbyte = (size_t)remain;
    }
    if (buf) {
        memcpy(buf, ctx->data + ctx->pos, nbyte);
    }
    ctx->pos += (uint32_t)nbyte;
    return nbyte;
}

/* outfunc 接收 RGB888 tile（JD_FORMAT=0），按比例缩放写入半屏 preview_buf */
static int jpeg_outfunc(JDEC *jd, void *bitmap, JRECT *rect)
{
    jpeg_dec_ctx_t *ctx = (jpeg_dec_ctx_t *)jd->device;
    uint8_t *rgb = (uint8_t *)bitmap;
    uint16_t tile_w = rect->right - rect->left + 1;

    /* 计算当前 tile 对应的目标半屏像素范围（避免遍历整个目标区域） */
    int dy0 = (int)rect->top      * PREVIEW_H / ctx->src_h;
    int dy1 = ((int)rect->bottom + 1) * PREVIEW_H / ctx->src_h;
    int dx0 = (int)rect->left     * HALF_W    / ctx->src_w;
    int dx1 = ((int)rect->right  + 1) * HALF_W    / ctx->src_w;

    if (dy1 > PREVIEW_H) dy1 = PREVIEW_H;
    if (dx1 > HALF_W)    dx1 = HALF_W;

    for (int dy = dy0; dy < dy1; dy++) {
        int sy = dy * ctx->src_h / PREVIEW_H;
        int ty = sy - (int)rect->top;
        for (int dx = dx0; dx < dx1; dx++) {
            int sx = dx * ctx->src_w / HALF_W;
            int tx = sx - (int)rect->left;
            uint8_t *px = rgb + (ty * (int)tile_w + tx) * 3;
            uint8_t r = px[0], g = px[1], b = px[2];
            preview_buf[dy * PREVIEW_W + ctx->x_off + dx] =
                (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
        }
    }
    return 1; /* 继续解码 */
}

static void mjpeg_to_preview_half(const uint8_t *mjpeg, uint32_t size, int x_off)
{
    void *pool = psram_malloc(JPEG_WORK_POOL_SIZE);
    if (!pool) {
        return;
    }

    jpeg_dec_ctx_t jctx = {
        .data  = mjpeg,
        .size  = size,
        .pos   = 0,
        .x_off = x_off,
        .src_w = 0,
        .src_h = 0,
    };

    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpeg_infunc, pool, JPEG_WORK_POOL_SIZE, &jctx);
    if (res != JDR_OK) {
        psram_free(pool);
        return;
    }

    /* 保存图像实际尺寸供 outfunc 计算缩放比例 */
    jctx.src_w = jdec.width;
    jctx.src_h = jdec.height;

    jd_decomp(&jdec, jpeg_outfunc, 0 /* scale=1:1 */);
    psram_free(pool);
}

#endif /* CONFIG_LV_USE_SJPG */

static display_slot_t *display_acquire_slot(uint8_t dev_idx)
{
    for (uint8_t i = 0; i < DISPLAY_SLOTS_PER_DEV; i++) {
        display_slot_t *slot = &display_slots[dev_idx][i];
        if (__sync_lock_test_and_set(&slot->busy, 1) == 0) {
            slot->dev_idx = dev_idx;
            return slot;
        }
    }

    return NULL;
}

static void display_release_slot(display_slot_t *slot)
{
    __sync_lock_release(&slot->busy);
}

/* ------------------------------------------------------------------ */
/*  workqueue 处理函数（在 display_wq 任务上下文中执行）                */
/* ------------------------------------------------------------------ */
static void display_wq_handler(void *param)
{
    display_slot_t *slot = (display_slot_t *)param;
    int x_off = (slot->dev_idx == 0) ? 0 : HALF_W;

    if (slot->format == VIDEO_DISPLAY_FORMAT_UNCOMPRESSED) {
        yuyv_to_preview_half(slot->frame_buf, slot->width, slot->height, x_off);
        lv_img_set_src(preview_obj, &preview_img_dsc);
        lv_label_set_text_fmt(info_label[slot->dev_idx], "CAM%u", (unsigned)slot->dev_idx);
    }
#if defined(CONFIG_LV_USE_SJPG) && CONFIG_LV_USE_SJPG
    else if (slot->format == VIDEO_DISPLAY_FORMAT_MJPEG) {
        mjpeg_to_preview_half(slot->frame_buf, slot->frame_size, x_off);
        lv_img_set_src(preview_obj, &preview_img_dsc);
        lv_label_set_text_fmt(info_label[slot->dev_idx], "CAM%u(MJPEG)", (unsigned)slot->dev_idx);
    }
#endif

    display_release_slot(slot);
}

/* ------------------------------------------------------------------ */
/*  公共 API：从 USB 帧回调提交显示更新                                 */
/* ------------------------------------------------------------------ */
void video_display_update(uint8_t dev_idx,
                          uint8_t *frame_buf, uint32_t frame_size,
                          uint16_t width, uint16_t height, uint8_t format)
{
    display_slot_t *slot;
    uint32_t required_size;
    uint8_t *new_buf;

    if (!display_wq || dev_idx >= 2) {
        return;
    }

    slot = display_acquire_slot(dev_idx);
    if (!slot) {
        return;
    }

    required_size = (format == VIDEO_DISPLAY_FORMAT_UNCOMPRESSED) ? ((uint32_t)width * height * 2) : frame_size;
    if (slot->frame_capacity < required_size) {
        new_buf = psram_realloc(slot->frame_buf, required_size);
        if (!new_buf) {
            display_release_slot(slot);
            return;
        }
        slot->frame_buf = new_buf;
        slot->frame_capacity = required_size;
    }

    memcpy(slot->frame_buf, frame_buf, frame_size);
    slot->frame_size = frame_size;
    slot->width = width;
    slot->height = height;
    slot->format = format;

    if (workqueue_submit(display_wq, display_wq_handler, slot) == 0) {
        display_release_slot(slot);
    }
}

/* ------------------------------------------------------------------ */
/*  LVGL 屏幕创建                                                       */
/* ------------------------------------------------------------------ */
static lv_obj_t *screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);

    /* 全屏图像（两路摄像头共用同一 preview_buf，左右各半） */
    preview_img_dsc.header.cf   = LV_IMG_CF_TRUE_COLOR;
    preview_img_dsc.header.w    = PREVIEW_W;
    preview_img_dsc.header.h    = PREVIEW_H;
    preview_img_dsc.data        = (uint8_t *)preview_buf;
    preview_img_dsc.data_size   = sizeof(preview_buf);

    preview_obj = lv_img_create(screen);
    lv_obj_set_pos(preview_obj, 0, 0);
    lv_obj_set_size(preview_obj, PREVIEW_W, PREVIEW_H);

    /* 竖向分隔线 */
    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
    lv_obj_set_pos(divider, HALF_W, 0);
    lv_obj_set_size(divider, 1, PREVIEW_H);

    /* 左半标签（CAM0） */
    info_label[0] = lv_label_create(screen);
    lv_obj_set_style_text_color(info_label[0], lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_pos(info_label[0], 4, 4);
    lv_label_set_text(info_label[0], "CAM0: waiting...");

    /* 右半标签（CAM1） */
    info_label[1] = lv_label_create(screen);
    lv_obj_set_style_text_color(info_label[1], lv_color_hex(0x00FFFF), LV_PART_MAIN);
    lv_obj_set_pos(info_label[1], HALF_W + 4, 4);
    lv_label_set_text(info_label[1], "CAM1: waiting...");

    return screen;
}

/* ------------------------------------------------------------------ */
/*  LVGL 任务                                                           */
/* ------------------------------------------------------------------ */
static void task_ui(void *pv)
{
    lv_obj_t *screen = screen_create();
    lv_scr_load(screen);

    while (1) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ------------------------------------------------------------------ */
/*  公共 API：初始化显示子系统                                           */
/* ------------------------------------------------------------------ */
void video_display_init(void)
{
    lv_init();

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {
            .spi_4wire = {
                .spi_dev = lisa_device_get("spi1"),
                .cs_gpio = gpiob_dev,
                .cs_pin  = LCD_CS_PIN,
                .dc_gpio = gpiob_dev,
                .dc_pin  = LCD_CD_PIN,
                .spi_freq = 50 * 1000 * 1000,
            }
        },
        .backlight = {
            .type               = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .channel = 0,
                .dev     = lisa_device_get("pwm0"),
                .freq    = 2000,
            }
        },
        .rst_gpio = gpioa_dev,
        .rst_pin  = LCD_RST_PIN,
    };

    lisa_device_t *display_dev = lisa_device_get("display");
    if (!display_dev) {
        printf("[DISP] Failed to get display device\r\n");
        return;
    }

    lisa_display_attach_bus(display_dev, &display_config);
    lv_port_disp_init(display_dev);
    lisa_display_blanking_off(display_dev);

    display_wq = workqueue_create("disp_wq", 9, 4, 8192);
    if (!display_wq) {
        printf("[DISP] Failed to create display workqueue\r\n");
    }

    xTaskCreate(task_ui, "task_ui", 4 * 1024, NULL, configMAX_PRIORITIES - 2, NULL);
}
