/*
 * sip_service.c - 设备侧 SIP 通话服务
 *
 * 任务模型（栈按字节给出，xTaskCreate 时 /4）：
 *   sip_sig  6144B prio 6  常驻：5060 收发 + sip_ua_poll(50ms) + 命令队列
 *                              + 媒体生命周期管理 + （M3 起）UI 桥
 *   sip_aud  4096B prio 5  会话期间：G.711 双向（mic 环 → RTP 上行；
 *                          RTP 下行 → 解码 → call 播放器流式写入）
 *
 * 播放器：app_player_create("call") 首次通话懒创建、终身复用（audiomgr
 * 通道终身制，destroy 后无法重注册 —— 与 video_player 相同的铁律）。
 * 焦点：voice_player_comm.c 注册 "call" 通道（prio 5，被抢走 = PAUSE 可恢复）。
 * mic 来源：app_wakeup 的 post-AEC 干净单麦 tap（sip_service_on_mic_pcm）。
 */
#define TAG "sip_service"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stream_buffer.h"

#include "lisa_log.h"
#include "shell.h"
#include "sysheap.h"

#include "app_player.h"
#include "lisa_kv.h"
#include "lisa_jpeg.h"

#include "sip_osal.h"
#include "sip_g711.h"
#include "sip_rtp.h"
#include "sip_jpeg_rtp.h"
#include "sip_ua.h"
#include "sip_service.h"

/* 相机/JPEG：头文件路径不跨层传播（services 与 utils 的 include 在
 * sip 之后注册），按本仓库先例用本地原型声明，链接期解析。 */
extern int service_camera_capture(uint8_t *buffer, uint32_t buffer_len);
extern int service_camera_stop(void);
extern int service_camera_get_framesize(uint16_t *width, uint16_t *height);
extern int image_rgb565_to_jpeg(const uint8_t *rgb565, uint32_t len, int width, int height,
                                uint8_t **jpeg_data, uint32_t *jpeg_len);
extern void image_jpeg_free(void *jpeg);

extern void voice_player_play_prompt_tone_url(const char *url);
extern void voice_player_tone_stop(void);
extern char *app_tone_get_url(uint16_t tone_id);

#define SIP_RING_TONE_ID 94u        /* 094_alarm.mp3 */
#define SIP_RING_PERIOD_MS 3000u    /* 来电铃每 3s 重播一次 */

#define SIP_VID_STACK 16384u
#define SIP_VID_PRIO 3u
#define SIP_VID_INTERVAL_MS 200u    /* 上行目标 5fps，按编码耗时自适应 */
#define SIP_VID_MAX_INTERVAL_MS 400u
#define SIP_VID_START_DELAY_MS 500u /* 等主页预览让出相机 */

#define SIP_SIG_STACK 16384u  /* poll+parse+构造链上多层 KB 级缓冲，6KB 实测 94.5% 占用 */
#define SIP_AUD_STACK 6144u
#define SIP_SIG_PRIO 6u
#define SIP_AUD_PRIO 5u
#define SIP_AUDIO_PORT 5062u
#define SIP_VIDEO_PORT 5064u
#define SIP_PTIME_MS 20u
#define SIP_PTIME_SAMPLES 160u       /* 8kHz 20ms */
#define SIP_MIC_RING_BYTES 4096u     /* 8k 样本 2048 个 = 256ms */
#define SIP_PREROLL_PKTS 4u          /* 80ms 预滚后再开流（防 prepare 水位死锁） */
#define SIP_RX_SILENCE_HANGUP_MS 30000u
#define SIP_CALL_PLAYER_NAME "call"

/* ============================ 状态 ============================ */

typedef enum {
    SIP_CMD_NONE = 0,
    SIP_CMD_CALL,
    SIP_CMD_ANSWER,
    SIP_CMD_REJECT,
    SIP_CMD_HANGUP,
    SIP_CMD_CANCEL,
} sip_cmd_t;

typedef struct {
    sip_cmd_t kind;
    char ip[16];
    uint16_t port;
} sip_cmd_msg_t;

