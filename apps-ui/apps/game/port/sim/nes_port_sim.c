/*
 * NES 平台 port —— PC (Ubuntu x86 / SDL2) 实现。
 *
 * 复用 arcs-sdk/demos/arcs/game/nes 的核心, 实现 nes_default.h 声明的平台钩子。
 *
 * 线程模型(生产者-消费者, LVGL 所有权清晰):
 *   - 模拟线程(nes_game_port_start 创建): nes_run() 阻塞跑核心, nes_frame 钩子
 *     负责 60fps 节拍(x86 全速会失控, demo 在设备上靠音频 ring 反压, PC 的
 *     SDL queue 无反压), nes_draw 钩子把帧 memcpy 到后备缓冲。
 *   - LVGL 主线程: 独占全部 lv_* 调用。nes_game_port_tick() 由 presenter 的
 *     LVGL timer 周期调用: 交换前后缓冲 -> invalidate -> 读键盘更新手柄状态。
 *
 * 显示: NES 帧缓冲 RGB565 256x240, 裁掉左右各 8px 过扫描后 240x240,
 * 居中到 320x240 窗口(与 arcs-mini ST7789P3 旋转后分辨率一致)。
 * 音频: SDL queue, 48kHz / S16 / 单声道(APU 原生采样率, 免重采样)。
 * 输入: SDL 键盘 -> NES 1P 手柄, 键位与 SDK demo README 一致, Esc 退回。
 */
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "game_nes_input.h"
#include "nes.h"

#define NES_SIM_TAG "nes_sim"

/* 与 nes_conf.h 一致: 可见区 240x240, 左右各裁 8px */
#ifndef NES_VISIBLE_X_OFFSET
#define NES_VISIBLE_X_OFFSET 8
#endif
#ifndef NES_VISIBLE_WIDTH
#define NES_VISIBLE_WIDTH 240
#endif
#ifndef NES_VISIBLE_HEIGHT
#define NES_VISIBLE_HEIGHT 240
#endif

/* 320 宽窗口(与 arcs-mini 屏旋转后一致)下可见区水平居中 */
#ifndef NES_SIM_HOR_RES
#define NES_SIM_HOR_RES 320
#endif
#define NES_SIM_IMG_X ((NES_SIM_HOR_RES - NES_VISIBLE_WIDTH) / 2)

/* 音频: APU 原生 48kHz, 每帧 800 样本; 队列水位超 100ms 丢弃, 防延迟累积 */
#define NES_SIM_AUDIO_RATE 48000U
#define NES_SIM_AUDIO_FRAME_SAMPLES (NES_SIM_AUDIO_RATE / 60U)
#define NES_SIM_AUDIO_QUEUE_LIMIT (NES_SIM_AUDIO_RATE / 10U)

/* ------------------------------------------------------------------ */
/* 状态                                                                */
/* ------------------------------------------------------------------ */
static nes_t *g_nes = NULL;
static lv_obj_t *g_img = NULL;
static lv_img_dsc_t g_img_dsc;

/* 双缓冲: 模拟线程写 back, 主线程交换后渲染 front */
static nes_color_t g_frame_buf[2][NES_WIDTH * NES_HEIGHT];
static volatile uint8_t g_front_idx = 0;
static volatile bool g_frame_pending = false;
static SDL_mutex *g_frame_lock = NULL;

static SDL_Thread *g_nes_thread = NULL;
static volatile bool g_thread_done = false;
static volatile bool g_quit_requested = false;

static SDL_AudioDeviceID g_audio_dev = 0;

static volatile uint16_t g_joypad_state = 0;
static volatile uint16_t g_sticky_keys = 0; /* 短按锁存: 保证至少被一个游戏帧消费 */
static uint32_t g_logic_fps = 0;
static uint32_t g_frame_count = 0;
static const char *g_dump_frame_path = NULL;

static void nes_sim_dump_frame_ppm(const char *path, const nes_color_t *frame);

/* ------------------------------------------------------------------ */
/* 内存 / 时间 / 日志                                                  */
/* ------------------------------------------------------------------ */
void *nes_sram_malloc(int num)
{
    return calloc(1, (size_t)num);
}

void nes_sram_free(void *address)
{
    free(address);
}

