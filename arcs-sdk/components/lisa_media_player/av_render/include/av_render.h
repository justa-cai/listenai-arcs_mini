/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 
 * display_dev/audio_dev 作为“opaque handle”，由具体 render_impl 解释。
 * 对具体平台而言，它们通常是“某种设备句柄/设备对象指针”。
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct av_render av_render_t;

typedef void (*av_render_free_fn_t)(void *data, void *user);

typedef struct {
    uint8_t *data;
    uint32_t size;
    av_render_free_fn_t free_fn;
    void *free_user;
} av_render_buf_t;

typedef enum {
    AV_RENDER_PKT_AUDIO_PCM_S16LE = 0,
    AV_RENDER_PKT_VIDEO_RGB565 = 1,
    AV_RENDER_PKT_VIDEO_MJPEG = 2,
    /* Encoded audio bitstream (decoder required) */
    AV_RENDER_PKT_AUDIO_MP3 = 3,
} av_render_pkt_type_t;

typedef struct {
    av_render_pkt_type_t type;
    uint32_t pts_ms;
    av_render_buf_t buf;

    union {
        struct {
            uint16_t width;
            uint16_t height;
        } video;
    } u;
} av_render_pkt_t;

typedef enum {
    AV_RENDER_SYNC_NONE = 0,
    AV_RENDER_SYNC_AUDIO = 1,
} av_render_sync_mode_t;

typedef enum {
    AV_RENDER_ROTATE_NONE = 0,
    /* Auto rotate only when frame WxH matches display HxW */
    AV_RENDER_ROTATE_AUTO_90_CW = 1,
    AV_RENDER_ROTATE_AUTO_90_CCW = 2,
    /* Force rotate (will be applied only when output size matches display, if display known) */
    AV_RENDER_ROTATE_FORCE_90_CW = 3,
    AV_RENDER_ROTATE_FORCE_90_CCW = 4,
} av_render_rotate_mode_t;

typedef struct {
    /*
     * 设备句柄（可选）：属于一种 render_impl 的参数。
     *
     * 默认行为：
     * - 当 display_dev/audio_dev 非空时，av_render 会使用内置的 LISA renderer 实现
     *   (components/lisa_media_player/av_render/src/render_impl) 完成输出。
     *
     * 自定义行为：
     * - 如果你通过 av_render_register_video_renderer / av_render_register_audio_renderer
     *   注册了自定义 renderer，则可以不依赖这两个字段（可置空）。
     */
    void *display_dev;
    void *audio_dev;
    av_render_sync_mode_t sync_mode;

    av_render_rotate_mode_t rotate_mode;

    /*
        * 可选的视频渲染帧率限制。
        * - 0：不限帧（尽可能快渲染）
        * - >0：限制到指定 FPS（尽力而为）
     */
    uint16_t video_fps_limit;

    /*
        * AV_RENDER_SYNC_AUDIO 的可选“低延迟控制”。
        * 限制 video PTS 相对音频时钟允许领先的最大范围。
        * - 0：关闭
        * - >0：在 video push 时做回压（小切片 sleep）
     */
    uint32_t max_video_ahead_ms;

    uint32_t video_queue_depth;
    uint32_t audio_queue_depth;
} av_render_cfg_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t fps;
} av_render_video_stream_info_t;

typedef struct {
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
} av_render_audio_stream_info_t;

/*
 * 运行期可插拔组件
 *
 * 说明：
 * - ops 指针必须在 av_render_t 生命周期内保持有效。
 * - ops 为 NULL 时，av_render 会回退到内置实现。
 * - 建议在推入第一包相关数据前完成注册，行为更可预期。
 */

typedef struct {
    /* 可选：视频线程在输出首帧前调用 */
    int (*init)(av_render_t *render, void *user);

    /* 必选：输出一帧 RGB565 */
    int (*write_rgb565)(av_render_t *render,
                        const uint8_t *rgb565,
                        uint32_t size,
                        uint16_t width,
                        uint16_t height,
                        uint32_t pts_ms,
                        void *user);

    /* 可选：renderer 关闭时调用 */
    void (*deinit)(av_render_t *render, void *user);
} av_render_video_renderer_ops_t;

