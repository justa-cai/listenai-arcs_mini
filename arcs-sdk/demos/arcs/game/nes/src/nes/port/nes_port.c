/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nes.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "game_nes_input.h"
#include "game_nes_usb_keyboard.h"
#include "IOMuxManager.h"
#include "board.h"
#include "lisa_audio.h"
#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_log.h"
#include "lisa_touch.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "heap_private.h"
#include "sysheap.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "game_nes_port"

#ifndef CONFIG_NES_PORT_PROFILE_ENABLE
#define CONFIG_NES_PORT_PROFILE_ENABLE 1
#endif
#ifndef CONFIG_NES_AUDIO_FAST_S16_COPY
#define CONFIG_NES_AUDIO_FAST_S16_COPY 1
#endif

#define NES_VISIBLE_X_OFFSET    8
#define NES_VISIBLE_WIDTH       240
#define NES_VISIBLE_HEIGHT      240
#define NES_LVGL_FRAME_DIV      1U
#define NES_LVGL_TASK_DIV_ACTIVE 2U
#define NES_LVGL_TASK_DIV_IDLE   4U
#define NES_DIRECT_LCD_STRIPE_LINES 8U
#define NES_DIRECT_LCD_MERGE_GAP_STRIPES 1U

#define TOUCH_DEVICE            "touch_cst328"
#define I2C_DEVICE              "i2c0"
#define AUDIO_DEVICE            "audio0"

#ifndef LCD_CS_PIN
#define LCD_CS_PIN 5
#endif

#ifndef LCD_SPI_CLK_PIN
#define LCD_SPI_CLK_PIN 3
#endif

#ifndef LCD_SPI_DATA_PIN
#define LCD_SPI_DATA_PIN 1
#endif

#define LISA_TOUCH_I2C_SDA_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN   22
#define LISA_TOUCH_I2C_SDA_FUNC  8

#define LISA_TOUCH_I2C_SCL_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN   23
#define LISA_TOUCH_I2C_SCL_FUNC  8

#define LISA_TOUCH_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN   25
#define LISA_TOUCH_I2C_RST_FUNC  0

#define LISA_TOUCH_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN   24
#define LISA_TOUCH_I2C_INT_FUNC  0

#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_RST_PORT, LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_INT_PORT, LISA_TOUCH_I2C_INT_PIN, LISA_TOUCH_I2C_INT_FUNC);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SDA_PORT, LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SCL_PORT, LISA_TOUCH_I2C_SCL_PIN, LISA_TOUCH_I2C_SCL_FUNC);
}

void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12);
}
#endif

static lisa_device_t *g_display = NULL;
#if !NES_LV_ZERO_COPY
static void *g_crop_buffer_raw = NULL;
static nes_color_t *g_crop_buffer = NULL;
#endif
static lv_img_dsc_t g_nes_frame_dsc;
static lv_obj_t *g_nes_frame_img = NULL;
static volatile uint16_t g_touch_joypad_state = 0;
static volatile bool g_rom_list_requested = false;
static uint16_t g_display_y_offset = 0;
static uint32_t g_frame_invalidate_counter = 0;
static uint32_t g_lv_task_counter = 0;

#if NES_DIRECT_LCD
static uint64_t *g_direct_lcd_hashes = NULL;
static uint16_t g_direct_lcd_stripe_count = 0;
static bool g_direct_lcd_hash_valid = false;
static uint32_t g_direct_lcd_changed_stripes_acc = 0;
static uint32_t g_direct_lcd_runs_acc = 0;
#endif

#if !NES_LV_ZERO_COPY
#define NES_CROP_BUF_BYTES       (NES_VISIBLE_WIDTH * NES_VISIBLE_HEIGHT * sizeof(nes_color_t))
#define NES_CROP_BUF_GUARD       64U
#endif

#if CONFIG_NES_PORT_PROFILE_ENABLE
static uint32_t g_prof_frames;
static uint32_t g_prof_draw_frames;
static uint32_t g_prof_last_ms;
static uint32_t g_prof_draw_ms_acc;
static uint32_t g_prof_lv_ms_acc;
static uint32_t g_prof_audio_ms_acc;
static uint32_t g_prof_audio_calls;
#endif

static inline uint32_t game_nes_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

int nes_audio_reference_path_enabled(void)
{
    return 0;
}

int nes_frame_skip_get(void)
{
    return NES_FRAME_SKIP;
}

int nes_draw_target_fps_get(void)
{
    return CONFIG_NES_DRAW_FPS_TARGET;
}

#if NES_DIRECT_LCD
static uint64_t game_nes_hash_visible_rows(const nes_color_t *color_data, uint16_t start_row, uint16_t rows)
{
    uint32_t sum = 0x9e3779b9UL ^ ((uint32_t)start_row << 16) ^ rows;
    uint32_t mix = 0x85ebca6bUL + ((uint32_t)rows << 8) + start_row;
    const uint32_t words_per_row = (NES_VISIBLE_WIDTH * sizeof(nes_color_t)) / sizeof(uint32_t);

    for (uint16_t r = 0; r < rows; r++) {
        const uint32_t *words =
            (const uint32_t *)(color_data + (start_row + r) * NES_WIDTH + NES_VISIBLE_X_OFFSET);
        for (uint32_t i = 0; i < words_per_row; i++) {
            const uint32_t value = words[i];
            sum += value;
            mix ^= value + 0x9e3779b9UL + (mix << 6) + (mix >> 2);
        }
    }

    const uint64_t hash = ((uint64_t)sum << 32) | mix;
    return hash ? hash : 1U;
}