static sip_ua_t *s_ua;                 /* PSRAM（~5KB，两块 2KB 报文缓冲） */
static QueueHandle_t s_cmd_q;
static TaskHandle_t s_sig_task;
static TaskHandle_t s_aud_task;
static volatile bool s_aud_running;
static StreamBufferHandle_t s_mic_ring;
static app_player_t *s_player;         /* 懒创建，终身复用 */
static char s_peer_ip[16];
static sip_rtp_t s_rtp_aud;
static volatile bool s_rtp_aud_open;
static sip_rtp_t s_rtp_vid;
static volatile bool s_rtp_vid_open;
static TaskHandle_t s_vid_task;
static volatile bool s_vid_running;
/* 下行：重组器 + RGB565 解码双缓冲（UI 换指针显示，同 video_player 手法） */
static sip_jpeg_rx_t *s_jpeg_rx;
static uint16_t *s_dec_frames[2];
static int s_dec_front;
static volatile uint32_t s_rx_vid_frames, s_dec_ms, s_rx_vid_drop;
/* 统计（shell status） */
static volatile uint32_t s_tx_pkts, s_rx_pkts, s_rx_loss;
static volatile uint32_t s_vid_frames, s_vid_enc_ms, s_vid_bytes;
/* UI 桥回调（模型层注册） */
static sip_service_event_cb_t s_event_cb;
static void *s_event_user;
static sip_service_frame_cb_t s_frame_cb;
static void *s_frame_user;

void sip_service_set_event_cb(sip_service_event_cb_t cb, void *user)
{
    s_event_cb = cb;
    s_event_user = user;
}

void sip_service_set_frame_cb(sip_service_frame_cb_t cb, void *user)
{
    s_frame_cb = cb;
    s_frame_user = user;
}

static void notify_event(int svc_state)
{
    if (s_event_cb) {
        s_event_cb(svc_state, s_event_user);
    }
}

void sip_service_push_frame(const uint16_t *frame, int w, int h)
{
    if (s_frame_cb) {
        s_frame_cb(frame, w, h, s_frame_user);
    }
}

/* ============================ 音频任务 ============================ */

