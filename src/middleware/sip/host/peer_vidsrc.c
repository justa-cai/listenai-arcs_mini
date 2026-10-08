/*
 * peer_vidsrc.c - JPEG 帧文件发送源
 *
 * 目录里的 frame_*.jpg 按序循环读取（读一次进内存，帧数上限 16、单帧上限
 * 96KB），按 fps 间隔发送。模拟设备端 240x320 相机上行。
 */
#define _TAG "peer_vidsrc"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sip_osal.h"
#include "sip_rtp.h"
#include "sip_jpeg_rtp.h"

#include "peer_vidsrc.h"

#define VIDSRC_MAX_FRAMES 16

typedef struct {
    uint8_t *data;
    uint32_t len;
} vframe_t;

static sip_rtp_t s_rtp;
static pthread_t s_thread;
static volatile int s_running;
static vframe_t s_frames[VIDSRC_MAX_FRAMES];
static int s_frame_count;
static int s_w, s_h, s_fps;

/* 兼容旧签名：宽高由 JPEG 解析，调用方传 0 即可。 */
static volatile uint32_t s_sent;

static int load_frames(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "open %s failed", dir);
        return -1;
    }
    char names[VIDSRC_MAX_FRAMES][64];
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && count < VIDSRC_MAX_FRAMES) {
        const char *n = e->d_name;
        size_t nl = strlen(n);
        if (nl < 4 || strcmp(n + nl - 4, ".jpg") != 0) {
            continue;
        }
        snprintf(names[count], sizeof(names[count]), "%s", n);
        count++;
    }
    closedir(d);
    if (count == 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "no .jpg in %s", dir);
        return -1;
    }
    /* 简单按文件名排序，保证播放顺序稳定。 */
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (strcmp(names[i], names[j]) > 0) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", names[i]);
                snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                snprintf(names[j], sizeof(names[j]), "%s", tmp);
            }
        }
    }
    for (int i = 0; i < count; ++i) {
        char path[320];
        snprintf(path, sizeof(path), "%.240s/%.60s", dir, names[i]);
        FILE *f = fopen(path, "rb");
        if (!f) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > (long)SIP_JPEG_MAX_FRAME) {
            fclose(f);
            continue;
        }
        s_frames[s_frame_count].data = malloc((size_t)sz);
        if (!s_frames[s_frame_count].data) {
            fclose(f);
            break;
        }
        if (fread(s_frames[s_frame_count].data, 1, (size_t)sz, f) == (size_t)sz) {
            s_frames[s_frame_count].len = (uint32_t)sz;
            s_frame_count++;
        } else {
            free(s_frames[s_frame_count].data);
        }
        fclose(f);
    }
    sip_osal_log(SIP_LOG_INFO, _TAG, "loaded %d/%d frames from %s", s_frame_count, count, dir);
    return s_frame_count > 0 ? 0 : -1;
}


/* 从 JPEG 流解析宽高（扫 SOF0/1/2 标记，足够覆盖 baseline/progressive）。 */
static int jpeg_dimensions(const uint8_t *d, uint32_t len, int *w, int *h)
{
    uint32_t i = 2;
    while (i + 9 < len) {
        if (d[i] != 0xFF) {
            i++;
            continue;
        }
        uint8_t m = d[i + 1];
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD9)) {
            i += 2;
            continue;
        }
        uint32_t seg = ((uint32_t)d[i + 2] << 8) | d[i + 3];
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            *h = ((int)d[i + 5] << 8) | d[i + 6];
            *w = ((int)d[i + 7] << 8) | d[i + 8];
            return 0;
        }
        i += 2 + seg;
    }
    return -1;
}

static void *vidsrc_thread(void *arg)
{
    (void)arg;
    uint32_t ts = 0;
    uint32_t interval = 1000u / (s_fps > 0 ? (uint32_t)s_fps : 5u);
    int idx = 0;
    while (s_running) {
        vframe_t *f = &s_frames[idx % s_frame_count];
        idx++;
        int frags = sip_jpeg_rtp_send_frame(&s_rtp, f->data, (int)f->len,
                                            s_w, s_h, ts);
        if (frags > 0) {
            s_sent++;
            ts += 90000u / (s_fps > 0 ? (uint32_t)s_fps : 5u);
        }
        sip_osal_sleep_ms(interval);
    }
    return NULL;
}

int peer_vidsrc_start(const char *frames_dir, int width, int height,
                      const char *remote_ip, uint16_t remote_port,
                      uint16_t rtp_local_port, int fps)
{
    if (s_running) {
        return 0;
    }
    if (load_frames(frames_dir) != 0) {
        return -1;
    }
    if (sip_rtp_open(&s_rtp, rtp_local_port, SIP_JPEG_PT) != 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "rtp bind %u failed", rtp_local_port);
        return -1;
    }
    sip_rtp_set_peer(&s_rtp, remote_ip, remote_port);

    int jw = 0, jh = 0;
    if (jpeg_dimensions(s_frames[0].data, s_frames[0].len, &jw, &jh) == 0 &&
        jw > 0 && jh > 0) {
        s_w = jw;
        s_h = jh;
    } else {
        s_w = width > 0 ? width : 240;
        s_h = height > 0 ? height : 320;
    }
    s_fps = fps;
    s_sent = 0;
    s_running = 1;
    pthread_create(&s_thread, NULL, vidsrc_thread, NULL);
    sip_osal_log(SIP_LOG_INFO, _TAG, "sending %dx%d@%d -> %s:%u",
                 s_w, s_h, fps, remote_ip, remote_port);
    return 0;
}

void peer_vidsrc_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = 0;
    sip_rtp_close(&s_rtp);
    pthread_join(s_thread, NULL);
    for (int i = 0; i < s_frame_count; ++i) {
        free(s_frames[i].data);
        s_frames[i].data = NULL;
    }
    s_frame_count = 0;
    sip_osal_log(SIP_LOG_INFO, _TAG, "stopped (sent=%u)", s_sent);
}

int peer_vidsrc_running(void)
{
    return s_running;
}

uint32_t peer_vidsrc_frames_sent(void)
{
    return s_sent;
}