static uint16_t game_nes_stripe_rows(uint16_t stripe)
{
    const uint16_t start_row = (uint16_t)(stripe * NES_DIRECT_LCD_STRIPE_LINES);
    uint16_t rows = NES_DIRECT_LCD_STRIPE_LINES;
    if (start_row + rows > NES_VISIBLE_HEIGHT) {
        rows = (uint16_t)(NES_VISIBLE_HEIGHT - start_row);
    }
    return rows;
}

static int game_nes_direct_lcd_write_run(const nes_color_t *color_data, uint16_t start_stripe, uint16_t stripe_count)
{
    const uint16_t start_row = (uint16_t)(start_stripe * NES_DIRECT_LCD_STRIPE_LINES);
    uint16_t rows = (uint16_t)(stripe_count * NES_DIRECT_LCD_STRIPE_LINES);
    if (start_row + rows > NES_VISIBLE_HEIGHT) {
        rows = (uint16_t)(NES_VISIBLE_HEIGHT - start_row);
    }

    lisa_display_buffer_desc_t desc = {
        .width = NES_VISIBLE_WIDTH,
        .height = rows,
        .pitch = NES_WIDTH,
        .buf_size = NES_WIDTH * rows * sizeof(nes_color_t),
    };
    const nes_color_t *visible_origin = color_data + start_row * NES_WIDTH + NES_VISIBLE_X_OFFSET;
    return lisa_display_write(g_display, 0, g_display_y_offset + start_row, &desc, visible_origin);
}

static int game_nes_direct_lcd_draw_diff(const nes_color_t *color_data)
{
    if (!g_direct_lcd_hashes || g_direct_lcd_stripe_count == 0U) {
        return game_nes_direct_lcd_write_run(color_data, 0, g_direct_lcd_stripe_count);
    }

    uint16_t run_start = 0;
    uint16_t run_count = 0;
    uint16_t merge_gap = 0;
    uint16_t changed_stripes = 0;
    uint16_t runs = 0;
    int ret = 0;

    for (uint16_t stripe = 0; stripe < g_direct_lcd_stripe_count; stripe++) {
        const uint16_t start_row = (uint16_t)(stripe * NES_DIRECT_LCD_STRIPE_LINES);
        const uint16_t rows = game_nes_stripe_rows(stripe);
        const uint64_t hash = game_nes_hash_visible_rows(color_data, start_row, rows);
        const bool changed = !g_direct_lcd_hash_valid || g_direct_lcd_hashes[stripe] != hash;
        g_direct_lcd_hashes[stripe] = hash;

        if (changed) {
            changed_stripes++;
            if (run_count == 0U) {
                run_start = stripe;
                run_count = 1;
            } else {
                run_count = (uint16_t)(run_count + merge_gap + 1U);
            }
            merge_gap = 0;
            continue;
        }

        if (run_count != 0U && merge_gap < NES_DIRECT_LCD_MERGE_GAP_STRIPES) {
            merge_gap++;
            continue;
        }

        if (run_count != 0U) {
            runs++;
            ret = game_nes_direct_lcd_write_run(color_data, run_start, run_count);
            if (ret != 0) {
                break;
            }
            run_count = 0;
            merge_gap = 0;
        }
    }

    if (ret == 0 && run_count != 0U) {
        runs++;
        ret = game_nes_direct_lcd_write_run(color_data, run_start, run_count);
    }

    g_direct_lcd_hash_valid = true;
    g_direct_lcd_changed_stripes_acc += changed_stripes;
    g_direct_lcd_runs_acc += runs;
    return ret;
}
#endif

#if (NES_ENABLE_SOUND == 1)
static lisa_device_t *g_audio_dev = NULL;
static int16_t *g_audio_pcm16 = NULL;
static int16_t *g_audio_ring = NULL;
static volatile uint32_t g_audio_ring_wr = 0;
static volatile uint32_t g_audio_ring_rd = 0;
static volatile uint32_t g_audio_ring_count = 0;
static volatile uint32_t g_audio_ring_drop_count = 0;
static volatile uint32_t g_audio_ring_empty_count = 0;
static volatile uint8_t g_audio_task_running = 0;
static TaskHandle_t g_audio_task = NULL;
static int32_t g_audio_lp_state = 0;
static int32_t g_audio_dc_x_prev = 0;
static int32_t g_audio_dc_y_prev = 0;