uint32_t nes_get_ms(void)
{
    return SDL_GetTicks();
}

int nes_log_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("[%s] ", NES_SIM_TAG);
    int ret = vprintf(format, args);
    va_end(args);
    fflush(stdout);
    return ret;
}

/* ------------------------------------------------------------------ */
/* 显示: 双缓冲交换 (模拟线程写, 主线程渲染)                            */
/* ------------------------------------------------------------------ */
/* 键盘 -> NES 1P 手柄位掩码 (主线程 tick 调用, SDL 事件已由 lv_drivers 泵过) */
uint16_t nes_sim_read_keyboard_state(void)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint16_t s = 0;

    if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) {
        s |= GAME_NES_BTN_U1;
    }
    if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) {
        s |= GAME_NES_BTN_D1;
    }
    if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) {
        s |= GAME_NES_BTN_L1;
    }
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) {
        s |= GAME_NES_BTN_R1;
    }
    if (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_K] || k[SDL_SCANCODE_LALT]) {
        s |= GAME_NES_BTN_A1;
    }
    if (k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_J] || k[SDL_SCANCODE_LCTRL]) {
        s |= GAME_NES_BTN_B1;
    }
    if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) {
        s |= GAME_NES_BTN_ST1;
    }
    if (k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_TAB]) {
        s |= GAME_NES_BTN_SE1;
    }

    return s;
}

void nes_game_port_attach(nes_t *nes, lv_obj_t *img)
{
    g_nes = nes;
    g_img = img;

    g_frame_lock = SDL_CreateMutex();
    g_front_idx = 0;
    g_frame_pending = false;
    memset(g_frame_buf, 0, sizeof(g_frame_buf));

    if (!img || !nes || !nes->nes_draw_data) {
        return;
    }

    memset(&g_img_dsc, 0, sizeof(g_img_dsc));
    g_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    g_img_dsc.header.w = NES_WIDTH;
    g_img_dsc.header.h = NES_HEIGHT;
    g_img_dsc.data_size = NES_WIDTH * NES_HEIGHT * sizeof(nes_color_t);
    g_img_dsc.data = (const uint8_t *)g_frame_buf[g_front_idx];

    lv_img_set_src(img, &g_img_dsc);
    lv_obj_set_size(img, NES_VISIBLE_WIDTH, NES_VISIBLE_HEIGHT);
    lv_img_set_offset_x(img, -NES_VISIBLE_X_OFFSET);
    lv_obj_set_pos(img, NES_SIM_IMG_X, 0);
}

void nes_game_port_detach(void)
{
    g_img = NULL;
    g_nes = NULL;
    if (g_frame_lock) {
        SDL_DestroyMutex(g_frame_lock);
        g_frame_lock = NULL;
    }
}

/* 主线程 (LVGL timer 回调): 交换缓冲 + 标脏 + 采集键盘 */
void nes_game_port_tick(void)
{
    g_joypad_state = nes_sim_read_keyboard_state();
    g_sticky_keys |= g_joypad_state; /* 锁存本次采到的键, 游戏帧消费后清除 */
    if ((SDL_GetKeyboardState(NULL))[SDL_SCANCODE_ESCAPE]) {
        g_quit_requested = true;
    }

    if (g_frame_lock && SDL_LockMutex(g_frame_lock) == 0) {
        if (g_frame_pending) {
            g_front_idx ^= 1U;
            g_frame_pending = false;
            if (g_img) {
                g_img_dsc.data = (const uint8_t *)g_frame_buf[g_front_idx];
                lv_img_set_src(g_img, &g_img_dsc);
                lv_obj_invalidate(g_img);
            }
        }
        SDL_UnlockMutex(g_frame_lock);
    }
}

bool nes_game_port_is_done(void)
{
    return g_thread_done;
}

void nes_game_port_request_quit(void)
{
    g_quit_requested = true;
}

