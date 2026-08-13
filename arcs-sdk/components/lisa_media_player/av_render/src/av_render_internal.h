/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "av_render.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_thread.h>

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint32_t pts_ms;

    av_render_free_fn_t free_fn;
    void *free_user;
} av_render_video_frame_t;

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t pts_ms;

    av_render_free_fn_t free_fn;
    void *free_user;
} av_render_audio_frame_t;

struct av_render {
    av_render_cfg_t cfg;

    /* 运行期注册的 ops（可选） */
    const av_render_video_renderer_ops_t *video_renderer_ops;
    void *video_renderer_user;

    const av_render_audio_renderer_ops_t *audio_renderer_ops;
    void *audio_renderer_user;

    const av_render_mjpeg_decoder_ops_t *mjpeg_decoder_ops;
    void *mjpeg_decoder_user;

    const av_render_audio_decoder_ops_t *audio_decoder_ops;
    void *audio_decoder_user;

    lisa_queue_t *video_q;
    lisa_queue_t *audio_q;

    /* 可选 codec 队列（编码后的输入） */
    lisa_queue_t *video_codec_q;
    lisa_queue_t *audio_codec_q;

    lisa_thread_t *video_thread;
    lisa_thread_t *audio_thread;

    /* 可选 codec 线程 */
    lisa_thread_t *video_codec_thread;
    lisa_thread_t *audio_codec_thread;

    /* 线程生命周期：lisa_thread_delete() 是 no-op；线程会自退出并自清理 */
    volatile bool video_thread_exited;
    volatile bool audio_thread_exited;
    volatile bool video_codec_thread_exited;
    volatile bool audio_codec_thread_exited;

    lisa_mutex_t *state_lock;

    volatile bool running;
    volatile bool should_stop;

    bool video_stream_set;
    bool audio_stream_set;
    av_render_video_stream_info_t video_info;
    av_render_audio_stream_info_t audio_info;

    uint16_t display_width;
    uint16_t display_height;
    uint8_t *rotate_buf;
    size_t rotate_buf_size;

    /* 解码后的 RGB565 帧缓存池（避免每帧 malloc/free） */
    lisa_queue_t *video_rgb_pool_q;
    uint8_t *video_rgb_pool_mem;
    uint32_t video_rgb_pool_slot_size;
    uint16_t video_rgb_pool_slots;

    uint64_t audio_start_tick_ms;
    bool audio_started;
    bool audio_cfg_done;

    /* Audio staging：把零散小块 PCM 合并为更大的写入，减少短写/间隙 */
    int16_t *audio_staging;
    uint32_t audio_staging_cap_samples;
    uint32_t audio_staging_len_samples;
    uint16_t audio_out_buffer_samples;
};

/*
 * 内置 render_impl：LISA 音频/显示设备实现。
 *
 * 说明：
 * - 这是一种 renderer 实现（ops）。
 * - 当 cfg->audio_dev/display_dev 非空时，av_render_open() 会默认安装它们。
 * - 上层也可以通过 av_render_register_* 在运行期替换为自定义实现。
 */
extern const av_render_audio_renderer_ops_t av_render_lisa_audio_renderer_ops;
extern const av_render_video_renderer_ops_t av_render_lisa_video_renderer_ops;

int audio_render_write_pcm_s16le(av_render_t *render, const av_render_audio_frame_t *frame);
void audio_render_stop(av_render_t *render);

int video_render_init(av_render_t *render);
int video_render_write_rgb565(av_render_t *render, const av_render_video_frame_t *frame);

void av_render_sync_on_audio_frame(av_render_t *render, uint32_t audio_pts_ms);
void av_render_sync_wait_video(av_render_t *render, uint32_t video_pts_ms);

/* MJPEG 解码器（内部使用） */
int av_render_mjpeg_push(av_render_t *render,
                         uint8_t *jpeg,
                         size_t size,
                         uint32_t pts_ms,
                         av_render_free_fn_t free_fn,
                         void *free_user);

void av_render_mjpeg_close(av_render_t *render);

int av_render_lisa_jpeg_push(av_render_t *render,
                             uint8_t *jpeg,
                             size_t size,
                             uint32_t pts_ms,
                             av_render_free_fn_t free_fn,
                             void *free_user);

void av_render_lisa_jpeg_close(av_render_t *render);

/* MP3 解码器（内部使用，可选） */
int av_render_mp3_push(av_render_t *render,
                       uint8_t *data,
                       size_t size,
                       uint32_t pts_ms,
                       av_render_free_fn_t free_fn,
                       void *free_user);

void av_render_mp3_close(av_render_t *render);
