/*
 * sip_service.c - 设备侧 SIP 通话服务
 *
 * 任务模型（栈按字节给出，xTaskCreate 时 /4）：
 *   sip_sig  6144B prio 6  常驻：5060 收发 + sip_ua_poll(50ms) + 命令队列
 *                              + 媒体生命周期管理 + （M3 起）UI 桥
 *   sip_vid_rx 4096B prio 4 会话期间：专职下行收包（5064 → 重组 → 硬解 →
 *                          UI 推帧）。曾与上行同任务：软件编码一帧 2.1s，
 *                          期间完全不收包，下行完整帧率掉到 ~25%。
 *   sip_aud  4096B prio 5  会话期间：G.711 双向（mic 环 → RTP 上行；
 *                          RTP 下行 → 解码 → 8k→16k 上采样 → call 播放器
 *                          以 16000Hz 流式写入 —— 不用 8000Hz 直喂：那是
 *                          预编译 lisa_player 从未被使用过的路径，实测在
 *                          prepare 阶段（speex 8k 重采样初始化）Store Fault）
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
#include "semphr.h"
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
#include "sip_screen.h"
#include "sip_hw_jpeg.h"

/* 头文件路径不跨层传播（services 与 utils 的 include 在
 * sip 之后注册），按本仓库先例用本地原型声明，链接期解析。 */
extern int service_camera_capture_ex(uint8_t *buffer, uint32_t buffer_len, uint8_t drop_frames);
extern int service_camera_stop(void);
extern int service_camera_get_framesize(uint16_t *width, uint16_t *height);
extern int image_rgb565_to_jpeg_q(const uint8_t *rgb565, uint32_t len, int width, int height,
                                  int quality, uint8_t **jpeg_data, uint32_t *jpeg_len);
extern void image_jpeg_free(void *jpeg);

/* 电源策略：通话期间保持 NORMAL，不因 15s 无操作进 idle（否则 UI 导航回
 * 主页 -> 通话页 pause -> 按设计挂断，实测 15s 整通话被切）。 */
extern void service_power_policy_set_miniapp_active(bool active);
static bool s_keep_awake_set;

extern void voice_player_play_prompt_tone_url(const char *url);
extern void voice_player_tone_stop(void);
extern char *app_tone_get_url(uint16_t tone_id);

#define SIP_RING_TONE_ID 94u        /* 094_alarm.mp3 */
#define SIP_RING_PERIOD_MS 3000u    /* 来电铃每 3s 重播一次 */

#define SIP_VID_STACK 16384u
#define SIP_VID_PRIO 3u
#define SIP_VID_RX_STACK 6144u
#define SIP_VID_RX_PRIO 4u
#define SIP_VID_INTERVAL_MS 200u    /* 上行目标 5fps，按编码耗时自适应 */
#define SIP_VID_MAX_INTERVAL_MS 400u
#define SIP_VID_START_DELAY_MS 500u /* 等主页预览让出相机 */

#define SIP_SIG_STACK 16384u  /* poll+parse+构造链上多层 KB 级缓冲，6KB 实测 94.5% 占用 */
#define SIP_AUD_STACK 12288u  /* play_stream -> lisa_player -> speex 重采样初始化调用链深（vp_play 同链路用 16KB） */
#define SIP_SIG_PRIO 6u
#define SIP_AUD_PRIO 5u
#define SIP_AUDIO_PORT 5062u
#define SIP_VIDEO_PORT 5064u
#define SIP_PTIME_MS 20u
#define SIP_PTIME_SAMPLES 160u       /* 8kHz 20ms */
#define SIP_MIC_RING_BYTES 4096u     /* 8k 样本 2048 个 = 256ms */
#define SIP_PLAY_RING_BYTES 65536u   /* 下行播放解耦环 ~2s @32KB/s */
#define SIP_PLAY_CHUNK 8192u         /* 每轮向播放器写一小段（短超时） */
#define SIP_PREROLL_BYTES 8192u     /* 预滚水量：对齐 vp 的 8192 样本（16 个 512 样本轨道帧，prepare
                                 * 需 ≥6 帧）。此前按包数攒（4/8 包 = 1280/2560 样本）都不够，
                                 * 首写在 preparing_sem 上空等 ~10s 超时，循环被拖成 ~1 圈/秒 */
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
static TaskHandle_t s_vidrx_task;
static volatile bool s_vidrx_running;
/* JPEG 硬件外设互斥：编码（上行）与解码（下行）是同一个 Jpeg0 IP，
 * 两个任务并发操作会互相打断硬件状态机 —— 实测表现为下行花屏 +
 * dec_ms 从 45ms 涨到 111ms（并发重试）。所有 jpeg_encoder/jpeg_decoder
 * 调用必须持锁。 */
