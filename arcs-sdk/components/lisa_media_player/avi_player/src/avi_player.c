/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "avi_player.h"

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_semaphore.h>
#include <lisa_thread.h>
#include <lisa_time.h>

#include "av_render.h"
#include "avifile.h"

#ifndef LOG_TAG
#define LOG_TAG "avi_player"
#endif

struct avi_player_ctrl {
    lisa_mutex_t *lock;
    lisa_semaphore_t *resume_sem;
    bool paused;
    bool stop;
    uint32_t duration_ms;
    uint32_t position_ms;
};

avi_player_ctrl_t *avi_player_ctrl_create(void)
{
    avi_player_ctrl_t *ctrl = (avi_player_ctrl_t *)lisa_mem_alloc(sizeof(*ctrl));
    if (!ctrl) {
        return NULL;
    }
    memset(ctrl, 0, sizeof(*ctrl));

    ctrl->lock = lisa_mutex_create();
    if (!ctrl->lock) {
        lisa_mem_free(ctrl);
        return NULL;
    }

    ctrl->resume_sem = lisa_semaphore_create(1);
    if (!ctrl->resume_sem) {
        lisa_mutex_delete(ctrl->lock);
        lisa_mem_free(ctrl);
        return NULL;
    }
    return ctrl;
}

void avi_player_ctrl_destroy(avi_player_ctrl_t *ctrl)
{
    if (!ctrl) {
        return;
    }
    if (ctrl->resume_sem) {
        lisa_semaphore_delete(ctrl->resume_sem);
        ctrl->resume_sem = NULL;
    }
    if (ctrl->lock) {
        lisa_mutex_delete(ctrl->lock);
        ctrl->lock = NULL;
    }
    lisa_mem_free(ctrl);
}

int avi_player_pause(avi_player_ctrl_t *ctrl)
{
    if (!ctrl || !ctrl->lock || !ctrl->resume_sem) {
        return -EINVAL;
    }
    lisa_mutex_lock(ctrl->lock, LISA_WAIT_FOREVER);
    ctrl->paused = true;
    (void)lisa_semaphore_clear(ctrl->resume_sem);
    lisa_mutex_unlock(ctrl->lock);
    return 0;
}

int avi_player_resume(avi_player_ctrl_t *ctrl)
{
    if (!ctrl || !ctrl->lock || !ctrl->resume_sem) {
        return -EINVAL;
    }
    lisa_mutex_lock(ctrl->lock, LISA_WAIT_FOREVER);
    ctrl->paused = false;
    lisa_mutex_unlock(ctrl->lock);
    (void)lisa_semaphore_give(ctrl->resume_sem);
    return 0;
}

int avi_player_stop(avi_player_ctrl_t *ctrl)
{
    if (!ctrl || !ctrl->lock || !ctrl->resume_sem) {
        return -EINVAL;
    }
    lisa_mutex_lock(ctrl->lock, LISA_WAIT_FOREVER);
    ctrl->stop = true;
    ctrl->paused = false;
    lisa_mutex_unlock(ctrl->lock);
    (void)lisa_semaphore_give(ctrl->resume_sem);
    return 0;
}

int avi_player_get_progress(avi_player_ctrl_t *ctrl, uint32_t *position_ms, uint32_t *duration_ms)
{
    if (!ctrl) {
        return -EINVAL;
    }
    if (position_ms) {
        *position_ms = ctrl->position_ms;
    }
    if (duration_ms) {
        *duration_ms = ctrl->duration_ms;
    }
    return 0;
}

static int ctrl_wait_if_paused_or_stopped(avi_player_ctrl_t *ctrl)
{
    if (!ctrl || !ctrl->lock || !ctrl->resume_sem) {
        return 0;
    }

    while (1) {
        bool paused = false;
        bool stop = false;

        lisa_mutex_lock(ctrl->lock, LISA_WAIT_FOREVER);
        paused = ctrl->paused;
        stop = ctrl->stop;
        lisa_mutex_unlock(ctrl->lock);

        if (stop) {
            return -ECANCELED;
        }
        if (!paused) {
            return 0;
        }

        /*
         * Wait for resume/stop.
         * Use finite timeout to re-check stop flag even if give is lost.
         */
        (void)lisa_semaphore_take(ctrl->resume_sem, 200);
    }
}

/* WAVEFORMATEX 的 wFormatTag 取值（AVI 音频流）。 */
#define AVI_WAVE_FORMAT_PCM        0x0001
#define AVI_WAVE_FORMAT_MPEGLAYER3 0x0055

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
typedef struct {
    uint32_t mjpeg_pool_hit;
    uint32_t mjpeg_pool_miss_no_slot;
    uint32_t mjpeg_fallback_no_pool;
    uint32_t mjpeg_fallback_too_big;
    uint32_t mjpeg_heap_alloc_ok;
    uint64_t mjpeg_bytes_pool;
    uint64_t mjpeg_bytes_heap;

    uint32_t audio_pool_hit;
    uint32_t audio_pool_miss_no_slot;
    uint32_t audio_fallback_no_pool;
    uint32_t audio_fallback_too_big;
    uint32_t audio_heap_alloc_ok;
    uint64_t audio_bytes_pool;
    uint64_t audio_bytes_heap;
} avi_pool_stats_t;

static avi_pool_stats_t s_pool_stats;

static void pool_stats_log_and_reset(void)
{
    LISA_LOGI(LOG_TAG,
              "pool stat: mjpeg hit=%u miss=%u fb_no_pool=%u fb_big=%u heap_ok=%u bytes(pool/heap)=%llu/%llu; "
              "pcm hit=%u miss=%u fb_no_pool=%u fb_big=%u heap_ok=%u bytes(pool/heap)=%llu/%llu",
              (unsigned)s_pool_stats.mjpeg_pool_hit,
              (unsigned)s_pool_stats.mjpeg_pool_miss_no_slot,
              (unsigned)s_pool_stats.mjpeg_fallback_no_pool,
              (unsigned)s_pool_stats.mjpeg_fallback_too_big,
              (unsigned)s_pool_stats.mjpeg_heap_alloc_ok,
              (unsigned long long)s_pool_stats.mjpeg_bytes_pool,
              (unsigned long long)s_pool_stats.mjpeg_bytes_heap,
              (unsigned)s_pool_stats.audio_pool_hit,
              (unsigned)s_pool_stats.audio_pool_miss_no_slot,
              (unsigned)s_pool_stats.audio_fallback_no_pool,
              (unsigned)s_pool_stats.audio_fallback_too_big,
              (unsigned)s_pool_stats.audio_heap_alloc_ok,
              (unsigned long long)s_pool_stats.audio_bytes_pool,
              (unsigned long long)s_pool_stats.audio_bytes_heap);
    memset(&s_pool_stats, 0, sizeof(s_pool_stats));
}
#endif

static const char *avi_video_codec_str(avi_video_format_t fmt)
{
    switch (fmt) {
    case AVI_VIDEO_FORMAT_MJPEG:
        return "MJPEG";
    default:
        return "UNKNOWN";
    }
}

static const char *avi_audio_codec_str(uint16_t format_tag)
{
    switch (format_tag) {
    case AVI_WAVE_FORMAT_PCM:
        return "PCM";
    case AVI_WAVE_FORMAT_MPEGLAYER3:
        return "MP3";
    default:
        return "UNKNOWN";
    }
}

