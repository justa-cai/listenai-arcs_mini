/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "av_render_internal.h"

#include <errno.h>
#include <string.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_time.h>

#ifndef LOG_TAG
#define LOG_TAG "av_adec"
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

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t pts_ms;

    av_render_free_fn_t free_fn;
    void *free_user;
} mp3_pkt_t;

#if defined(CONFIG_AVI_PLAYER_AUDIO_MP3)
#include <mp3dec.h>

typedef struct {
    HMP3Decoder dec;

    /* 跨 AVI chunk 累积 bitstream（适配流式输入，chunk 不一定按帧对齐）。 */
    uint8_t *bs;
    size_t bs_cap;
    size_t bs_len;

    bool pts_inited;
    uint32_t base_pts_ms;
    uint64_t decoded_frames_total; /* 以“每声道采样帧”为单位累计 */
} mp3_state_t;

static void mp3_state_reset_pts(mp3_state_t *st)
{
    if (!st) {
        return;
    }
    st->pts_inited = false;
    st->base_pts_ms = 0;
    st->decoded_frames_total = 0;
}

static void mp3_state_free(mp3_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->dec) {
        MP3FreeDecoder(st->dec);
        st->dec = NULL;
    }
    if (st->bs) {
        lisa_mem_free(st->bs);
        st->bs = NULL;
    }
    st->bs_cap = 0;
    st->bs_len = 0;
}

static int mp3_state_init(mp3_state_t *st)
{
    if (!st) {
        return -EINVAL;
    }
    memset(st, 0, sizeof(*st));

    st->dec = MP3InitDecoder();
    if (!st->dec) {
        return -ENOMEM;
    }

    /* 16KB 通常足够容纳若干 MP3 帧 + main data。 */
    st->bs_cap = 16 * 1024;
    st->bs = (uint8_t *)lisa_mem_alloc(st->bs_cap);
    if (!st->bs) {
        mp3_state_free(st);
        return -ENOMEM;
    }
    st->bs_len = 0;

    mp3_state_reset_pts(st);
    return 0;
}

static uint32_t calc_pcm_pts_ms(const av_render_t *r, const mp3_state_t *st)
{
    if (!r || !st || !st->pts_inited) {
        return 0;
    }
    const uint32_t sr = r->audio_info.sample_rate;
    const uint16_t ch = r->audio_info.channels;
    if (sr == 0 || ch == 0) {
        return st->base_pts_ms;
    }

    uint64_t ms = (st->decoded_frames_total * 1000ULL) / (uint64_t)sr;
    return st->base_pts_ms + (uint32_t)ms;
}

static int push_pcm_to_audio_q(av_render_t *r, const int16_t *pcm, uint32_t samples, uint32_t pts_ms)
{
    if (!r || !pcm || samples == 0) {
        return -EINVAL;
    }

    size_t bytes = (size_t)samples * sizeof(int16_t);
    uint8_t *buf = (uint8_t *)lisa_mem_alloc(bytes);
    if (!buf) {
        return -ENOMEM;
    }
    memcpy(buf, pcm, bytes);

    av_render_audio_frame_t frame = {
        .data = buf,
        .size = (uint32_t)bytes,
        .pts_ms = pts_ms,
        .free_fn = NULL,
        .free_user = NULL,
    };

    /* 行为与 PCM push 类似：允许短暂等待，降低音频 underrun 概率。 */
    lisa_err_t err = lisa_queue_push(r->audio_q, &frame, sizeof(frame), 200);
    if (err != LISA_OK) {
        lisa_mem_free(buf);
        return -EAGAIN;
    }
    return 0;
}