static SemaphoreHandle_t s_jpeg_lock;
/* 下行：重组器 + 解码 scratch + 显示双缓冲。解码写 scratch、完成后 memcpy
 * 到显示缓冲再推 UI —— UI 拿到的指针内容不会被下一次解码覆写（纯双缓冲
 * 换指针时 LVGL 异步渲染仍读旧缓冲，实测表现为花屏/撕裂）。 */
static sip_jpeg_rx_t *s_jpeg_rx;
static uint8_t *s_dec_scratch_raw;      /* 原始分配指针（释放用） */
static uint16_t *s_dec_scratch;         /* 32B 对齐后的解码输出（GPDMA 写
                                         * PSRAM 有对齐要求；库的编码路径自己
                                         * 做对齐，解码路径直接用调用方指针，
                                         * 不对齐时输出错位=花屏） */
static uint16_t *s_dec_frames[2];       /* 显示双缓冲（memcpy 后才给 UI） */
static int s_dec_front;
static volatile uint32_t s_rx_vid_frames, s_dec_ms, s_rx_vid_drop;
/* drop 分项（共用计数看不出丢在哪一环）：size=超尺寸 sampling=采样
 * 不在白名单 dec=硬解返回错误；另有网络断片在 sip_jpeg_rx 内部整帧
 * 丢弃、不进任何计数。 */
