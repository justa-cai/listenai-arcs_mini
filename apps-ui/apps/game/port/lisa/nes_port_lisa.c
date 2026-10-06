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
#include "semphr.h"

#include "game_nes_input.h"
#include "gamepad.h"
#include "lisa_audio.h"
#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_log.h"
#include "lisa_player_adapter.h" /* audio0 整机共享播放契约 (16kHz) + 幂等打开 */
#include "pa_manager.h"      /* SDK 功放使能: NES 直接写 audio0, 必须自己持有 PA 引用 */
#include "voice_player_comm.h"  /* voice_audio_owner_yield / voice_audio_in_use: audio0 让出仲裁 */
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "heap_private.h"
#include "sysheap.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "nes_port_lisa"

#define AUDIO_DEVICE            "audio0"

#ifndef CONFIG_NES_PORT_PROFILE_ENABLE
#define CONFIG_NES_PORT_PROFILE_ENABLE 1
#endif
#ifndef CONFIG_NES_AUDIO_FAST_S16_COPY
/* 关闭 s16 直拷: APU 的 s16 混音 (nes_apu_mix_sample) 输出是**单极性 0..8192**
 * (静音=0, 满幅=8192), 不是居中音频。原直拷 (memcpy) 会把这份带巨大直流、
 * 幅度仅 ~12% 满量程的信号直接送 DAC -> 扬声器基本听不到, 就是「游戏没声音」。
 * 现在统一走下面的高通去直流 + 增益归一化路径。 */
#define CONFIG_NES_AUDIO_FAST_S16_COPY 0
#endif
/* s16 混音幅值 (0..8192) 需放大才接近满量程。×8 在响的段落会削顶 -> 破音,
 * 降到 ×4 以保留约 12dB 余量, 再由下面的软限幅兜底。 */
#define NES_AUDIO_S16_GAIN_SHIFT 2

#define NES_VISIBLE_X_OFFSET    8
#define NES_VISIBLE_WIDTH       240
#define NES_VISIBLE_HEIGHT      240
#define NES_LVGL_FRAME_DIV      1U
#define NES_LVGL_TASK_DIV_ACTIVE 2U
#define NES_LVGL_TASK_DIV_IDLE   4U
#define NES_DIRECT_LCD_STRIPE_LINES 8U
#define NES_DIRECT_LCD_MERGE_GAP_STRIPES 1U


static lisa_device_t *g_display = NULL;
#if !NES_LV_ZERO_COPY
static void *g_crop_buffer_raw = NULL;
static nes_color_t *g_crop_buffer = NULL;
#endif
static lv_img_dsc_t g_nes_frame_dsc;
static lv_obj_t *g_nes_frame_img = NULL;
static volatile uint16_t g_ext_joypad_state = 0; /* TODO: BLE HID Host (HOGPRH) 写入 */
static volatile bool g_rom_list_requested = false;
static uint16_t g_display_y_offset = 0;
static uint32_t g_frame_invalidate_counter = 0;
static uint32_t g_lv_task_counter = 0;

/* 双缓冲: NES 任务写 back, workq 线程 swap 后渲染 front (零拷贝 + 无 lv_* 竞争) */
static nes_color_t *g_frame_buf[2] = {NULL, NULL};
static volatile uint8_t g_frame_front = 0;
static volatile bool g_frame_pending = false;
static SemaphoreHandle_t g_frame_lock = NULL;
static volatile uint32_t g_logic_fps = 0;
static uint32_t g_fps_frames = 0;
static uint32_t g_fps_last_ms = 0;

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
static volatile bool g_audio_pa_ref_held = false;       /* 是否已持有 PA 引用 */

/* ---- 让出 audio0 给语音助手 (实现 src/category 的 weak 钩子) ----
 * 采样率已全机统一为 16kHz (见 lisa_player_adapter.h 的共享契约), 所以让出
 * 不再需要停流/改采样率: 播放流保持运行, 停掉"喂数据"就是静音
 * (底层在 DMA 队列空时会自动用静音 buffer 补位)。 */
static volatile bool g_audio_yield_req = false;   /* 语音侧已请求让出 */
static volatile bool g_audio_yielded = false;     /* 已让出 (游戏静音中) */
/* 刚唤醒时会话/提示音还没起来, voice_audio_in_use() 会短暂为假;
 * 这段宽限期内不判定"语音已放手", 避免刚让出就把声音抢回来。 */
