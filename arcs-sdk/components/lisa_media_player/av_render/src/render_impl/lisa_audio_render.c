/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "av_render_internal.h"

#include <errno.h>
#include <string.h>

#include <lisa_audio.h>
#include <lisa_log.h>
#include <lisa_time.h>

/* cfg.audio_dev 在强解耦模式下是 opaque handle；LISA 平台下约定为 lisa_device_t* */
#include <lisa_device.h>

#ifndef LOG_TAG
#define LOG_TAG "av_a_render"
#endif

static int lisa_audio_ops_write_pcm_s16le(av_render_t *render,
                                         const uint8_t *pcm,
                                         uint32_t size,
                                         uint32_t pts_ms,
                                         void *user)
{
    (void)user;
    if (!render || !pcm || size == 0) {
        return -EINVAL;
    }

    av_render_audio_frame_t frame = {
        .data = (uint8_t *)pcm,
        .size = size,
        .pts_ms = pts_ms,
        .free_fn = NULL,
        .free_user = NULL,
    };
    return audio_render_write_pcm_s16le(render, &frame);
}

static void lisa_audio_ops_stop(av_render_t *render, void *user)
{
    (void)user;
    audio_render_stop(render);
}

const av_render_audio_renderer_ops_t av_render_lisa_audio_renderer_ops = {
    .write_pcm_s16le = lisa_audio_ops_write_pcm_s16le,
    .stop = lisa_audio_ops_stop,
};

static bool should_log_every(uint64_t *last_ms, uint32_t period_ms)
{
    uint64_t now = lisa_os_get_tick_ms();
    if (*last_ms == 0 || (now - *last_ms) >= period_ms) {
        *last_ms = now;
        return true;
    }
    return false;
}

static uint64_t s_last_stat_log_ms;
static uint64_t s_last_rate_log_ms;
static uint64_t s_last_rate_samples;
static uint32_t s_frames;
static uint32_t s_write_calls;
static uint32_t s_short_writes;
static uint32_t s_zero_writes;
static uint32_t s_write_errors;
static uint64_t s_samples_total;
static uint64_t s_write_cost_total_ms;
static uint32_t s_write_cost_max_ms;
static uint32_t s_write_cost_slow;

static uint32_t normalize_audio_rate_hz(uint32_t sample_rate_hz)
{
    switch (sample_rate_hz) {
    case 8000:
    case 16000:
    case 24000:
    case 32000:
    case 48000:
    case 96000:
        return sample_rate_hz;
    default:
        return 16000;
    }
}

static uint32_t get_audio_input_channels(const av_render_t *r)
{
    return (r && r->audio_info.channels == 2U) ? 2U : 1U;
}

static uint32_t get_audio_write_chunk_samples(const av_render_t *r)
{
    if (!r || r->audio_out_buffer_samples == 0U) {
        return 0U;
    }
    return (uint32_t)r->audio_out_buffer_samples * get_audio_input_channels(r);
}

static int ensure_audio_started(av_render_t *r)
{
    if (!r->cfg.audio_dev) {
        return -ENODEV;
    }
    if (!r->audio_stream_set) {
        return -EINVAL;
    }

    if (!r->audio_cfg_done) {
        if (r->audio_info.bits_per_sample != 16) {
            LISA_LOGE(LOG_TAG, "Only support 16-bit PCM currently (bits=%u)", (unsigned)r->audio_info.bits_per_sample);
            return -ENOTSUP;
        }

        lisa_audio_channel_t out_channels;
        if (r->audio_info.channels == 1) {
            out_channels = LISA_AUDIO_CH_LEFT;
        } else if (r->audio_info.channels == 2) {
            out_channels = LISA_AUDIO_CH_STEREO;
        } else {
            LISA_LOGE(LOG_TAG, "Only support mono/stereo PCM currently (channels=%u)", (unsigned)r->audio_info.channels);
            return -ENOTSUP;
        }

        uint32_t effective_rate_hz = normalize_audio_rate_hz(r->audio_info.sample_rate);

        lisa_audio_play_config_t play_config = {
            .format = {
                .sample_rate = (lisa_audio_rate_t)effective_rate_hz,
                .channels = out_channels,
                .sample_bits = LISA_AUDIO_BIT_16,
            },
            .gain = {
                .analog_gain = 0,
                .digital_gain = -14,
            },
            /* 增加 buffer 以降低重负载视频解码时的音频卡顿 */
            .buffer_count = 16,
            .buffer_samples = 512,
        };

        int ret = lisa_audio_play_config((lisa_device_t *)r->cfg.audio_dev, &play_config);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Audio play config failed: %d", ret);
            return ret;
        }

        r->audio_out_buffer_samples = play_config.buffer_samples;
        if (r->audio_out_buffer_samples == 0) {
            r->audio_out_buffer_samples = 512;
        }

        if (!r->audio_staging) {
            /* 预留多个 buffer 的容量，用于吸收 demux 的突发写入 */
            uint32_t chunk_samples = get_audio_write_chunk_samples(r);
            if (chunk_samples == 0U) {
                chunk_samples = 512U;
            }
            uint32_t cap = chunk_samples * 8U;
            r->audio_staging = (int16_t *)lisa_mem_alloc((size_t)cap * sizeof(int16_t));
            if (!r->audio_staging) {
                return -ENOMEM;
            }
            r->audio_staging_cap_samples = cap;
            r->audio_staging_len_samples = 0;
        }

        r->audio_cfg_done = true;
    }

    if (!r->audio_started) {
        int ret = lisa_audio_play_start((lisa_device_t *)r->cfg.audio_dev);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Audio play start failed: %d", ret);
            return ret;
        }
        r->audio_started = true;
    }

    return 0;
}