static volatile uint32_t s_rx_drop_size, s_rx_drop_sampling, s_rx_drop_dec;
static volatile uint32_t s_rx_drop_frag; /* 重组断片：网络丢包撕掉的整帧 */
/* 统计（shell status） */
static volatile uint32_t s_tx_pkts, s_rx_pkts, s_rx_loss;
static volatile uint32_t s_vid_frames, s_vid_enc_ms, s_vid_bytes;
static volatile uint32_t s_vid_cap_ms;   /* capture 单项耗时（分项定位用） */
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
    int16_t pcm8[SIP_PTIME_SAMPLES];                 /* 8k 域 20ms = 160 样本 */
    int16_t pcm16[SIP_PTIME_SAMPLES * 2];            /* 16k 域 20ms = 320 样本 */
    uint8_t *preroll = psram_malloc(SIP_PREROLL_BYTES);  /* 8KB 预滚（栈放不下） */
    uint8_t *pend = psram_malloc(SIP_PLAY_CHUNK);        /* 未写完部分暂存 */
    /* 下行收包与播放器写入解耦：write_stream 会因播放器水位阻塞（最长
     * 200ms），若在同一循环里收包，阻塞期间 lwip 收包队列溢出 —— 实测
     * 音频丢包 46%（修复 write 返回值误判、播放器进入正常节流后出现；
     * 之前"低丢包"其实是播放器被反复重开、write 立即返回的 bug 副产品）。
     * 现在收包只入 ring（非阻塞），播放器写入每轮一小段、20ms 短超时、
     * 部分写入保留剩余。 */
    uint8_t *ringbuf = psram_malloc(SIP_PLAY_RING_BYTES);
    StaticStreamBuffer_t *ringcb = psram_malloc(sizeof(StaticStreamBuffer_t));
    StreamBufferHandle_t play_ring = NULL;
    uint32_t preroll_len = 0, pend_len = 0;
    if (preroll && pend && ringbuf && ringcb) {
        play_ring = xStreamBufferCreateStatic(SIP_PLAY_RING_BYTES, 1, ringbuf, ringcb);
    }
    if (!preroll || !pend || !play_ring) {
        LISA_LOGE(TAG, "aud: downlink buffer alloc failed");
        goto out_free_ring;
    }
    uint32_t next_tick = sip_osal_now_ms();
    uint32_t last_rx = sip_osal_now_ms();
    uint32_t write_fail = 0;
    bool player_streaming = false;

    if (sip_rtp_open(&s_rtp_aud, SIP_AUDIO_PORT, (uint8_t)session->audio_pt) != 0) {
        LISA_LOGE(TAG, "audio rtp bind %u failed", SIP_AUDIO_PORT);
        goto out_free_ring;
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
                got = xStreamBufferReceive(s_mic_ring, pcm8,
                                           SIP_PTIME_SAMPLES * sizeof(int16_t), 0);
            }
            if (got == SIP_PTIME_SAMPLES * sizeof(int16_t)) {
                sip_g711_encode(session->audio_pt, pcm8, enc, SIP_PTIME_SAMPLES);
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

        /* 下行：非阻塞把积压的包全部收掉，解码后只入 ring（不碰播放器，
         * 保证 write 阻塞不住收包）。ring 满丢新数据（= 丢延迟）。 */
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
            sip_g711_decode(session->audio_pt, net, pcm8, SIP_PTIME_SAMPLES);
            /* 8k -> 16k 线性插值：偶数点直取，奇数点取相邻均值。 */
            for (int i = 0; i < SIP_PTIME_SAMPLES; ++i) {
                int16_t a = pcm8[i];
                int16_t b = (i + 1 < SIP_PTIME_SAMPLES) ? pcm8[i + 1] : a;
                pcm16[i * 2] = a;
                pcm16[i * 2 + 1] = (int16_t)(((int32_t)a + b) / 2);
            }
            xStreamBufferSend(play_ring, pcm16, sizeof(pcm16), 0);
        }

        if (!player_streaming) {
            /* 预滚：ring 攒够水量再开流 + 一次性首写（首写内部会等
             * prepare，水量不够就空等到超时 —— vp 的前车之鉴）。 */
            if (xStreamBufferBytesAvailable(play_ring) < SIP_PREROLL_BYTES) {
                goto tail;
            }
            if (!s_player) {
                s_player = app_player_create(SIP_CALL_PLAYER_NAME);
                if (!s_player) {
                    LISA_LOGE(TAG, "create '%s' player failed", SIP_CALL_PLAYER_NAME);
                    goto out_free_ring;
                }
            }
            if (app_player_play_stream(s_player, 16000, 1, 16) != APP_PLAYER_OK) {
                LISA_LOGW(TAG, "play_stream refused (focus?), retry later");
                sip_osal_sleep_ms(100);
                goto tail;
            }
            preroll_len = xStreamBufferReceive(play_ring, preroll,
                                               SIP_PREROLL_BYTES, 0);
            /* 注意：app_player_write_stream 成功时返回写入字节数
             * （>0），不是 APP_PLAYER_OK(0) —— 头文件注释与实现不符。
             * 只有 <0 才是错误。曾用 != APP_PLAYER_OK 判断：每次首写
             * 都被误判失败 → 每 ~260ms 重新 play_stream → flow_base_init
             * 反复新建 flow 不释放 → PSRAM 以 ~300KB/s 泄漏，20 余秒
             * 耗尽 6MB 后 heap_caps 分配失败断言死机。 */
            if (app_player_write_stream(s_player, preroll, preroll_len, 200) < 0) {
                LISA_LOGW(TAG, "preroll first write failed, finish and retry");
                (void)app_player_finish_stream(s_player); /* 释放流资源防泄漏 */
                goto tail;
            }
            player_streaming = true;
            LISA_LOGI(TAG, "aud: downlink streaming started (%u bytes preroll)",
                      (unsigned)preroll_len);
        } else {
            /* 播放：每轮从 ring 取一小段写播放器。部分写入（返回值是
             * 字节数）保留剩余下轮续写；20ms 短超时保证收包不被长阻塞。 */
            if (pend_len == 0) {
                pend_len = xStreamBufferReceive(play_ring, pend, SIP_PLAY_CHUNK, 0);
            }
            if (pend_len > 0) {
                int w = app_player_write_stream(s_player, pend, pend_len, 50);
                if (w < 0) {
                    write_fail++;
                    if (write_fail >= 3) {
                        LISA_LOGW(TAG, "write_stream failed x3, restart stream");
                        (void)app_player_finish_stream(s_player);
                        player_streaming = false;
                        write_fail = 0;
                        pend_len = 0;
                    }
                } else if (w == 0) {
                    /* 超时零写入（播放器节流）：数据保留下一轮再写。
                     * 曾把 w==0 误当"写完"清空 —— 每 20ms 扔一段音频，
                     * 设备端表现为持续跳变噪声。 */
                } else if ((uint32_t)w < pend_len) {
                    memmove(pend, pend + w, pend_len - (uint32_t)w);
                    pend_len -= (uint32_t)w;
                    write_fail = 0;
                } else {
                    pend_len = 0;
                    write_fail = 0;
                }
            }
        }