static void sip_aud_task(void *arg)
{
    const sip_session_t *session = &s_ua->session;
    uint8_t enc[SIP_PTIME_SAMPLES];
    uint8_t net[SIP_PTIME_SAMPLES];
    int16_t pcm[SIP_PTIME_SAMPLES];
    uint8_t preroll[SIP_PREROLL_PKTS][SIP_PTIME_SAMPLES];
    uint32_t preroll_count = 0;
    uint32_t next_tick = sip_osal_now_ms();
    uint32_t last_rx = sip_osal_now_ms();
    uint32_t write_fail = 0;
    bool player_streaming = false;

    if (sip_rtp_open(&s_rtp_aud, SIP_AUDIO_PORT, (uint8_t)session->audio_pt) != 0) {
        LISA_LOGE(TAG, "audio rtp bind %u failed", SIP_AUDIO_PORT);
        goto out;
    }
    s_rtp_aud_open = true;
    sip_rtp_set_peer(&s_rtp_aud, session->peer_ip, session->peer_audio_port);
    LISA_LOGI(TAG, "aud: %s:%u pt=%d", session->peer_ip, session->peer_audio_port,
              session->audio_pt);

    while (s_aud_running) {
        uint32_t now = sip_osal_now_ms();
        if ((int32_t)(now - next_tick) >= 0) {
            next_tick = now + SIP_PTIME_MS;

            /* 上行：mic 环凑满 20ms 就发（凑不满发静音，维持节拍与 ts 连续）。 */
            size_t got = 0;
            if (s_mic_ring) {
                got = xStreamBufferReceive(s_mic_ring, pcm,
                                           SIP_PTIME_SAMPLES * sizeof(int16_t), 0);
            }
            if (got == SIP_PTIME_SAMPLES * sizeof(int16_t)) {
                sip_g711_encode(session->audio_pt, pcm, enc, SIP_PTIME_SAMPLES);
                if (sip_rtp_send_audio(&s_rtp_aud, enc, SIP_PTIME_SAMPLES) > 0) {
                    s_tx_pkts++;
                }
            } else {
                /* mic 环没凑满：发舒适静音维持节拍与时间戳连续。 */
                memset(enc, session->audio_pt == 0 ? 0xFF : 0x55, sizeof(enc));
                if (sip_rtp_send_audio(&s_rtp_aud, enc, SIP_PTIME_SAMPLES) > 0) {
                    s_tx_pkts++;
                }
            }
        }

        /* 下行：非阻塞把积压的包全部收掉。 */
        for (int i = 0; i < 8; ++i) {
            int n = sip_rtp_recv_audio(&s_rtp_aud, net, sizeof(net), 0);
            if (n <= 0) {
                break;
            }
            last_rx = sip_osal_now_ms();
            s_rx_pkts++;
            s_rx_loss = s_rtp_aud.rx_loss_events;
            if (n != SIP_PTIME_SAMPLES) {
                continue; /* 只接受 20ms 整包（本栈固定 ptime） */
            }
            sip_g711_decode(session->audio_pt, net, pcm, SIP_PTIME_SAMPLES);

            if (!player_streaming) {
                /* 预滚：攒够再开流 + 首写（对齐 vp_aac_start_stream 的手法）。 */
                if (preroll_count < SIP_PREROLL_PKTS) {
                    memcpy(preroll[preroll_count], pcm, sizeof(pcm));
                    preroll_count++;
                    if (preroll_count < SIP_PREROLL_PKTS) {
                        continue;
                    }
                }
                if (!s_player) {
                    s_player = app_player_create(SIP_CALL_PLAYER_NAME);
                    if (!s_player) {
                        LISA_LOGE(TAG, "create '%s' player failed", SIP_CALL_PLAYER_NAME);
                        goto out;
                    }
                }
                if (app_player_play_stream(s_player, 8000, 1, 16) != APP_PLAYER_OK) {
                    LISA_LOGW(TAG, "play_stream refused (focus?), retry next pkt");
                    preroll_count = 0;
                    continue;
                }
                for (uint32_t k = 0; k < SIP_PREROLL_PKTS; ++k) {
                    (void)app_player_write_stream(s_player, preroll[k],
                                                  sizeof(pcm), 200);
                }
                player_streaming = true;
                LISA_LOGI(TAG, "aud: downlink streaming started");
            } else if (app_player_write_stream(s_player, (const uint8_t *)pcm,
                                               sizeof(pcm), 200) != APP_PLAYER_OK) {
                write_fail++;
                if (write_fail >= 3) {
                    LISA_LOGW(TAG, "write_stream failed x3, restart stream");
                    (void)app_player_finish_stream(s_player);
                    player_streaming = false;
                    preroll_count = 0;
                    write_fail = 0;
                }
            } else {
                write_fail = 0;
            }
        }

        /* 静默看门狗：对端 30s 无 RTP（掉线/对端死了）→ 请求挂断。 */
        if (sip_osal_now_ms() - last_rx > SIP_RX_SILENCE_HANGUP_MS) {
            LISA_LOGW(TAG, "aud: 30s RTP silence, request hangup");
            sip_cmd_msg_t cmd = {.kind = SIP_CMD_HANGUP};
            xQueueSend(s_cmd_q, &cmd, 0);
            last_rx = sip_osal_now_ms();
        }

        sip_osal_sleep_ms(2);
    }

    if (player_streaming) {
        (void)app_player_finish_stream(s_player);
    }

out:
    if (s_rtp_aud_open) {
        sip_rtp_close(&s_rtp_aud);
        s_rtp_aud_open = false;
    }
    if (s_mic_ring) {
        xStreamBufferReset(s_mic_ring);
    }
    LISA_LOGI(TAG, "aud: stopped (tx=%u rx=%u loss=%u)", s_tx_pkts, s_rx_pkts, s_rx_loss);
    s_aud_task = NULL;
    vTaskDelete(NULL);
}

/* ============================ 视频任务 ============================ */

/*
 * 上行视频：相机采集（RGB565 240x320）→ 软件 JPEG（tiny_jpeg，拍照上传
 * 在用的同一条编码路径；不用硬编 —— JPEG 硬编与下行硬解共用 GPDMA ch0，
 * 通话中二者互斥，软件编码彻底避开）→ RFC2435 分片发送。
 * 帧率自适应：周期 = max(200ms, 实测编码耗时 + 20ms)，上限 400ms（2.5fps）。
 */
