/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "av_render_internal.h"

#include <errno.h>
#include <string.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>

#ifndef LOG_TAG
#define LOG_TAG "av_render"
#endif

static void free_video_frame(av_render_video_frame_t *f)
{
    if (f && f->data) {
        if (f->free_fn) {
            f->free_fn(f->data, f->free_user);
        } else {
            lisa_mem_free(f->data);
        }
        f->data = NULL;
    }
}

static void free_audio_frame(av_render_audio_frame_t *f)
{
    if (f && f->data) {
        if (f->free_fn) {
            f->free_fn(f->data, f->free_user);
        } else {
            lisa_mem_free(f->data);
        }
        f->data = NULL;
    }
}

static void default_free(void *data, void *user)
{
    (void)user;
    if (data) {
        lisa_mem_free(data);
    }
}

static void release_buf(uint8_t *data, av_render_free_fn_t free_fn, void *free_user)
{
    if (!data) {
        return;
    }
    if (free_fn) {
        free_fn(data, free_user);
    } else {
        lisa_mem_free(data);
    }
}

static bool calc_audio_clock_ms_if_ready(const av_render_t *render, uint32_t *out_audio_ms)
{
    if (!render || !out_audio_ms) {
        return false;
    }
    if (render->cfg.sync_mode != AV_RENDER_SYNC_AUDIO) {
        return false;
    }
    if (!render->audio_started || render->audio_start_tick_ms == 0) {
        return false;
    }
    uint64_t now = lisa_os_get_tick_ms();
    if (now < render->audio_start_tick_ms) {
        return false;
    }
    *out_audio_ms = (uint32_t)(now - render->audio_start_tick_ms);
    return true;
}

static void video_ahead_backpressure(av_render_t *render, uint32_t video_pts_ms)
{
    if (!render || render->cfg.max_video_ahead_ms == 0) {
        return;
    }

    uint32_t audio_ms = 0;
    if (!calc_audio_clock_ms_if_ready(render, &audio_ms)) {
        return;
    }

    int32_t ahead = (int32_t)video_pts_ms - (int32_t)audio_ms;

    /*
        * 把“已缓冲”考虑进去，但要克制：
        * - 用当前 video_q 占用来估算已经缓冲的时间。
        * - 最多只扣掉 max_video_ahead_ms 的一半；否则可能把 video 线程饿死
        *   （ahead 变得太小，pop 超时增多）。
     */
    uint32_t max_ms = render->cfg.max_video_ahead_ms;
    uint32_t effective_max = max_ms;
    if (render->video_stream_set && render->video_info.fps > 0) {
        uint32_t frame_ms = 1000U / (uint32_t)render->video_info.fps;
        if (frame_ms == 0) {
            frame_ms = 1;
        }

        uint32_t q_wait = 0;
        if (render->video_q) {
            q_wait = (uint32_t)lisa_queue_waiting(render->video_q);
        }

        /* +1：近似考虑 video 线程手里“挂着”的 pending frame */
        uint32_t buffered_ms = (q_wait + 1U) * frame_ms;
        uint32_t max_sub = max_ms / 2U;
        uint32_t sub = (buffered_ms < max_sub) ? buffered_ms : max_sub;
        effective_max = max_ms - sub;
    }

    int32_t excess = ahead - (int32_t)effective_max;
    if (excess <= 0) {
        return;
    }

    /*
        * 重要：每个包在这里都不要阻塞 demux 线程太久。
        * 如果 sleep 太长，会导致 chunk 吞吐下降，从而饿死 decoder/video 线程。
     */
    uint32_t wait_ms = (uint32_t)excess;
    if (wait_ms > 50) {
        wait_ms = 50;
    }
    lisa_thread_mdelay(wait_ms);

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    static uint64_t s_last_ms = 0;
    static uint32_t s_cnt = 0;
    static uint64_t s_sum = 0;
    static uint32_t s_max = 0;

    uint64_t now = lisa_os_get_tick_ms();
    if (s_last_ms == 0) {
        s_last_ms = now;
    }

    s_cnt++;
    s_sum += wait_ms;
    if (wait_ms > s_max) {
        s_max = wait_ms;
    }

    if ((now - s_last_ms) >= 1000) {
        uint32_t avg = (s_cnt > 0) ? (uint32_t)(s_sum / s_cnt) : 0;
        LISA_LOGI(LOG_TAG, "prof: push_throttle avg=%ums max=%ums cnt=%u", avg, s_max, s_cnt);
        s_last_ms = now;
        s_cnt = 0;
        s_sum = 0;
        s_max = 0;
    }
#endif
}