static void adec_thread_entry(void *arg)
{
    av_render_t *r = (av_render_t *)arg;

    mp3_state_t st;
    int init_ret = mp3_state_init(&st);
    if (init_ret != 0) {
        LISA_LOGE(LOG_TAG, "mp3 state init failed: %d", init_ret);
        r->audio_codec_thread_exited = true;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            r->audio_codec_thread = NULL;
            lisa_mutex_unlock(r->state_lock);
        } else {
            r->audio_codec_thread = NULL;
        }
        return;
    }

    uint64_t last_err_log_ms = 0;

    while (!r->should_stop) {
        mp3_pkt_t pkt = {0};
        if (!r->audio_codec_q) {
            lisa_thread_mdelay(10);
            continue;
        }

        lisa_err_t err = lisa_queue_pop(r->audio_codec_q, &pkt, sizeof(pkt), 50);
        if (err != LISA_OK) {
            continue;
        }

        if (!pkt.data || pkt.size == 0) {
            if (pkt.free_fn) {
                pkt.free_fn(pkt.data, pkt.free_user);
            }
            continue;
        }

        /*
         * PTS 锚定：用第一包的 pts_ms 作为起点；后续根据“已解码输出的采样数”推进（对 VBR 更可靠）。
         */
        if (!st.pts_inited) {
            st.pts_inited = true;
            st.base_pts_ms = pkt.pts_ms;
            st.decoded_frames_total = 0;
        }

        /* 追加 bitstream 字节 */
        if ((st.bs_len + pkt.size) > st.bs_cap) {
            /* 尝试腾挪空间：仅保留末尾 4KB（作为“兜底”缓存），丢弃更早的数据。 */
            if (st.bs_len > 4096) {
                memmove(st.bs, &st.bs[st.bs_len - 4096], 4096);
                st.bs_len = 4096;
            }
            if ((st.bs_len + pkt.size) > st.bs_cap) {
                LISA_LOGW(LOG_TAG, "mp3 bitstream overflow, drop packet (len=%u add=%u)",
                          (unsigned)st.bs_len, (unsigned)pkt.size);
                if (pkt.free_fn) {
                    pkt.free_fn(pkt.data, pkt.free_user);
                } else {
                    lisa_mem_free(pkt.data);
                }
                continue;
            }
        }
        memcpy(&st.bs[st.bs_len], pkt.data, pkt.size);
        st.bs_len += pkt.size;

        /* 立即释放 encoded buffer（push 成功后由此处接管并释放） */
        if (pkt.free_fn) {
            pkt.free_fn(pkt.data, pkt.free_user);
        } else {
            lisa_mem_free(pkt.data);
        }
        pkt.data = NULL;

        /* 尽可能多地解码输出 */
        while (!r->should_stop && st.bs_len > 0) {
            unsigned char *inbuf = (unsigned char *)st.bs;
            int bytes_left = (int)st.bs_len;

            /* 最坏情况下的输出采样数：MAX_NSAMP * MAX_NGRAN * MAX_NCHAN */
            int16_t pcm[MAX_NSAMP * MAX_NGRAN * MAX_NCHAN];
            int dec_ret = MP3Decode(st.dec, &inbuf, &bytes_left, pcm, 0);

            if (dec_ret == ERR_MP3_INDATA_UNDERFLOW || dec_ret == ERR_MP3_MAINDATA_UNDERFLOW) {
                /* 数据不够：需要更多字节 */
                break;
            }

            if (dec_ret != ERR_MP3_NONE) {
                /* 解码失败：尝试重同步（寻找下一处 sync word） */
                int off = MP3FindSyncWord((unsigned char *)st.bs, (int)st.bs_len);

                if (should_log_every(&last_err_log_ms, 1000)) {
                    LISA_LOGW(LOG_TAG, "mp3 decode err=%d bs_len=%u sync_off=%d", dec_ret, (unsigned)st.bs_len, off);
                }

                if (off > 0 && (size_t)off < st.bs_len) {
                    memmove(st.bs, &st.bs[off], st.bs_len - (size_t)off);
                    st.bs_len -= (size_t)off;
                    continue;
                }
                if (off == 0 && st.bs_len > 1) {
                    /* sync word 在开头但仍失败：丢 1 字节继续找，避免卡死在同一位置。 */
                    memmove(st.bs, &st.bs[1], st.bs_len - 1);
                    st.bs_len -= 1;
                    continue;
                }
                /* 无法重同步：清空缓存 */
                st.bs_len = 0;
                break;
            }

            MP3FrameInfo fi;
            MP3GetLastFrameInfo(st.dec, &fi);

            if (fi.outputSamps <= 0) {
                /* 没有产生输出：避免死循环 */
                break;
            }

            /* 若缺少流信息，尝试从解码出的帧信息中补全 */
            if (r->audio_info.sample_rate == 0 && fi.samprate > 0) {
                r->audio_info.sample_rate = (uint32_t)fi.samprate;
            }
            if (r->audio_info.channels == 0 && fi.nChans > 0) {
                r->audio_info.channels = (uint16_t)fi.nChans;
            }

            /* 消耗输入字节：inbuf 已前移，bytes_left 表示剩余字节数 */
            size_t consumed = (size_t)((uint8_t *)inbuf - st.bs);
            if (consumed > st.bs_len) {
                consumed = st.bs_len;
            }
            if (bytes_left < 0) {
                bytes_left = 0;
            }
            if ((size_t)bytes_left > st.bs_len) {
                bytes_left = (int)st.bs_len;
            }

            if (bytes_left > 0 && (size_t)bytes_left <= st.bs_len) {
                memmove(st.bs, &st.bs[consumed], (size_t)bytes_left);
                st.bs_len = (size_t)bytes_left;
            } else {
                st.bs_len = 0;
            }

            /* 计算本次输出 PCM chunk 的 PTS */
            uint32_t pcm_pts = calc_pcm_pts_ms(r, &st);

            /* 更新累计已解码的采样帧数（按“每声道采样帧”计） */
            uint16_t ch = r->audio_info.channels;
            if (ch == 0) {
                ch = (fi.nChans > 0) ? (uint16_t)fi.nChans : 1;
            }
            uint32_t frames = (uint32_t)fi.outputSamps / (uint32_t)ch;
            st.decoded_frames_total += (uint64_t)frames;

            /* 把 PCM 推给音频渲染线程 */
            (void)push_pcm_to_audio_q(r, pcm, (uint32_t)fi.outputSamps, pcm_pts);

            /* Continue decoding if there are remaining bytes */
            if (st.bs_len == 0) {
                break;
            }
        }
    }

    mp3_state_free(&st);

    if (r) {
        r->audio_codec_thread_exited = true;
        if (r->state_lock) {
            lisa_mutex_lock(r->state_lock, LISA_WAIT_FOREVER);
            r->audio_codec_thread = NULL;
            lisa_mutex_unlock(r->state_lock);
        } else {
            r->audio_codec_thread = NULL;
        }
    }
}