#define NES_AUDIO_OUT_RATE          LISA_AUDIO_RATE_48K
#define NES_AUDIO_OUT_CHANNELS      LISA_AUDIO_CH_LEFT
#define NES_AUDIO_OUT_BITS          LISA_AUDIO_BIT_16
#define NES_AUDIO_OUT_BUF_COUNT     12
#define NES_AUDIO_FRAME_SAMPLES     ((uint32_t)NES_AUDIO_OUT_RATE / 60U)
#define NES_AUDIO_OUT_BUF_SAMPLES   NES_AUDIO_FRAME_SAMPLES
#define NES_AUDIO_TMP_MAX_SAMPLES   NES_AUDIO_OUT_BUF_SAMPLES
#define NES_AUDIO_RING_DEPTH        8U
#define NES_AUDIO_LP_SHIFT          3
#define NES_AUDIO_NOISE_GATE        96
#endif

static inline int16_t game_nes_clip_s16(int32_t x)
{
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

static void game_nes_btn_event_cb(lv_event_t *e)
{
    uint16_t mask = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        g_touch_joypad_state |= mask;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_touch_joypad_state &= (uint16_t)(~mask);
    }
}

static lv_obj_t *game_nes_create_btn(lv_obj_t *parent,
                                     const char *text,
                                     int x,
                                     int y,
                                     int w,
                                     int h,
                                     uint16_t key_mask)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, game_nes_btn_event_cb, LV_EVENT_PRESSED, (void *)(uintptr_t)key_mask);
    lv_obj_add_event_cb(btn, game_nes_btn_event_cb, LV_EVENT_PRESSING, (void *)(uintptr_t)key_mask);
    lv_obj_add_event_cb(btn, game_nes_btn_event_cb, LV_EVENT_RELEASED, (void *)(uintptr_t)key_mask);
    lv_obj_add_event_cb(btn, game_nes_btn_event_cb, LV_EVENT_PRESS_LOST, (void *)(uintptr_t)key_mask);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void game_nes_rom_list_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    g_touch_joypad_state = 0;
    g_rom_list_requested = true;
}

static lv_obj_t *game_nes_create_rom_list_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 72, 30);
    lv_obj_set_pos(btn, 2, 202);
    lv_obj_add_event_cb(btn, game_nes_rom_list_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "ROM");
    lv_obj_center(label);
    return btn;
}

static void game_nes_ui_init(const nes_color_t *frame_buffer)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    memset(&g_nes_frame_dsc, 0, sizeof(g_nes_frame_dsc));
    g_nes_frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
#if NES_LV_ZERO_COPY
    g_nes_frame_dsc.header.w = NES_WIDTH;
    g_nes_frame_dsc.header.h = NES_VISIBLE_HEIGHT;
    g_nes_frame_dsc.data_size = NES_WIDTH * NES_VISIBLE_HEIGHT * sizeof(nes_color_t);
    g_nes_frame_dsc.data = (const uint8_t *)frame_buffer;
#else
    (void)frame_buffer;
    g_nes_frame_dsc.header.w = NES_VISIBLE_WIDTH;
    g_nes_frame_dsc.header.h = NES_VISIBLE_HEIGHT;
    g_nes_frame_dsc.data_size = NES_CROP_BUF_BYTES;
    g_nes_frame_dsc.data = (const uint8_t *)g_crop_buffer;
#endif

    g_nes_frame_img = lv_img_create(screen);
    lv_img_set_src(g_nes_frame_img, &g_nes_frame_dsc);
    lv_obj_set_pos(g_nes_frame_img, 0, 0);
#if NES_LV_ZERO_COPY
    lv_obj_set_size(g_nes_frame_img, NES_VISIBLE_WIDTH, NES_VISIBLE_HEIGHT);
    lv_img_set_offset_x(g_nes_frame_img, -NES_VISIBLE_X_OFFSET);
#endif

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_pos(panel, 240, 0);
    lv_obj_set_size(panel, 80, 240);
    lv_obj_set_style_pad_all(panel, 2, 0);

    game_nes_create_btn(panel, "UP", 28, 8, 24, 24, GAME_NES_BTN_U1);
    game_nes_create_btn(panel, "L", 2, 34, 24, 24, GAME_NES_BTN_L1);
    game_nes_create_btn(panel, "R", 54, 34, 24, 24, GAME_NES_BTN_R1);
    game_nes_create_btn(panel, "DN", 28, 60, 24, 24, GAME_NES_BTN_D1);

    game_nes_create_btn(panel, "A", 44, 104, 32, 28, GAME_NES_BTN_A1);
    game_nes_create_btn(panel, "B", 4, 104, 32, 28, GAME_NES_BTN_B1);

    game_nes_create_btn(panel, "SE", 4, 146, 32, 24, GAME_NES_BTN_SE1);
    game_nes_create_btn(panel, "ST", 44, 146, 32, 24, GAME_NES_BTN_ST1);
    game_nes_create_rom_list_btn(panel);
}

uint16_t game_nes_get_joypad_state(void)
{
    return (uint16_t)(g_touch_joypad_state | game_nes_usb_keyboard_get_state());
}

bool game_nes_consume_rom_list_request(void)
{
    bool requested = g_rom_list_requested;
    g_rom_list_requested = false;
    return requested;
}

void game_nes_clear_rom_list_request(void)
{
    g_rom_list_requested = false;
}

void *nes_malloc(int num)
{
    return exram_malloc(4, (size_t)num);
}