tail:

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

out_free_ring:
    if (play_ring) {
        vStreamBufferDelete(play_ring);
    }
    psram_free(preroll);
    psram_free(pend);
    psram_free(ringbuf);
    psram_free(ringcb);

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
/*
 * JPEG 采样白名单：设备硬解只正确处理 4:2:0 / 4:2:2（含 h2v1）与灰度。
 * 4:4:4 帧（如 ffmpeg 默认从 RGB 源编码、libjpeg JCS_RGB 默认）会被
 * 硬解解成规则水平条纹垃圾且返回成功 —— 实测条纹花屏一整轮才定位到。
 * 解码前扫 SOF 采样因子，不支持的整帧丢弃计数，不喂解码器。
 */
static bool jpeg_sampling_supported(const uint8_t *jpg, uint32_t len)
{
    if (len < 4 || jpg[0] != 0xFF || jpg[1] != 0xD8) {
        return false;
    }
    uint32_t i = 2;
    while (i + 4 < len) {
        if (jpg[i] != 0xFF) {
            i++;
            continue;
        }
        uint8_t m = jpg[i + 1];
        if (m == 0xD8 || m == 0x01 || m == 0xD9 ||
            (m >= 0xD0 && m <= 0xD7)) {
            i += 2;
            continue;
        }
        uint32_t seg = ((uint32_t)jpg[i + 2] << 8) | jpg[i + 3];
        if (m == 0xC0 || m == 0xC1) { /* SOF0/1：基线/扩展 */
            if (i + 10 > len) {
                break;
            }
            int ncomp = jpg[i + 9];
            if (ncomp == 1) {
                return true; /* 灰度 */
            }
            if (ncomp != 3 || i + 10 + 3 * 3 > len) {
                return false;
            }
            int yhv = jpg[i + 11];
            int chv = jpg[i + 14];
            /* 白名单：4:2:0(2x2/1x1)、4:2:2(2x2/1x2 或 2x1/1x1) */
            if (yhv == 0x22 && (chv == 0x11 || chv == 0x12)) {
                return true;
            }
            if (yhv == 0x21 && chv == 0x11) {
                return true;
            }
            return false;
        }
        if (m == 0xC2 || m == 0xC3) { /* 渐进式/无损：硬解同样不吃 */
            return false;
        }
        i += 2 + seg;
    }
    return true; /* 没扫到 SOF：交给解码器自己判断 */
}