#define NES_AUDIO_YIELD_GRACE_MS 1500U
/* 兜底: 语音侧状态万一卡住 (历史故障: 提示音写不进已停的 audio0 -> 播放线程阻塞
 * -> 播放器永远停在 PLAYING), 不能让它把游戏静音拖到天荒地老。
 * 超过这个时长就强制收回并告警。 */
#define NES_AUDIO_YIELD_MAX_MS   90000U

/* 输出采样率 16kHz: 与语音侧共用同一条播放流 (预编译 lisa_player track 内置 16k),
 * APU 仍按 48kHz 生成, 由本端口的重采样器降到 16k, 避免为游戏单独霸占 audio0。
 * 通道/位深/DMA 粒度由 lisa_player_adapter.h 的共享契约统一规定, 这里不再重复定义
 * (采样率也必须与 APP_AUDIO0_SAMPLE_RATE 一致)。 */
#define NES_AUDIO_OUT_RATE          LISA_AUDIO_RATE_16K
_Static_assert(NES_AUDIO_OUT_RATE == APP_AUDIO0_SAMPLE_RATE,
               "NES output rate must match the shared audio0 contract");
/* 每帧输出样本数 = 16000/60 = 266 */
#define NES_AUDIO_FRAME_SAMPLES     ((uint32_t)NES_AUDIO_OUT_RATE / 60U)
/* 混合/重采样缓冲按"每帧输出样本数"给, 必须 >= NES_AUDIO_FRAME_SAMPLES */
#define NES_AUDIO_TMP_MAX_SAMPLES   NES_AUDIO_FRAME_SAMPLES
#define NES_AUDIO_RING_DEPTH        8U
/* 一阶低通的 pole: y += (x-y) >> SHIFT。滤波在**输出采样率**上运行, 所以同一个
 * SHIFT 在不同采样率下截止频率差 3 倍 —— 输出从 48k 改到 16k 时同步把 3 调成 2,
 * 让截止频率与之前接近 (原 ~950Hz @48k, 现 ~640Hz @16k), 避免听感突然变闷。
 * 这个值影响音色, 需要试听后微调。 */
#define NES_AUDIO_LP_SHIFT          2
#define NES_AUDIO_NOISE_GATE        96
#endif

#if (NES_ENABLE_SOUND == 1)
/* 播放流是整机共享的 (见 lisa_player_adapter.h 契约), DMA 粒度固定
 * APP_AUDIO0_DMA_SAMPLES; 每帧 266 样本先在这里累积到它的整数倍再写,
 * 否则不足一个 DMA buffer 的写入会被底层补静音 -> 声音断续。
 * 容量 = 一个整批 (2×256) + 一帧余量; 放 PSRAM (本任务栈只有 2KB)。 */
static int16_t *g_audio_dma_buf = NULL;
static uint32_t g_audio_dma_len = 0;
#define NES_AUDIO_DMA_BUF_SAMPLES ((APP_AUDIO0_DMA_SAMPLES * 2U) + NES_AUDIO_FRAME_SAMPLES)
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

/* 软限幅: |x| 超过 knee 后按二次曲线压到满量程, 避免硬削顶造成的破音。
 * knee = 24576 (~0.75 FS), 峰值被平滑压向 32767 而不产生方波边沿。 */
#define NES_AUDIO_SOFT_KNEE   24576
static inline int16_t game_nes_soft_limit(int32_t x)
{
    int32_t sign = (x < 0) ? -1 : 1;
    int32_t a = (x < 0) ? -x : x;
    const int32_t range = 32767 - NES_AUDIO_SOFT_KNEE;   /* 8191 */
    if (a > NES_AUDIO_SOFT_KNEE) {
        int32_t t = (a - NES_AUDIO_SOFT_KNEE);           /* 0.. */
        if (t > range * 4) {
            t = range * 4;                               /* 防溢出: 极端输入直接饱和 */
        }
        /* y = knee + range * (2 - (1 - t/range)^2) 的离散近似 (二次软膝) */
        int32_t frac_q8 = (t * 256) / (range > 0 ? range : 1);
        if (frac_q8 > 256) {
            frac_q8 = 256;
        }
        int32_t comp = (frac_q8 * (512 - frac_q8)) >> 8;  /* 0..256, 软膝曲线 */
        a = NES_AUDIO_SOFT_KNEE + ((range * comp) >> 8);
        if (a > 32767) {
            a = 32767;
        }
    }
    return (int16_t)(sign * a);
}