void *nes_try_malloc(int num)
{
    if (num <= 0) {
        return NULL;
    }

    const size_t size = (size_t)num;
    return heap_caps_aligned_alloc_base(4, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}

void nes_free(void *address)
{
    exram_free(address);
}

void *nes_sram_malloc(int num)
{
    return inram_malloc(4, (size_t)num);
}

void nes_sram_free(void *address)
{
    inram_free(address);
}

void *nes_memcpy(void *str1, const void *str2, size_t n)
{
    return memcpy(str1, str2, n);
}

void *nes_memset(void *str, int c, size_t n)
{
    return memset(str, c, n);
}

int nes_memcmp(const void *str1, const void *str2, size_t n)
{
    return memcmp(str1, str2, n);
}

uint32_t nes_get_ms(void)
{
    return game_nes_now_ms();
}

#if (NES_USE_FS == 1)
FILE *nes_fopen(const char *filename, const char *mode)
{
    return fopen(filename, mode);
}

size_t nes_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fread(ptr, size, nmemb, stream);
}

size_t nes_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

int nes_fseek(FILE *stream, long int offset, int whence)
{
    return fseek(stream, offset, whence);
}

int nes_fclose(FILE *stream)
{
    return fclose(stream);
}
#endif

int nes_log_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

#if (NES_ENABLE_SOUND == 1)
static void game_nes_audio_task(void *arg)
{
    (void)arg;

    while (g_audio_task_running) {
        if (g_audio_ring_count == 0U) {
            g_audio_ring_empty_count++;
            lisa_thread_mdelay(1);
            continue;
        }

        uint32_t slot;
        taskENTER_CRITICAL();
        slot = g_audio_ring_rd;
        g_audio_ring_rd = (g_audio_ring_rd + 1U) % NES_AUDIO_RING_DEPTH;
        g_audio_ring_count--;
        taskEXIT_CRITICAL();

        int16_t *frame = g_audio_ring + (slot * NES_AUDIO_FRAME_SAMPLES);
        (void)lisa_audio_play_write(g_audio_dev, frame, NES_AUDIO_FRAME_SAMPLES);
    }

    vTaskDelete(NULL);
}

static void game_nes_audio_wait_ring_space(void)
{
    uint32_t wait_start = game_nes_now_ms();

    while (g_audio_ring_count >= NES_AUDIO_RING_DEPTH) {
        lisa_thread_mdelay(1);
        if (game_nes_now_ms() - wait_start > 100U) {
            taskENTER_CRITICAL();
            g_audio_ring_rd = (g_audio_ring_rd + 1U) % NES_AUDIO_RING_DEPTH;
            g_audio_ring_count--;
            g_audio_ring_drop_count++;
            taskEXIT_CRITICAL();
            break;
        }
    }
}

static int16_t *game_nes_audio_next_ring_slot(void)
{
    uint32_t slot;

    game_nes_audio_wait_ring_space();

    taskENTER_CRITICAL();
    slot = g_audio_ring_wr;
    taskEXIT_CRITICAL();

    return g_audio_ring + (slot * NES_AUDIO_FRAME_SAMPLES);
}

static void game_nes_audio_commit_ring_slot(void)
{
    taskENTER_CRITICAL();
    g_audio_ring_wr = (g_audio_ring_wr + 1U) % NES_AUDIO_RING_DEPTH;
    g_audio_ring_count++;
    taskEXIT_CRITICAL();
}