static void heap_free_cb(void *data, void *user)
{
    (void)user;
    if (data) {
        lisa_mem_free(data);
    }
}

/*
 * 定长块内存池（可选）
 * - 目标：减少 malloc/free 抖动，并支持 demux->render 的零拷贝传递。
 * - 所有权：由 av_render 通过 free_fn 释放。
 */
#define AVI_POOL_MAX_SLOTS 8

typedef struct {
    lisa_mutex_t *lock;
    uint8_t *buf[AVI_POOL_MAX_SLOTS];
    bool used[AVI_POOL_MAX_SLOTS];
    uint32_t slot_bytes;
    uint32_t slots;
    bool inited;
    bool disabled;
} avi_fixed_pool_t;

#if defined(CONFIG_AVI_PLAYER_MJPEG_POOL)
static avi_fixed_pool_t s_mjpeg_pool;

static void mjpeg_pool_free_cb(void *data, void *user)
{
    (void)data;
    if (!s_mjpeg_pool.inited || s_mjpeg_pool.disabled || !s_mjpeg_pool.lock) {
        return;
    }
    uintptr_t idx = (uintptr_t)user;
    if (idx >= s_mjpeg_pool.slots) {
        return;
    }
    lisa_mutex_lock(s_mjpeg_pool.lock, LISA_WAIT_FOREVER);
    s_mjpeg_pool.used[idx] = false;
    lisa_mutex_unlock(s_mjpeg_pool.lock);
}

static void mjpeg_pool_init_if_needed(void)
{
    if (s_mjpeg_pool.inited || s_mjpeg_pool.disabled) {
        return;
    }

    s_mjpeg_pool.lock = lisa_mutex_create();
    if (!s_mjpeg_pool.lock) {
        s_mjpeg_pool.disabled = true;
        return;
    }

    uint32_t slots = 2;
#if defined(CONFIG_AVI_PLAYER_MJPEG_POOL_SLOTS)
    slots = (uint32_t)CONFIG_AVI_PLAYER_MJPEG_POOL_SLOTS;
#endif
    if (slots == 0 || slots > AVI_POOL_MAX_SLOTS) {
        slots = AVI_POOL_MAX_SLOTS;
    }
    s_mjpeg_pool.slots = slots;

    s_mjpeg_pool.slot_bytes = (uint32_t)CONFIG_AVI_PLAYER_MJPEG_POOL_SLOT_BYTES;
    if (s_mjpeg_pool.slot_bytes < 4096) {
        s_mjpeg_pool.slot_bytes = 4096;
    }

    for (uint32_t i = 0; i < s_mjpeg_pool.slots; i++) {
        s_mjpeg_pool.buf[i] = (uint8_t *)lisa_mem_alloc(s_mjpeg_pool.slot_bytes);
        s_mjpeg_pool.used[i] = false;
        if (!s_mjpeg_pool.buf[i]) {
            s_mjpeg_pool.disabled = true;
            break;
        }
    }

    s_mjpeg_pool.inited = true;
}

static uint8_t *mjpeg_alloc(size_t size, av_render_free_fn_t *out_free_fn, void **out_free_user)
{
    if (out_free_fn) {
        *out_free_fn = heap_free_cb;
    }
    if (out_free_user) {
        *out_free_user = NULL;
    }

    mjpeg_pool_init_if_needed();
    if (!s_mjpeg_pool.inited || s_mjpeg_pool.disabled || !s_mjpeg_pool.lock) {
        uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
        s_pool_stats.mjpeg_fallback_no_pool++;
        if (p) {
            s_pool_stats.mjpeg_heap_alloc_ok++;
            s_pool_stats.mjpeg_bytes_heap += (uint64_t)size;
        }
#endif
        return p;
    }
    if (size == 0 || size > s_mjpeg_pool.slot_bytes) {
        uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
        s_pool_stats.mjpeg_fallback_too_big++;
        if (p) {
            s_pool_stats.mjpeg_heap_alloc_ok++;
            s_pool_stats.mjpeg_bytes_heap += (uint64_t)size;
        }
#endif
        return p;
    }

    lisa_mutex_lock(s_mjpeg_pool.lock, LISA_WAIT_FOREVER);
    for (uintptr_t i = 0; i < s_mjpeg_pool.slots; i++) {
        if (!s_mjpeg_pool.used[i] && s_mjpeg_pool.buf[i]) {
            s_mjpeg_pool.used[i] = true;
            lisa_mutex_unlock(s_mjpeg_pool.lock);
            if (out_free_fn) {
                *out_free_fn = mjpeg_pool_free_cb;
            }
            if (out_free_user) {
                *out_free_user = (void *)i;
            }
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
            s_pool_stats.mjpeg_pool_hit++;
            s_pool_stats.mjpeg_bytes_pool += (uint64_t)size;
#endif
            return s_mjpeg_pool.buf[i];
        }
    }
    lisa_mutex_unlock(s_mjpeg_pool.lock);

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    s_pool_stats.mjpeg_pool_miss_no_slot++;
#endif
    uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    if (p) {
        s_pool_stats.mjpeg_heap_alloc_ok++;
        s_pool_stats.mjpeg_bytes_heap += (uint64_t)size;
    }
#endif
    return p;
}
#endif

#if defined(CONFIG_AVI_PLAYER_AUDIO_POOL)
static avi_fixed_pool_t s_audio_pool;

static void audio_pool_free_cb(void *data, void *user)
{
    (void)data;
    if (!s_audio_pool.inited || s_audio_pool.disabled || !s_audio_pool.lock) {
        return;
    }
    uintptr_t idx = (uintptr_t)user;
    if (idx >= s_audio_pool.slots) {
        return;
    }
    lisa_mutex_lock(s_audio_pool.lock, LISA_WAIT_FOREVER);
    s_audio_pool.used[idx] = false;
    lisa_mutex_unlock(s_audio_pool.lock);
}

static void audio_pool_init_if_needed(void)
{
    if (s_audio_pool.inited || s_audio_pool.disabled) {
        return;
    }

    s_audio_pool.lock = lisa_mutex_create();
    if (!s_audio_pool.lock) {
        s_audio_pool.disabled = true;
        return;
    }

    uint32_t slots = (uint32_t)CONFIG_AVI_PLAYER_AUDIO_POOL_SLOTS;
    if (slots == 0 || slots > AVI_POOL_MAX_SLOTS) {
        slots = AVI_POOL_MAX_SLOTS;
    }
    s_audio_pool.slots = slots;

    s_audio_pool.slot_bytes = (uint32_t)CONFIG_AVI_PLAYER_AUDIO_POOL_SLOT_BYTES;
    if (s_audio_pool.slot_bytes < 512) {
        s_audio_pool.slot_bytes = 512;
    }

    for (uint32_t i = 0; i < s_audio_pool.slots; i++) {
        s_audio_pool.buf[i] = (uint8_t *)lisa_mem_alloc(s_audio_pool.slot_bytes);
        s_audio_pool.used[i] = false;
        if (!s_audio_pool.buf[i]) {
            s_audio_pool.disabled = true;
            break;
        }
    }

    s_audio_pool.inited = true;
}