static int wait_for_threads_exit(av_render_t *render, bool wait_video, bool wait_audio, bool wait_vdec, bool wait_adec, uint32_t timeout_ms)
{
    if (!render) {
        return -EINVAL;
    }

    uint64_t start = lisa_os_get_tick_ms();
    while (1) {
        bool v_ok = !wait_video || render->video_thread_exited;
        bool a_ok = !wait_audio || render->audio_thread_exited;
        bool d_ok = !wait_vdec || render->video_codec_thread_exited;
        bool ad_ok = !wait_adec || render->audio_codec_thread_exited;

        if (v_ok && a_ok && d_ok && ad_ok) {
            return 0;
        }

        uint64_t now = lisa_os_get_tick_ms();
        if (timeout_ms > 0 && (now - start) >= timeout_ms) {
            return -ETIMEDOUT;
        }
        lisa_thread_mdelay(10);
    }
}

static void video_thread_entry(void *arg)
{
    av_render_t *r = (av_render_t *)arg;

    uint64_t stat_last_ms = 0;
    uint32_t rendered = 0;
    uint32_t drained = 0;
    uint32_t late_render = 0;

    uint64_t last_render_tick_ms = 0;

    av_render_video_frame_t pending = {0};
    bool have_pending = false;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
    uint64_t prof_last_ms = 0;
    uint32_t prof_pop_ok = 0;
    uint32_t prof_pop_timeout = 0;
    uint64_t prof_sync_wait_ms_sum = 0;
    uint32_t prof_sync_wait_ms_max = 0;
    uint32_t prof_sync_wait_cnt = 0;
    int32_t prof_ahead_ms_max = 0;
#endif

    /* 初始化 renderer（自定义或内置） */
    const av_render_video_renderer_ops_t *vops = NULL;
    void *vuser = NULL;
    if (r->state_lock) {
        lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
        vops = r->video_renderer_ops;
        vuser = r->video_renderer_user;
        lisa_mutex_unlock(r->state_lock);
    }
    if (vops && vops->init) {
        (void)vops->init(r, vuser);
    } else {
        (void)video_render_init(r);
    }

    while (!r->should_stop) {
        if (!have_pending) {
            lisa_err_t err = lisa_queue_pop(r->video_q, &pending, sizeof(pending), 50);
            if (err != LISA_OK) {
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                prof_pop_timeout++;
#endif
                continue;
            }

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            prof_pop_ok++;
#endif
            have_pending = true;
        }

        if (!r->video_stream_set || !pending.data || pending.size == 0) {
            free_video_frame(&pending);
            have_pending = false;
            continue;
        }

        const bool sync_enabled = (r->cfg.sync_mode == AV_RENDER_SYNC_AUDIO && r->audio_started && r->audio_start_tick_ms != 0);

        /*
         * Queue draining policy:
         * - No sync: keep newest frame (latest-frame, low latency).
         * - Audio sync: do NOT drain.
         *   Keep FIFO order so we can render consecutive frames (50ms apart).
         *   Draining in sync mode discards future frames and can collapse output to ~1fps.
         */
        if (!sync_enabled) {
            while (lisa_queue_waiting(r->video_q) > 0) {
                av_render_video_frame_t newer = {0};
                if (lisa_queue_pop(r->video_q, &newer, sizeof(newer), 0) != LISA_OK) {
                    break;
                }
                free_video_frame(&pending);
                pending = newer;
                drained++;
            }
        }

        /* 可选的帧率限制（尽力而为） */
        if (r->cfg.video_fps_limit > 0) {
            uint32_t min_interval_ms = 1000U / (uint32_t)r->cfg.video_fps_limit;
            if (min_interval_ms == 0) {
                min_interval_ms = 1;
            }

            uint64_t now_rl = lisa_os_get_tick_ms();
            if (last_render_tick_ms != 0 && (now_rl - last_render_tick_ms) < min_interval_ms) {
                lisa_thread_mdelay((uint32_t)(min_interval_ms - (uint32_t)(now_rl - last_render_tick_ms)));
            }

            /* 节流等待后，仅在“非同步模式”下再次 drain */
            if (!sync_enabled) {
                while (lisa_queue_waiting(r->video_q) > 0) {
                    av_render_video_frame_t newer = {0};
                    if (lisa_queue_pop(r->video_q, &newer, sizeof(newer), 0) != LISA_OK) {
                        break;
                    }
                    free_video_frame(&pending);
                    pending = newer;
                    drained++;
                }
            }
        }

        bool is_late = false;
        if (sync_enabled) {
            uint32_t interval_ms = (r->video_info.fps > 0) ? (1000U / r->video_info.fps) : 40;
            uint64_t now = lisa_os_get_tick_ms();
            uint32_t audio_cur_ms = (uint32_t)(now - r->audio_start_tick_ms);
            int32_t ahead = (int32_t)pending.pts_ms - (int32_t)audio_cur_ms;

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            if (ahead > prof_ahead_ms_max) {
                prof_ahead_ms_max = ahead;
            }
#endif

            if (ahead > 2) {
                /*
                 * 避免长时间阻塞 sleep：采用小切片等待，
                 * 这样可以持续处理队列，降低解码侧丢帧概率。
                 */
                uint32_t want = (uint32_t)ahead;
                uint32_t slice = want;
                if (slice > 20) {
                    slice = 20;
                }
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                uint64_t t0 = lisa_os_get_tick_ms();
#endif
                lisa_thread_mdelay(slice);
#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
                uint64_t t1 = lisa_os_get_tick_ms();
                uint32_t wms = (t1 >= t0) ? (uint32_t)(t1 - t0) : 0;
                prof_sync_wait_ms_sum += wms;
                if (wms > prof_sync_wait_ms_max) {
                    prof_sync_wait_ms_max = wms;
                }
                prof_sync_wait_cnt++;
#endif
                continue;
            }

            if ((-ahead) > (int32_t)(interval_ms * 3U)) {
                /* 已明显落后：立即渲染（不丢帧），否则屏幕可能“卡住不动” */
                is_late = true;
            }
        }

        /* 每帧快照 ops：支持运行期动态注册/替换 */
        vops = NULL;
        vuser = NULL;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            vops = r->video_renderer_ops;
            vuser = r->video_renderer_user;
            lisa_mutex_unlock(r->state_lock);
        }

        if (vops && vops->write_rgb565) {
            (void)vops->write_rgb565(r, pending.data, pending.size, pending.width, pending.height, pending.pts_ms, vuser);
        } else {
            (void)video_render_write_rgb565(r, &pending);
        }
        last_render_tick_ms = lisa_os_get_tick_ms();
        if (is_late) {
            late_render++;
        }
        rendered++;
        free_video_frame(&pending);
        have_pending = false;

        if (stat_last_ms == 0) {
            stat_last_ms = lisa_os_get_tick_ms();
        }
        uint64_t now2 = lisa_os_get_tick_ms();
        if ((now2 - stat_last_ms) >= 1000) {
#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
            LISA_LOGI(LOG_TAG, "video stat: render=%u late_render=%u drained=%u q_wait=%u",
                      (unsigned)rendered,
                      (unsigned)late_render,
                      (unsigned)drained,
                      (unsigned)lisa_queue_waiting(r->video_q));
#endif

#if defined(CONFIG_AVI_PLAYER_PROFILE_LOG)
            {
                uint64_t nowp = lisa_os_get_tick_ms();
                if (prof_last_ms == 0) {
                    prof_last_ms = nowp;
                }
                if ((nowp - prof_last_ms) >= 1000) {
                    uint32_t avg_wait = (prof_sync_wait_cnt > 0) ? (uint32_t)(prof_sync_wait_ms_sum / prof_sync_wait_cnt) : 0;
                    LISA_LOGI(LOG_TAG,
                              "prof: pop(ok/to)=%u/%u sync_wait(avg/max)=%u/%ums cnt=%u ahead_max=%dms",
                              (unsigned)prof_pop_ok,
                              (unsigned)prof_pop_timeout,
                              (unsigned)avg_wait,
                              (unsigned)prof_sync_wait_ms_max,
                              (unsigned)prof_sync_wait_cnt,
                              (int)prof_ahead_ms_max);
                    prof_last_ms = nowp;
                    prof_pop_ok = 0;
                    prof_pop_timeout = 0;
                    prof_sync_wait_ms_sum = 0;
                    prof_sync_wait_ms_max = 0;
                    prof_sync_wait_cnt = 0;
                    prof_ahead_ms_max = 0;
                }
            }
#endif
            stat_last_ms = now2;
            rendered = 0;
            late_render = 0;
            drained = 0;
        }
    }

    while (lisa_queue_waiting(r->video_q) > 0) {
        av_render_video_frame_t frame = {0};
        if (lisa_queue_pop(r->video_q, &frame, sizeof(frame), 0) != LISA_OK) {
            break;
        }
        free_video_frame(&frame);
    }

    if (have_pending) {
        free_video_frame(&pending);
        have_pending = false;
    }

    /* 反初始化 renderer（仅自定义实现需要；内置实现无需 deinit） */
    vops = NULL;
    vuser = NULL;
    if (r->state_lock) {
        lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
        vops = r->video_renderer_ops;
        vuser = r->video_renderer_user;
        lisa_mutex_unlock(r->state_lock);
    }
    if (vops && vops->deinit) {
        vops->deinit(r, vuser);
    }

    if (r) {
        r->video_thread_exited = true;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            r->video_thread = NULL;
            lisa_mutex_unlock(r->state_lock);
        } else {
            r->video_thread = NULL;
        }
    }
}