static int game_nes_sound_output_pcm(const void *buffer, size_t len, bool input_s16)
{
    uint32_t t0 = game_nes_now_ms();

    if (!g_audio_dev || !g_audio_pcm16 || !g_audio_ring || !buffer || len == 0) {
        return 0;
    }

    uint32_t out_samples = NES_AUDIO_FRAME_SAMPLES;
    if (out_samples > NES_AUDIO_TMP_MAX_SAMPLES) {
        out_samples = NES_AUDIO_TMP_MAX_SAMPLES;
    }

#if CONFIG_NES_AUDIO_FAST_S16_COPY
    if (input_s16 && len == out_samples) {
        int16_t *dst = game_nes_audio_next_ring_slot();
        memcpy(dst, buffer, out_samples * sizeof(int16_t));
        game_nes_audio_commit_ring_slot();
#if CONFIG_NES_PORT_PROFILE_ENABLE
        g_prof_audio_ms_acc += (game_nes_now_ms() - t0);
        g_prof_audio_calls++;
#endif
        return 0;
    }
#endif

    if (len < 2U) {
        int16_t sample = 0;
        if (len == 1U) {
            sample = input_s16 ? ((const int16_t *)buffer)[0] :
                     (int16_t)(((int32_t)((const uint8_t *)buffer)[0] - 128) << 8);
        }
        for (uint32_t i = 0; i < out_samples; i++) {
            g_audio_lp_state += (((int32_t)sample - g_audio_lp_state) >> NES_AUDIO_LP_SHIFT);
            int32_t hp = g_audio_lp_state - g_audio_dc_x_prev + g_audio_dc_y_prev - (g_audio_dc_y_prev >> 7);
            g_audio_dc_x_prev = g_audio_lp_state;
            g_audio_dc_y_prev = hp;
            if (hp < NES_AUDIO_NOISE_GATE && hp > -NES_AUDIO_NOISE_GATE) {
                hp = 0;
            }
            g_audio_pcm16[i] = game_nes_clip_s16(hp);
        }
    } else {
        uint32_t step_q16 = (uint32_t)(((uint64_t)(len - 1U) << 16) / (uint64_t)(out_samples - 1U));
        uint32_t pos_q16 = 0;

        for (uint32_t out_idx = 0; out_idx < out_samples; out_idx++) {
            uint32_t in_idx = pos_q16 >> 16;
            uint32_t frac = pos_q16 & 0xFFFFU;
            uint32_t next_idx = (in_idx + 1U < len) ? (in_idx + 1U) : in_idx;

            int32_t s0;
            int32_t s1;
            if (input_s16) {
                s0 = ((const int16_t *)buffer)[in_idx];
                s1 = ((const int16_t *)buffer)[next_idx];
            } else {
                s0 = ((int32_t)((const uint8_t *)buffer)[in_idx] - 128) << 8;
                s1 = ((int32_t)((const uint8_t *)buffer)[next_idx] - 128) << 8;
            }
            /* Linear interpolation: reduce frac to Q8 to keep multiply in 32-bit.
             * (s1-s0) range: [-65535, 65535], frac8: [0, 255]
             * product max: 65535*255 = 16,711,425 — fits int32_t. */
            int32_t frac8 = (int32_t)(frac >> 8);
            int32_t sample = s0 + (((s1 - s0) * frac8) >> 8);
            g_audio_lp_state += ((sample - g_audio_lp_state) >> NES_AUDIO_LP_SHIFT);
            /* DC blocking filter: y[n] = x[n] - x[n-1] + alpha*y[n-1]
             * alpha ≈ 0.99 ≈ 32440/32768 ≈ (1 - 1/128)
             * Use shift approximation: y * (1 - 1/128) = y - (y >> 7) */
            int32_t hp = g_audio_lp_state - g_audio_dc_x_prev + g_audio_dc_y_prev - (g_audio_dc_y_prev >> 7);
            g_audio_dc_x_prev = g_audio_lp_state;
            g_audio_dc_y_prev = hp;
            if (hp < NES_AUDIO_NOISE_GATE && hp > -NES_AUDIO_NOISE_GATE) {
                hp = 0;
            }
            g_audio_pcm16[out_idx] = game_nes_clip_s16(hp);

            pos_q16 += step_q16;
        }
    }

    int16_t *dst = game_nes_audio_next_ring_slot();
    for (uint32_t i = 0; i < out_samples; i++) {
        dst[i] = g_audio_pcm16[i];
    }
    game_nes_audio_commit_ring_slot();

    int ret = 0;
#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_audio_ms_acc += (game_nes_now_ms() - t0);
    g_prof_audio_calls++;
#endif
    return ret;
}

int nes_sound_output_s16(const int16_t *buffer, size_t len)
{
    return game_nes_sound_output_pcm(buffer, len, true);
}

int nes_sound_output(uint8_t *buffer, size_t len)
{
    return game_nes_sound_output_pcm(buffer, len, false);
}

static int game_nes_audio_init(void)
{
    g_audio_dev = lisa_device_get(AUDIO_DEVICE);
    if (!g_audio_dev) {
        LISA_LOGW(LOG_TAG, "Audio device not found, run without sound");
        return 0;
    }

    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = NES_AUDIO_OUT_RATE,
            .channels = NES_AUDIO_OUT_CHANNELS,
            .sample_bits = NES_AUDIO_OUT_BITS,
        },
        .gain = {
            .analog_gain = -12,
            .digital_gain = -6,
        },
        .buffer_count = NES_AUDIO_OUT_BUF_COUNT,
        .buffer_samples = NES_AUDIO_OUT_BUF_SAMPLES,
    };

    int ret = lisa_audio_play_config(g_audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Audio play config failed: %d", ret);
        g_audio_dev = NULL;
        return 0;
    }

    g_audio_pcm16 = (int16_t *)exram_malloc(4, sizeof(int16_t) * NES_AUDIO_TMP_MAX_SAMPLES);
    if (!g_audio_pcm16) {
        LISA_LOGE(LOG_TAG, "Audio buffer alloc failed");
        g_audio_dev = NULL;
        return 0;
    }

    g_audio_ring = (int16_t *)exram_malloc(4, sizeof(int16_t) * NES_AUDIO_FRAME_SAMPLES * NES_AUDIO_RING_DEPTH);
    if (!g_audio_ring) {
        LISA_LOGE(LOG_TAG, "Audio ring alloc failed");
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
        g_audio_dev = NULL;
        return 0;
    }
    g_audio_ring_wr = 0;
    g_audio_ring_rd = 0;
    g_audio_ring_count = 0;
    g_audio_lp_state = 0;
    g_audio_dc_x_prev = 0;
    g_audio_dc_y_prev = 0;

    ret = lisa_audio_play_start(g_audio_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Audio play start failed: %d", ret);
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
        g_audio_dev = NULL;
        return 0;
    }

    g_audio_task_running = 1;
    if (xTaskCreate(game_nes_audio_task, "nes_audio", 2048, NULL, configMAX_PRIORITIES - 3, &g_audio_task) != pdPASS) {
        LISA_LOGE(LOG_TAG, "Audio task create failed");
        g_audio_task_running = 0;
        lisa_audio_play_stop(g_audio_dev);
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
        g_audio_dev = NULL;
        return 0;
    }

    LISA_LOGI(LOG_TAG, "Audio playback started (%dHz)", (int)NES_AUDIO_OUT_RATE);
    return 0;
}