/* ------------------------------------------------------------------ */
/* 模拟线程侧钩子                                                      */
/* ------------------------------------------------------------------ */
int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *color_data)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

    if (!color_data || !g_frame_lock) {
        return 0;
    }

    /* 调试: NES_DUMP_FRAME=<prefix> 时, 每 60 帧 dump 一张 <prefix>_<n>.ppm (到 600 帧) */
    if (g_dump_frame_path == NULL) {
        g_dump_frame_path = getenv("NES_DUMP_FRAME");
    }
    if (g_dump_frame_path && g_frame_count > 0 && g_frame_count <= 600U &&
        (g_frame_count % 60U) == 0) {
        char path[512];
        snprintf(path, sizeof(path), "%s_%03u.ppm", g_dump_frame_path, g_frame_count);
        nes_sim_dump_frame_ppm(path, color_data);
    }

    /* 写后备缓冲(主线程渲染 front, 互不干扰) */
    if (SDL_LockMutex(g_frame_lock) == 0) {
        nes_color_t *back = g_frame_buf[g_front_idx ^ 1U];
        memcpy(back, color_data, NES_WIDTH * NES_HEIGHT * sizeof(nes_color_t));
        g_frame_pending = true;
        SDL_UnlockMutex(g_frame_lock);
    }
    return 0;
}