static int mp3_open(av_render_t *r)
{
    if (!r) {
        return -EINVAL;
    }

    if (r->audio_codec_q || r->audio_codec_thread) {
        return 0;
    }

    uint32_t q_depth = 8;
#if defined(CONFIG_AVI_PLAYER_MP3_DECODE_QUEUE_DEPTH)
    q_depth = (uint32_t)CONFIG_AVI_PLAYER_MP3_DECODE_QUEUE_DEPTH;
#endif

    uint32_t stack_size = 6 * 1024;
#if defined(CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_STACK_SIZE)
    stack_size = (uint32_t)CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_STACK_SIZE;
#endif

    uint32_t priority = 4;
#if defined(CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_PRIORITY)
    priority = (uint32_t)CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_PRIORITY;
#endif

    r->audio_codec_thread_exited = false;
    r->audio_codec_q = lisa_queue_create(q_depth, (uint8_t *)"av_mp3", sizeof(mp3_pkt_t));
    if (!r->audio_codec_q) {
        return -ENOMEM;
    }

    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"av_mp3dec",
        .stack_size = stack_size,
        .priority = priority,
    };

    r->audio_codec_thread = lisa_thread_create(&attr, adec_thread_entry, r);
    if (!r->audio_codec_thread) {
        lisa_queue_delete(r->audio_codec_q);
        r->audio_codec_q = NULL;
        return -ENOMEM;
    }

    return 0;
}

#endif /* CONFIG_AVI_PLAYER_AUDIO_MP3 */

int av_render_mp3_push(av_render_t *render,
                       uint8_t *data,
                       size_t size,
                       uint32_t pts_ms,
                       av_render_free_fn_t free_fn,
                       void *free_user)
{
    if (!render || !data || size == 0) {
        if (data && free_fn) {
            free_fn(data, free_user);
        } else if (data) {
            lisa_mem_free(data);
        }
        return -EINVAL;
    }

#if !defined(CONFIG_AVI_PLAYER_AUDIO_MP3)
    if (free_fn) {
        free_fn(data, free_user);
    } else {
        lisa_mem_free(data);
    }
    return -ENOTSUP;
#else
    int oret = mp3_open(render);
    if (oret != 0) {
        if (free_fn) {
            free_fn(data, free_user);
        } else {
            lisa_mem_free(data);
        }
        return oret;
    }

    if (!render->audio_codec_q) {
        if (free_fn) {
            free_fn(data, free_user);
        } else {
            lisa_mem_free(data);
        }
        return -EIO;
    }

    mp3_pkt_t pkt = {
        .data = data,
        .size = (uint32_t)size,
        .pts_ms = pts_ms,
        .free_fn = free_fn,
        .free_user = free_user,
    };

    /* Prefer not to drop audio; allow some waiting. */
    lisa_err_t err = lisa_queue_push(render->audio_codec_q, &pkt, sizeof(pkt), 200);
    if (err != LISA_OK) {
        if (free_fn) {
            free_fn(data, free_user);
        } else {
            lisa_mem_free(data);
        }
        return -EAGAIN;
    }

    return 0;
#endif
}

void av_render_mp3_close(av_render_t *render)
{
    if (!render) {
        return;
    }

#if defined(CONFIG_AVI_PLAYER_AUDIO_MP3)
    if (render->audio_codec_thread) {
        lisa_thread_delete(render->audio_codec_thread);
        render->audio_codec_thread = NULL;
    }

    if (render->audio_codec_q) {
        while (lisa_queue_waiting(render->audio_codec_q) > 0) {
            mp3_pkt_t pkt = {0};
            if (lisa_queue_pop(render->audio_codec_q, &pkt, sizeof(pkt), 0) != LISA_OK) {
                break;
            }
            if (pkt.data) {
                if (pkt.free_fn) {
                    pkt.free_fn(pkt.data, pkt.free_user);
                } else {
                    lisa_mem_free(pkt.data);
                }
                pkt.data = NULL;
            }
        }
        lisa_queue_delete(render->audio_codec_q);
        render->audio_codec_q = NULL;
    }
#else
    (void)render;
#endif
}