int audio_render_write_pcm_s16le(av_render_t *render, const av_render_audio_frame_t *frame)
{
    if (!render || !frame || !frame->data || frame->size == 0) {
        return -EINVAL;
    }

    int ret = ensure_audio_started(render);
    if (ret != 0) {
        return ret;
    }
    if (!render->audio_started) {
        return -EIO;
    }

    if (!render->audio_staging || render->audio_staging_cap_samples == 0 || render->audio_out_buffer_samples == 0) {
        return -EIO;
    }

    const int16_t *samples = (const int16_t *)frame->data;
    uint32_t sample_count = (uint32_t)(frame->size / sizeof(int16_t));
    uint32_t write_chunk_samples = get_audio_write_chunk_samples(render);
    if (write_chunk_samples == 0U) {
        return -EIO;
    }
    s_frames++;

    /* 把输入 samples 追加到 staging 缓冲 */
    while (sample_count > 0) {
        uint32_t cap_left = render->audio_staging_cap_samples - render->audio_staging_len_samples;
        if (cap_left == 0) {
            /* staging 溢出：丢弃最旧的数据以保证继续推进（理论上应较少发生） */
            uint32_t drop = write_chunk_samples;
            if (drop > render->audio_staging_len_samples) {
                drop = render->audio_staging_len_samples;
            }
            if (drop > 0) {
                memmove(render->audio_staging,
                        &render->audio_staging[drop],
                        (size_t)(render->audio_staging_len_samples - drop) * sizeof(int16_t));
                render->audio_staging_len_samples -= drop;
                cap_left = render->audio_staging_cap_samples - render->audio_staging_len_samples;
            }
        }

        uint32_t to_copy = (sample_count < cap_left) ? sample_count : cap_left;
        memcpy(&render->audio_staging[render->audio_staging_len_samples], samples, (size_t)to_copy * sizeof(int16_t));
        render->audio_staging_len_samples += to_copy;
        samples += to_copy;
        sample_count -= to_copy;

        /* 仅按“完整设备 buffer”写入，避免因为 padding 造成可闻间隙 */
        while (render->audio_staging_len_samples >= write_chunk_samples) {
            uint32_t req = write_chunk_samples;

            uint64_t t0 = lisa_os_get_tick_ms();
            int w = lisa_audio_play_write((lisa_device_t *)render->cfg.audio_dev, render->audio_staging, req);
            uint32_t write_cost_ms = (uint32_t)(lisa_os_get_tick_ms() - t0);

            s_write_calls++;
            s_write_cost_total_ms += write_cost_ms;
            if (write_cost_ms > s_write_cost_max_ms) {
                s_write_cost_max_ms = write_cost_ms;
            }
            if (write_cost_ms >= 5U) {
                s_write_cost_slow++;
            }

            if (w < 0) {
                LISA_LOGE(LOG_TAG, "Audio play write failed: %d", w);
                s_write_errors++;
                return w;
            }
            if (w == 0) {
                s_zero_writes++;
                lisa_thread_mdelay(1);
                break;
            }
            if ((uint32_t)w < req) {
                s_short_writes++;
            }

            s_samples_total += (uint64_t)w;

            /* 从 staging 中移除已消费的 samples */
            uint32_t consumed = (uint32_t)w;
            if (consumed > render->audio_staging_len_samples) {
                consumed = render->audio_staging_len_samples;
            }
            memmove(render->audio_staging,
                    &render->audio_staging[consumed],
                    (size_t)(render->audio_staging_len_samples - consumed) * sizeof(int16_t));
            render->audio_staging_len_samples -= consumed;

            if ((uint32_t)w < req) {
                /* 设备当前吃不下完整 buffer：稍后再试 */
                break;
            }
        }
    }

    if (should_log_every(&s_last_stat_log_ms, 1000)) {
        uint64_t now = lisa_os_get_tick_ms();
        uint32_t feed_sps = 0;
        if (s_last_rate_log_ms != 0 && now > s_last_rate_log_ms) {
            uint64_t delta_ms = now - s_last_rate_log_ms;
            uint64_t delta_samples = s_samples_total - s_last_rate_samples;
            feed_sps = (uint32_t)((delta_samples * 1000ULL) / delta_ms);
        }
        s_last_rate_log_ms = now;
        s_last_rate_samples = s_samples_total;

#if defined(CONFIG_AVI_PLAYER_STATS_LOG)
    uint32_t write_cost_avg_ms = (s_write_calls > 0U)
                     ? (uint32_t)(s_write_cost_total_ms / s_write_calls)
                     : 0U;

        LISA_LOGI(LOG_TAG,
          "write stat: frames=%u out_samples=%llu calls=%u short=%u zero=%u err=%u feed=%uHz cfg=%uHz stage=%u w_avg=%ums w_max=%ums w_slow=%u",
                  (unsigned)s_frames,
                  (unsigned long long)s_samples_total,
                  (unsigned)s_write_calls,
                  (unsigned)s_short_writes,
                  (unsigned)s_zero_writes,
                  (unsigned)s_write_errors,
                  (unsigned)feed_sps,
                  (unsigned)normalize_audio_rate_hz(render->audio_info.sample_rate),
          (unsigned)render->audio_staging_len_samples,
          (unsigned)write_cost_avg_ms,
          (unsigned)s_write_cost_max_ms,
          (unsigned)s_write_cost_slow);
#endif
    }

    return 0;
}

