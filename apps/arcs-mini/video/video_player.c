/*
 * video_player.c - 在线 H264+AAC 拉流播放服务（实现见 video_player.h）
 *
 * 任务模型：
 *   [net task]    lisa_http 分块拉流 → 环形 StreamBuffer（PSRAM 256KB）
 *   [player task] 环形缓冲 → tsdemux → 视频软解/色彩转换、音频解码
 *                 → UI 线程投递（lisa_ui_invoke）/ PCM 写入 app_player
 *
 * 直播滞后控制：环形缓冲超过 90% 时中断 HTTP 重连，重新对齐到直播沿；
 * 解码错误依赖 IDR 自愈（IDR 清 DPB 重建参考链）。
 */
#define TAG "video_player"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "lisa_log.h"
#include "lisa_http.h"
#include "sysheap.h"
#include "shell.h"

#include "h264dec.h"
#include "aacdec.h"
#include "app_player.h"

#include "tsdemux.h"
#include "video_player.h"

#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "video_player.h"

/* ---- 资源参数 ---- */
#define VP_NET_RING_SIZE (256 * 1024)      /* TS 环形缓冲 */
#define VP_NET_TASK_STACK 4096
#define VP_PLAY_TASK_STACK (16 * 1024)
#define VP_TASK_PRIO 3
#define VP_HTTP_TIMEOUT_MS 15000
#define VP_RING_HIGH_PCT 90                /* 高于此占用断开重连对齐直播沿 */
#define VP_RECONNECT_DELAY_MS 2000
#define VP_MAX_URL 128

/* ---- 状态 ---- */
typedef enum {
    VP_STATE_IDLE = 0,
    VP_STATE_RUNNING,
    VP_STATE_STOPPING,
} vp_state_t;

static struct {
    volatile vp_state_t state;
    char url[VP_MAX_URL];

    TaskHandle_t net_task;
    TaskHandle_t play_task;

    /* 网络环形缓冲（静态创建，缓冲在 PSRAM） */
    StaticStreamBuffer_t ring_desc;
    uint8_t *ring_buf;
    StreamBufferHandle_t ring;

    /* 解码器 */
    h264_dec_t *dec;
    tsdemux_t tsd;
    uint8_t *ts_vbuf, *ts_abuf;

    /* 音频 */
    HAACDecoder aac;
    app_player_t *player;
    int audio_started;
    int16_t *pcm;                         /* AAC 解码输出（双声道最大） */
    uint8_t *aac_pend;                    /* ADTS 拼接缓冲 */
    size_t aac_pend_len;
    uint32_t audio_rate;

    /* 显示双缓冲（RGB565，PSRAM） */
    uint16_t *frames[2];
    volatile int front;

    /* 统计 */
    video_player_status_t st;
} vp;

/* ================================================================== */
/* 色彩转换：YUV420P → RGB565（BT.601 limited range，定点）            */
/* ================================================================== */

/* 系数 <<6：1.164→74，1.596→102，0.391→25，0.813→52，2.018→129 */
static inline uint16_t vp_yuv_to_rgb565(int y, int cb, int cr) {
    int c = y - 16, d = cb - 128, e = cr - 128;
    int r = (74 * c + 102 * e) >> 6;
    int g = (74 * c - 25 * d - 52 * e) >> 6;
    int b = (74 * c + 129 * d) >> 6;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void vp_convert_frame(const uint8_t *y, const uint8_t *u,
                             const uint8_t *v, int w, int h, int stride_y,
                             int stride_c, uint16_t *dst) {
    for (int row = 0; row < h; row++) {
        const uint8_t *yr = y + (size_t)row * stride_y;
        const uint8_t *ur = u + (size_t)(row >> 1) * stride_c;
        const uint8_t *vr = v + (size_t)(row >> 1) * stride_c;
        uint16_t *dr = dst + (size_t)row * w;
        for (int col = 0; col < w; col += 2) {
            int cb = ur[col >> 1], cr = vr[col >> 1];
            dr[col] = vp_yuv_to_rgb565(yr[col], cb, cr);
            dr[col + 1] = vp_yuv_to_rgb565(yr[col + 1], cb, cr);
        }
    }
}

/* ================================================================== */
/* 显示投递（UI 线程）                                                  */
/* ================================================================== */

/* 视频页视图接口（apps-ui 提供）：把最新 RGB565 帧交给页面 */
extern void lisa_ui_video_view_set_frame(const uint16_t *rgb565, int w, int h);

static void vp_ui_show_frame(void *unused, uint32_t len) {
    (void)unused;
    (void)len;
    if (vp.frames[vp.front]) {
        lisa_ui_video_view_set_frame(vp.frames[vp.front],
                                     vp.st.video_width, vp.st.video_height);
    }
}

static void vp_ui_open_page(void *unused, uint32_t len) {
    (void)unused;
    (void)len;
    lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_VIDEO);
}