static void audio_thread_entry(void *arg)
{
    av_render_t *r = (av_render_t *)arg;

    uint64_t stat_last_ms = 0;
    uint32_t pop_ok = 0;
    uint32_t pop_timeout = 0;

    while (!r->should_stop) {
        av_render_audio_frame_t frame = {0};
        lisa_err_t err = lisa_queue_pop(r->audio_q, &frame, sizeof(frame), 50);
        if (err != LISA_OK) {
            pop_timeout++;
            if (stat_last_ms == 0) {
                stat_last_ms = lisa_os_get_tick_ms();
            }
            uint64_t now = lisa_os_get_tick_ms();
            if ((now - stat_last_ms) >= 1000) {
#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
                LISA_LOGI(LOG_TAG, "audio pop stat: ok=%u timeout=%u q_wait=%u",
                          (unsigned)pop_ok,
                          (unsigned)pop_timeout,
                          (unsigned)lisa_queue_waiting(r->audio_q));
#endif
                stat_last_ms = now;
                pop_ok = 0;
                pop_timeout = 0;
            }
            continue;
        }

        pop_ok++;
        if (stat_last_ms == 0) {
            stat_last_ms = lisa_os_get_tick_ms();
        }
        uint64_t now = lisa_os_get_tick_ms();
        if ((now - stat_last_ms) >= 1000) {
#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
            LISA_LOGI(LOG_TAG, "audio pop stat: ok=%u timeout=%u q_wait=%u",
                      (unsigned)pop_ok,
                      (unsigned)pop_timeout,
                      (unsigned)lisa_queue_waiting(r->audio_q));
#endif
            stat_last_ms = now;
            pop_ok = 0;
            pop_timeout = 0;
        }

        if (!frame.data || frame.size == 0) {
            free_audio_frame(&frame);
            continue;
        }

        av_render_sync_on_audio_frame(r, frame.pts_ms);

        /* 每帧快照 ops：支持运行期动态注册/替换 */
        const av_render_audio_renderer_ops_t *aops = NULL;
        void *auser = NULL;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            aops = r->audio_renderer_ops;
            auser = r->audio_renderer_user;
            lisa_mutex_unlock(r->state_lock);
        }

        if (aops && aops->write_pcm_s16le) {
            int wret = aops->write_pcm_s16le(r, frame.data, frame.size, frame.pts_ms, auser);
            if (wret == 0) {
                r->audio_started = true;
            }
        } else {
            (void)audio_render_write_pcm_s16le(r, &frame);
        }
        free_audio_frame(&frame);
    }

    while (lisa_queue_waiting(r->audio_q) > 0) {
        av_render_audio_frame_t frame = {0};
        if (lisa_queue_pop(r->audio_q, &frame, sizeof(frame), 0) != LISA_OK) {
            break;
        }
        free_audio_frame(&frame);
    }

    /* 停止 renderer */
    const av_render_audio_renderer_ops_t *aops = NULL;
    void *auser = NULL;
    if (r->state_lock) {
        lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
        aops = r->audio_renderer_ops;
        auser = r->audio_renderer_user;
        lisa_mutex_unlock(r->state_lock);
    }
    if (aops && aops->stop) {
        aops->stop(r, auser);
    } else {
        audio_render_stop(r);
    }

    if (r) {
        r->audio_thread_exited = true;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            r->audio_thread = NULL;
            lisa_mutex_unlock(r->state_lock);
        } else {
            r->audio_thread = NULL;
        }
    }
}