/*
 * 下行视频：RTP 收包 → sip_jpeg_rx 重组 → jpeg_decoder 硬解直出 RGB565
 * （GPDMA IN ch0/OUT ch5，与上行软件编码互不冲突）→ 双缓冲换指针 →
 * UI 帧回调（模型层投 worq.ui 上屏）。
 * 超过屏幕尺寸的帧直接丢弃（RTP 头里带 w/h，解码前就能判断）。
 */
static void vid_handle_downlink(void)
{
    uint8_t pkt[1500];
    for (int i = 0; i < 16 && s_vid_running; ++i) {
        int n = sip_osal_udp_recv(s_rtp_vid.fd, pkt, sizeof(pkt), 0, NULL, NULL);
        if (n <= 0) {
            break;
        }
        const uint8_t *frame;
        uint32_t len;
        int w = 0, h = 0;
        int rc = sip_jpeg_rx_feed(s_jpeg_rx, pkt, n, SIP_JPEG_PT,
                                  &frame, &len, &w, &h);
        if (rc != 1 || len == 0) {
            continue;
        }
        if (w <= 0 || h <= 0 || w > 240 || h > 320) {
            s_rx_vid_drop++;
            LISA_LOGW(TAG, "vid: drop frame %dx%d (screen 240x320)", w, h);
            continue;
        }
        uint32_t t0 = sip_osal_now_ms();
        int back = 1 - s_dec_front;
        uint16_t dw = (uint16_t)w, dh = (uint16_t)h;
        Jpeg_DecoderCfg cfg = {.output_format = JPEG_PIXEL_FORMAT_RGB565};
        if (jpeg_decoder(frame, len, (uint8_t *)s_dec_frames[back], &dw, &dh, cfg) != 0) {
            s_rx_vid_drop++;
            continue;
        }
        s_dec_ms = sip_osal_now_ms() - t0;
        s_dec_front = back;
        s_rx_vid_frames++;
        sip_service_push_frame(s_dec_frames[s_dec_front], dw, dh);
    }
}

