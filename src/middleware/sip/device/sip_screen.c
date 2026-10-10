/*
 * sip_screen.c - LCD 画面回传（调试工具，设备侧）
 *
 * 用途：定位"设备屏幕显示异常"类问题时，把屏幕此刻的实际内容抓回主机，
 * 免去拍照。抓帧点在 LVGL（lv_snapshot 整屏重绘 = "LVGL 认为屏幕该是
 * 什么样"）：若回传图像正常而实物异常 → 问题在 flush/LCD 链路；若回传
 * 也异常 → 问题在解码/渲染数据链路。这正是本工具要二分的两段。
 *
 * 数据通路（全部在低 prio 的 sip_scr 任务，snapshot 部分借道 worq.ui）：
 *   grab_cb（UI 线程 lv_snapshot_take_to_buf）→ RGB565 快照缓冲
 *   → image_rgb565_to_jpeg 软编（与上行视频同一纯函数路径）
 *   → 16B 头分片 UDP 发送
 *
 * 分片协议（两端同为小端，字段裸拷不转字节序）：
 *   uint16 magic=0x5343('SC') | uint16 frame_id（递增回绕）
 *   uint16 width | uint16 height | uint32 total（整帧 JPEG 字节数）
 *   uint32 offset（本片帧内偏移）
 * 净荷长度 = UDP 包长 - 16。不做重传/校验/重发：1s 一帧，主机按 frame_id
 * 位图重组，丢片整帧作废等下一帧（局域网 20 片/帧丢失率可忽略）。
 */
#define TAG "sip_screen"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_log.h"
#include "sysheap.h"

#include "sip_osal.h"
#include "sip_screen.h"

/* 头文件路径不跨层传播，链接期解析（sip_service.c 同款手法）。 */
extern int image_rgb565_to_jpeg(const uint8_t *rgb565, uint32_t len, int width, int height,
                                uint8_t **jpeg_data, uint32_t *jpeg_len);
extern void image_jpeg_free(void *jpeg);

#define SIP_SCREEN_PORT          5070u
#define SIP_SCREEN_INTERVAL_MS   1000u
#define SIP_SCREEN_FRAG          1400u   /* 每片净荷（+16B 头 < 1472B UDP 上限） */
#define SIP_SCREEN_MAX_PIXELS    (240u * 320u)
#define SIP_SCREEN_STACK         12288u
#define SIP_SCREEN_PRIO          2u      /* 低于全部媒体/信令任务 */
#define SIP_SCREEN_MAGIC         0x5343u

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t frame_id;
    uint16_t width;
    uint16_t height;
    uint32_t total;
    uint32_t offset;
} sip_scr_hdr_t; /* 16B，小端裸拷 */

static sip_screen_grab_cb_t s_grab_cb;
static void *s_grab_user;
static TaskHandle_t s_scr_task;
static volatile bool s_scr_running;
static char s_peer_ip[16];
static uint16_t s_peer_port;

void sip_screen_set_grab_cb(sip_screen_grab_cb_t cb, void *user)
{
    s_grab_cb = cb;
    s_grab_user = user;
}

static int scr_send_frame(int fd, uint8_t frame_id, const uint8_t *jpeg,
                          uint32_t jpeg_len, uint16_t w, uint16_t h)
{
    uint8_t pkt[16 + SIP_SCREEN_FRAG];
    uint32_t offset = 0;
    int frags = 0;
    while (offset < jpeg_len) {
        uint32_t n = jpeg_len - offset;
        if (n > SIP_SCREEN_FRAG) {
            n = SIP_SCREEN_FRAG;
        }
        sip_scr_hdr_t hdr = {
            .magic = SIP_SCREEN_MAGIC,
            .frame_id = frame_id,
            .width = w,
            .height = h,
            .total = jpeg_len,
            .offset = offset,
        };
        memcpy(pkt, &hdr, sizeof(hdr));
        memcpy(pkt + sizeof(hdr), jpeg + offset, n);
        if (sip_osal_udp_send(fd, s_peer_ip, s_peer_port, pkt, sizeof(hdr) + n) < 0) {
            return -1;
        }
        offset += n;
        frags++;
        sip_osal_sleep_ms(2); /* 片间微歇，避免 lwip 发送队列瞬间打满 */
    }
    return frags;
}