int av_render_open(const av_render_cfg_t *cfg, av_render_t **out)
{
    if (!cfg || !out) {
        return -EINVAL;
    }

    av_render_t *r = (av_render_t *)lisa_mem_alloc(sizeof(*r));
    if (!r) {
        return -ENOMEM;
    }
    memset(r, 0, sizeof(*r));
    r->cfg = *cfg;
    r->running = true;
    r->video_thread_exited = false;
    r->audio_thread_exited = false;
    r->video_codec_thread_exited = false;

    if (r->cfg.video_queue_depth == 0) {
        r->cfg.video_queue_depth = 4;
    }
    if (r->cfg.audio_queue_depth == 0) {
        /* 默认值稍大：在视频解码较重时降低音频 underrun 概率 */
        r->cfg.audio_queue_depth = 48;
    }

    r->state_lock = lisa_mutex_create();
    if (!r->state_lock) {
        lisa_mem_free(r);
        return -ENOMEM;
    }

    /*
     * 默认安装 LISA render_impl：
     * - cfg->display_dev/audio_dev 是 LISA 设备句柄时，使用内置实现即可完成输出。
     * - 上层如果注册了自定义 renderer，会在运行期覆盖这些默认值。
     */
    if (r->cfg.display_dev) {
        r->video_renderer_ops = &av_render_lisa_video_renderer_ops;
        r->video_renderer_user = NULL;
    }
    if (r->cfg.audio_dev) {
        r->audio_renderer_ops = &av_render_lisa_audio_renderer_ops;
        r->audio_renderer_user = NULL;
    }

    r->video_q = lisa_queue_create(r->cfg.video_queue_depth, (uint8_t *)"av_vq", sizeof(av_render_video_frame_t));
    r->audio_q = lisa_queue_create(r->cfg.audio_queue_depth, (uint8_t *)"av_aq", sizeof(av_render_audio_frame_t));
    if (!r->video_q || !r->audio_q) {
        av_render_close(r);
        return -ENOMEM;
    }

    lisa_thread_attr_t vattr = {
        .name = (uint8_t *)"av_video",
        .stack_size = 6 * 1024,
        .priority = 3,
    };
    lisa_thread_attr_t aattr = {
        .name = (uint8_t *)"av_audio",
        .stack_size = 6 * 1024,
        .priority = 4,
    };
    r->video_thread = lisa_thread_create(&vattr, video_thread_entry, r);
    r->audio_thread = lisa_thread_create(&aattr, audio_thread_entry, r);

    if (!r->video_thread || !r->audio_thread) {
        av_render_close(r);
        return -ENOMEM;
    }

    *out = r;
    return 0;
}

