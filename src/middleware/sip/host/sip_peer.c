/*
 * sip_peer.c - Ubuntu 主机侧 SIP 对端程序（与设备共用 core/ 的同一份源码）
 *
 * 用法：
 *   sip_peer -w                 守候模式：等设备呼入，振铃 1.5s 后自动应答
 *   sip_peer -w -m              守候但不自动应答（手动按回车接听/挂断）
 *   sip_peer -c <设备IP>        主动呼叫设备
 *   sip_peer -p <本地信令端口>  换端口（默认 5060）
 *
 * 音频走 ALSA default 设备（8k 单声道）；视频（V4L2 MJPEG）M4/M5 落地。
 */
#define TAG "sip_peer"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "sip_osal.h"
#include "sip_ua.h"

#include "peer_alsa.h"

#define SIG_PORT_DEFAULT 5060
#define AUDIO_PORT_DEFAULT 5062
#define VIDEO_PORT_DEFAULT 5064

static sip_ua_t s_ua;
static int s_manual = 0;      /* -m：手动接听 */
static int s_wait = 0;

static void on_state(void *user, sip_ua_state_t st)
{
    (void)user;
    sip_osal_log(SIP_LOG_INFO, TAG, "state -> %d", (int)st);
    const sip_session_t *s = sip_ua_session(&s_ua);
    if (st == SIP_UA_ACTIVE) {
        peer_alsa_start(s->audio_pt, s->peer_ip, s->peer_audio_port,
                        AUDIO_PORT_DEFAULT);
    } else if (st == SIP_UA_ENDED || st == SIP_UA_IDLE) {
        peer_alsa_stop();
    }
}

static void on_incoming(void *user, const char *ip, uint16_t port)
{
    (void)user;
    (void)port;
    sip_osal_log(SIP_LOG_INFO, TAG, "incoming call from %s", ip);
    if (s_manual) {
        sip_osal_log(SIP_LOG_INFO, TAG, "manual mode: press Enter to answer");
    } else {
        /* 自动应答：稍等 1.5s 模拟振铃。 */
        for (int i = 0; i < 15 && sip_ua_state(&s_ua) == SIP_UA_RINGING_IN; ++i) {
            sip_ua_poll(&s_ua, 100);
        }
        if (sip_ua_state(&s_ua) == SIP_UA_RINGING_IN) {
            sip_ua_answer(&s_ua);
        }
    }
}

int main(int argc, char **argv)
{
    sip_osal_set_log_level(SIP_LOG_INFO);

    const char *call_ip = NULL;
    uint16_t sig_port = SIG_PORT_DEFAULT;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-w") == 0) {
            s_wait = 1;
        } else if (strcmp(argv[i], "-m") == 0) {
            s_manual = 1;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            call_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            sig_port = (uint16_t)atoi(argv[++i]);
        } else {
            fprintf(stderr,
                    "usage: %s -w [-m] | -c <device-ip> [-p <sig-port>]\n"
                    "  -w        wait for incoming calls (auto-answer)\n"
                    "  -m        manual answer (Enter)\n"
                    "  -c <ip>   call the device\n",
                    argv[0]);
            return 0;
        }
    }
    if (!s_wait && !call_ip) {
        fprintf(stderr, "nothing to do: use -w or -c <ip>\n");
        return 1;
    }

    sip_ua_cfg_t cfg = {
        .sig_port = sig_port,
        .audio_port = AUDIO_PORT_DEFAULT,
        .video_port = VIDEO_PORT_DEFAULT,
        .offer_video = 1,
        .local_user = "peer",
        .on_state = on_state,
        .on_incoming = on_incoming,
    };
    if (sip_ua_init(&s_ua, &cfg) != 0) {
        sip_osal_log(SIP_LOG_ERR, TAG, "ua init failed (port %u busy?)", sig_port);
        return 1;
    }

    if (call_ip) {
        if (sip_ua_invite(&s_ua, call_ip, SIG_PORT_DEFAULT) != 0) {
            sip_osal_log(SIP_LOG_ERR, TAG, "invite %s failed", call_ip);
            return 1;
        }
    }

    char line[16];
    while (1) {
        sip_ua_poll(&s_ua, 50);

        /* 手动模式：回车 = 接听（振铃中）或挂断（通话中）。 */
        if (s_manual) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(0, &rfds);
            struct timeval tv = {0, 0};
            if (select(1, &rfds, NULL, NULL, &tv) > 0 && fgets(line, sizeof(line), stdin)) {
                if (sip_ua_state(&s_ua) == SIP_UA_RINGING_IN) {
                    sip_ua_answer(&s_ua);
                } else if (sip_ua_state(&s_ua) == SIP_UA_ACTIVE) {
                    sip_ua_bye(&s_ua);
                }
            }
        }
    }
    return 0;
}