static void game_nes_audio_deinit(void)
{
    g_audio_task_running = 0;
    if (g_audio_task) {
        vTaskDelete(g_audio_task);
        g_audio_task = NULL;
    }

    if (g_audio_dev) {
        lisa_audio_play_flush(g_audio_dev);
        lisa_audio_play_stop(g_audio_dev);
    }
    if (g_audio_ring) {
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
    }
    if (g_audio_pcm16) {
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
    }
    g_audio_dev = NULL;
}
#endif

int nes_initex(nes_t *nes)
{
    if (!nes || !nes->nes_draw_data) {
        return -1;
    }

#if !NES_DIRECT_LCD
    lv_init();
#endif

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {
            .spi_4wire = {
                .spi_dev = lisa_device_get("spi1"),
                .cs_gpio = gpiob_dev,
                .cs_pin = LCD_CS_PIN,
                .dc_gpio = gpiob_dev,
                .dc_pin = LCD_CD_PIN,
                .spi_freq = 50 * 1000 * 1000,
            }
        },
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .channel = 0,
                .dev = lisa_device_get("pwm0"),
                .freq = 2000,
            }
        },
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };

    g_display = lisa_device_get("display");
    if (!g_display) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    lisa_display_attach_bus(g_display, &display_config);
#if !NES_DIRECT_LCD
    lv_port_disp_init(g_display);
#endif

#if NES_DIRECT_LCD
    g_direct_lcd_stripe_count =
        (uint16_t)((NES_VISIBLE_HEIGHT + NES_DIRECT_LCD_STRIPE_LINES - 1U) / NES_DIRECT_LCD_STRIPE_LINES);
    g_direct_lcd_hashes =
        (uint64_t *)exram_malloc(8, sizeof(uint64_t) * g_direct_lcd_stripe_count);
    if (!g_direct_lcd_hashes) {
        LISA_LOGE(LOG_TAG, "DIRECT_LCD: stripe hash alloc failed");
        return -1;
    }
    nes_memset(g_direct_lcd_hashes, 0, sizeof(uint64_t) * g_direct_lcd_stripe_count);
    g_direct_lcd_hash_valid = false;
#endif

#if !NES_LV_ZERO_COPY
    g_crop_buffer_raw = exram_malloc(64, NES_CROP_BUF_BYTES + NES_CROP_BUF_GUARD);
    if (!g_crop_buffer_raw) {
        LISA_LOGE(LOG_TAG, "crop buffer alloc failed");
        return -1;
    }
    g_crop_buffer = (nes_color_t *)((uint8_t *)g_crop_buffer_raw + NES_CROP_BUF_GUARD);
    nes_memset(g_crop_buffer, 0, NES_CROP_BUF_BYTES);
#endif

#if !NES_DIRECT_LCD
    lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);
    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(touch_dev) || !lisa_device_ready(i2c_dev)) {
        LISA_LOGE(LOG_TAG, "touch/i2c device not ready");
        return -1;
    }

    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {
            .i2c = {
                .i2c_dev = i2c_dev,
                .int_gpio = gpioa_dev,
                .int_pin = LISA_TOUCH_I2C_INT_PIN,
                .rst_gpio = gpioa_dev,
                .rst_pin = LISA_TOUCH_I2C_RST_PIN,
            }
        }
    };

    int touch_ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (touch_ret != 0) {
        LISA_LOGE(LOG_TAG, "touch bus attach failed: %d", touch_ret);
        return -1;
    }
    lv_port_indev_init(touch_dev);
    game_nes_ui_init(nes->nes_draw_data);
#endif

    lisa_display_capabilities_t caps;
    lisa_display_get_capabilities(g_display, &caps);
    if (caps.height > NES_VISIBLE_HEIGHT) {
        g_display_y_offset = (uint16_t)((caps.height - NES_VISIBLE_HEIGHT) / 2U);
    } else {
        g_display_y_offset = 0;
    }

    lisa_display_blanking_off(g_display);
    LISA_LOGI(LOG_TAG, "display: %ux%u, draw region: %ux%u@(%u,%u)",
              caps.width,
              caps.height,
              NES_VISIBLE_WIDTH,
              NES_VISIBLE_HEIGHT,
              0U,
              g_display_y_offset);

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_frames = 0;
    g_prof_draw_frames = 0;
    g_prof_last_ms = 0;
    g_prof_draw_ms_acc = 0;
    g_prof_lv_ms_acc = 0;
    g_prof_audio_ms_acc = 0;
    g_prof_audio_calls = 0;
#endif