static uint8_t *audio_alloc(size_t size, av_render_free_fn_t *out_free_fn, void **out_free_user)
{
    if (out_free_fn) {
        *out_free_fn = heap_free_cb;
    }
    if (out_free_user) {
        *out_free_user = NULL;
    }

    audio_pool_init_if_needed();
    if (!s_audio_pool.inited || s_audio_pool.disabled || !s_audio_pool.lock) {
        uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
        s_pool_stats.audio_fallback_no_pool++;
        if (p) {
            s_pool_stats.audio_heap_alloc_ok++;
            s_pool_stats.audio_bytes_heap += (uint64_t)size;
        }
#endif
        return p;
    }
    if (size == 0 || size > s_audio_pool.slot_bytes) {
        uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
        s_pool_stats.audio_fallback_too_big++;
        if (p) {
            s_pool_stats.audio_heap_alloc_ok++;
            s_pool_stats.audio_bytes_heap += (uint64_t)size;
        }
#endif
        return p;
    }

    lisa_mutex_lock(s_audio_pool.lock, LISA_WAIT_FOREVER);
    for (uintptr_t i = 0; i < s_audio_pool.slots; i++) {
        if (!s_audio_pool.used[i] && s_audio_pool.buf[i]) {
            s_audio_pool.used[i] = true;
            lisa_mutex_unlock(s_audio_pool.lock);
            if (out_free_fn) {
                *out_free_fn = audio_pool_free_cb;
            }
            if (out_free_user) {
                *out_free_user = (void *)i;
            }
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
            s_pool_stats.audio_pool_hit++;
            s_pool_stats.audio_bytes_pool += (uint64_t)size;
#endif
            return s_audio_pool.buf[i];
        }
    }
    lisa_mutex_unlock(s_audio_pool.lock);

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    s_pool_stats.audio_pool_miss_no_slot++;
#endif
    uint8_t *p = (uint8_t *)lisa_mem_alloc(size);
#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    if (p) {
        s_pool_stats.audio_heap_alloc_ok++;
        s_pool_stats.audio_bytes_heap += (uint64_t)size;
    }
#endif
    return p;
}
#endif

static bool should_log_every(uint64_t *last_ms, uint32_t period_ms)
{
    uint64_t now = lisa_os_get_tick_ms();
    if (*last_ms == 0 || (now - *last_ms) >= period_ms) {
        *last_ms = now;
        return true;
    }
    return false;
}

static int io_read_exact(avi_io_t *io, void *buf, size_t size)
{
    uint8_t *p = (uint8_t *)buf;
    size_t left = size;
    while (left > 0) {
        size_t got = 0;
        int ret = avi_io_read(io, p, left, &got);
        if (ret != 0) {
            return ret;
        }
        if (got == 0) {
            return -EIO;
        }
        p += got;
        left -= got;
    }
    return 0;
}

static int io_read_some(avi_io_t *io, void *buf, size_t size, size_t *out_got)
{
    if (out_got) {
        *out_got = 0;
    }
    size_t got = 0;
    int ret = avi_io_read(io, buf, size, &got);
    if (ret != 0) {
        return ret;
    }
    if (out_got) {
        *out_got = got;
    }
    return 0;
}

int avi_player_play_io_ex(avi_io_t *io, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl)
{
    if (!io || !render) {
        return -EINVAL;
    }

    int cret = ctrl_wait_if_paused_or_stopped(ctrl);
    if (cret != 0) {
        return cret;
    }

    const uint32_t header_read = 64 * 1024;
    uint8_t *header_buf = (uint8_t *)lisa_mem_alloc(header_read);
    if (!header_buf) {
        return -ENOMEM;
    }
    memset(header_buf, 0, header_read);

    size_t got_total = 0;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t prof_last_ms = 0;
    uint64_t prof_hdr_ms = 0;
    uint64_t prof_io_ms_sum = 0;
    uint32_t prof_io_ms_max = 0;
    uint64_t prof_push_ms_sum = 0;
    uint32_t prof_push_ms_max = 0;
    uint32_t prof_chunks = 0;
#endif

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t t_hdr0 = lisa_os_get_tick_ms();
#endif
    while (got_total < header_read) {
        cret = ctrl_wait_if_paused_or_stopped(ctrl);
        if (cret != 0) {
            lisa_mem_free(header_buf);
            return cret;
        }
        size_t got = 0;
        int r = io_read_some(io, header_buf + got_total, header_read - got_total, &got);
        if (r != 0) {
            lisa_mem_free(header_buf);
            return r;
        }
        if (got == 0) {
            break;
        }
        got_total += got;
    }
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    prof_hdr_ms = lisa_os_get_tick_ms() - t_hdr0;
#endif
    if (got_total == 0) {
        lisa_mem_free(header_buf);
        return -EIO;
    }

    avi_file_info_t info;
    int ret = avi_parse(&info, header_buf, (uint32_t)got_total);
    lisa_mem_free(header_buf);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "AVI parse failed: %d", ret);
        return ret;
    }

    /* 打印音视频编码格式，便于播放时验证解析结果。 */
    LISA_LOGI(LOG_TAG,
              "AVI stream: video=%s %ux%u fps=%u; audio=%s tag=0x%04x %uHz %uch %ubits",
              avi_video_codec_str(info.vids_format),
              (unsigned)info.vids_width,
              (unsigned)info.vids_height,
              (unsigned)info.vids_fps,
              avi_audio_codec_str(info.auds_format_tag),
              (unsigned)info.auds_format_tag,
              (unsigned)info.auds_sample_rate,
              (unsigned)info.auds_channels,
              (unsigned)info.auds_bits);

    if (info.vids_format != AVI_VIDEO_FORMAT_MJPEG) {
        LISA_LOGE(LOG_TAG, "Only support MJPEG video currently");
        return -ENOTSUP;
    }

    /*
     * 视频时序：
     * 有些 AVI 的 STRH rate/scale 不可靠，优先使用 AVIH 的 us_per_frame（若存在）。
     */
    uint32_t video_frame_interval_ms = 40;
    uint32_t effective_fps = 0;
    if (info.avih_us_per_frame > 0) {
        video_frame_interval_ms = (info.avih_us_per_frame + 500U) / 1000U;
        if (video_frame_interval_ms == 0) {
            video_frame_interval_ms = 1;
        }
        effective_fps = 1000U / video_frame_interval_ms;
        if (effective_fps == 0) {
            effective_fps = 1;
        }
    } else if (info.vids_fps > 0) {
        video_frame_interval_ms = 1000U / info.vids_fps;
        if (video_frame_interval_ms == 0) {
            video_frame_interval_ms = 1;
        }
        effective_fps = info.vids_fps;
    }
    if (effective_fps == 0) {
        effective_fps = 25;
    }

    if (ctrl) {
        ctrl->duration_ms = (uint32_t)((uint64_t)info.total_frames * video_frame_interval_ms);
        ctrl->position_ms = 0;
    }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    LISA_LOGI(LOG_TAG,
              "AVI video: %ux%u fps(strh)=%u rate/scale=%u/%u avih_uspf=%u -> use fps=%u interval=%ums",
              (unsigned)info.vids_width,
              (unsigned)info.vids_height,
              (unsigned)info.vids_fps,
              (unsigned)info.vids_strh_rate,
              (unsigned)info.vids_strh_scale,
              (unsigned)info.avih_us_per_frame,
              (unsigned)effective_fps,
              (unsigned)video_frame_interval_ms);