static void sip_vid_task(void *arg)
{
    const sip_session_t *session = &s_ua->session;

    if (!session->video || session->peer_video_port == 0) {
        LISA_LOGI(TAG, "vid: video not negotiated, task exits");
        goto out;
    }
    if (sip_rtp_open(&s_rtp_vid, SIP_VIDEO_PORT, SIP_JPEG_PT) != 0) {
        LISA_LOGE(TAG, "video rtp bind %u failed", SIP_VIDEO_PORT);
        goto out;
    }
    s_rtp_vid_open = true;
    sip_rtp_set_peer(&s_rtp_vid, session->peer_ip, session->peer_video_port);

    uint16_t cw = 240, ch = 320;
    (void)service_camera_get_framesize(&cw, &ch);
    const uint32_t cam_bytes = (uint32_t)cw * ch * 2u;
    uint8_t *cam = psram_malloc(cam_bytes);
    if (!cam) {
        LISA_LOGE(TAG, "vid: camera buffer alloc failed (%u)", (unsigned)cam_bytes);
        goto out;
    }
    /* 下行重组器 + 解码双缓冲（240x320 上限，同屏幕）。 */
    s_jpeg_rx = psram_malloc(sizeof(*s_jpeg_rx));
    s_dec_frames[0] = psram_malloc(240u * 320u * 2u);
    s_dec_frames[1] = psram_malloc(240u * 320u * 2u);
    if (!s_jpeg_rx || !s_dec_frames[0] || !s_dec_frames[1]) {
        LISA_LOGE(TAG, "vid: downlink buffer alloc failed");
        goto out;
    }
    memset(s_jpeg_rx, 0, sizeof(*s_jpeg_rx));
    s_dec_front = 0;
    s_rx_vid_frames = s_dec_ms = s_rx_vid_drop = 0;

    /* 等主页相机预览随导航让出（通话开始时 UI 切到通话页）。 */
    sip_osal_sleep_ms(SIP_VID_START_DELAY_MS);

    uint32_t vid_ts = 0;
    uint32_t interval = SIP_VID_INTERVAL_MS;
    LISA_LOGI(TAG, "vid: uplink %ux%u -> %s:%u", cw, ch,
              session->peer_ip, session->peer_video_port);

    while (s_vid_running) {
        uint32_t t0 = sip_osal_now_ms();

        /* 下行优先排空（实时通话宁丢帧不积压），再采上行。 */
        vid_handle_downlink();

        if (service_camera_capture(cam, cam_bytes) != 0) {
            LISA_LOGW(TAG, "vid: capture failed");
            sip_osal_sleep_ms(200);
            continue;
        }

        uint8_t *jpeg = NULL;
        uint32_t jpeg_len = 0;
        if (image_rgb565_to_jpeg(cam, cam_bytes, cw, ch, &jpeg, &jpeg_len) != 0 ||
            !jpeg || jpeg_len == 0) {
            LISA_LOGW(TAG, "vid: jpeg encode failed");
            continue;
        }

        uint32_t enc_ms = sip_osal_now_ms() - t0;
        int frags = sip_jpeg_rtp_send_frame(&s_rtp_vid, jpeg, (int)jpeg_len,
                                            (int)cw, (int)ch, vid_ts);
        image_jpeg_free(jpeg);
        if (frags <= 0) {
            LISA_LOGW(TAG, "vid: send failed (%u bytes)", jpeg_len);
            sip_osal_sleep_ms(200);
            continue;
        }
        vid_ts += 90000u / (1000u / interval); /* 按实际帧间隔推进 90kHz 时钟 */
        s_vid_frames++;
        s_vid_bytes += jpeg_len;
        s_vid_enc_ms = enc_ms;

        uint32_t elapsed = sip_osal_now_ms() - t0;
        uint32_t wait = (elapsed + 20u > SIP_VID_INTERVAL_MS) ? elapsed + 20u
                                                              : SIP_VID_INTERVAL_MS;
        if (wait > SIP_VID_MAX_INTERVAL_MS) {
            wait = SIP_VID_MAX_INTERVAL_MS;
        }
        interval = wait;
        sip_osal_sleep_ms(wait);
        vid_handle_downlink();
    }

    psram_free(cam);
    if (s_jpeg_rx) {
        psram_free(s_jpeg_rx);
        s_jpeg_rx = NULL;
    }
    for (int i = 0; i < 2; ++i) {
        if (s_dec_frames[i]) {
            psram_free(s_dec_frames[i]);
            s_dec_frames[i] = NULL;
        }
    }
    (void)service_camera_stop();

out:
    if (s_rtp_vid_open) {
        sip_rtp_close(&s_rtp_vid);
        s_rtp_vid_open = false;
    }
    LISA_LOGI(TAG, "vid: stopped (tx=%u enc_ms=%u rx=%u dec_ms=%u drop=%u)",
              s_vid_frames, s_vid_enc_ms, s_rx_vid_frames, s_dec_ms, s_rx_vid_drop);
    s_vid_task = NULL;
    vTaskDelete(NULL);
}

static void media_start(void)
{
    if (s_aud_task) {
        return;
    }
    if (!s_mic_ring) {
        s_mic_ring = xStreamBufferCreate(SIP_MIC_RING_BYTES, 1);
        if (!s_mic_ring) {
            LISA_LOGE(TAG, "mic ring create failed");
            return;
        }
    }
    s_aud_running = true;
    if (xTaskCreate(sip_aud_task, "sip_aud", SIP_AUD_STACK / 4u, NULL,
                    SIP_AUD_PRIO, &s_aud_task) != pdPASS) {
        s_aud_running = false;
        s_aud_task = NULL;
        LISA_LOGE(TAG, "create sip_aud failed");
    }
    s_vid_frames = s_vid_enc_ms = s_vid_bytes = 0;
    s_vid_running = true;
    if (xTaskCreate(sip_vid_task, "sip_vid", SIP_VID_STACK / 4u, NULL,
                    SIP_VID_PRIO, &s_vid_task) != pdPASS) {
        s_vid_running = false;
        s_vid_task = NULL;
        LISA_LOGE(TAG, "create sip_vid failed");
    }
}