#if (NES_ENABLE_SOUND == 1)
    if (game_nes_audio_init() != 0) {
        return -1;
    }
#endif

    return 0;
}

int nes_deinitex(nes_t *nes)
{
    (void)nes;

#if (NES_ENABLE_SOUND == 1)
    game_nes_audio_deinit();
#endif

#if !NES_LV_ZERO_COPY
    if (g_crop_buffer_raw) {
        exram_free(g_crop_buffer_raw);
        g_crop_buffer_raw = NULL;
        g_crop_buffer = NULL;
    }
#endif

#if NES_DIRECT_LCD
    if (g_direct_lcd_hashes) {
        exram_free(g_direct_lcd_hashes);
        g_direct_lcd_hashes = NULL;
    }
    g_direct_lcd_stripe_count = 0;
    g_direct_lcd_hash_valid = false;
#endif

    return 0;
}

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *color_data)
{
#if NES_DIRECT_LCD
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

    uint32_t t0 = game_nes_now_ms();
    if (!g_display || !color_data) {
        return -1;
    }
    g_frame_invalidate_counter++;
    if (g_frame_invalidate_counter < NES_LVGL_FRAME_DIV) {
#if CONFIG_NES_PORT_PROFILE_ENABLE
        g_prof_draw_ms_acc += (game_nes_now_ms() - t0);
#endif
        return 0;
    }
    g_frame_invalidate_counter = 0;

    int wret = game_nes_direct_lcd_draw_diff(color_data);
    if (wret != 0) {
        LISA_LOGW(LOG_TAG, "DIRECT_LCD: display_write failed: %d", wret);
        g_direct_lcd_hash_valid = false;
    }

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_draw_frames++;
    g_prof_draw_ms_acc += (game_nes_now_ms() - t0);
#endif
    return 0;
#else
    uint32_t t0 = game_nes_now_ms();

    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

#if NES_LV_ZERO_COPY
    if (!g_display || !color_data || !g_nes_frame_img) {
        return -1;
    }
#else
    if (!g_display || !color_data || !g_crop_buffer || !g_nes_frame_img) {
        return -1;
    }
#endif

    g_frame_invalidate_counter++;
    if (g_frame_invalidate_counter < NES_LVGL_FRAME_DIV) {
#if CONFIG_NES_PORT_PROFILE_ENABLE
        g_prof_draw_ms_acc += (game_nes_now_ms() - t0);
#endif
        return 0;
    }
    g_frame_invalidate_counter = 0;

#if !NES_LV_ZERO_COPY
    for (uint16_t row = 0; row < NES_VISIBLE_HEIGHT; row++) {
        nes_color_t *dst = &g_crop_buffer[row * NES_VISIBLE_WIDTH];
        const nes_color_t *src = &color_data[row * NES_WIDTH + NES_VISIBLE_X_OFFSET];
        memcpy(dst, src, NES_VISIBLE_WIDTH * sizeof(nes_color_t));
    }
#endif

    lv_obj_invalidate(g_nes_frame_img);

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_draw_frames++;
    g_prof_draw_ms_acc += (game_nes_now_ms() - t0);
#endif
    return 0;
#endif
}

void __attribute__((weak)) game_nes_poll_joypad(nes_t *nes)
{
    (void)nes;
}