int av_render_close(av_render_t *render)
{
    if (!render) {
        return 0;
    }

    bool wait_video = (render->video_thread != NULL);
    bool wait_audio = (render->audio_thread != NULL);
    bool wait_vdec = (render->video_codec_thread != NULL);
    bool wait_adec = (render->audio_codec_thread != NULL);

    render->running = false;
    render->should_stop = true;

    /*
     * 重要说明：
     * - 本 SDK 中 lisa_thread_delete() 基本是 no-op；线程在入口函数返回后自销毁。
     * - 因此，在工作线程退出前，绝不能删除 queue/mutex/render 等资源。
     */
    int wret = wait_for_threads_exit(render, wait_video, wait_audio, wait_vdec, wait_adec, 5000);
    if (wret != 0) {
        /* 安全优先：线程可能仍在访问资源时，不要释放 */
        return wret;
    }

    /* 关闭解码器（自定义或内置）：必须在各线程不再访问队列之后 */
    const av_render_mjpeg_decoder_ops_t *dops = NULL;
    void *duser = NULL;
    if (render->state_lock) {
        lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
        dops = render->mjpeg_decoder_ops;
        duser = render->mjpeg_decoder_user;
        lisa_mutex_unlock(render->state_lock);
    }
    if (dops && dops->close) {
        dops->close(render, duser);
    } else {
#if defined(CONFIG_AVI_PLAYER_VIDEO_DECODER_LISA_JPEG)
        av_render_lisa_jpeg_close(render);
#else
        av_render_mjpeg_close(render);
#endif
    }

    /* 关闭音频解码器（自定义或内置）：必须在各线程不再访问队列之后 */
    const av_render_audio_decoder_ops_t *aops_dec = NULL;
    void *auser_dec = NULL;
    if (render->state_lock) {
        lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
        aops_dec = render->audio_decoder_ops;
        auser_dec = render->audio_decoder_user;
        lisa_mutex_unlock(render->state_lock);
    }
    if (aops_dec && aops_dec->close) {
        aops_dec->close(render, auser_dec);
    } else {
        av_render_mp3_close(render);
    }

    if (render->video_q) {
        lisa_queue_delete(render->video_q);
        render->video_q = NULL;
    }
    if (render->audio_q) {
        lisa_queue_delete(render->audio_q);
        render->audio_q = NULL;
    }
    if (render->state_lock) {
        lisa_mutex_delete(render->state_lock);
        render->state_lock = NULL;
    }

    if (render->rotate_buf) {
        lisa_mem_free(render->rotate_buf);
        render->rotate_buf = NULL;
        render->rotate_buf_size = 0;
    }

    if (render->video_rgb_pool_q) {
        lisa_queue_delete(render->video_rgb_pool_q);
        render->video_rgb_pool_q = NULL;
    }
    if (render->video_rgb_pool_mem) {
        lisa_mem_free(render->video_rgb_pool_mem);
        render->video_rgb_pool_mem = NULL;
    }
    render->video_rgb_pool_slot_size = 0;
    render->video_rgb_pool_slots = 0;

    lisa_mem_free(render);
    return 0;
}