void audio_render_stop(av_render_t *render)
{
    if (!render || !render->cfg.audio_dev) {
        return;
    }

    /* 尽力 flush 剩余 staging：不足一个整 buffer 时用 0 补齐 */
    if (render->audio_started && render->audio_staging && render->audio_out_buffer_samples) {
        uint32_t write_chunk_samples = get_audio_write_chunk_samples(render);
        if (write_chunk_samples == 0U) {
            write_chunk_samples = render->audio_out_buffer_samples;
        }

        if (render->audio_staging_len_samples > 0 && render->audio_staging_len_samples < write_chunk_samples) {
            uint32_t need = write_chunk_samples - render->audio_staging_len_samples;
            memset(&render->audio_staging[render->audio_staging_len_samples], 0, (size_t)need * sizeof(int16_t));
            render->audio_staging_len_samples += need;
        }
        while (render->audio_staging_len_samples >= write_chunk_samples) {
            (void)lisa_audio_play_write((lisa_device_t *)render->cfg.audio_dev,
                                        render->audio_staging,
                                        write_chunk_samples);
            memmove(render->audio_staging,
                    &render->audio_staging[write_chunk_samples],
                    (size_t)(render->audio_staging_len_samples - write_chunk_samples) * sizeof(int16_t));
            render->audio_staging_len_samples -= write_chunk_samples;
        }
    }

    if (render->audio_started) {
        lisa_audio_play_flush((lisa_device_t *)render->cfg.audio_dev);
        lisa_audio_play_stop((lisa_device_t *)render->cfg.audio_dev);
        render->audio_started = false;
    }

    if (render->audio_staging) {
        lisa_mem_free(render->audio_staging);
        render->audio_staging = NULL;
        render->audio_staging_cap_samples = 0;
        render->audio_staging_len_samples = 0;
        render->audio_out_buffer_samples = 0;
    }
}