static void sip_scr_task(void *arg)
{
    (void)arg;
    if (!s_grab_cb) {
        LISA_LOGE(TAG, "no grab cb (UI bridge not registered), task exits");
        s_scr_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    int fd = sip_osal_udp_open(0);
    if (fd < 0) {
        LISA_LOGE(TAG, "udp open failed");
        s_scr_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    uint8_t *snap = psram_malloc(SIP_SCREEN_MAX_PIXELS * 2u);
    if (!snap) {
        LISA_LOGE(TAG, "snapshot buffer alloc failed");
        sip_osal_udp_close(fd);
        s_scr_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    LISA_LOGI(TAG, "screen cast -> %s:%u every %ums", s_peer_ip, s_peer_port,
              (unsigned)SIP_SCREEN_INTERVAL_MS);
    uint8_t frame_id = 0;
    uint32_t sent = 0;
    while (s_scr_running) {
        uint16_t w = 0, h = 0;
        if (!s_grab_cb(snap, SIP_SCREEN_MAX_PIXELS * 2u, &w, &h, s_grab_user)) {
            LISA_LOGW(TAG, "grab failed (UI busy?), retry next tick");
            sip_osal_sleep_ms(SIP_SCREEN_INTERVAL_MS);
            continue;
        }

        uint32_t t0 = sip_osal_now_ms();
        uint8_t *jpeg = NULL;
        uint32_t jpeg_len = 0;
        if (image_rgb565_to_jpeg(snap, (uint32_t)w * h * 2u, (int)w, (int)h,
                                 &jpeg, &jpeg_len) != 0 || !jpeg || jpeg_len == 0) {
            LISA_LOGW(TAG, "sw encode failed (%ux%u)", w, h);
            sip_osal_sleep_ms(SIP_SCREEN_INTERVAL_MS);
            continue;
        }
        int frags = scr_send_frame(fd, frame_id, jpeg, jpeg_len, w, h);
        uint32_t enc_ms = sip_osal_now_ms() - t0;
        image_jpeg_free(jpeg);
        frame_id++;
        if (frags > 0) {
            sent++;
            if ((sent % 10u) == 1u) {
                LISA_LOGI(TAG, "frame %u: %ux%u %uB in %d frags, enc+tx=%ums",
                          (unsigned)frame_id, w, h, (unsigned)jpeg_len, frags,
                          (unsigned)enc_ms);
            }
        }
        sip_osal_sleep_ms(SIP_SCREEN_INTERVAL_MS);
    }

    psram_free(snap);
    sip_osal_udp_close(fd);
    LISA_LOGI(TAG, "screen cast stopped (%u frames sent)", (unsigned)sent);
    s_scr_task = NULL;
    vTaskDelete(NULL);
}

int sip_screen_start(const char *peer_ip, uint16_t port)
{
    if (!peer_ip || !peer_ip[0]) {
        return -1;
    }
    snprintf(s_peer_ip, sizeof(s_peer_ip), "%s", peer_ip);
    s_peer_port = port ? port : SIP_SCREEN_PORT;
    if (s_scr_task) {
        return 0; /* 已在跑：只换目标 */
    }
    s_scr_running = true;
    if (xTaskCreate(sip_scr_task, "sip_scr", SIP_SCREEN_STACK / 4u, NULL,
                    SIP_SCREEN_PRIO, &s_scr_task) != pdPASS) {
        s_scr_running = false;
        s_scr_task = NULL;
        LISA_LOGE(TAG, "create sip_scr failed");
        return -1;
    }
    return 0;
}

int sip_screen_stop(void)
{
    if (!s_scr_task) {
        return 0;
    }
    s_scr_running = false;
    for (int i = 0; i < 100 && s_scr_task; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return s_scr_task ? -1 : 0;
}