#endif

    av_render_video_stream_info_t vinfo = {
        .width = info.vids_width,
        .height = info.vids_height,
        .fps = (uint16_t)effective_fps,
    };
    av_render_add_video_stream(render, &vinfo);

    uint64_t vstat_last_ms = 0;
    uint32_t vstat_enq = 0;
    uint32_t vstat_drop = 0;
    uint32_t vstat_replace = 0;
    uint32_t vstat_inline = 0;
    uint32_t vstat_skip = 0;

    /* 视频平均帧率统计：以“成功 push 到 render”的帧为基准（更接近实际播放）。 */
    uint64_t vfps_start_ms = 0;
    uint32_t vfps_total_enq = 0;

#if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
    uint64_t pstat_last_ms = 0;
#endif

    /* demux 侧音频统计：用于判断 SD/I/O 带宽是否导致音频“吃不饱” */
    uint64_t astat_last_ms = 0;
    uint64_t astat_last_window_ms = 0;
    uint64_t astat_bytes_window = 0;
    uint32_t astat_chunks_window = 0;
    uint8_t drop_video_for_audio = 0;
    uint32_t expected_audio_bps = 0;
    uint32_t adaptive_audio_bps_ref = 0;
    uint32_t adaptive_audio_bps_windows = 0;
    int drop_low_streak = 0;
    int drop_good_streak = 0;

    /* 视频解码由 av_render 内部处理（mjpeg decoder）。 */

    if (info.auds_sample_rate != 0) {
        /* 支持 PCM（WAVE_FORMAT_PCM）与 MP3（WAVE_FORMAT_MPEGLAYER3）。 */
        if (info.auds_format_tag == AVI_WAVE_FORMAT_PCM) {
            if (info.auds_bits != 16) {
                LISA_LOGW(LOG_TAG,
                          "AVI PCM audio not supported: bits=%u (only PCM s16le supported). Audio will be muted.",
                          (unsigned)info.auds_bits);
                info.auds_sample_rate = 0;
            }
        } else if (info.auds_format_tag == AVI_WAVE_FORMAT_MPEGLAYER3) {
            /* MP3 在 av_render 侧解码为 PCM s16le（内置或运行期注册）。 */
            info.auds_bits = 16;
        } else {
            LISA_LOGW(LOG_TAG,
                      "AVI audio not supported: format_tag=0x%04x bits=%u. Audio will be muted.",
                      (unsigned)info.auds_format_tag,
                      (unsigned)info.auds_bits);
            info.auds_sample_rate = 0;
        }
    }

    if (info.auds_sample_rate != 0) {
        uint32_t effective_sample_rate = info.auds_sample_rate;
        uint32_t derived_by_strf = 0;
        uint32_t derived_by_strh = 0;

        if (info.auds_avg_bytes_per_sec && info.auds_block_align) {
            derived_by_strf = info.auds_avg_bytes_per_sec / info.auds_block_align;
        }
        if (info.auds_strh_rate && info.auds_strh_scale) {
            derived_by_strh = info.auds_strh_rate / info.auds_strh_scale;
        }

        uint32_t preferred = 0;
        if (derived_by_strf > 0) {
            preferred = derived_by_strf;
        } else if (derived_by_strh > 0) {
            preferred = derived_by_strh;
        }

        const bool header_rate_valid = (info.auds_sample_rate >= 8000U && info.auds_sample_rate <= 96000U);
        const bool derived_rate_valid = (preferred >= 8000U && preferred <= 96000U);

        if (!header_rate_valid && derived_rate_valid) {
            effective_sample_rate = preferred;
        } else if (header_rate_valid && derived_rate_valid) {
            uint32_t hi = (effective_sample_rate > preferred) ? effective_sample_rate : preferred;
            uint32_t lo = (effective_sample_rate > preferred) ? preferred : effective_sample_rate;
            if (lo != 0 && (hi - lo) <= (hi / 20)) { /* 差异 <=5% */
                effective_sample_rate = preferred;
            }
        }

        if (!header_rate_valid && !derived_rate_valid) {
            effective_sample_rate = 16000U;
        }

        LISA_LOGI(LOG_TAG,
              "AVI audio: %s %u Hz, %u ch, %u bits (tag=0x%04x align=%u avgBps=%u strh=%u/%u eff=%u)",
              avi_audio_codec_str(info.auds_format_tag),
                  (unsigned)info.auds_sample_rate,
                  (unsigned)info.auds_channels,
                  (unsigned)info.auds_bits,
                  (unsigned)info.auds_format_tag,
                  (unsigned)info.auds_block_align,
                  (unsigned)info.auds_avg_bytes_per_sec,
                  (unsigned)info.auds_strh_rate,
                  (unsigned)info.auds_strh_scale,
                  (unsigned)effective_sample_rate);

        av_render_audio_stream_info_t ainfo = {
            .channels = info.auds_channels,
            .sample_rate = effective_sample_rate,
            /* MP3 会解码为 PCM s16le；PCM 则保持其原始位宽（当前仅支持 16bit） */
            .bits_per_sample = 16,
        };
        av_render_add_audio_stream(render, &ainfo);

        /* 把最终采用的采样率写回，供后续 audio_pts 计算使用 */
        info.auds_sample_rate = effective_sample_rate;

        /* 期望 bytes/sec：用于 demux 吞吐诊断 */
        if (info.auds_format_tag == AVI_WAVE_FORMAT_PCM) {
            uint32_t bytes_per_frame = info.auds_block_align;
            if (bytes_per_frame == 0) {
                bytes_per_frame = (info.auds_bits / 8U) * info.auds_channels;
            }
            if (bytes_per_frame && info.auds_sample_rate) {
                expected_audio_bps = info.auds_sample_rate * bytes_per_frame;
            }
        } else if (info.auds_format_tag == AVI_WAVE_FORMAT_MPEGLAYER3) {
            /* MP3：avg_bytes_per_sec 基本是我们能拿到的最好线索 */
            expected_audio_bps = info.auds_avg_bytes_per_sec;
        }
    } else {
        LISA_LOGW(LOG_TAG, "AVI has no audio stream info; audio will be ignored");
    }

    ret = avi_io_seek(io, (int64_t)info.movi_start, AVI_IO_SEEK_SET);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "seek movi failed: %d", ret);
        return ret;
    }

    uint32_t frame_no = 0;
    uint32_t video_pts = 0;
    uint32_t audio_pts = 0;

#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
    uint32_t adaptive_target_fps = effective_fps;
    uint32_t adaptive_keep_acc = 0;
    int adaptive_demux_low_streak = 0;
    int adaptive_demux_good_streak = 0;
    uint64_t adaptive_fps_last_change_ms = 0;
    uint8_t adaptive_conservative_down_count = 0;
    const uint8_t adaptive_conservative_down_max = 2;
    uint32_t adaptive_vframe_size_ref = 0;
    int adaptive_vframe_big_streak = 0;