/* dump 一帧为二进制 PPM (RGB565 -> RGB888) */
static void nes_sim_dump_frame_ppm(const char *path, const nes_color_t *frame)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    uint8_t *rgb = malloc(NES_WIDTH * NES_HEIGHT * 3);
    if (rgb) {
        for (int i = 0; i < NES_WIDTH * NES_HEIGHT; i++) {
            uint16_t p = frame[i];
            rgb[i * 3 + 0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
            rgb[i * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
            rgb[i * 3 + 2] = (uint8_t)((p & 0x1F) << 3);
        }
        fprintf(f, "P6\n%d %d\n255\n", NES_WIDTH, NES_HEIGHT);
        fwrite(rgb, 1, NES_WIDTH * NES_HEIGHT * 3, f);
        free(rgb);
    }
    fclose(f);
    nes_log_printf("frame dumped to %s\n", path);
}

/* 键盘扫描由主线程 tick 完成, 这里合并持续按住 + 短按锁存两个来源 */
void game_nes_poll_joypad(nes_t *nes)
{
    if (!nes) {
        return;
    }
    if (g_quit_requested) {
        nes->nes_quit = 1;
        return;
    }
    /* 键位映射 (与 SDK demo README 一致):
     *   方向键/WASD -> U1/D1/L1/R1   X/K/左Alt -> A1   Z/J/左Ctrl -> B1
     *   Enter/小键盘Enter -> ST1     Space/Tab -> SE1
     * 持续按住状态 (g_joypad_state) OR 短按锁存 (g_sticky_keys), 消费后清零 */
    nes->nes_cpu.joypad.joypad = g_joypad_state | g_sticky_keys;
    g_sticky_keys = 0;
}

/* ------------------------------------------------------------------ */
/* 60fps 节拍 + 统计 (nes_run 每帧回调一次)                             */
/* ------------------------------------------------------------------ */
static uint32_t g_next_frame_ms = 0;
static uint16_t g_ms_remainder = 0;
static uint32_t g_fps_frames = 0;
static uint32_t g_fps_last_ms = 0;

void nes_frame(nes_t *nes)
{
    if (!nes || nes->nes_quit) {
        return;
    }

    game_nes_poll_joypad(nes);

    /* 节拍: 每帧 16.67ms(1000/60), 余数累进避免长期漂移 */
    g_ms_remainder += (uint16_t)(1000U % 60U);
    g_next_frame_ms += 1000U / 60U;
    if (g_ms_remainder >= 60U) {
        g_next_frame_ms++;
        g_ms_remainder = (uint16_t)(g_ms_remainder - 60U);
    }

    uint32_t now = nes_get_ms();
    if ((int32_t)(g_next_frame_ms - now) > 2) {
        SDL_Delay(g_next_frame_ms - now - 2U);
    }

    /* fps 统计 */
    now = nes_get_ms();
    g_fps_frames++;
    g_frame_count++;
    if (now - g_fps_last_ms >= 2000U) {
        g_logic_fps = g_fps_frames * 1000U / (now - g_fps_last_ms);
        nes_log_printf("logic_fps=%u\n", g_logic_fps);
        g_fps_frames = 0;
        g_fps_last_ms = now;
    }
}

uint32_t nes_game_port_get_logic_fps(void)
{
    return g_logic_fps;
}

/* ------------------------------------------------------------------ */
/* 音频: SDL queue, 48kHz / S16 / mono                                 */
/* ------------------------------------------------------------------ */
int nes_sound_output_s16(const int16_t *buffer, size_t len)
{
    if (g_audio_dev == 0 || buffer == NULL || len == 0) {
        return 0;
    }
    if (SDL_GetQueuedAudioSize(g_audio_dev) > NES_SIM_AUDIO_QUEUE_LIMIT) {
        return 0; /* 水位超限, 丢帧防延迟累积 */
    }
    SDL_QueueAudio(g_audio_dev, buffer, (Uint32)(len * sizeof(int16_t)));
    return 0;
}

int nes_sound_output(uint8_t *buffer, size_t len)
{
    /* 8bit 无符号兜底路径: 居中 128 -> S16。当前配置走 MIX_S16, 一般不会进来 */
    if (g_audio_dev == 0 || buffer == NULL || len == 0) {
        return 0;
    }
    if (SDL_GetQueuedAudioSize(g_audio_dev) > NES_SIM_AUDIO_QUEUE_LIMIT) {
        return 0;
    }

    static int16_t tmp[NES_SIM_AUDIO_FRAME_SAMPLES * 2];
    size_t n = len;
    if (n > sizeof(tmp) / sizeof(tmp[0])) {
        n = sizeof(tmp) / sizeof(tmp[0]);
    }
    for (size_t i = 0; i < n; i++) {
        tmp[i] = (int16_t)(((int32_t)buffer[i] - 128) << 8);
    }
    SDL_QueueAudio(g_audio_dev, tmp, (Uint32)(n * sizeof(int16_t)));
    return 0;
}

int nes_audio_reference_path_enabled(void)
{
    return 0;
}

static int nes_sim_audio_init(void)
{
    SDL_AudioSpec want;
    SDL_AudioSpec have;

    /* lv_drivers 的 sdl_init 只初始化了 video, 音频子系统需自行初始化 */
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        nes_log_printf("audio subsystem init failed: %s\n", SDL_GetError());
        return -1;
    }

    memset(&want, 0, sizeof(want));
    want.freq = (int)NES_SIM_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = (Uint16)NES_SIM_AUDIO_FRAME_SAMPLES;

    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_dev == 0) {
        nes_log_printf("audio open failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(g_audio_dev, 0);
    nes_log_printf("audio started (%dHz)\n", have.freq);
    return 0;
}

static void nes_sim_audio_deinit(void)
{
    if (g_audio_dev != 0) {
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }
}

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */
int nes_initex(nes_t *nes)
{
    if (!nes || !nes->nes_draw_data) {
        return -1;
    }

    /* 锁定 APU 采样率: 48kHz 时 sample_per_sync=800, 与帧率一一对应 */
    nes_apu_request_sample_rate(NES_APU_SAMPLE_RATE);

    g_quit_requested = false;
    g_thread_done = false;
    g_joypad_state = 0;
    g_logic_fps = 0;
    g_next_frame_ms = nes_get_ms();
    g_ms_remainder = 0;
    g_fps_frames = 0;
    g_fps_last_ms = g_next_frame_ms;

    return nes_sim_audio_init();
}

int nes_deinitex(nes_t *nes)
{
    (void)nes;
    nes_sim_audio_deinit();
    return 0;
}

/* ------------------------------------------------------------------ */
/* 模拟线程与启停 (presenter 调用)                                      */
/* ------------------------------------------------------------------ */
static int nes_sim_thread_entry(void *arg)
{
    nes_t *nes = (nes_t *)arg;
    nes_log_printf("sim thread start\n");
    nes_run(nes);
    g_thread_done = true;
    nes_log_printf("sim thread exit\n");
    return 0;
}

int nes_game_port_start(nes_t *nes)
{
    g_thread_done = false;
    g_nes_thread = SDL_CreateThread(nes_sim_thread_entry, "nes_sim", nes);
    if (g_nes_thread == NULL) {
        return -1;
    }
    return 0;
}

void nes_game_port_stop(void)
{
    if (g_nes_thread) {
        g_quit_requested = true;
        SDL_WaitThread(g_nes_thread, NULL); /* nes_run 每帧检查 quit, 最多 ~17ms */
        g_nes_thread = NULL;
    }
}