uint16_t game_nes_get_joypad_state(void)
{
    /* 数据源: gamepad 中间件 (WebSocket 手柄 / 后续 BLE HID / 触摸手柄) */
    return gamepad_get_joypad_mask();
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

/* arcs-mini 内部 SRAM 堆放不下 nes_t (约 10~14KB), 统一走 PSRAM */
void *nes_sram_malloc(int num)
{
    return exram_malloc(4, (size_t)num);
}

void nes_sram_free(void *address)
{
    exram_free(address);
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

/* ------------------------------------------------------------------ */
/* audio0 让出 / 收回                                                   */
/* ------------------------------------------------------------------ */

/* 复位音频环 + 输出累积 (让出期间核心仍在写? 不会 —— 让出时 g_audio_dev 为 NULL,
 * game_nes_sound_output_pcm 直接早退; 但恢复前清一次更保险) */
static void game_nes_audio_reset_ring(void)
{
    taskENTER_CRITICAL();
    g_audio_ring_wr = 0;
    g_audio_ring_rd = 0;
    g_audio_ring_count = 0;
    taskEXIT_CRITICAL();
    g_audio_dma_len = 0;
    g_audio_lp_state = 0;
    g_audio_dc_x_prev = 0;
    g_audio_dc_y_prev = 0;
}

/* 确保共享播放流 (16kHz) 在跑。
 * 不自己配置: 采样率与 DMA 粒度由 lisa_player_adapter.h 的共享契约统一定义,
 * 谁先来谁配置, 后来者直接写 —— 这样任何一方都不需要停流/改采样率,
 * 也就没有"对方停掉 audio0 导致自己写不进去"的整类问题。 */
static int game_nes_audio_open_stream(void)
{
    if (app_audio0_ensure_play() != 0) {
        LISA_LOGE(LOG_TAG, "audio0 ensure(%uHz) failed", (unsigned)NES_AUDIO_OUT_RATE);
        return -1;
    }
    return 0;
}

/* 让出 (静音): 只停掉"喂数据", 不停流。
 * - 不再 play_stop: 播放流是共享的, 停掉会波及语音侧 (历史故障: 提示音写进
 *   已停的流 -> 渲染线程永久阻塞 -> 播放器卡在 PLAYING -> TTS 被焦点策略取消);
 * - 底层在 DMA 队列空时会自动用静音 buffer 补位, 所以不写 = 静音;
 * - g_audio_dev 置 NULL 让模拟器切到时间兜底节拍 (正常 60fps 节拍来自音频环的
 *   DMA 背压; 让出后没有背压, 不切的话模拟器会全速空转)。 */
static void game_nes_audio_hold(void)
{
    if (g_audio_pa_ref_held) {
        pa_manager_control(0, 0);   /* 语音侧自己按需开关功放 */
        g_audio_pa_ref_held = false;
    }
    g_audio_dev = NULL;
    g_audio_dma_len = 0;
    LISA_LOGI(LOG_TAG, "audio0 yielded to voice assistant (game audio muted)");
}

/* 收回: 重新以共享契约"确保在跑" + 重新持有 PA 引用 (采样率不变, 无需重配) */
static void game_nes_audio_retake(void)
{
    g_audio_dev = lisa_device_get(AUDIO_DEVICE);
    if (!g_audio_dev) {
        LISA_LOGW(LOG_TAG, "audio0 device unavailable on retake");
        return;
    }
    if (game_nes_audio_open_stream() != 0) {
        g_audio_dev = NULL;
        return;
    }
    game_nes_audio_reset_ring();

    pa_manager_control(1, 0);
    g_audio_pa_ref_held = true;
    LISA_LOGI(LOG_TAG, "audio0 reclaimed, game audio restored");
}

/* src/category 的 weak 钩子 (音频独占者提供 strong 实现)。
 * 唤醒词路径与手柄 L1 都会先调它, 请游戏把游戏声音静下来, 再开始语音交互。
 * 这里做有界等待, 确认音频任务已经停止喂数据。 */
void voice_audio_owner_yield(void)
{
    if (g_audio_yielded) {
        return;                     /* 已经在让出态 */
    }
    if (!g_audio_task_running) {
        return;                     /* 游戏音频没在跑 (未初始化 / 已 deinit) */
    }

    g_audio_yield_req = true;
    for (uint32_t i = 0; i < 40U && !g_audio_yielded; i++) {
        lisa_thread_mdelay(10);
    }
    if (!g_audio_yielded) {
        LISA_LOGW(LOG_TAG, "audio0 yield not confirmed within 400ms");
    }
}

static void game_nes_audio_task(void *arg)
{
    (void)arg;

    uint32_t last_check_ms = game_nes_now_ms();
    uint32_t hold_start_ms = 0;

    while (g_audio_task_running) {
        uint32_t now = game_nes_now_ms();

        if (!g_audio_dma_buf) {     /* 分配失败时不该起任务; 兜底防越界 */
            lisa_thread_mdelay(50);
            continue;
        }

        /* 让出态: 等语音侧不再用 audio0 (会话结束 / 提示音与 TTS 播完)。
         * 会话自然结束、L2 主动退出、断网等路径都会收敛到"没人用了",
         * 所以这里轮询判断比逐个订阅消息更不容易漏。 */
        if (g_audio_yield_req) {
            if (!g_audio_yielded) {
                game_nes_audio_hold();
                g_audio_yielded = true;
                hold_start_ms = now;
            } else if ((now - hold_start_ms) >= NES_AUDIO_YIELD_MAX_MS) {
                LISA_LOGW(LOG_TAG, "voice still holds audio0 after %us, force reclaim",
                          (unsigned)(NES_AUDIO_YIELD_MAX_MS / 1000U));
                g_audio_yield_req = false;      /* 下一轮收回 */
            } else if ((now - hold_start_ms) >= NES_AUDIO_YIELD_GRACE_MS &&
                       !voice_audio_in_use()) {
                g_audio_yield_req = false;      /* 下一轮收回 */
                LISA_LOGI(LOG_TAG, "voice released audio0, restoring game audio");
            }
            lisa_thread_mdelay(50);
            continue;
        }

        if (g_audio_yielded) {
            game_nes_audio_retake();
            g_audio_yielded = false;
            last_check_ms = game_nes_now_ms();
            continue;
        }

        /* 周期性健康检查 (每 1s, 限流): 共享播放流若被停掉/抢走 (状态非 RUNNING),
         * 幂等把它按共享契约拉回来。采样率全机统一, 所以这里不再"重配 48kHz",
         * 也不会改到语音侧正在用的参数。
         * 注意: 本任务优先级很高(-3), 任何分支都必须 sleep, 绝不能 busy-loop,
         * 否则会饿死 gp_disc/gp_ws/nes_game 及日志线程。 */
        if (g_audio_dev && (now - last_check_ms) >= 1000U) {
            last_check_ms = now;

            if (game_nes_audio_open_stream() != 0) {
                LISA_LOGW(LOG_TAG, "audio0 not running, ensure failed");
            }

            if (!g_audio_pa_ref_held) {     /* 兜底: 确认功放引用仍持有 */
                pa_manager_control(1, 0);
                g_audio_pa_ref_held = true;
            }
        }

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

        const int16_t *frame = g_audio_ring + (slot * NES_AUDIO_FRAME_SAMPLES);

        /* 累积到 DMA 粒度的整数倍再写 (见 g_audio_dma_buf 注释):
         * 每帧 266 样本, 直接写会让底层给每个不足 256 的 DMA buffer 补静音。 */
        memcpy(g_audio_dma_buf + g_audio_dma_len, frame,
               NES_AUDIO_FRAME_SAMPLES * sizeof(int16_t));
        g_audio_dma_len += NES_AUDIO_FRAME_SAMPLES;
        if (g_audio_dma_len < APP_AUDIO0_DMA_SAMPLES) {
            continue;   /* 还不够一个 DMA buffer, 继续攒 */
        }

        uint32_t out = (g_audio_dma_len / APP_AUDIO0_DMA_SAMPLES) * APP_AUDIO0_DMA_SAMPLES;
        if (lisa_audio_play_write(g_audio_dev, g_audio_dma_buf, out) <= 0) {
            /* 写入异常: 交给下一次健康检查恢复; 这里必须 sleep 再继续 */
            lisa_thread_mdelay(10);
        }
        g_audio_dma_len -= out;
        if (g_audio_dma_len > 0U) {
            memmove(g_audio_dma_buf, g_audio_dma_buf + out,
                    g_audio_dma_len * sizeof(int16_t));
        }
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
            /* 队列长时间满(播放端被拖住): 丢弃最旧帧, 由音频任务的周期健康检查兜底恢复 */
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
        int32_t v = g_audio_pcm16[i];
        if (input_s16) {
            v = (v << NES_AUDIO_S16_GAIN_SHIFT);   /* 归一化放大 */
            dst[i] = game_nes_soft_limit(v);       /* 软限幅防破音 */
        } else {
            dst[i] = (int16_t)v;
        }
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
    game_nes_audio_reset_ring();

    /* 写 DMA 对齐用 */
    g_audio_dma_buf = (int16_t *)exram_malloc(4, sizeof(int16_t) * NES_AUDIO_DMA_BUF_SAMPLES);
    if (!g_audio_dma_buf) {
        LISA_LOGE(LOG_TAG, "Audio dma buffer alloc failed");
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
        g_audio_dev = NULL;
        return 0;
    }

    /* 共享播放流: 已在跑就直接用 (通常是唤醒引擎开机建的 16k 流),
     * 没在跑才按共享契约建。不停流、不改采样率。 */
    if (game_nes_audio_open_stream() != 0) {
        exram_free(g_audio_dma_buf);
        g_audio_dma_buf = NULL;
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
        exram_free(g_audio_dma_buf);
        g_audio_dma_buf = NULL;
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
        g_audio_dev = NULL;
        return 0;
    }

    LISA_LOGI(LOG_TAG, "Audio playback started (%dHz, shared stream)", (int)NES_AUDIO_OUT_RATE);

    /* 关键: NES 绕过产品 app_player 直接写 audio0, 产品功放 (PA) 只由 app_player
     * 打开, 提示音播完即关 -> 游戏中功放常闭, 听不到任何声音。
     * 这里以引用计数方式常持 PA, 直到游戏音频停止。 */
    pa_manager_control(1, 0);
    g_audio_pa_ref_held = true;
    LISA_LOGI(LOG_TAG, "PA ref held for game audio");

    /* 语音会话正在占用 audio0 (例如游戏中途被唤醒): 别抢, 直接进入让出态,
     * 由音频任务在语音侧放手后收回。 */
    if (g_audio_yield_req) {
        LISA_LOGI(LOG_TAG, "voice owns audio0 during init, start in yielded state");
        g_audio_yielded = true;
        game_nes_audio_hold();
    }
    return 0;
}

static void game_nes_audio_deinit(void)
{
    g_audio_task_running = 0;
    if (g_audio_task) {
        vTaskDelete(g_audio_task);
        g_audio_task = NULL;
    }

    /* 让出态下 audio0 已经交回语音侧, 这里不能再碰它 */
    g_audio_yield_req = false;
    g_audio_yielded = false;

    if (g_audio_pa_ref_held) {
        pa_manager_control(0, 0);   /* 释放游戏持有的 PA 引用 */
        g_audio_pa_ref_held = false;
    }

    /* 播放流是整机共享的 (语音侧随时可能在用), 停它会把别人一起弄哑,
     * 所以这里只放弃自己的设备句柄, 不 stop/flush。 */
    g_audio_dev = NULL;
    g_audio_dma_len = 0;
    if (g_audio_ring) {
        exram_free(g_audio_ring);
        g_audio_ring = NULL;
    }
    if (g_audio_pcm16) {
        exram_free(g_audio_pcm16);
        g_audio_pcm16 = NULL;
    }
    if (g_audio_dma_buf) {
        exram_free(g_audio_dma_buf);
        g_audio_dma_buf = NULL;
    }
}
#endif

int nes_initex(nes_t *nes)
{
    if (!nes || !nes->nes_draw_data) {
        return -1;
    }

    /* 显示/LVGL 由 apps-ui 框架 (lisa_ui_lvgl_init) 初始化, 这里不重复。
     * arcs-mini ST7789P3 240x240: NES 可见区 240x240 全屏, 居中偏移恒 0。
     * 保留 capabilities 读取以便换屏时自适应。 */
    g_display = lisa_device_get("display");
    lisa_display_capabilities_t caps;
    if (g_display) {
        lisa_display_get_capabilities(g_display, &caps);
        g_display_y_offset =
            (caps.height > NES_VISIBLE_HEIGHT)
                ? (uint16_t)((caps.height - NES_VISIBLE_HEIGHT) / 2U)
                : 0U;
        LISA_LOGI(LOG_TAG, "display: %ux%u, draw region: %ux%u@(0,%u)",
                  caps.width, caps.height,
                  NES_VISIBLE_WIDTH, NES_VISIBLE_HEIGHT, g_display_y_offset);
    }

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_frames = 0;
    g_prof_draw_frames = 0;
    g_prof_last_ms = 0;
    g_prof_draw_ms_acc = 0;
    g_prof_lv_ms_acc = 0;
    g_prof_audio_ms_acc = 0;
    g_prof_audio_calls = 0;
#endif

    /* 双缓冲帧缓冲 (PSRAM) */
    if (!g_frame_buf[0]) {
        uint32_t buf_bytes = NES_WIDTH * NES_VISIBLE_HEIGHT * sizeof(nes_color_t);
        g_frame_buf[0] = (nes_color_t *)exram_malloc(4, buf_bytes);
        g_frame_buf[1] = (nes_color_t *)exram_malloc(4, buf_bytes);
        if (!g_frame_buf[0] || !g_frame_buf[1]) {
            LISA_LOGE(LOG_TAG, "frame buffers alloc failed");
            return -1;
        }
        memset(g_frame_buf[0], 0, buf_bytes);
        memset(g_frame_buf[1], 0, buf_bytes);
    }
    if (!g_frame_lock) {
        g_frame_lock = xSemaphoreCreateMutex();
    }
    g_frame_front = 0;
    g_frame_pending = false;
    g_logic_fps = 0;
    g_fps_frames = 0;
    g_fps_last_ms = game_nes_now_ms();

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

    if (g_frame_buf[0]) {
        exram_free(g_frame_buf[0]);
        g_frame_buf[0] = NULL;
    }
    if (g_frame_buf[1]) {
        exram_free(g_frame_buf[1]);
        g_frame_buf[1] = NULL;
    }

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
    /* 双缓冲: 写 back 缓冲 (无锁), workq 线程 swap 后渲染 front。
     * NES 任务不触碰任何 lv_* API, 与 LVGL 无线程竞争。 */
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

    if (!color_data || !g_frame_buf[0]) {
        return -1;
    }

    uint32_t t0 = game_nes_now_ms();

    nes_color_t *back = g_frame_buf[g_frame_front ^ 1U];
    memcpy(back, color_data, NES_WIDTH * NES_VISIBLE_HEIGHT * sizeof(nes_color_t));

    if (g_frame_lock && xSemaphoreTake(g_frame_lock, 0) == pdTRUE) {
        g_frame_pending = true;
        xSemaphoreGive(g_frame_lock);
    }

#if CONFIG_NES_PORT_PROFILE_ENABLE
    g_prof_draw_frames++;
    g_prof_draw_ms_acc += (game_nes_now_ms() - t0);
#endif
    return 0;
#endif
}

/* 每逻辑帧把手柄位图写入 NES 核心 (此前为空 weak 实现, 输入断链) */
void game_nes_poll_joypad(nes_t *nes)
{
    if (!nes) {
        return;
    }
    if (game_nes_consume_rom_list_request()) {
        nes->nes_quit = 1;
        return;
    }
    if (gamepad_consume_exit()) {
        nes->nes_quit = 1;
        return;
    }
    nes->nes_cpu.joypad.joypad = game_nes_get_joypad_state();
}

void nes_frame(nes_t *nes)
{
    uint32_t t0 = game_nes_now_ms();

    game_nes_poll_joypad(nes);

    /* LVGL 刷新由 workq 线程的 tick 驱动, 这里只做音频反压节拍与统计 */

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
/* fps 统计 */
    g_fps_frames++;
    uint32_t fps_now = game_nes_now_ms();
    if (fps_now - g_fps_last_ms >= 2000U) {
        g_logic_fps = g_fps_frames * 1000U / (fps_now - g_fps_last_ms);
        g_fps_frames = 0;
        g_fps_last_ms = fps_now;
    }
}

/* ------------------------------------------------------------------ */
/* nes_game_port 接口 (apps-ui game presenter 调用)                     */
/* ------------------------------------------------------------------ */
static TaskHandle_t g_nes_task = NULL;
static volatile bool g_nes_task_done = false;

static void nes_lisa_task_entry(void *arg)
{
    nes_t *nes = (nes_t *)arg;
    LISA_LOGI(LOG_TAG, "nes task start");
    nes_run(nes);
    g_nes_task_done = true;
    LISA_LOGI(LOG_TAG, "nes task exit");
    /* 不自删: TCB 保持有效, 由 nes_game_port_stop() 统一 vTaskDelete。
     * (自删后句柄失效, 外部再 vTaskDelete 会双重删除崩溃) */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int nes_game_port_start(nes_t *nes)
{
    g_nes_task_done = false;
    if (xTaskCreate(nes_lisa_task_entry, "nes_game", 4 * 1024, nes,
                    configMAX_PRIORITIES - 2, &g_nes_task) != pdPASS) {
        g_nes_task = NULL;
        return -1;
    }
    return 0;
}

void nes_game_port_stop(void)
{
    if (g_nes_task) {
        /* nes_run 每帧检查 nes_quit; 任务退出后挂起等待, 由这里统一删除 */
        uint32_t wait_ms = 0;
        while (!g_nes_task_done && wait_ms < 2000U) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10U;
        }
        vTaskDelete(g_nes_task); /* 任务挂起于 portMAX_DELAY, TCB 有效, 删除合法 */
        g_nes_task = NULL;
    }
}

void nes_game_port_attach(nes_t *nes, lv_obj_t *img)
{
    if (!nes || !nes->nes_draw_data || !img) {
        return;
    }

    memset(&g_nes_frame_dsc, 0, sizeof(g_nes_frame_dsc));
    g_nes_frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    g_nes_frame_dsc.header.w = NES_WIDTH;
    g_nes_frame_dsc.header.h = NES_VISIBLE_HEIGHT;
    g_nes_frame_dsc.data_size = NES_WIDTH * NES_VISIBLE_HEIGHT * sizeof(nes_color_t);
    g_nes_frame_dsc.data =
        (const uint8_t *)(g_frame_buf[0] ? g_frame_buf[g_frame_front] : nes->nes_draw_data);

    g_nes_frame_img = img;
    lv_img_set_src(g_nes_frame_img, &g_nes_frame_dsc);
    lv_obj_set_size(g_nes_frame_img, NES_VISIBLE_WIDTH, NES_VISIBLE_HEIGHT);
    lv_img_set_offset_x(g_nes_frame_img, -NES_VISIBLE_X_OFFSET);
    /* arcs-mini 240x240: NES 可见区全屏 */
    lv_obj_set_pos(g_nes_frame_img, 0, g_display_y_offset);
}

void nes_game_port_detach(void)
{
    g_nes_frame_img = NULL;
}

/* workq 线程 (presenter 的 LVGL timer) 调用: 交换双缓冲并标脏 */
void nes_game_port_tick(void)
{
    if (!g_frame_lock || !g_nes_frame_img) {
        return;
    }
    if (xSemaphoreTake(g_frame_lock, 0) != pdTRUE) {
        return;
    }
    if (g_frame_pending) {
        g_frame_front ^= 1U;
        g_frame_pending = false;
        g_nes_frame_dsc.data = (const uint8_t *)g_frame_buf[g_frame_front];
        lv_img_set_src(g_nes_frame_img, &g_nes_frame_dsc);
        lv_obj_invalidate(g_nes_frame_img);
    }
    xSemaphoreGive(g_frame_lock);
}

bool nes_game_port_is_done(void)
{
    return g_nes_task_done;
}

uint32_t nes_game_port_get_logic_fps(void)
{
    return g_logic_fps;
}
