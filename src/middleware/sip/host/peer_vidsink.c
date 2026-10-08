/*
 * peer_vidsink.c - 视频接收落盘实现
 *
 * 帧写到 tmp/sip_host/frame_<n>.jpg，n 取模滚动（默认保留 8 帧历史，
 * 用 ls -t 排序即可观察刷新）。落盘路径相对当前工作目录，产物不进源码树。
 */
#define _TAG "peer_vidsink"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sip_osal.h"
#include "sip_rtp.h"
#include "sip_jpeg_rtp.h"

#include "peer_vidsink.h"

#define VIDSINK_KEEP 8u

static sip_rtp_t s_rtp;
static sip_jpeg_rx_t *s_rx;
static pthread_t s_thread;
static volatile int s_running;
static volatile uint32_t s_ok, s_drop;

static void *vidsink_thread(void *arg)
{
    (void)arg;
    uint8_t pkt[1500];
    mkdir("tmp", 0755);
    mkdir("tmp/sip_host", 0755);

    while (s_running) {
        int n = sip_osal_udp_recv(s_rtp.fd, pkt, sizeof(pkt), 50, NULL, NULL);
        if (n <= 0) {
            continue;
        }
        const uint8_t *frame;
        uint32_t len;
        int w, h;
        int rc = sip_jpeg_rx_feed(s_rx, pkt, n, SIP_JPEG_PT, &frame, &len, &w, &h);
        if (rc == 1) {
            static uint32_t seq;
            char path[64];
            snprintf(path, sizeof(path), "tmp/sip_host/frame_%u.jpg", seq % VIDSINK_KEEP);
            seq++;
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(frame, 1, len, f);
                fclose(f);
            }
            s_ok++;
            if ((s_ok % 10u) == 1u) {
                sip_osal_log(SIP_LOG_INFO, _TAG, "frame #%u %u bytes %dx%d -> %s",
                             s_ok, len, w, h, path);
            }
        }
    }
    return NULL;
}

int peer_vidsink_start(const char *remote_ip, uint16_t remote_port,
                       uint16_t rtp_local_port)
{
    if (s_running) {
        return 0;
    }
    if (sip_rtp_open(&s_rtp, rtp_local_port, SIP_JPEG_PT) != 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "rtp bind %u failed", rtp_local_port);
        return -1;
    }
    sip_rtp_set_peer(&s_rtp, remote_ip, remote_port);

    s_rx = malloc(sizeof(*s_rx));
    if (!s_rx) {
        sip_rtp_close(&s_rtp);
        return -1;
    }
    memset(s_rx, 0, sizeof(*s_rx));

    s_ok = s_drop = 0;
    s_running = 1;
    pthread_create(&s_thread, NULL, vidsink_thread, NULL);
    sip_osal_log(SIP_LOG_INFO, _TAG, "video sink %s:%u -> tmp/sip_host/",
                 remote_ip, remote_port);
    return 0;
}

void peer_vidsink_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = 0;
    sip_rtp_close(&s_rtp);
    pthread_join(s_thread, NULL);
    s_drop = s_rx->frames_drop;
    free(s_rx);
    s_rx = NULL;
    sip_osal_log(SIP_LOG_INFO, _TAG, "stopped (ok=%u drop=%u)", s_ok, s_drop);
}

int peer_vidsink_running(void)
{
    return s_running;
}

uint32_t peer_vidsink_frames_ok(void)
{
    return s_ok;
}

uint32_t peer_vidsink_frames_drop(void)
{
    return s_rx ? s_rx->frames_drop : s_drop;
}