static void vp_ui_close_page(void *unused, uint32_t len) {
    (void)unused;
    (void)len;
    lisa_ui_nav_scr_nav_back();
}

/* ================================================================== */
/* tsdemux 回调（player task 上下文）                                   */
/* ================================================================== */

static void vp_flush_display_frame(void) {
    TickType_t t0 = xTaskGetTickCount();

    int back = 1 - vp.front;
    vp_convert_frame(h264_dec_y(vp.dec), h264_dec_u(vp.dec), h264_dec_v(vp.dec),
                     vp.st.video_width, vp.st.video_height,
                     h264_dec_stride_y(vp.dec), h264_dec_stride_c(vp.dec),
                     vp.frames[back]);
    vp.front = back;
    LISA_UI_INVOKE_UI(vp_ui_show_frame, NULL, 0);
    vp.st.display_frames++;

    vp.st.last_csc_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
}

static int vp_on_h264(void *user, const uint8_t *data, size_t len,
                      uint64_t pts) {
    (void)user;
    (void)pts;
    if (!vp.dec) return 0;

    TickType_t t0 = xTaskGetTickCount();
    int r = h264_dec_stream_write(vp.dec, data, len);
    uint32_t ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    if (r < 0) {
        /* 直播码流错误：IDR 会自愈，仅计数 */
        vp.st.last_decode_ms = ms;
        return 0;
    }
    if (r > 0) {
        vp.st.frames += (uint32_t)r;
        vp.st.last_decode_ms = ms / (uint32_t)r;
        vp.st.video_width = h264_dec_width(vp.dec);
        vp.st.video_height = h264_dec_height(vp.dec);
        uint32_t nuit = h264_dec_num_units_in_tick(vp.dec);
        vp.st.declared_fps = nuit ? h264_dec_time_scale(vp.dec) / (2 * nuit) : 0;
        /* 解慢了只显示最新帧：多个帧完成也只投递最后一帧 */
        vp_flush_display_frame();
    }
    return 0;
}

/* ---- 音频：ADTS 拼接 + 解码 + 播放 ---- */

#define AAC_PEND_MAX 8192

static int vp_aac_start_stream(void) {
    AACFrameInfo info;
    if (!vp.player) {
        vp.player = app_player_create("video");
        if (!vp.player) {
            LOGE("app_player create failed");
            return -1;
        }
    }
    AACGetLastFrameInfo(vp.aac, &info);
    vp.audio_rate = (uint32_t)info.sampRateOut;
    if (app_player_play_stream(vp.player, info.sampRateOut, 1, 16) !=
        APP_PLAYER_OK) {
        LOGE("play_stream(%d Hz) failed", info.sampRateOut);
        return -1;
    }
    vp.audio_started = 1;
    LOGI("audio stream started: %d Hz %d ch", info.sampRateOut, info.nChans);
    return 0;
}