typedef struct {
    /* 必选：输出 PCM s16le */
    int (*write_pcm_s16le)(av_render_t *render,
                           const uint8_t *pcm,
                           uint32_t size,
                           uint32_t pts_ms,
                           void *user);

    /* 可选：renderer 关闭时调用 */
    void (*stop)(av_render_t *render, void *user);
} av_render_audio_renderer_ops_t;

typedef struct {
    /*
    * 推入 JPEG bitstream 进行解码。
    *
    * 所有权规则：
    * - 成功：jpeg 所有权转移给 decoder。
    * - 失败：decoder 不得接管 jpeg；由 av_render 释放。
     */
    int (*push)(av_render_t *render,
                uint8_t *jpeg,
                size_t size,
                uint32_t pts_ms,
                av_render_free_fn_t free_fn,
                void *free_user,
                void *user);

    /* 可选：render 关闭时调用 */
    void (*close)(av_render_t *render, void *user);
} av_render_mjpeg_decoder_ops_t;

/*
 * 音频解码器 ops（实例级，运行期可插拔）
 *
 * 说明：
 * - 当前用于 AV_RENDER_PKT_AUDIO_MP3。
 * - 成功时 decoder 接管 encoded buffer 的所有权。
 * - decoder 需要把解码后的 PCM s16le 推入 av_render（供音频线程播放）。
 */
typedef struct {
    int (*push)(av_render_t *render,
                uint8_t *encoded,
                size_t size,
                uint32_t pts_ms,
                av_render_free_fn_t free_fn,
                void *free_user,
                void *user);

    void (*close)(av_render_t *render, void *user);
} av_render_audio_decoder_ops_t;

int av_render_open(const av_render_cfg_t *cfg, av_render_t **out);
int av_render_close(av_render_t *render);

int av_render_reset(av_render_t *render);

int av_render_add_video_stream(av_render_t *render, const av_render_video_stream_info_t *info);
int av_render_add_audio_stream(av_render_t *render, const av_render_audio_stream_info_t *info);

/* 运行期注册（实例级） */
int av_render_register_video_renderer(av_render_t *render, const av_render_video_renderer_ops_t *ops, void *user);
int av_render_register_audio_renderer(av_render_t *render, const av_render_audio_renderer_ops_t *ops, void *user);
int av_render_register_mjpeg_decoder(av_render_t *render, const av_render_mjpeg_decoder_ops_t *ops, void *user);
int av_render_register_audio_decoder(av_render_t *render, const av_render_audio_decoder_ops_t *ops, void *user);

/*
 * 内置视频输出辅助函数。
 *
 * 自定义 video renderer（通过 av_render_register_video_renderer 注册）可以调用该函数，
 * 把实际 LCD 输出委托给 av_render 的内置实现（包含 rotate_mode）。
 *
 * 说明：
 * - 本函数不接管 rgb565 所有权。
 * - 适合做链式/包装场景（例如 OSD 叠加）。
 */
int av_render_default_video_write_rgb565(av_render_t *render,
                                        const uint8_t *rgb565,
                                        uint32_t size,
                                        uint16_t width,
                                        uint16_t height,
                                        uint32_t pts_ms);

/*
 * 内置 display 写屏辅助函数（RGB565）。
 *
 * 把一块较小的 RGB565 buffer 写到配置好的显示设备 (x, y) 位置。
 * 适合简单的 overlay/OSD。
 */
int av_render_default_display_write_rgb565(av_render_t *render,
                                          uint16_t x,
                                          uint16_t y,
                                          const uint16_t *rgb565,
                                          uint16_t width,
                                          uint16_t height);

/*
 * Push 模型（统一入口）：
 * - 调用者准备一个 av_render_pkt_t 并传给 av_render_push()。
 * - 成功：pkt->buf.data 的所有权转移给 av_render。
 * - 失败：av_render 会通过 pkt->buf.free_fn 释放 pkt->buf.data
 *   （若 free_fn 为空，则使用 lisa_mem_free）。
 */
int av_render_push(av_render_t *render, av_render_pkt_t *pkt);

#ifdef __cplusplus
}
#endif