void nes_frame(nes_t *nes)
{
    uint32_t t0 = game_nes_now_ms();

    game_nes_poll_joypad(nes);
    uint32_t t1 = t0;
#if !NES_DIRECT_LCD
    uint32_t task_div = (game_nes_get_joypad_state() != 0U) ? NES_LVGL_TASK_DIV_ACTIVE : NES_LVGL_TASK_DIV_IDLE;
    g_lv_task_counter++;
    if (g_lv_task_counter >= task_div) {
        g_lv_task_counter = 0;
        (void)lv_task_handler();
        if (game_nes_consume_rom_list_request()) {
            nes->nes_quit = 1;
        }
        t1 = game_nes_now_ms();
    }
#endif
#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_lv_ms_acc += (t1 - t0);
#endif

#if (NES_ENABLE_SOUND == 1)
    if (!g_audio_dev)
#endif
    {
        /* Fallback frame pacing when audio is inactive */
        static uint32_t pace_base_ms = 0;
        static uint32_t pace_frame_no = 0;
        if (pace_base_ms == 0U) {
            pace_base_ms = game_nes_now_ms();
        }
        pace_frame_no++;
        uint32_t target_ms = pace_base_ms + (uint32_t)((uint64_t)pace_frame_no * 1000ULL / 60ULL);
        uint32_t now = game_nes_now_ms();
        if (now < target_ms) {
            lisa_thread_mdelay(target_ms - now);
        } else if (now - target_ms > 1000U) {
            pace_base_ms = now;
            pace_frame_no = 0;
        }
    }

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_frames++;
    uint32_t prof_now = game_nes_now_ms();
    if (g_prof_last_ms == 0U) {
        g_prof_last_ms = prof_now;
#if (NES_ENABLE_SOUND == 1)
        g_audio_ring_drop_count = 0;
        g_audio_ring_empty_count = 0;
#endif
    }
    uint32_t prof_elapsed_ms = prof_now - g_prof_last_ms;
    if (g_prof_frames >= 120U && prof_elapsed_ms > 0U) {
        uint32_t logic_fps_x10 = (uint32_t)(((uint64_t)g_prof_frames * 10000ULL +
                                             (prof_elapsed_ms / 2U)) /
                                            prof_elapsed_ms);
        uint32_t draw_fps_x10 = (uint32_t)(((uint64_t)g_prof_draw_frames * 10000ULL +
                                            (prof_elapsed_ms / 2U)) /
                                           prof_elapsed_ms);
#if (NES_ENABLE_SOUND == 1)
        uint32_t audio_avg = (g_prof_audio_calls > 0U) ? (g_prof_audio_ms_acc / g_prof_audio_calls) : 0U;
#endif
#if NES_DIRECT_LCD
        uint32_t diff_stripes_avg = g_direct_lcd_changed_stripes_acc / g_prof_frames;
        uint32_t diff_runs_avg = g_direct_lcd_runs_acc / g_prof_frames;
#endif
#if (NES_ENABLE_SOUND == 1)
#if NES_DIRECT_LCD
        LISA_LOGI(LOG_TAG,
                  "perf[120f]: logic_fps=%lu.%lu draw_fps=%lu.%lu draw_avg=%lums lv_avg=%lums audio_avg=%lums audio_calls=%lu drop=%lu empty=%lu lcd_stripes=%lu lcd_runs=%lu",
                  (unsigned long)(logic_fps_x10 / 10U),
                  (unsigned long)(logic_fps_x10 % 10U),
                  (unsigned long)(draw_fps_x10 / 10U),
                  (unsigned long)(draw_fps_x10 % 10U),
                  (unsigned long)(g_prof_draw_ms_acc / g_prof_frames),
                  (unsigned long)(g_prof_lv_ms_acc / g_prof_frames),
                  (unsigned long)audio_avg,
                  (unsigned long)g_prof_audio_calls,
                  (unsigned long)g_audio_ring_drop_count,
                  (unsigned long)g_audio_ring_empty_count,
                  (unsigned long)diff_stripes_avg,
                  (unsigned long)diff_runs_avg);
#else
        LISA_LOGI(LOG_TAG,
                  "perf[120f]: logic_fps=%lu.%lu draw_fps=%lu.%lu draw_avg=%lums lv_avg=%lums audio_avg=%lums audio_calls=%lu drop=%lu empty=%lu",
                  (unsigned long)(logic_fps_x10 / 10U),
                  (unsigned long)(logic_fps_x10 % 10U),
                  (unsigned long)(draw_fps_x10 / 10U),
                  (unsigned long)(draw_fps_x10 % 10U),
                  (unsigned long)(g_prof_draw_ms_acc / g_prof_frames),
                  (unsigned long)(g_prof_lv_ms_acc / g_prof_frames),
                  (unsigned long)audio_avg,
                  (unsigned long)g_prof_audio_calls,
                  (unsigned long)g_audio_ring_drop_count,
                  (unsigned long)g_audio_ring_empty_count);
#endif
#else
#if NES_DIRECT_LCD
        LISA_LOGI(LOG_TAG,
                  "perf[120f]: logic_fps=%lu.%lu draw_fps=%lu.%lu draw_avg=%lums lv_avg=%lums lcd_stripes=%lu lcd_runs=%lu",
                  (unsigned long)(logic_fps_x10 / 10U),
                  (unsigned long)(logic_fps_x10 % 10U),
                  (unsigned long)(draw_fps_x10 / 10U),
                  (unsigned long)(draw_fps_x10 % 10U),
                  (unsigned long)(g_prof_draw_ms_acc / g_prof_frames),
                  (unsigned long)(g_prof_lv_ms_acc / g_prof_frames),
                  (unsigned long)diff_stripes_avg,
                  (unsigned long)diff_runs_avg);
#else
        LISA_LOGI(LOG_TAG,
                  "perf[120f]: logic_fps=%lu.%lu draw_fps=%lu.%lu draw_avg=%lums lv_avg=%lums",
                  (unsigned long)(logic_fps_x10 / 10U),
                  (unsigned long)(logic_fps_x10 % 10U),
                  (unsigned long)(draw_fps_x10 / 10U),
                  (unsigned long)(draw_fps_x10 % 10U),
                  (unsigned long)(g_prof_draw_ms_acc / g_prof_frames),
                  (unsigned long)(g_prof_lv_ms_acc / g_prof_frames));
#endif
#endif

        g_prof_frames = 0;
        g_prof_draw_frames = 0;
        g_prof_last_ms = prof_now;
        g_prof_draw_ms_acc = 0;
        g_prof_lv_ms_acc = 0;
        g_prof_audio_ms_acc = 0;
        g_prof_audio_calls = 0;
#if (NES_ENABLE_SOUND == 1)
        g_audio_ring_drop_count = 0;
        g_audio_ring_empty_count = 0;
#endif
#if NES_DIRECT_LCD
        g_direct_lcd_changed_stripes_acc = 0;
        g_direct_lcd_runs_acc = 0;
#endif
    }
#endif
}