static int vp_on_aac(void *user, const uint8_t *data, size_t len,
                     uint64_t pts) {
    (void)user;
    (void)pts;
    if (!vp.aac) return 0;

    /* 拼接到待解码缓冲（PES 边界不一定与 ADTS 帧对齐） */
    if (vp.aac_pend_len + len > AAC_PEND_MAX) {
        vp.aac_pend_len = 0; /* 异常溢出：丢弃重来，靠下个 sync 字重同步 */
    }
    memcpy(vp.aac_pend + vp.aac_pend_len, data, len);
    vp.aac_pend_len += len;

    int left = (int)vp.aac_pend_len;
    unsigned char *in = vp.aac_pend;
    while (left > 7) {
        int off = AACFindSyncWord(in, left);
        if (off < 0) {
            vp.aac_pend_len = 0;
            break;
        }
        in += off;
        left -= off;
        int err = AACDecode(vp.aac, &in, &left, vp.pcm);
        if (err == ERR_AAC_INDATA_UNDERFLOW) {
            /* 帧不完整：保留残余等下次 */
            memmove(vp.aac_pend, in, (size_t)left);
            vp.aac_pend_len = (size_t)left;
            return 0;
        }
        if (err != ERR_AAC_NONE) {
            continue; /* 跳过坏帧，继续找 sync */
        }
        AACFrameInfo info;
        AACGetLastFrameInfo(vp.aac, &info);
        int samples = info.outputSamps;
        if (samples > 0) {
            if (!vp.audio_started) {
                if (vp_aac_start_stream() != 0) {
                    vp.audio_started = 0;
                    memmove(vp.aac_pend, in, (size_t)left);
                    vp.aac_pend_len = (size_t)left;
                    return 0;
                }
            }
            const int16_t *src = vp.pcm;
            int chans = info.nChans > 1 ? 2 : 1;
            int mono_samples = samples / chans;
            int16_t mono[AAC_MAX_NSAMPS];
            if (chans == 2) {
                for (int i = 0; i < mono_samples; i++) {
                    mono[i] = (int16_t)(((int)src[2 * i] + src[2 * i + 1]) / 2);
                }
            } else {
                memcpy(mono, src, (size_t)mono_samples * 2);
            }
            uint32_t bytes = (uint32_t)mono_samples * 2;
            int written = app_player_write_stream(vp.player, (const uint8_t *)mono,
                                                  bytes, 200);
            (void)written;
            vp.st.audio_frames++;
        }
    }
    /* 消耗完：残余清零或收尾 */
    if (left <= 0) {
        vp.aac_pend_len = 0;
    } else {
        memmove(vp.aac_pend, in, (size_t)left);
        vp.aac_pend_len = (size_t)left;
    }
    return 0;
}

static void vp_on_pmt_ready(void *user, uint16_t h264_pid, uint16_t aac_pid) {
    (void)user;
    LOGI("pmt: h264_pid=%u aac_pid=%u", h264_pid, aac_pid);
}

static const tsdemux_cb_t vp_ts_cb = {
    .on_h264 = vp_on_h264,
    .on_aac = vp_on_aac,
    .on_ready = vp_on_pmt_ready,
};

/* ================================================================== */
/* 播放任务：环形缓冲 → 解复用 → 解码                                  */
/* ================================================================== */

static void vp_play_task(void *arg) {
    (void)arg;
    static uint8_t chunk[4096];
    while (vp.state == VP_STATE_RUNNING) {
        size_t n = xStreamBufferReceive(vp.ring, chunk, sizeof(chunk),
                                        pdMS_TO_TICKS(200));
        if (n == 0) continue;
        tsdemux_feed(&vp.tsd, chunk, n);
    }
    vTaskDelete(NULL);
}

/* ================================================================== */
/* 网络任务：HTTP 分块拉流 → 环形缓冲                                  */
/* ================================================================== */

static int vp_http_on_chunk(lisa_http_data_t *data) {
    if (vp.state != VP_STATE_RUNNING) return 1;

    /* 直播沿控制：缓冲接近满说明消费不过来，断开重连即对齐到最新 */
    size_t used = xStreamBufferBytesAvailable(vp.ring);
    if (used * 100 > VP_NET_RING_SIZE * VP_RING_HIGH_PCT) {
        LOGW("ring %zu/%d, reconnect to live edge", used, VP_NET_RING_SIZE);
        vp.st.http_hops++;
        return 1;
    }

    size_t sent = xStreamBufferSend(vp.ring, data->buf, (size_t)data->len, 0);
    if (sent < (size_t)data->len) {
        /* 无等待发送下不足：丢弃该分片，计数 */
        vp.st.drop_chunks++;
    }
    return 0;
}