#endif

    bool seen_audio_chunk = false;

    /* 基于 wall-clock 的 lead 控制：用于改善音频连续性 */
    uint64_t play_start_ms = 0;
    int lead_low_streak = 0;
    int lead_good_streak = 0;

    while (1) {
        cret = ctrl_wait_if_paused_or_stopped(ctrl);
        if (cret != 0) {
            ret = cret;
            break;
        }

        uint8_t hdr_buf[8];
        size_t hdr_got = 0;
        while (hdr_got < sizeof(hdr_buf)) {
            size_t got = 0;
            ret = io_read_some(io, hdr_buf + hdr_got, sizeof(hdr_buf) - hdr_got, &got);
            if (ret != 0) {
                break;
            }
            if (got == 0) {
                if (hdr_got == 0) {
                    ret = 0;
                } else {
                    ret = -EIO;
                }
                break;
            }
            hdr_got += got;
        }
        if (ret != 0 || hdr_got == 0) {
            break;
        }
        if (hdr_got < sizeof(hdr_buf)) {
            ret = -EIO;
            break;
        }

        uint8_t id[4] = {hdr_buf[0], hdr_buf[1], hdr_buf[2], hdr_buf[3]};
        uint32_t size = (uint32_t)hdr_buf[4] | ((uint32_t)hdr_buf[5] << 8) | ((uint32_t)hdr_buf[6] << 16) |
                        ((uint32_t)hdr_buf[7] << 24);

        if (size == 0) {
            continue;
        }

        /* AVI chunk 以 2 字节对齐 */
        uint32_t padded = (size + 1) & ~1U;

    #if defined(CONFIG_AVI_PLAYER_POOL_STATS_LOG)
        if (should_log_every(&pstat_last_ms, 2000)) {
            pool_stats_log_and_reset();
        }
    #endif

        if (id[2] == 'd' && id[3] == 'c') {
            /* 视频（MJPEG） */

            cret = ctrl_wait_if_paused_or_stopped(ctrl);
            if (cret != 0) {
                ret = cret;
                break;
            }

            /*
             * 注意：丢视频的策略由“音频 chunk 路径”的 demux 带宽检查控制
             *（bps vs expected_audio_bps）。不要在这里再引入基于 wall-clock
             * 的启发式规则，否则可能误触发，导致在音频顺畅时视频也被永久性饿死。
             */

            /*
             * 如果 demux 跟不上音频带宽（通常受限于 SD/I/O），
             * 则跳过读取视频 payload，以优先保障音频连续性。
             */
            if (drop_video_for_audio && info.auds_sample_rate) {
                (void)avi_io_seek(io, (int64_t)padded, AVI_IO_SEEK_CUR);
                vstat_skip++;
                frame_no++;

                /*
                 * 重要：当我们主动跳过视频以保障音频时，不要让 video_pts 持续
                 * 向“遥远的未来”推进。否则视频恢复后，A/V 同步会认为视频严重超前，
                 * 从而等待数百毫秒，导致视觉卡顿。
                 */
                if (audio_pts > 0) {
                    uint32_t target = audio_pts;
                    /* 保持极小的提前量，避免下一帧被判定为“迟到” */
                    if (video_frame_interval_ms > 0) {
                        target += video_frame_interval_ms;
                    }
                    video_pts = target;
                } else {
                    video_pts += video_frame_interval_ms;
                }
                if (ctrl) { ctrl->position_ms = video_pts; }
                continue;
            }

#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
            {
                uint32_t min_fps = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_MIN;
                uint64_t now_adapt_ms = lisa_os_get_tick_ms();
                bool cooldown_ok = (adaptive_fps_last_change_ms == 0) ||
                                   ((now_adapt_ms - adaptive_fps_last_change_ms) >= 2000U);

                if (adaptive_vframe_size_ref == 0) {
                    adaptive_vframe_size_ref = size;
                } else {
                    uint32_t old_ref = adaptive_vframe_size_ref;
                    adaptive_vframe_size_ref = (old_ref * 7U + size) / 8U;
                    if (adaptive_vframe_size_ref < 1024U) {
                        adaptive_vframe_size_ref = 1024U;
                    }
                }

                bool large_abs = (size >= 9000U);
                bool large_rel = (adaptive_vframe_size_ref > 0U) &&
                                 (size > (adaptive_vframe_size_ref * 17U / 10U));
                bool large_frame = large_abs || large_rel;

                if (large_frame) {
                    adaptive_vframe_big_streak++;
                } else {
                    adaptive_vframe_big_streak = 0;
                }

                if (cooldown_ok && adaptive_vframe_big_streak >= 4 &&
                    adaptive_target_fps > min_fps &&
                    adaptive_conservative_down_count < adaptive_conservative_down_max &&
                    frame_no >= 120U) {
                    uint32_t old = adaptive_target_fps;
                    uint32_t step = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_STEP;
                    if (adaptive_conservative_down_count == 0U) {
                        step *= 2U;
                    }
                    if (step == 0U) {
                        step = 1U;
                    }
                    adaptive_target_fps = (adaptive_target_fps > (min_fps + step)) ? (adaptive_target_fps - step) : min_fps;
                    adaptive_demux_low_streak = 0;
                    adaptive_demux_good_streak = 0;
                    adaptive_fps_last_change_ms = now_adapt_ms;
                    adaptive_keep_acc = 0;
                    adaptive_conservative_down_count++;
                    adaptive_vframe_big_streak = 0;
                    LISA_LOGW(LOG_TAG,
                              "adaptive fps down: %u -> %u (vsz=%u ref=%u mode=oneway n=%u)",
                              (unsigned)old,
                              (unsigned)adaptive_target_fps,
                              (unsigned)size,
                              (unsigned)adaptive_vframe_size_ref,
                              (unsigned)adaptive_conservative_down_count);
                }
            }

            if (adaptive_target_fps == 0) {
                adaptive_target_fps = effective_fps;
            }
            if (effective_fps > 0 && adaptive_target_fps < effective_fps) {
                adaptive_keep_acc += adaptive_target_fps;
                if (adaptive_keep_acc < effective_fps) {
                    (void)avi_io_seek(io, (int64_t)padded, AVI_IO_SEEK_CUR);
                    vstat_skip++;
                    frame_no++;
                    video_pts += video_frame_interval_ms;
                    if (ctrl) { ctrl->position_ms = video_pts; }
                    continue;
                }
                adaptive_keep_acc -= effective_fps;
            }
#endif

            av_render_free_fn_t free_fn = heap_free_cb;
            void *free_user = NULL;

#if defined(CONFIG_AVI_PLAYER_MJPEG_POOL)
            uint8_t *jpeg = mjpeg_alloc(size, &free_fn, &free_user);
#else
            uint8_t *jpeg = (uint8_t *)lisa_mem_alloc(size);
#endif
            if (!jpeg) {
                ret = -ENOMEM;
                break;
            }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            uint64_t t_io0 = lisa_os_get_tick_ms();
#endif
            ret = io_read_exact(io, jpeg, size);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            uint64_t t_io1 = lisa_os_get_tick_ms();
            uint32_t ioms = (t_io1 >= t_io0) ? (uint32_t)(t_io1 - t_io0) : 0;
            prof_io_ms_sum += ioms;
            if (ioms > prof_io_ms_max) {
                prof_io_ms_max = ioms;
            }
#endif
            if (ret != 0) {
                if (free_fn) {
                    free_fn(jpeg, free_user);
                } else {
                    lisa_mem_free(jpeg);
                }
                break;
            }
            if (padded > size) {
                (void)avi_io_seek(io, 1, AVI_IO_SEEK_CUR);
            }

            /* 把 MJPEG payload 推给 av_render 进行解码 */
            av_render_pkt_t vpkt = {
                .type = AV_RENDER_PKT_VIDEO_MJPEG,
                .pts_ms = video_pts,
                .buf = {
                    .data = jpeg,
                    .size = (uint32_t)size,
                    .free_fn = free_fn,
                    .free_user = free_user,
                },
            };

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG) || defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
            uint64_t t_p0 = lisa_os_get_tick_ms();
#endif
            int pret = av_render_push(render, &vpkt);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG) || defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
            uint64_t t_p1 = lisa_os_get_tick_ms();
            uint32_t pms = (t_p1 >= t_p0) ? (uint32_t)(t_p1 - t_p0) : 0;