static void media_stop(void)
{
    if (s_aud_task) {
        s_aud_running = false;
        if (s_rtp_aud_open) {
            sip_osal_udp_close(s_rtp_aud.fd); /* 解除 recv 阻塞（gamepad_udp 手法） */
        }
        /* 任务自清理（s_aud_task = NULL + vTaskDelete）。等待最多 2s。 */
        for (int i = 0; i < 100 && s_aud_task; ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (s_vid_task) {
        s_vid_running = false;
        if (s_rtp_vid_open) {
            sip_osal_udp_close(s_rtp_vid.fd);
        }
        for (int i = 0; i < 200 && s_vid_task; ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

/* ============================ 信令任务 ============================ */

static void ua_on_state(void *user, sip_ua_state_t st)
{
    (void)user;
    switch (st) {
    case SIP_UA_ACTIVE:
        snprintf(s_peer_ip, sizeof(s_peer_ip), "%s", sip_ua_session(s_ua)->peer_ip);
        LISA_LOGI(TAG, "call active with %s (pt=%d video=%d)", s_peer_ip,
                  sip_ua_session(s_ua)->audio_pt, sip_ua_session(s_ua)->video);
        s_tx_pkts = s_rx_pkts = s_rx_loss = 0;
        media_start();
        break;
    case SIP_UA_IDLE:
    case SIP_UA_ENDED:
        media_stop();
        break;
    default:
        break; /* 振铃等中间态：UI 桥下面统一通知 */
    }
    notify_event((int)st); /* sip_svc_state_t 与 sip_ua_state_t 取值一一对应 */
}

static void ua_on_incoming(void *user, const char *peer_ip, uint16_t sig_port)
{
    (void)user;
    (void)sig_port;
    snprintf(s_peer_ip, sizeof(s_peer_ip), "%s", peer_ip);
    LISA_LOGI(TAG, "incoming call from %s (单击/按键接听，sip reject 拒接)", peer_ip);
}

static void sip_sig_task(void *arg)
{
    (void)arg;
    sip_ua_cfg_t cfg = {
        .sig_port = SIP_DEFAULT_PORT,
        .audio_port = SIP_AUDIO_PORT,
        .video_port = SIP_VIDEO_PORT,
        .offer_video = 1,
        .local_user = "arcs",
        .on_state = ua_on_state,
        .on_incoming = ua_on_incoming,
    };
    s_ua = psram_malloc(sizeof(*s_ua));
    if (!s_ua) {
        LISA_LOGE(TAG, "ua alloc failed (%u bytes)", (unsigned)sizeof(*s_ua));
        s_sig_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    if (sip_ua_init(s_ua, &cfg) != 0) {
        LISA_LOGE(TAG, "ua init failed");
        psram_free(s_ua);
        s_ua = NULL;
        s_sig_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    sip_cmd_msg_t cmd;
    uint32_t last_ring_ms = 0;
    while (1) {
        sip_ua_poll(s_ua, 50);
        while (s_cmd_q && xQueueReceive(s_cmd_q, &cmd, 0) == pdTRUE) {
            switch (cmd.kind) {
            case SIP_CMD_CALL:
                if (sip_ua_invite(s_ua, cmd.ip, cmd.port) != 0) {
                    LISA_LOGE(TAG, "invite %s failed", cmd.ip);
                }
                break;
            case SIP_CMD_ANSWER:
                if (sip_ua_answer(s_ua) != 0) {
                    LISA_LOGW(TAG, "answer rejected (state=%d)", (int)sip_ua_state(s_ua));
                }
                break;
            case SIP_CMD_REJECT:
                sip_ua_reject(s_ua);
                break;
            case SIP_CMD_HANGUP:
                sip_ua_bye(s_ua);
                sip_ua_cancel(s_ua);
                break;
            case SIP_CMD_CANCEL:
                sip_ua_cancel(s_ua);
                break;
            default:
                break;
            }
        }

        /* 来电铃：Ringing 期间每 3s 重播一段（tone 通道 prio 0，此时 call
         * 流尚未建立，无焦点冲突）；离开振铃态即停。 */
        if (sip_ua_state(s_ua) == SIP_UA_RINGING_IN) {
            uint32_t now = sip_osal_now_ms();
            if (last_ring_ms == 0 || (int32_t)(now - last_ring_ms) >= (int32_t)SIP_RING_PERIOD_MS) {
                voice_player_play_prompt_tone_url(app_tone_get_url(SIP_RING_TONE_ID));
                last_ring_ms = now;
            }
        } else if (last_ring_ms != 0) {
            voice_player_tone_stop();
            last_ring_ms = 0;
        }
    }
}

/* ============================ 对外 API ============================ */

int sip_service_init(void)
{
    sip_osal_set_log_level(SIP_LOG_INFO);
    if (s_sig_task) {
        return 0;
    }
    s_cmd_q = xQueueCreate(4, sizeof(sip_cmd_msg_t));
    if (!s_cmd_q) {
        LISA_LOGE(TAG, "cmd queue create failed");
        return -1;
    }
    if (xTaskCreate(sip_sig_task, "sip_sig", SIP_SIG_STACK / 4u, NULL,
                    SIP_SIG_PRIO, &s_sig_task) != pdPASS) {
        LISA_LOGE(TAG, "create sip_sig failed");
        vQueueDelete(s_cmd_q);
        s_cmd_q = NULL;
        return -1;
    }
    return 0;
}

static int send_cmd(sip_cmd_msg_t *cmd)
{
    if (!s_cmd_q) {
        return -1;
    }
    return xQueueSend(s_cmd_q, cmd, pdMS_TO_TICKS(200)) == pdTRUE ? 0 : -1;
}

sip_svc_state_t sip_service_state(void)
{
    if (!s_ua) {
        return SIP_SVC_IDLE;
    }
    switch (sip_ua_state(s_ua)) {
    case SIP_UA_IDLE:
        return SIP_SVC_IDLE;
    case SIP_UA_INVITING:
        return SIP_SVC_INVITING;
    case SIP_UA_RINGING_OUT:
        return SIP_SVC_RINGING_OUT;
    case SIP_UA_RINGING_IN:
        return SIP_SVC_RINGING_IN;
    case SIP_UA_ACTIVE:
        return SIP_SVC_ACTIVE;
    default:
        return SIP_SVC_ENDED;
    }
}

bool sip_service_in_call(void)
{
    return sip_service_state() == SIP_SVC_ACTIVE;
}

const char *sip_service_state_name(void)
{
    switch (sip_service_state()) {
    case SIP_SVC_IDLE:
        return "IDLE";
    case SIP_SVC_INVITING:
        return "INVITING";
    case SIP_SVC_RINGING_OUT:
        return "RINGING_OUT";
    case SIP_SVC_RINGING_IN:
        return "RINGING_IN";
    case SIP_SVC_ACTIVE:
        return "ACTIVE";
    case SIP_SVC_ENDED:
        return "ENDED";
    default:
        return "?";
    }
}

const char *sip_service_peer_ip(void)
{
    return s_peer_ip;
}

int sip_service_start_call(const char *peer_ip, uint16_t peer_port)
{
    if (!peer_ip || !peer_ip[0]) {
        return -1;
    }
    sip_cmd_msg_t cmd = {.kind = SIP_CMD_CALL, .port = peer_port};
    snprintf(cmd.ip, sizeof(cmd.ip), "%s", peer_ip);
    return send_cmd(&cmd);
}

int sip_service_answer(void)
{
    sip_cmd_msg_t cmd = {.kind = SIP_CMD_ANSWER};
    return send_cmd(&cmd);
}

int sip_service_reject(void)
{
    sip_cmd_msg_t cmd = {.kind = SIP_CMD_REJECT};
    return send_cmd(&cmd);
}

int sip_service_hangup(void)
{
    sip_cmd_msg_t cmd = {.kind = SIP_CMD_HANGUP};
    return send_cmd(&cmd);
}

void sip_service_cancel(void)
{
    sip_cmd_msg_t cmd = {.kind = SIP_CMD_CANCEL};
    (void)send_cmd(&cmd);
}

void sip_service_on_mic_pcm(const int16_t *pcm, uint32_t samples)
{
    if (!s_aud_running || !s_mic_ring || !pcm || !samples) {
        return;
    }
    /* 16k → 8k：相邻两样本均值抽取。tap 跑在唤醒输出任务（高 prio），
     * 这里只做抽取 + 一次非阻塞写，超出的量直接丢（保持低延迟）。 */
    static int16_t down[128];
    uint32_t out = 0;
    for (uint32_t i = 0; i + 1 < samples && out < 128; i += 2) {
        down[out++] = (int16_t)(((int32_t)pcm[i] + pcm[i + 1]) / 2);
    }
    if (out) {
        xStreamBufferSend(s_mic_ring, down, out * sizeof(int16_t), 0);
    }
}

/* ============================ 对端配置（KV） ============================ */

#define SIP_KV_PEER_KEY "user.sip.peer"

int sip_service_peer_cfg_get(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return -1;
    }
    buf[0] = '\0';
    char *val = NULL;
    if (lisa_kv_get_string(SIP_KV_PEER_KEY, &val) != 0 || !val || !val[0]) {
        if (val) {
            lisa_kv_free(val);
        }
        return -1;
    }
    snprintf(buf, len, "%s", val);
    lisa_kv_free(val);
    return buf[0] ? 0 : -1;
}

int sip_service_peer_cfg_set(const char *ip)
{
    if (!ip || !ip[0]) {
        return -1;
    }
    return lisa_kv_set_string(SIP_KV_PEER_KEY, ip) == 0 ? 0 : -1;
}

void sip_service_debug_ring(void)
{
    /* 不发信令，只驱动 UI 桥（验证来电页/铃声/按键接听链路）。 */
    snprintf(s_peer_ip, sizeof(s_peer_ip), "%s", "self-test");
    notify_event((int)SIP_SVC_RINGING_IN);
}

/* ============================ shell ============================ */

static int sip_shell_cmd(int argc, char **argv)
{
    if (argc < 2) {
        shellPrint(shellGetCurrent(),
                   "usage: sip status | call <ip> [port] | answer | reject | "
                   "hangup | cancel | cfg [<ip>] | ring | log <0-3>\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        const sip_session_t *s = s_ua ? sip_ua_session(s_ua) : NULL;
        char peer[24];
        sip_service_peer_cfg_get(peer, sizeof(peer));
        shellPrint(shellGetCurrent(),
                   "sip: state=%s peer=%s cfg=%s tx=%u rx=%u loss=%u%s\n",
                   sip_service_state_name(), s_peer_ip, peer,
                   s_tx_pkts, s_rx_pkts, s_rx_loss,
                   (s && sip_service_in_call()) ? " (in call)" : "");
        if (s && sip_service_in_call()) {
            shellPrint(shellGetCurrent(),
                       "     audio pt=%d -> %s:%u | vid tx=%u enc_ms=%u rx=%u dec_ms=%u drop=%u\n",
                       s->audio_pt, s->peer_ip, s->peer_audio_port,
                       s_vid_frames, s_vid_enc_ms, s_rx_vid_frames, s_dec_ms,
                       s_rx_vid_drop);
        }
        return 0;
    }
    if (strcmp(argv[1], "call") == 0 && argc >= 3) {
        uint16_t port = argc >= 4 ? (uint16_t)atoi(argv[3]) : 0;
        return sip_service_start_call(argv[2], port);
    }
    if (strcmp(argv[1], "answer") == 0) {
        return sip_service_answer();
    }
    if (strcmp(argv[1], "reject") == 0) {
        return sip_service_reject();
    }
    if (strcmp(argv[1], "hangup") == 0) {
        return sip_service_hangup();
    }
    if (strcmp(argv[1], "cancel") == 0) {
        sip_service_cancel();
        return 0;
    }
    if (strcmp(argv[1], "cfg") == 0) {
        if (argc >= 3) {
            if (sip_service_peer_cfg_set(argv[2]) != 0) {
                shellPrint(shellGetCurrent(), "sip: cfg set failed\n");
                return -1;
            }
        }
        char peer[24];
        int rc = sip_service_peer_cfg_get(peer, sizeof(peer));
        shellPrint(shellGetCurrent(), "sip: peer cfg = %s\n", rc == 0 ? peer : "(unset)");
        return 0;
    }
    if (strcmp(argv[1], "ring") == 0) {
        sip_service_debug_ring();
        return 0;
    }
    if (strcmp(argv[1], "log") == 0 && argc >= 3) {
        sip_osal_set_log_level(atoi(argv[2]));
        shellPrint(shellGetCurrent(), "sip: log level=%d\n", atoi(argv[2]));
        return 0;
    }
    shellPrint(shellGetCurrent(), "sip: unknown '%s'\n", argv[1]);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 sip, sip_shell_cmd, lan sip video call);