static void vp_net_task(void *arg) {
    (void)arg;
    while (vp.state == VP_STATE_RUNNING) {
        lisa_http_request_t req = {
            .method = LISA_HTTP_GET,
            .url = (uint8_t *)vp.url,
            .timeout = VP_HTTP_TIMEOUT_MS,
            .user = NULL,
        };
        lisa_http_t *http = lisa_http_init(&req);
        if (http) {
            lisa_http_err_e r = lisa_http_perform_chunked_with_cb(http,
                                                                  vp_http_on_chunk);
            lisa_http_cleanup(http);
            if (vp.state != VP_STATE_RUNNING) break;
            LOGW("http ended (%d), reconnect in %d ms", r,
                 VP_RECONNECT_DELAY_MS);
        } else {
            LOGE("http init failed");
        }
        vp.st.http_hops++;
        vTaskDelay(pdMS_TO_TICKS(VP_RECONNECT_DELAY_MS));
    }
    vTaskDelete(NULL);
}

/* ================================================================== */
/* 资源创建/释放                                                        */
/* ================================================================== */

static void vp_free_resources(void) {
    if (vp.dec) {
        h264_dec_destroy(vp.dec);
        vp.dec = NULL;
    }
    if (vp.aac) {
        AACFreeDecoder(vp.aac);
        vp.aac = NULL;
    }
    if (vp.player) {
        app_player_stop(vp.player);
        app_player_destroy(vp.player);
        vp.player = NULL;
    }
    if (vp.ring) {
        vStreamBufferDelete(vp.ring);
        vp.ring = NULL;
    }
    if (vp.ring_buf) {
        psram_free(vp.ring_buf);
        vp.ring_buf = NULL;
    }
    if (vp.ts_vbuf) {
        psram_free(vp.ts_vbuf);
        vp.ts_vbuf = NULL;
    }
    if (vp.ts_abuf) {
        psram_free(vp.ts_abuf);
        vp.ts_abuf = NULL;
    }
    if (vp.pcm) {
        psram_free(vp.pcm);
        vp.pcm = NULL;
    }
    if (vp.aac_pend) {
        psram_free(vp.aac_pend);
        vp.aac_pend = NULL;
    }
    if (vp.frames[0]) {
        psram_free(vp.frames[0]);
        vp.frames[0] = NULL;
    }
    if (vp.frames[1]) {
        psram_free(vp.frames[1]);
        vp.frames[1] = NULL;
    }
}

static int vp_alloc_resources(void) {
    vp.ring_buf = psram_malloc(VP_NET_RING_SIZE);
    vp.ts_vbuf = psram_malloc(64 * 1024);
    vp.ts_abuf = psram_malloc(8 * 1024);
    vp.pcm = psram_malloc(AAC_MAX_NSAMPS * AAC_MAX_NCHANS * 2);
    vp.aac_pend = psram_malloc(AAC_PEND_MAX);
    vp.frames[0] = psram_malloc(240 * 240 * 2);
    vp.frames[1] = psram_malloc(240 * 240 * 2);
    if (!vp.ring_buf || !vp.ts_vbuf || !vp.ts_abuf || !vp.pcm ||
        !vp.aac_pend || !vp.frames[0] || !vp.frames[1]) {
        LOGE("psram alloc failed");
        return -1;
    }
    vp.ring = xStreamBufferCreateStatic(VP_NET_RING_SIZE, 1, vp.ring_buf,
                                        &vp.ring_desc);

    vp.dec = h264_dec_create(240, 240);
    if (!vp.dec) {
        LOGE("h264 decoder create failed");
        return -1;
    }
    h264_dec_set_max_refs(vp.dec, 1);

    vp.aac = AACInitDecoder();
    if (!vp.aac) {
        LOGE("aac decoder create failed");
        return -1;
    }

    tsdemux_init(&vp.tsd, &vp_ts_cb, NULL, vp.ts_vbuf, 64 * 1024, vp.ts_abuf,
                 8 * 1024);
    return 0;
}