#endif
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            prof_push_ms_sum += pms;
            if (pms > prof_push_ms_max) {
                prof_push_ms_max = pms;
            }
            prof_chunks++;
#endif
            if (pret != 0) {
                /* 出错时由 av_render 释放 jpeg */
                vstat_drop++;
            } else {
                vstat_enq++;
                vfps_total_enq++;
                if (vfps_start_ms == 0) {
                    vfps_start_ms = lisa_os_get_tick_ms();
                }

#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
                (void)pms;
#endif
            }

            cret = ctrl_wait_if_paused_or_stopped(ctrl);
            if (cret != 0) {
                ret = cret;
                break;
            }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            {
                uint64_t nowp = lisa_os_get_tick_ms();
                if (prof_last_ms == 0) {
                    prof_last_ms = nowp;
                }
                if ((nowp - prof_last_ms) >= 1000) {
                    uint32_t avg_io = (prof_chunks > 0) ? (uint32_t)(prof_io_ms_sum / prof_chunks) : 0;
                    uint32_t avg_p = (prof_chunks > 0) ? (uint32_t)(prof_push_ms_sum / prof_chunks) : 0;
                    LISA_LOGI(LOG_TAG,
                              "prof: hdr=%llums io(avg/max)=%u/%ums push(avg/max)=%u/%ums chunks=%u",
                              (unsigned long long)prof_hdr_ms,
                              (unsigned)avg_io,
                              (unsigned)prof_io_ms_max,
                              (unsigned)avg_p,
                              (unsigned)prof_push_ms_max,
                              (unsigned)prof_chunks);
                    prof_last_ms = nowp;
                    prof_hdr_ms = 0;
                    prof_io_ms_sum = 0;
                    prof_io_ms_max = 0;
                    prof_push_ms_sum = 0;
                    prof_push_ms_max = 0;
                    prof_chunks = 0;
                }
            }
#endif

            if (should_log_every(&vstat_last_ms, 1000)) {
                if (vfps_start_ms != 0 && vfps_total_enq != 0) {
                    uint64_t now_ms = lisa_os_get_tick_ms();
                    uint64_t elapsed_ms = (now_ms >= vfps_start_ms) ? (now_ms - vfps_start_ms) : 0;
                    if (elapsed_ms == 0) {
                        elapsed_ms = 1;
                    }

                    /* avg_fps_x100 = frames / (elapsed_ms/1000) * 100 */
                    uint64_t avg_fps_x100 = ((uint64_t)vfps_total_enq * 100000ULL) / elapsed_ms;
                    LISA_LOGI(LOG_TAG,
                              "video fps(avg): %u.%02u (target=%u enq=%u elapsed=%llums)",
                              (unsigned)(avg_fps_x100 / 100ULL),
                              (unsigned)(avg_fps_x100 % 100ULL),
#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
                              (unsigned)adaptive_target_fps,
#else
                              (unsigned)effective_fps,
#endif
                              (unsigned)vfps_total_enq,
                              (unsigned long long)elapsed_ms);
                }
#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
                LISA_LOGI(LOG_TAG,
                          "video stat: enq=%u drop=%u repl=%u skip=%u",
                          (unsigned)vstat_enq,
                          (unsigned)vstat_drop,
                          (unsigned)vstat_replace,
                          (unsigned)vstat_skip);
#endif
            }

            frame_no++;
            video_pts += video_frame_interval_ms;
            if (ctrl) { ctrl->position_ms = video_pts; }

            if (max_frames && frame_no >= max_frames) {
                break;
            }
        } else if (id[2] == 'w' && id[3] == 'b') {
            /* 音频（PCM/MP3） */

            cret = ctrl_wait_if_paused_or_stopped(ctrl);
            if (cret != 0) {
                ret = cret;
                break;
            }
            /*
             * 有些 AVI 会把音频 chunk 插得很晚（例如先出 ~1s 视频才开始音频）。
             * 如果这种情况下 audio_pts 仍从 0 开始，视频会显得“超前 ~1s”，
             * render 侧会等待，直到追平为止，表现为 ~1fps。
             *
             * 因此：把第一包音频 PTS 对齐到当前视频 PTS，保持 A/V 时间轴一致。
             */
            if (!seen_audio_chunk) {
                seen_audio_chunk = true;
                if (video_pts > 0 && audio_pts == 0) {
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                    LISA_LOGI(LOG_TAG, "AVI A/V align: first audio chunk, shift audio_pts 0 -> %ums", (unsigned)video_pts);
#endif
                    audio_pts = video_pts;
                }
            }

            uint32_t pkt_pts = audio_pts;

            if (info.auds_sample_rate) {
                uint64_t now_ms = lisa_os_get_tick_ms();
                if (play_start_ms == 0) {
                    play_start_ms = now_ms;
                }
            }

            uint32_t bytes_per_frame2 = 0;
            if (info.auds_block_align) {
                bytes_per_frame2 = info.auds_block_align;
            } else if (info.auds_channels && info.auds_bits) {
                bytes_per_frame2 = (info.auds_bits / 8U) * info.auds_channels;
            }

            const bool is_mp3 = (info.auds_format_tag == AVI_WAVE_FORMAT_MPEGLAYER3);
            if (is_mp3) {
                /* MP3 是压缩码流：绝不能 trim/align，否则解码器可能丢同步导致无声。 */
                bytes_per_frame2 = 0;
            }

            size_t push_size = (size_t)size;
            if (!is_mp3) {
                if (bytes_per_frame2) {
                    push_size = (push_size / (size_t)bytes_per_frame2) * (size_t)bytes_per_frame2;
                } else {
                    /* 至少保证 s16 采样对齐 */
                    push_size = (push_size / sizeof(int16_t)) * sizeof(int16_t);
                }
            }

            /* 直接把音频 payload 读到 pool/heap 内存并 push（demux->render 零拷贝）。 */
            av_render_free_fn_t free_fn = heap_free_cb;
            void *free_user = NULL;

#if defined(CONFIG_AVI_PLAYER_AUDIO_POOL)
            uint8_t *pcm = audio_alloc(size, &free_fn, &free_user);
#else
            uint8_t *pcm = (uint8_t *)lisa_mem_alloc(size);
#endif
            if (!pcm) {
                ret = -ENOMEM;
                break;
            }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            uint64_t t_io0 = lisa_os_get_tick_ms();
#endif
            ret = io_read_exact(io, pcm, size);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            uint64_t t_io1 = lisa_os_get_tick_ms();
            uint32_t ioms = (t_io1 >= t_io0) ? (uint32_t)(t_io1 - t_io0) : 0;
            prof_io_ms_sum += ioms;
            if (ioms > prof_io_ms_max) {
                prof_io_ms_max = ioms;
            }
#endif
            if (ret != 0) {
                if (free_fn) {
                    free_fn(pcm, free_user);
                } else {
                    lisa_mem_free(pcm);
                }
                break;
            }
            if (padded > size) {
                (void)avi_io_seek(io, 1, AVI_IO_SEEK_CUR);
            }

            /* demux 侧带宽统计（真实 1s 窗口；避免首次打印 bps=0） */
            astat_bytes_window += (uint64_t)size;
            astat_chunks_window++;
            uint64_t now_ms = lisa_os_get_tick_ms();
            if (astat_last_window_ms == 0) {
                astat_last_window_ms = now_ms;
            }
            if (astat_last_ms == 0) {
                astat_last_ms = now_ms;
            }
            if ((now_ms - astat_last_ms) >= 1000) {
                uint64_t delta_ms = now_ms - astat_last_window_ms;
                uint32_t bps = 0;
                if (delta_ms > 0) {
                    bps = (uint32_t)((astat_bytes_window * 1000ULL) / delta_ms);
                }

                if (expected_audio_bps == 0 && delta_ms > 0 && astat_chunks_window >= 8 && bps > 0 && !drop_video_for_audio) {
                    if (adaptive_audio_bps_ref == 0) {
                        adaptive_audio_bps_ref = bps;
                    } else if (bps >= adaptive_audio_bps_ref) {
                        uint32_t delta = bps - adaptive_audio_bps_ref;
                        adaptive_audio_bps_ref += (delta / 8U) + 1U;
                    } else {
                        uint32_t delta = adaptive_audio_bps_ref - bps;
                        uint32_t decay = (delta / 4U) + 1U;
                        adaptive_audio_bps_ref = (adaptive_audio_bps_ref > decay) ? (adaptive_audio_bps_ref - decay) : bps;
                    }
                    if (adaptive_audio_bps_ref < 1000U) {
                        adaptive_audio_bps_ref = 1000U;
                    }
                    adaptive_audio_bps_windows++;
                }

                if (expected_audio_bps) {
                    /*
                     * 仅当音频带宽“严重低于”期望值且持续一段时间，才允许丢视频；
                     * 否则视频会抖动过于明显。
                     *
                     * 滞回：
                     * - 连续 2 个低窗口后启用
                     * - 连续 2 个好窗口后关闭
                     */
                    bool low = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps < (expected_audio_bps * 6U / 10U));
                    bool good = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps > (expected_audio_bps * 9U / 10U));

                    if (low) {
                        drop_low_streak++;
                        drop_good_streak = 0;
                    } else if (good) {
                        drop_good_streak++;
                        drop_low_streak = 0;
                    } else {
                        drop_low_streak = 0;
                        drop_good_streak = 0;
                    }

                    if (!drop_video_for_audio && drop_low_streak >= 2) {
                        drop_video_for_audio = 1;
                    } else if (drop_video_for_audio && drop_good_streak >= 2) {
                        drop_video_for_audio = 0;
                    }

#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
                    {
                        bool fps_low = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps < (expected_audio_bps * 88U / 100U));
                        bool fps_good = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps > (expected_audio_bps * 98U / 100U));
                        uint64_t now_adapt_ms = lisa_os_get_tick_ms();
                        bool cooldown_ok = (adaptive_fps_last_change_ms == 0) ||
                                           ((now_adapt_ms - adaptive_fps_last_change_ms) >= 2000U);

                        if (fps_low) {
                            adaptive_demux_low_streak++;
                            adaptive_demux_good_streak = 0;
                        } else if (fps_good) {
                            adaptive_demux_good_streak++;
                            adaptive_demux_low_streak = 0;
                        } else {
                            adaptive_demux_low_streak = 0;
                            adaptive_demux_good_streak = 0;
                        }

                        if (cooldown_ok && adaptive_demux_low_streak >= 3 &&
                            adaptive_target_fps > (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_MIN &&
                            adaptive_conservative_down_count < adaptive_conservative_down_max) {
                            uint32_t old = adaptive_target_fps;
                            uint32_t step = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_STEP;
                            uint32_t min_fps = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_MIN;
                            adaptive_target_fps = (adaptive_target_fps > (min_fps + step)) ? (adaptive_target_fps - step) : min_fps;
                            adaptive_demux_low_streak = 0;
                            adaptive_demux_good_streak = 0;
                            adaptive_fps_last_change_ms = now_adapt_ms;
                            adaptive_keep_acc = 0;
                            adaptive_conservative_down_count++;
                            LISA_LOGW(LOG_TAG,
                                      "adaptive fps down: %u -> %u (demux bps=%u exp=%u mode=oneway n=%u)",
                                      (unsigned)old,
                                      (unsigned)adaptive_target_fps,
                                      (unsigned)bps,
                                      (unsigned)expected_audio_bps,
                                      (unsigned)adaptive_conservative_down_count);
                        }
                    }