static void sip_vidrx_task(void *arg)
{
    const sip_session_t *session = &s_ua->session;
    uint8_t pkt[1500];

    if (!session->video || session->peer_video_port == 0) {
        goto out;
    }
    if (sip_rtp_open(&s_rtp_vid, SIP_VIDEO_PORT, SIP_JPEG_PT) != 0) {
        LISA_LOGE(TAG, "video rtp bind %u failed", SIP_VIDEO_PORT);
        goto out;
    }
    s_rtp_vid_open = true;
    sip_rtp_set_peer(&s_rtp_vid, session->peer_ip, session->peer_video_port);

    s_jpeg_rx = psram_malloc(sizeof(*s_jpeg_rx));
    s_dec_scratch_raw = psram_malloc(240u * 320u * 2u + 31u);
    s_dec_scratch = (uint16_t *)(((uintptr_t)s_dec_scratch_raw + 31u) & ~(uintptr_t)31u);
    s_dec_frames[0] = psram_malloc(240u * 320u * 2u);
    s_dec_frames[1] = psram_malloc(240u * 320u * 2u);
    if (!s_jpeg_rx || !s_dec_scratch_raw || !s_dec_frames[0] || !s_dec_frames[1]) {
        LISA_LOGE(TAG, "vid_rx: buffer alloc failed");
        goto out;
    }
    LISA_LOGI(TAG, "vid_rx: scratch %p (raw %p)", (void *)s_dec_scratch,
              (void *)s_dec_scratch_raw);
    memset(s_jpeg_rx, 0, sizeof(*s_jpeg_rx));
    s_dec_front = 0;
    s_rx_vid_frames = s_dec_ms = s_rx_vid_drop = 0;
    s_rx_drop_size = s_rx_drop_sampling = s_rx_drop_dec = 0;
    s_rx_drop_frag = 0;
    LISA_LOGI(TAG, "vid_rx: %s:%u", session->peer_ip, session->peer_video_port);

    while (s_vidrx_running) {
        int n = sip_osal_udp_recv(s_rtp_vid.fd, pkt, sizeof(pkt), 50, NULL, NULL);
        if (n <= 0) {
            continue;
        }
        const uint8_t *frame;
        uint32_t len;
        int w = 0, h = 0;
        int rc = sip_jpeg_rx_feed(s_jpeg_rx, pkt, n, SIP_JPEG_PT,
                                  &frame, &len, &w, &h);
        if (rc < 0) {
            s_rx_vid_drop++;
            s_rx_drop_frag++;
            continue;
        }
        if (rc != 1 || len == 0) {
            continue;
        }
        if (w <= 0 || h <= 0 || w > 240 || h > 320) {
            s_rx_vid_drop++;
            s_rx_drop_size++;
            continue;
        }
        if (!jpeg_sampling_supported(frame, len)) {
            s_rx_vid_drop++;
            s_rx_drop_sampling++;
            continue;
        }
        uint32_t t0 = sip_osal_now_ms();
        int back = 1 - s_dec_front;
        uint16_t dw = (uint16_t)w, dh = (uint16_t)h;
        Jpeg_DecoderCfg cfg = {.output_format = JPEG_PIXEL_FORMAT_RGB565};
        if (s_jpeg_lock) {
            xSemaphoreTake(s_jpeg_lock, portMAX_DELAY);
        }
        uint32_t dec_rc = jpeg_decoder(frame, len, (uint8_t *)s_dec_scratch, &dw, &dh, cfg);
        sip_hw_jpeg_mark_decode(); /* 解码动过 Jpeg0：上行编码前需重配 */
        if (s_jpeg_lock) {
            xSemaphoreGive(s_jpeg_lock);
        }
        if (dec_rc != 0) {
            s_rx_vid_drop++;
            s_rx_drop_dec++;
            LISA_LOGW(TAG, "vid_rx: dec fail rc=%u len=%u %ux%u ts=%u",
                      (unsigned)dec_rc, (unsigned)len, w, h,
                      (unsigned)sip_osal_now_ms());
            continue;
        }
        /* 拷贝到显示缓冲：此后 UI 读到的内容不再变化（消除渲染撕裂）。 */
        memcpy(s_dec_frames[back], s_dec_scratch, (size_t)dw * dh * 2u);
        s_dec_ms = sip_osal_now_ms() - t0;
        s_dec_front = back;
        s_rx_vid_frames++;
        sip_service_push_frame(s_dec_frames[s_dec_front], dw, dh);
    }

out:
    if (s_rtp_vid_open) {
        sip_rtp_close(&s_rtp_vid);
        s_rtp_vid_open = false;
    }
    if (s_jpeg_rx) {
        psram_free(s_jpeg_rx);
        s_jpeg_rx = NULL;
    }
    if (s_dec_scratch_raw) {
        psram_free(s_dec_scratch_raw);
        s_dec_scratch_raw = NULL;
        s_dec_scratch = NULL;
    }
    for (int i = 0; i < 2; ++i) {
        if (s_dec_frames[i]) {
            psram_free(s_dec_frames[i]);
            s_dec_frames[i] = NULL;
        }
    }
    LISA_LOGI(TAG, "vid_rx: stopped (rx=%u dec_ms=%u drop=%u)",
              s_rx_vid_frames, s_dec_ms, s_rx_vid_drop);
    s_vidrx_task = NULL;
    vTaskDelete(NULL);
}