/* ================================================================== */
/* 公开 API                                                            */
/* ================================================================== */

int video_player_start(const char *url) {
    if (!url || !url[0]) return -1;
    if (vp.state == VP_STATE_RUNNING) {
        if (strcmp(vp.url, url) == 0) return 0;
        video_player_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    memset(&vp.st, 0, sizeof(vp.st));
    vp.front = 0;
    vp.audio_started = 0;
    vp.aac_pend_len = 0;
    if (vp_alloc_resources() != 0) {
        vp_free_resources();
        return -1;
    }
    snprintf(vp.url, sizeof(vp.url), "%s", url);
    vp.state = VP_STATE_RUNNING;

    if (xTaskCreate(vp_play_task, "vp_play", VP_PLAY_TASK_STACK / 4, NULL,
                    VP_TASK_PRIO, &vp.play_task) != pdPASS) {
        LOGE("play task create failed");
        vp_free_resources();
        vp.state = VP_STATE_IDLE;
        return -1;
    }
    if (xTaskCreate(vp_net_task, "vp_net", VP_NET_TASK_STACK / 4, NULL,
                    VP_TASK_PRIO + 1, &vp.net_task) != pdPASS) {
        LOGE("net task create failed");
        vp.state = VP_STATE_STOPPING;
        vTaskDelay(pdMS_TO_TICKS(300));
        vp_free_resources();
        vp.state = VP_STATE_IDLE;
        return -1;
    }

    LISA_UI_INVOKE_UI(vp_ui_open_page, NULL, 0);
    LOGI("playing: %s", vp.url);
    return 0;
}

int video_player_stop(void) {
    if (vp.state == VP_STATE_IDLE) return 0;
    vp.state = VP_STATE_STOPPING;

    /* 任务自行退出（http 回调/200ms 超时感知状态变化） */
    for (int i = 0; i < 50 && (vp.net_task || vp.play_task); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vp.net_task = NULL;
    vp.play_task = NULL;
    vp_free_resources();
    vp.state = VP_STATE_IDLE;
    vp.url[0] = '\0';

    LISA_UI_INVOKE_UI(vp_ui_close_page, NULL, 0);
    LOGI("stopped: %u frames, %u audio, %u hops, %u drops",
         vp.st.frames, vp.st.audio_frames, vp.st.http_hops,
         vp.st.drop_chunks);
    return 0;
}

bool video_player_is_playing(void) {
    return vp.state == VP_STATE_RUNNING;
}

void video_player_get_status(video_player_status_t *out) {
    if (!out) return;
    *out = vp.st;
    out->playing = (vp.state == VP_STATE_RUNNING);
}

/* ================================================================== */
/* shell: vp play <url> | vp stop | vp status                          */
/* ================================================================== */

static int vp_shell_cmd(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: vp play <url> | vp stop | vp status\n");
        return 0;
    }
    if (strcmp(argv[1], "play") == 0 && argc >= 3) {
        printf("vp: starting %s\n", argv[2]);
        return video_player_start(argv[2]);
    }
    if (strcmp(argv[1], "stop") == 0) {
        return video_player_stop();
    }
    if (strcmp(argv[1], "status") == 0) {
        video_player_status_t st;
        video_player_get_status(&st);
        printf("vp: playing=%d %dx%d fps=%u frames=%u disp=%u audio=%u "
               "hops=%u drops=%u dec_ms=%u csc_ms=%u\n",
               st.playing, st.video_width, st.video_height, st.declared_fps,
               st.frames, st.display_frames, st.audio_frames, st.http_hops,
               st.drop_chunks, st.last_decode_ms, st.last_csc_ms);
        return 0;
    }
    printf("vp: unknown '%s'\n", argv[1]);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 vp, vp_shell_cmd, online h264 stream player);