int av_render_register_video_renderer(av_render_t *render, const av_render_video_renderer_ops_t *ops, void *user)
{
    if (!render || !render->state_lock) {
        return -EINVAL;
    }
    if (ops && !ops->write_rgb565) {
        return -EINVAL;
    }

    lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
    render->video_renderer_ops = ops;
    render->video_renderer_user = user;
    lisa_mutex_unlock(render->state_lock);
    return 0;
}

int av_render_register_audio_renderer(av_render_t *render, const av_render_audio_renderer_ops_t *ops, void *user)
{
    if (!render || !render->state_lock) {
        return -EINVAL;
    }
    if (ops && !ops->write_pcm_s16le) {
        return -EINVAL;
    }

    lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
    render->audio_renderer_ops = ops;
    render->audio_renderer_user = user;
    lisa_mutex_unlock(render->state_lock);
    return 0;
}

int av_render_register_mjpeg_decoder(av_render_t *render, const av_render_mjpeg_decoder_ops_t *ops, void *user)
{
    if (!render || !render->state_lock) {
        return -EINVAL;
    }
    if (ops && !ops->push) {
        return -EINVAL;
    }

    lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
    render->mjpeg_decoder_ops = ops;
    render->mjpeg_decoder_user = user;
    lisa_mutex_unlock(render->state_lock);
    return 0;
}

