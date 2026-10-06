/*
 * video_player.c - 在线 H264+AAC 拉流播放服务（实现见 video_player.h）
 *
 * 任务模型：
 *   [net task]    lisa_http 分块拉流 → 环形 StreamBuffer（PSRAM 256KB）
 *   [player task] 环形缓冲 → TLV 解析 → JPEG 硬解(直出 RGB565)/音频解码
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

#include "lisa_jpeg.h"
#include "aacdec.h"
#include "app_player.h"
#include "app_player_focus.h"

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
#define VP_RECONNECT_DELAY_MS 500          /* 快速重连，缩短音视频断流窗口 */
#define VP_MAX_URL 128
#define VP_AUDIO_WRITE_FAIL_MAX 3          /* 连续写失败次数阈值，触发音频流重建 */

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

    /* TLV 流解析（JPEG 帧累积）与解码 */
    uint8_t *tlv_buf;      /* PSRAM，含 8 字节头的完整 TLV 帧累积 */
    size_t tlv_len;
    uint32_t decode_errors;

    /* 音频 */
    HAACDecoder aac;
    app_player_t *player;
    int audio_started;
    int16_t *pcm;                         /* AAC 解码输出（双声道最大） */
    uint8_t *aac_pend;                    /* ADTS 拼接缓冲 */
    size_t aac_pend_len;
    uint32_t audio_rate;
    int audio_write_fails;                /* 连续写失败计数（流被 reset 自愈） */
    int audio_start_retries;              /* 启动重试计数（焦点被拒/prepare 失败） */

    /* 直播沿对齐请求：net 任务置位，play 任务清空积压后复位 */
    volatile int seek_live;

    /* 显示双缓冲（RGB565，PSRAM） */
    uint16_t *frames[2];
    volatile int front;

    /* 统计 */
    video_player_status_t st;
} vp;

/* ================================================================== */
/* TLV 帧流解析：[0xA5][type][len:u16be][pts:u32be][payload]           */
/*   V = JPEG 帧（硬件解码直出 RGB565），A = AAC ADTS 块               */
/* ================================================================== */

#define VP_TLV_HDR 8
#define VP_TLV_PAYLOAD_MAX (48 * 1024)    /* 单帧上限，JPEG q≥2 时足够 */

static void vp_ui_show_frame(void *unused, uint32_t len);

static int vp_on_jpeg(const uint8_t *jpg, uint32_t len);

static void vp_tlv_dispatch(uint8_t type, const uint8_t *payload,
                            uint16_t len, uint32_t pts) {
    if (type == 'V') {
        vp_on_jpeg(payload, len);
    } else if (type == 'A') {
        extern int vp_on_aac_pub(const uint8_t *data, size_t len);
        vp_on_aac_pub(payload, len);
        (void)pts;
    }
}

static void vp_tlv_feed(const uint8_t *data, size_t n) {
    while (n > 0) {
        if (vp.tlv_len < VP_TLV_HDR) {
            /* 凑帧头 */
            size_t take = VP_TLV_HDR - vp.tlv_len;
            if (take > n) take = n;
            memcpy(vp.tlv_buf + vp.tlv_len, data, take);
            vp.tlv_len += take;
            data += take;
            n -= take;
            if (vp.tlv_len < VP_TLV_HDR) return;

            /* 校验：失同步则丢 1 字节重扫 */
            uint16_t plen = ((uint16_t)vp.tlv_buf[2] << 8) | vp.tlv_buf[3];
            if (vp.tlv_buf[0] != 0xA5 || plen > VP_TLV_PAYLOAD_MAX) {
                memmove(vp.tlv_buf, vp.tlv_buf + 1, VP_TLV_HDR - 1);
                vp.tlv_len = VP_TLV_HDR - 1;
            }
            continue;
        }
        /* 凑载荷 */
        uint16_t plen = ((uint16_t)vp.tlv_buf[2] << 8) | vp.tlv_buf[3];
        size_t need = (size_t)plen + VP_TLV_HDR - vp.tlv_len;
        size_t take = need < n ? need : n;
        memcpy(vp.tlv_buf + vp.tlv_len, data, take);
        vp.tlv_len += take;
        data += take;
        n -= take;
        if (vp.tlv_len == (size_t)plen + VP_TLV_HDR) {
            uint32_t pts = ((uint32_t)vp.tlv_buf[4] << 24) |
                           ((uint32_t)vp.tlv_buf[5] << 16) |
                           ((uint32_t)vp.tlv_buf[6] << 8) | vp.tlv_buf[7];
            vp_tlv_dispatch(vp.tlv_buf[1], vp.tlv_buf + VP_TLV_HDR, plen, pts);
            vp.tlv_len = 0;
        }
    }
}