#endif

#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
                    LISA_LOGI(LOG_TAG,
                              "audio demux stat: bps=%u exp=%u chunks=%u drop_video=%u",
                              (unsigned)bps,
                              (unsigned)expected_audio_bps,
                              (unsigned)astat_chunks_window,
                              (unsigned)drop_video_for_audio);
#endif
                } else {
                    bool low = false;
                    bool good = false;

                    if (adaptive_audio_bps_windows >= 3 && adaptive_audio_bps_ref > 0) {
                        low = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps < (adaptive_audio_bps_ref * 65U / 100U));
                        good = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps > (adaptive_audio_bps_ref * 90U / 100U));

                        if (low) {
                            drop_low_streak++;
                            drop_good_streak = 0;
                        } else if (good) {
                            drop_good_streak++;
                            drop_low_streak = 0;
                        } else {
                            drop_low_streak = 0;
                            drop_good_streak = 0;
                        }

                        if (!drop_video_for_audio && drop_low_streak >= 2) {
                            drop_video_for_audio = 1;
                        } else if (drop_video_for_audio && drop_good_streak >= 2) {
                            drop_video_for_audio = 0;
                        }

#if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
                        {
                            bool fps_low = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps < (adaptive_audio_bps_ref * 86U / 100U));
                            bool fps_good = (delta_ms > 0) && (astat_chunks_window >= 8) && (bps > (adaptive_audio_bps_ref * 95U / 100U));
                            uint64_t now_adapt_ms = lisa_os_get_tick_ms();
                            bool cooldown_ok = (adaptive_fps_last_change_ms == 0) ||
                                               ((now_adapt_ms - adaptive_fps_last_change_ms) >= 2000U);

                            if (fps_low) {
                                adaptive_demux_low_streak++;
                                adaptive_demux_good_streak = 0;
                            } else if (fps_good) {
                                adaptive_demux_good_streak++;
                                adaptive_demux_low_streak = 0;
                            } else {
                                adaptive_demux_low_streak = 0;
                                adaptive_demux_good_streak = 0;
                            }

                            if (cooldown_ok && adaptive_demux_low_streak >= 3 &&
                                adaptive_target_fps > (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_MIN &&
                                adaptive_audio_bps_windows >= 6 &&
                                adaptive_conservative_down_count < adaptive_conservative_down_max) {
                                uint32_t old = adaptive_target_fps;
                                uint32_t step = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_STEP;
                                uint32_t min_fps = (uint32_t)CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS_MIN;
                                adaptive_target_fps = (adaptive_target_fps > (min_fps + step)) ? (adaptive_target_fps - step) : min_fps;
                                adaptive_demux_low_streak = 0;
                                adaptive_demux_good_streak = 0;
                                adaptive_fps_last_change_ms = now_adapt_ms;
                                adaptive_keep_acc = 0;
                                adaptive_conservative_down_count++;
                                LISA_LOGW(LOG_TAG,
                                          "adaptive fps down: %u -> %u (demux bps=%u ref=%u mode=oneway n=%u)",
                                          (unsigned)old,
                                          (unsigned)adaptive_target_fps,
                                          (unsigned)bps,
                                          (unsigned)adaptive_audio_bps_ref,
                                          (unsigned)adaptive_conservative_down_count);
                            }
                        }