/*
 * 上行视频：相机采集（RGB565 240x320，本 SoC 的 GC0328 硬件跳采上限，
 * 无 640x480 可用）→ 全画幅软件 JPEG（tiny_jpeg q75，见下）→ RFC2435
 * 分片发送。
 *
 * 为什么不用硬件编码：JPEG 编/解是同一外设，GPDMA IN 只有 ch0~5 且全被
 * 占（解码 IN/OUT=ch0/ch5、音频 ch1/2/3、DVP ch4），编解码共用 ch0 靠
 * 交替 init 在实测中不稳定 —— 编码 DMA 等待 1s 超时（每次内部 malloc
 * ~460KB），与预滚重试叠加把 6MB 大池吃到 OOM 断言死机。
 *
 * 分辨率/质量权衡：tiny_jpeg 只有 1/2/3 三档质量（1 最小最快，传
 * libjpeg 式的 1-100 会被直接拒绝 —— 曾传 75 导致上行 0 帧的回归）。
 * 全幅 240x320 用第 1 档：帧率 ~0.3fps（软编 ~3.7s/帧），分辨率优先
 * （"全幅导致 40% 音频丢包"曾是残留 peer 进程共享端口的假象，已排除）。
 */
static void sip_vid_task(void *arg)
{
    const sip_session_t *session = &s_ua->session;

    if (!session->video || session->peer_video_port == 0) {
        LISA_LOGI(TAG, "vid: video not negotiated, task exits");
        goto out;
    }
    /* 视频收发分 socket：上行独占一个，不再与下行收包互相阻塞。 */
    sip_rtp_t up;
    if (sip_rtp_open(&up, 0, SIP_JPEG_PT) != 0) {
        LISA_LOGE(TAG, "vid: uplink socket failed");
        goto out;
    }
    sip_rtp_set_peer(&up, session->peer_ip, session->peer_video_port);

    uint16_t cw = 240, ch = 320;
    (void)service_camera_get_framesize(&cw, &ch);
    const uint32_t px = (uint32_t)cw * ch;
    uint8_t *cam = psram_malloc(px * 2u);
    uint8_t *jpeg = psram_malloc(px * 2u + 640u); /* 硬编输出 + JFIF 头 */
    if (!cam || !jpeg) {
        LISA_LOGE(TAG, "vid: uplink buffer alloc failed");
        goto out_free;
    }

    /* 硬件编码（DMA2D 路线，一次 init 常驻）：失败自动回退软编。 */
    bool hw_enc = (sip_hw_jpeg_init(cw, ch) == 0);

    /* 等主页相机预览随导航让出（通话开始时 UI 切到通话页）。 */
    sip_osal_sleep_ms(SIP_VID_START_DELAY_MS);

    uint32_t vid_ts = 0;
    uint32_t interval = SIP_VID_INTERVAL_MS;
    uint32_t hw_fail = 0;
    LISA_LOGI(TAG, "vid: uplink %ux%u %s -> %s:%u", cw, ch,
              hw_enc ? "hw-enc(DMA2D)" : "sw-enc(tje)",
              session->peer_ip, session->peer_video_port);

    while (s_vid_running) {
        uint32_t t0 = sip_osal_now_ms();

        if (service_camera_capture_ex(cam, px * 2u, 0) != 0) {
            LISA_LOGW(TAG, "vid: capture failed");
            sip_osal_sleep_ms(200);
            continue;
        }
        uint32_t cap_ms = sip_osal_now_ms() - t0;

        /* 编码：优先硬件（Jpeg0 外设，与下行解码共用 —— 持互斥锁；
         * 解码动过 Jpeg0 后编码前重配，见 sip_hw_jpeg_encode 的脏标志）。
         * 连续 3 次失败回退软编（tje 全幅 ~3.7s/帧，保底可用）。 */
        uint32_t enc_start = sip_osal_now_ms();
        uint32_t jpeg_len = 0;
        if (hw_enc) {
            if (s_jpeg_lock) {
                xSemaphoreTake(s_jpeg_lock, portMAX_DELAY);
            }
            int rc = sip_hw_jpeg_encode(cam, jpeg, &jpeg_len);
            if (s_jpeg_lock) {
                xSemaphoreGive(s_jpeg_lock);
            }
            if (rc != 0) {
                hw_fail++;
                if (hw_fail >= 3) {
                    LISA_LOGW(TAG, "vid: hw encode failed x%u, fallback to sw",
                              (unsigned)hw_fail);
                    hw_enc = false;
                }
                continue;
            }
            hw_fail = 0;
        } else {
            uint8_t *sw_jpeg = NULL;
            if (image_rgb565_to_jpeg_q(cam, px * 2u, (int)cw, (int)ch, 1,
                                       &sw_jpeg, &jpeg_len) != 0 ||
                !sw_jpeg || jpeg_len == 0 || jpeg_len > px * 2u) {
                LISA_LOGW(TAG, "vid: sw encode failed");
                continue;
            }
            memcpy(jpeg, sw_jpeg, jpeg_len);
            image_jpeg_free(sw_jpeg);
        }
        uint32_t sw_ms = sip_osal_now_ms() - enc_start;

        int frags = sip_jpeg_rtp_send_frame(&up, jpeg, (int)jpeg_len,
                                            (int)cw, (int)ch, vid_ts);
        if (frags <= 0) {
            LISA_LOGW(TAG, "vid: send failed (%u bytes)", jpeg_len);
            sip_osal_sleep_ms(200);
            continue;
        }
        vid_ts += 90000u / (1000u / interval);
        s_vid_frames++;
        s_vid_bytes += jpeg_len;
        s_vid_enc_ms = cap_ms + sw_ms;
        s_vid_cap_ms = cap_ms;
        if ((s_vid_frames % 10u) == 1u) {
            LISA_LOGI(TAG, "vid: cap=%ums enc=%ums size=%u (%ux%u)",
                      (unsigned)cap_ms, (unsigned)sw_ms,
                      jpeg_len, cw, ch);
        }

        uint32_t elapsed = sip_osal_now_ms() - t0;
        uint32_t wait = (elapsed + 20u > SIP_VID_INTERVAL_MS) ? elapsed + 20u
                                                              : SIP_VID_INTERVAL_MS;
        if (wait > SIP_VID_MAX_INTERVAL_MS) {
            wait = SIP_VID_MAX_INTERVAL_MS;
        }
        interval = wait;
        sip_osal_sleep_ms(wait);
    }

out_free:
    psram_free(cam);
    psram_free(jpeg);
    sip_hw_jpeg_stop();
    (void)service_camera_stop();
    sip_rtp_close(&up);

out:
    LISA_LOGI(TAG, "vid: stopped (tx=%u enc_ms=%u bytes=%u)",
              s_vid_frames, s_vid_enc_ms, s_vid_bytes);
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
    s_vidrx_running = true;
    if (xTaskCreate(sip_vidrx_task, "sip_vidrx", SIP_VID_RX_STACK / 4u, NULL,
                    SIP_VID_RX_PRIO, &s_vidrx_task) != pdPASS) {
        s_vidrx_running = false;
        s_vidrx_task = NULL;
        LISA_LOGE(TAG, "create sip_vidrx failed");
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
        for (int i = 0; i < 200 && s_vid_task; ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (s_vidrx_task) {
        s_vidrx_running = false;
        if (s_rtp_vid_open) {
            sip_osal_udp_close(s_rtp_vid.fd);
        }
        for (int i = 0; i < 200 && s_vidrx_task; ++i) {
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
        if (!s_keep_awake_set) {
            service_power_policy_set_miniapp_active(true);
            s_keep_awake_set = true;
        }
        media_start();
        break;
    case SIP_UA_IDLE:
    case SIP_UA_ENDED:
        media_stop();
        if (s_keep_awake_set) {
            service_power_policy_set_miniapp_active(false);
            s_keep_awake_set = false;
        }
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
    if (!s_jpeg_lock) {
        s_jpeg_lock = xSemaphoreCreateMutex();
        if (!s_jpeg_lock) {
            LISA_LOGE(TAG, "jpeg lock create failed");
            return -1;
        }
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
                   "hangup | cancel | cfg [<ip>] | ring | screen <ip>|off | log <0-3>\n");
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
                       "     audio pt=%d -> %s:%u | vid tx=%u enc_ms=%u rx=%u dec_ms=%u drop=%u (frag=%u size=%u sampling=%u dec=%u)\n",
                       s->audio_pt, s->peer_ip, s->peer_audio_port,
                       s_vid_frames, s_vid_enc_ms, s_rx_vid_frames, s_dec_ms,
                       s_rx_vid_drop, s_rx_drop_frag, s_rx_drop_size,
                       s_rx_drop_sampling, s_rx_drop_dec);
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
    if (strcmp(argv[1], "screen") == 0) {
        if (argc >= 3 && strcmp(argv[2], "off") == 0) {
            return sip_screen_stop();
        }
        if (argc >= 3) {
            uint16_t port = argc >= 4 ? (uint16_t)atoi(argv[3]) : 0;
            return sip_screen_start(argv[2], port);
        }
        shellPrint(shellGetCurrent(), "sip: screen <ip> [port] | screen off\n");
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