/* JPEG 硬解直出 RGB565 到显示双缓冲（pixel_build 行距紧凑=width，
 * 与 LVGL 帧缓冲布局一致，零拷贝） */
static int vp_on_jpeg(const uint8_t *jpg, uint32_t len) {
    TickType_t t0 = xTaskGetTickCount();

    int back = 1 - vp.front;
    uint16_t w = 0, h = 0;
    Jpeg_DecoderCfg cfg = {.output_format = JPEG_PIXEL_FORMAT_RGB565};
    int r = jpeg_decoder(jpg, len, (uint8_t *)vp.frames[back], &w, &h, cfg);
    vp.st.last_decode_ms =
        (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    if (r != 0 || w == 0 || h == 0) {
        vp.decode_errors++;
        return -1;
    }

    vp.st.video_width = w;
    vp.st.video_height = h;
    vp.front = back;
    LISA_UI_INVOKE_UI(vp_ui_show_frame, NULL, 0);
    vp.st.frames++;
    vp.st.display_frames++;
    return 0;
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
/* 帧处理（player task 上下文）                                         */
/* ================================================================== */

/* ---- 音频：ADTS 拼接 + 解码 + 播放 ---- */
/* 会话结束的音频收尾：player 实例终身复用，绝不 destroy。
 * 两个原因：
 * 1. audiomgr 通道是终身制（无注销 API），destroy 后同名重注册必失败
 *    （"Channel id already registered"），焦点从此永远拿不到；
 * 2. destroy 会留下未收尾的 flow 线程访问已释放的信号量
 *    （xQueueSend(NULL) assert 崩机）。
 * 半死流由 play_stream 的内部 reset 自愈 + 写失败重试路径兜底。 */
static void vp_audio_detach(void) {
    if (!vp.player) return;
    if (app_player_finish_stream(vp.player) == APP_PLAYER_OK) {
        vTaskDelay(pdMS_TO_TICKS(300)); /* 等 flow 线程走完退出路径 */
    }
    vp.audio_started = 0;
    vp.audio_write_fails = 0;
}
int vp_on_aac_pub(const uint8_t *data, size_t len) {
    return vp_on_aac(data, len);
}

#define AAC_PEND_MAX 8192

/* 首写水量：lisa_player 音频管线 prepare 需要积累 ≥6 个解码块
 * （"decode count 6 not enough"），app_player 的 first write 又是同步
 * 等 prepared（10s 超时）——只写 1 帧会互相等待死锁。先攒满 preroll
 * 再一次性启动+首写。 */
#define VP_AUDIO_PREROLL_FRAMES 8
static int16_t *a_preroll;    /* PSRAM，8 帧 x 1024 样本 */
static int a_preroll_samples; /* 已攒样本数（单声道） */

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
    vp.audio_start_retries++;
    int ps = app_player_play_stream(vp.player, info.sampRateOut, 1, 16);
    if (ps != APP_PLAYER_OK) {
        LOGW("play_stream ret=%d focus=%d retry#%d", ps,
             app_player_focus_get_state(vp.player), vp.audio_start_retries);
        LOGE("play_stream(%d Hz) failed", info.sampRateOut);
        return -1;
    }
    /* 首写 preroll（一次性超过 prepare 水量） */
    int written = app_player_write_stream(
        vp.player, (const uint8_t *)a_preroll,
        (uint32_t)a_preroll_samples * 2, 2000);
    if (written < 0) {
        LOGE("preroll write failed");
        return -1;
    }
    vp.audio_started = 1;
    vp.audio_write_fails = 0;
    LOGI("audio stream started: %d Hz %d ch (preroll %d samples)",
         info.sampRateOut, info.nChans, a_preroll_samples);
    return 0;
}

/* 解码出一帧单声道 PCM（mono/mono_samples）后的分发：preroll 攒够前
 * 只入攒冲，攒够一次性启动；启动后直写 */
static void vp_aac_deliver_pcm(const int16_t *mono, int mono_samples) {
    if (!vp.audio_started) {
        int room = AAC_MAX_NSAMPS * VP_AUDIO_PREROLL_FRAMES - a_preroll_samples;
        int n = mono_samples < room ? mono_samples : room;
        memcpy(a_preroll + a_preroll_samples, mono, (size_t)n * 2);
        a_preroll_samples += n;
        if (a_preroll_samples >= AAC_MAX_NSAMPS * VP_AUDIO_PREROLL_FRAMES) {
            if (vp_aac_start_stream() != 0) {
                /* 启动失败（焦点被拒/prepare 失败）：丢弃 preroll 重新攒；
                 * 每 5 次退避 1s，给占用焦点的通道（如提示音）时间结束 */
                a_preroll_samples = 0;
                if (vp.audio_start_retries % 5 == 0) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        }
        return;
    }
    int written = app_player_write_stream(vp.player, (const uint8_t *)mono,
                                          (uint32_t)mono_samples * 2, 200);
    if (written < 0) {
        /* 流被底层 reset（如 10s 读超时）等场景：连续失败达阈值
         * 后标记重建，避免每 2ms 一条错误把日志打爆 */
        if (++vp.audio_write_fails >= VP_AUDIO_WRITE_FAIL_MAX) {
            vp.audio_started = 0;
            vp.audio_write_fails = 0;
            a_preroll_samples = 0;
            vp_audio_detach();
            LOGW("audio write failed x%d, stream detached for restart",
                 VP_AUDIO_WRITE_FAIL_MAX);
        }
        return;
    }
    vp.audio_write_fails = 0;
    vp.st.audio_frames++;
}

int vp_on_aac(const uint8_t *data, size_t len);
int vp_on_aac(const uint8_t *data, size_t len) {
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
            vp_aac_deliver_pcm(mono, mono_samples);
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

/* ================================================================== */
/* 播放任务：环形缓冲 → 解复用 → 解码                                  */
/* ================================================================== */

/* 追直播沿：清空全部积压（网络重连后从服务器最新位置重新起播）。
 * 不清积压的话，旧数据持续占着环形缓冲，新数据立刻又触发高水位重连，
 * 形成重连风暴；同时长时间消化旧数据会让音频流断供触发底层 reset。 */
static void vp_seek_live_flush(void) {
    /* 先收干：xStreamBufferReset 检测到挂起的读/写会返回失败，
     * net 任务断连后不写，但保险起见先把残余读空 */
    static uint8_t sink[1024];
    while (xStreamBufferReceive(vp.ring, sink, sizeof(sink), 0) > 0) {
    }
    xStreamBufferReset(vp.ring);
    vp.tlv_len = 0;
    vp.aac_pend_len = 0;
    vp.audio_write_fails = 0;
    a_preroll_samples = 0;
    /* 注意：这里绝不 stop/destroy 播放器——destroy 会 join 底层回调
     * 线程，在流不完整时永不返回，把 vp_play 任务整个卡死（ring 无人
     * 消费 -> 高水位 -> 重连风暴）。PCM 序列断档仅表现为短暂静音，
     * 流仍存活可继续写。流真坏掉时由连续写失败路径重建。 */
    vp.st.drop_chunks += 0; /* 积压丢弃不计入网络分片丢弃 */
    LOGI("seek live: backlog flushed");
}

static void vp_play_task(void *arg) {
    (void)arg;
    static uint8_t chunk[4096];
    while (vp.state == VP_STATE_RUNNING) {
        if (vp.seek_live) {
            vp.seek_live = 0;
            vp_seek_live_flush();
        }
        size_t n = xStreamBufferReceive(vp.ring, chunk, sizeof(chunk),
                                        pdMS_TO_TICKS(200));
        if (n == 0) continue;
        vp_tlv_feed(chunk, n);
    }
    vTaskDelete(NULL);
}

/* ================================================================== */
/* 网络任务：HTTP 分块拉流 → 环形缓冲                                  */
/* ================================================================== */

static int vp_http_on_chunk(lisa_http_data_t *data) {
    if (vp.state != VP_STATE_RUNNING) return 1;

    /* 直播沿控制：缓冲接近满说明消费不过来，断开重连并请求清空积压
     * （play 任务在 seek_live 里执行，避免与 StreamBuffer 写端竞争） */
    size_t used = xStreamBufferBytesAvailable(vp.ring);
    if (used * 100 > VP_NET_RING_SIZE * VP_RING_HIGH_PCT) {
        LOGW("ring %zu/%d, reconnect to live edge", used, VP_NET_RING_SIZE);
        vp.st.http_hops++;
        vp.seek_live = 1;
        return 1;
    }

    size_t sent = xStreamBufferSend(vp.ring, data->buf, (size_t)data->len, 0);
    if (sent < (size_t)data->len) {
        /* 无等待发送下不足：丢弃该分片，计数 */
        vp.st.drop_chunks++;
    }
    return 0;
}

/* lisa_http_init 强制要求 on_data 非空（即使走 chunk 回调路径），给个空实现 */
static void vp_http_ignore_data(lisa_http_data_t *data) {
    (void)data;
}

static void vp_net_task(void *arg) {
    (void)arg;
    while (vp.state == VP_STATE_RUNNING) {
        lisa_http_request_t req = {
            .method = LISA_HTTP_GET,
            .url = (uint8_t *)vp.url,
            .timeout = VP_HTTP_TIMEOUT_MS,
            .user = NULL,
            .on_data = vp_http_ignore_data,
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

    if (vp.aac) {
        AACFreeDecoder(vp.aac);
        vp.aac = NULL;
    }
    /* player 终身复用（见 vp_audio_detach），此处仅收尾流 */
    if (vp.ring) {
        vStreamBufferDelete(vp.ring);
        vp.ring = NULL;
    }
    if (vp.ring_buf) {
        psram_free(vp.ring_buf);
        vp.ring_buf = NULL;
    }
    if (vp.tlv_buf) {
        psram_free(vp.tlv_buf);
        vp.tlv_buf = NULL;
    }
    if (vp.pcm) {
        psram_free(vp.pcm);
        vp.pcm = NULL;
    }
    if (vp.aac_pend) {
        psram_free(vp.aac_pend);
        vp.aac_pend = NULL;
    }
    if (a_preroll) {
        psram_free(a_preroll);
        a_preroll = NULL;
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
    vp.tlv_buf = psram_malloc(VP_TLV_PAYLOAD_MAX + VP_TLV_HDR);
    vp.pcm = psram_malloc(AAC_MAX_NSAMPS * AAC_MAX_NCHANS * 2);
    vp.aac_pend = psram_malloc(AAC_PEND_MAX);
    a_preroll = psram_malloc(AAC_MAX_NSAMPS * VP_AUDIO_PREROLL_FRAMES * 2);
    vp.frames[0] = psram_malloc(240 * 240 * 2);
    vp.frames[1] = psram_malloc(240 * 240 * 2);
    if (!vp.ring_buf || !vp.tlv_buf || !vp.pcm ||
        !vp.aac_pend || !a_preroll || !vp.frames[0] || !vp.frames[1]) {
        LOGE("psram alloc failed");
        return -1;
    }
    vp.ring = xStreamBufferCreateStatic(VP_NET_RING_SIZE, 1, vp.ring_buf,
                                        &vp.ring_desc);

    vp.aac = AACInitDecoder();
    if (!vp.aac) {
        LOGE("aac decoder create failed");
        return -1;
    }

    return 0;
}

/* ================================================================== */
/* 公开 API                                                            */
/* ================================================================== */

int video_player_start(const char *url) {
    if (!url || !url[0]) return -1;
    if (vp.state == VP_STATE_STOPPING) {
        LOGW("previous session still stopping, retry later");
        return -1;
    }
    if (vp.state == VP_STATE_RUNNING) {
        if (strcmp(vp.url, url) == 0) return 0;
        video_player_stop();
        /* 等清理完成（清完 state 回 IDLE） */
        for (int i = 0; i < 60 && vp.state != VP_STATE_IDLE; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (vp.state != VP_STATE_IDLE) return -1;
    }

    memset(&vp.st, 0, sizeof(vp.st));
    vp.front = 0;
    vp.audio_started = 0;
    vp.audio_write_fails = 0;
    vp.audio_start_retries = 0;
    vp.decode_errors = 0;
    vp.aac_pend_len = 0;
    vp.tlv_len = 0;
    a_preroll_samples = 0;
    vp.seek_live = 0;
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

/* 清理任务：等 net/play 任务退出后释放全部资源。独立于调用线程，
 * 使 UI 线程的 stop（presenter pause）立即返回，也避免跨线程销毁
 * app_player 的竞态。 */
static void vp_cleanup_task(void *arg) {
    (void)arg;
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
    vTaskDelete(NULL);
}

int video_player_stop(void) {
    if (vp.state == VP_STATE_IDLE) return 0;
    if (vp.state == VP_STATE_STOPPING) return 0; /* 正在停，幂等 */

    vp.state = VP_STATE_STOPPING;
    TaskHandle_t t;
    /* app_player_destroy 调用链深（stop 尝试/回调线程收尾/焦点释放），
     * 栈不足会溢出踩内存（函数指针被字符串覆盖后跳数据区执行） */
    if (xTaskCreate(vp_cleanup_task, "vp_clean", 16384 / 4, NULL,
                    VP_TASK_PRIO, &t) != pdPASS) {
        /* 极端情况起不了任务：退化为同步清理 */
        vp_cleanup_task(NULL);
    }
    return 0;
}

bool video_player_is_playing(void) {
    return vp.state == VP_STATE_RUNNING;
}

void video_player_get_status(video_player_status_t *out) {
    if (!out) return;
    *out = vp.st;
    out->playing = (vp.state == VP_STATE_RUNNING);
    out->audio_start_retries = (uint32_t)vp.audio_start_retries;
    out->decode_errors = vp.decode_errors;
}

/* ================================================================== */
/* shell: vp play <url> | vp stop | vp status                          */
/* ================================================================== */

static int vp_shell_cmd(int argc, char **argv) {
    if (argc < 2) {
        shellPrint(shellGetCurrent(),
                   "usage: vp play <url> | vp stop | vp status\n");
        return 0;
    }
    if (strcmp(argv[1], "play") == 0 && argc >= 3) {
        shellPrint(shellGetCurrent(), "vp: starting %s\n", argv[2]);
        return video_player_start(argv[2]);
    }
    if (strcmp(argv[1], "stop") == 0) {
        return video_player_stop();
    }
    if (strcmp(argv[1], "status") == 0) {
        video_player_status_t st;
        video_player_get_status(&st);
        shellPrint(shellGetCurrent(),
                   "vp: playing=%d %dx%d fps=%u frames=%u disp=%u audio=%u "
                   "hops=%u drops=%u dec_ms=%u a_retry=%u dec_err=%u\n",
                   st.playing, st.video_width, st.video_height,
                   st.declared_fps, st.frames, st.display_frames,
                   st.audio_frames, st.http_hops, st.drop_chunks,
                   st.last_decode_ms, st.audio_start_retries,
                   st.decode_errors);
        return 0;
    }
    shellPrint(shellGetCurrent(), "vp: unknown '%s'\n", argv[1]);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 vp, vp_shell_cmd, online h264 stream player);