int av_render_register_audio_decoder(av_render_t *render, const av_render_audio_decoder_ops_t *ops, void *user)
{
    if (!render || !render->state_lock) {
        return -EINVAL;
    }
    if (ops && !ops->push) {
        return -EINVAL;
    }

    lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
    render->audio_decoder_ops = ops;
    render->audio_decoder_user = user;
    lisa_mutex_unlock(render->state_lock);
    return 0;
}

int av_render_reset(av_render_t *render)
{
    if (!render) {
        return -EINVAL;
    }

    lisa_queue_clear(render->video_q);
    lisa_queue_clear(render->audio_q);
    render->audio_start_tick_ms = 0;
    return 0;
}

int av_render_add_video_stream(av_render_t *render, const av_render_video_stream_info_t *info)
{
    if (!render || !info) {
        return -EINVAL;
    }
    render->video_info = *info;
    render->video_stream_set = true;
    return 0;
}

int av_render_add_audio_stream(av_render_t *render, const av_render_audio_stream_info_t *info)
{
    if (!render || !info) {
        return -EINVAL;
    }
    render->audio_info = *info;
    render->audio_stream_set = true;
    return 0;
}

int av_render_default_video_write_rgb565(av_render_t *render,
                                        const uint8_t *rgb565,
                                        uint32_t size,
                                        uint16_t width,
                                        uint16_t height,
                                        uint32_t pts_ms)
{
    if (!render || !rgb565 || size == 0 || width == 0 || height == 0) {
        return -EINVAL;
    }

    av_render_video_frame_t frame = {
        .data = (uint8_t *)rgb565,
        .size = size,
        .width = width,
        .height = height,
        .pts_ms = pts_ms,
        .free_fn = NULL,
        .free_user = NULL,
    };

    return video_render_write_rgb565(render, &frame);
}