#endif
                    }

#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
                    LISA_LOGI(LOG_TAG,
                              "audio demux stat: bps=%u ref=%u w=%u chunks=%u drop_video=%u",
                              (unsigned)bps,
                              (unsigned)adaptive_audio_bps_ref,
                              (unsigned)adaptive_audio_bps_windows,
                              (unsigned)astat_chunks_window,
                              (unsigned)drop_video_for_audio);
#endif
                }

                astat_last_ms = now_ms;
                astat_last_window_ms = now_ms;
                astat_bytes_window = 0;
                astat_chunks_window = 0;
            }

            /* 推送音频 chunk */
            if (push_size > 0) {
                av_render_pkt_t apkt = {
                    .type = is_mp3 ? AV_RENDER_PKT_AUDIO_MP3 : AV_RENDER_PKT_AUDIO_PCM_S16LE,
                    .pts_ms = pkt_pts,
                    .buf = {
                        .data = pcm,
                        .size = (uint32_t)push_size,
                        .free_fn = free_fn,
                        .free_user = free_user,
                    },
                };

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                uint64_t t_p0 = lisa_os_get_tick_ms();
#endif
                int pret = av_render_push(render, &apkt);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                uint64_t t_p1 = lisa_os_get_tick_ms();
                uint32_t pms = (t_p1 >= t_p0) ? (uint32_t)(t_p1 - t_p0) : 0;
                prof_push_ms_sum += pms;
                if (pms > prof_push_ms_max) {
                    prof_push_ms_max = pms;
                }
                prof_chunks++;
#endif
                if (pret != 0) {
                    /* 出错时由 av_render 释放 pcm */
                }
            } else {
                if (free_fn) {
                    free_fn(pcm, free_user);
                } else {
                    lisa_mem_free(pcm);
                }
            }

            cret = ctrl_wait_if_paused_or_stopped(ctrl);
            if (cret != 0) {
                ret = cret;
                break;
            }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            {
                uint64_t nowp = lisa_os_get_tick_ms();
                if (prof_last_ms == 0) {
                    prof_last_ms = nowp;
                }
                if ((nowp - prof_last_ms) >= 1000) {
                    uint32_t avg_io = (prof_chunks > 0) ? (uint32_t)(prof_io_ms_sum / prof_chunks) : 0;
                    uint32_t avg_p = (prof_chunks > 0) ? (uint32_t)(prof_push_ms_sum / prof_chunks) : 0;
                    LISA_LOGI(LOG_TAG,
                              "prof: hdr=%llums io(avg/max)=%u/%ums push(avg/max)=%u/%ums chunks=%u",
                              (unsigned long long)prof_hdr_ms,
                              (unsigned)avg_io,
                              (unsigned)prof_io_ms_max,
                              (unsigned)avg_p,
                              (unsigned)prof_push_ms_max,
                              (unsigned)prof_chunks);
                    prof_last_ms = nowp;
                    prof_hdr_ms = 0;
                    prof_io_ms_sum = 0;
                    prof_io_ms_max = 0;
                    prof_push_ms_sum = 0;
                    prof_push_ms_max = 0;
                    prof_chunks = 0;
                }
            }
#endif

            /* Update demux-side audio pts by estimated chunk duration (monotonic).
             * - PCM: exact by samples
             * - MP3 (CBR/VBR): estimate by avg_bytes_per_sec; decoder will generate accurate PCM pacing by decoded samples.
             */
            if (info.auds_format_tag == AVI_WAVE_FORMAT_PCM) {
                if (info.auds_sample_rate && bytes_per_frame2) {
                    uint32_t frames = (uint32_t)(push_size / (size_t)bytes_per_frame2);
                    uint32_t dur_ms = (uint32_t)((uint64_t)frames * 1000ULL / info.auds_sample_rate);
                    audio_pts += dur_ms;
                }
            } else if (info.auds_format_tag == AVI_WAVE_FORMAT_MPEGLAYER3) {
                if (info.auds_avg_bytes_per_sec) {
                    uint32_t dur_ms = (uint32_t)((uint64_t)push_size * 1000ULL / (uint64_t)info.auds_avg_bytes_per_sec);
                    audio_pts += dur_ms;
                }
            }
        } else {
            /* 未知/忽略的 chunk */
            (void)avi_io_seek(io, (int64_t)padded, AVI_IO_SEEK_CUR);
        }
    }

    /* 播放结束时打印一次总平均帧率（不依赖 CONFIG_AVI_PLAYER_STATS_LOG）。 */
    if (vfps_start_ms != 0 && vfps_total_enq != 0) {
        uint64_t now_ms = lisa_os_get_tick_ms();
        uint64_t elapsed_ms = (now_ms >= vfps_start_ms) ? (now_ms - vfps_start_ms) : 0;
        if (elapsed_ms == 0) {
            elapsed_ms = 1;
        }
        uint64_t avg_fps_x100 = ((uint64_t)vfps_total_enq * 100000ULL) / elapsed_ms;
        LISA_LOGI(LOG_TAG,
                  "video fps(avg total): %u.%02u (target=%u enq=%u elapsed=%llums)",
                  (unsigned)(avg_fps_x100 / 100ULL),
                  (unsigned)(avg_fps_x100 % 100ULL),
    #if defined(CONFIG_AVI_PLAYER_ADAPTIVE_DECODE_SLOW_FPS)
              (unsigned)adaptive_target_fps,
    #else
                  (unsigned)effective_fps,
    #endif
                  (unsigned)vfps_total_enq,
                  (unsigned long long)elapsed_ms);
    }

    return ret;
}

int avi_player_play_io(avi_io_t *io, av_render_t *render, uint32_t max_frames)
{
    return avi_player_play_io_ex(io, render, max_frames, NULL);
}

int avi_player_play_file_ex(const char *path, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl)
{
    if (!path || !render) {
        return -EINVAL;
    }

    avi_io_t io;
    int ret = avi_io_open_lsfs(&io, path);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Open AVI failed: %s (ret=%d)", path, ret);
        return ret;
    }

    ret = avi_player_play_io_ex(&io, render, max_frames, ctrl);
    (void)avi_io_close(&io);

    if (ret == 0) {
        LISA_LOGI(LOG_TAG, "AVI playback finished: %s", path);
    }
    return ret;
}

int avi_player_play_file(const char *path, av_render_t *render, uint32_t max_frames)
{
    return avi_player_play_file_ex(path, render, max_frames, NULL);
}

#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)
int avi_player_play_url_ex(const char *url, av_render_t *render, uint32_t max_frames, avi_player_ctrl_t *ctrl)
{
    if (!url || !render) {
        return -EINVAL;
    }

    avi_io_t io;
    int ret = avi_io_open_http_range(&io, url);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Open AVI URL failed: %s (ret=%d)", url, ret);
        return ret;
    }

    ret = avi_player_play_io_ex(&io, render, max_frames, ctrl);
    (void)avi_io_close(&io);
    return ret;
}

int avi_player_play_url(const char *url, av_render_t *render, uint32_t max_frames)
{
    return avi_player_play_url_ex(url, render, max_frames, NULL);
}
#endif