int av_render_push(av_render_t *render, av_render_pkt_t *pkt)
{
    if (!render || !pkt || !pkt->buf.data || pkt->buf.size == 0) {
        if (pkt && pkt->buf.data) {
            release_buf(pkt->buf.data, pkt->buf.free_fn, pkt->buf.free_user);
            pkt->buf.data = NULL;
        }
        return -EINVAL;
    }

    if (!pkt->buf.free_fn) {
        pkt->buf.free_fn = default_free;
        pkt->buf.free_user = NULL;
    }

    if (!render->running || render->should_stop) {
        release_buf(pkt->buf.data, pkt->buf.free_fn, pkt->buf.free_user);
        pkt->buf.data = NULL;
        return -EPIPE;
    }

    int ret = 0;
    switch (pkt->type) {
    case AV_RENDER_PKT_AUDIO_PCM_S16LE: {
        if ((pkt->buf.size % sizeof(int16_t)) != 0) {
            ret = -EINVAL;
            break;
        }
        av_render_audio_frame_t frame = {
            .data = pkt->buf.data,
            .size = pkt->buf.size,
            .pts_ms = pkt->pts_ms,
            .free_fn = pkt->buf.free_fn,
            .free_user = pkt->buf.free_user,
        };
        lisa_err_t err = lisa_queue_push(render->audio_q, &frame, sizeof(frame), 200);
        ret = (err == LISA_OK) ? 0 : -EAGAIN;
        break;
    }
    case AV_RENDER_PKT_AUDIO_MP3: {
        const av_render_audio_decoder_ops_t *dops_a = NULL;
        void *duser_a = NULL;
        if (render->state_lock) {
            lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
            dops_a = render->audio_decoder_ops;
            duser_a = render->audio_decoder_user;
            lisa_mutex_unlock(render->state_lock);
        }

        if (dops_a && dops_a->push) {
            ret = dops_a->push(render,
                               pkt->buf.data,
                               pkt->buf.size,
                               pkt->pts_ms,
                               pkt->buf.free_fn,
                               pkt->buf.free_user,
                               duser_a);
        } else {
            ret = av_render_mp3_push(render,
                                     pkt->buf.data,
                                     pkt->buf.size,
                                     pkt->pts_ms,
                                     pkt->buf.free_fn,
                                     pkt->buf.free_user);
        }
        break;
    }
    case AV_RENDER_PKT_VIDEO_RGB565: {
        if (pkt->u.video.width == 0 || pkt->u.video.height == 0) {
            ret = -EINVAL;
            break;
        }

        /* 低延迟控制：在音频同步模式下限制视频超前量 */
        video_ahead_backpressure(render, pkt->pts_ms);

        av_render_video_frame_t frame = {
            .data = pkt->buf.data,
            .size = pkt->buf.size,
            .width = pkt->u.video.width,
            .height = pkt->u.video.height,
            .pts_ms = pkt->pts_ms,
            .free_fn = pkt->buf.free_fn,
            .free_user = pkt->buf.free_user,
        };
        lisa_err_t err = lisa_queue_push(render->video_q, &frame, sizeof(frame), 0);
        ret = (err == LISA_OK) ? 0 : -EAGAIN;
        break;
    }
    case AV_RENDER_PKT_VIDEO_MJPEG:
        {
            const av_render_mjpeg_decoder_ops_t *dops2 = NULL;
            void *duser2 = NULL;
            if (render->state_lock) {
                lisa_mutex_lock(render->state_lock, LISA_WAIT_FOREVER);
                dops2 = render->mjpeg_decoder_ops;
                duser2 = render->mjpeg_decoder_user;
                lisa_mutex_unlock(render->state_lock);
            }

            /* 低延迟控制：在音频同步模式下限制视频超前量 */
            video_ahead_backpressure(render, pkt->pts_ms);

            if (dops2 && dops2->push) {
                ret = dops2->push(render, pkt->buf.data, pkt->buf.size, pkt->pts_ms, pkt->buf.free_fn, pkt->buf.free_user, duser2);
            } else {
#if defined(CONFIG_AVI_PLAYER_VIDEO_DECODER_LISA_JPEG)
                ret = av_render_lisa_jpeg_push(render, pkt->buf.data, pkt->buf.size, pkt->pts_ms, pkt->buf.free_fn, pkt->buf.free_user);
#else
                ret = av_render_mjpeg_push(render, pkt->buf.data, pkt->buf.size, pkt->pts_ms, pkt->buf.free_fn, pkt->buf.free_user);
#endif
            }
        }
        break;
    default:
        ret = -ENOTSUP;
        break;
    }

    if (ret != 0) {
        release_buf(pkt->buf.data, pkt->buf.free_fn, pkt->buf.free_user);
        pkt->buf.data = NULL;
        return ret;
    }

    pkt->buf.data = NULL;
    return 0;
}
