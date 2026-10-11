/*
 * sip_peer.c - Ubuntu 主机侧 SIP 对端程序（与设备共用 core/ 的同一份源码）
 *
 * 用法：
 *   sip_peer -w                    守候：等呼入，振铃 1.5s 后自动应答
 *   sip_peer -w -m                 守候但不自动应答（回车接听/挂断）
 *   sip_peer -c <对端IP>           主动呼叫对端
 *   sip_peer -v <帧目录>           通话建立后从目录循环发 MJPEG（模拟相机）
 *   选项：
 *   -p <信令端口>   默认 5060（同机双实例对跑时第二个用 7060）
 *   -a <音频RTP端口> 默认 5062     -o <视频RTP端口> 默认 5064
 *
 * 音频走 ALSA default（无声卡时自动降级为纯视频，不中断通话）；
 * 收到的对端视频落盘 tmp/sip_host/frame_N.jpg（滚动 8 帧）。
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
#include "peer_vidsink.h"
#include "peer_vidsrc.h"
#include "peer_v4l2.h"

#define SIG_PORT_DEFAULT 5060
#define AUDIO_PORT_DEFAULT 5062
#define VIDEO_PORT_DEFAULT 5064

static sip_ua_t s_ua;
static int s_manual = 0;
static uint16_t s_audio_port = AUDIO_PORT_DEFAULT;
static uint16_t s_video_port = VIDEO_PORT_DEFAULT;
static const char *s_vidsrc_dir;
static const char *s_v4l2_dev = "/dev/video0";
static int s_audio_only = 0;   /* -A：应答时拒掉视频（调试隔离用） */

static void on_state(void *user, sip_ua_state_t st)
{
    (void)user;
    sip_osal_log(SIP_LOG_INFO, TAG, "state -> %d", (int)st);
    const sip_session_t *s = sip_ua_session(&s_ua);
    if (st == SIP_UA_ACTIVE) {
        if (s->video) {
            peer_vidsink_start(s->peer_ip, s->peer_video_port, s_video_port);
            if (s_vidsrc_dir) {
                peer_vidsrc_start(s_vidsrc_dir, 0, 0, s->peer_ip,
                                  s->peer_video_port, s_video_port + 100, 5);
            } else if (peer_v4l2_start(s_v4l2_dev, 240, 320, s->peer_ip,
                                       s->peer_video_port, s_video_port + 100, 5) != 0) {
                sip_osal_log(SIP_LOG_WARN, TAG,
                             "no video source (-v dir or camera); rx only");
            }
        }
        peer_alsa_start(s->audio_pt, s->peer_ip, s->peer_audio_port, s_audio_port);
    } else if (st == SIP_UA_ENDED || st == SIP_UA_IDLE) {
        peer_alsa_stop();
        peer_vidsink_stop();
        peer_vidsrc_stop();
        peer_v4l2_stop();
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

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s -w [-m] [-v <frames-dir>] | -c <peer-ip> [-v <frames-dir>]\n"
            "  -w              wait for incoming calls (auto-answer after 1.5s)\n"
            "  -m              manual answer (Enter)\n"
            "  -c <ip>         call the peer\n"
            "  -v <dir>        send MJPEG from dir after connect (device camera sim)\n"
            "  -d <dev>        V4L2 camera (default /dev/video0, used when no -v)\n"
            "  -W              SDL window showing device uplink video\n"
            "  -p <port>       signaling port (default %d)\n"
            "  -a <port>       audio RTP port (default %d)\n"
            "  -o <port>       video RTP port (default %d)\n",
            prog, SIG_PORT_DEFAULT, AUDIO_PORT_DEFAULT, VIDEO_PORT_DEFAULT);
}

/*
 * SIGTERM/SIGINT 处理：SDL2 的视频子系统初始化时会接管 SIGTERM（转成
 * SDL_QUIT 事件），而我们的事件泵只在收帧时跑 —— 带视频窗口的进程
 * 对 pkill 免疫，残留进程与新进程共享 SO_REUSEADDR 端口各自发流，
 * 曾把音频变成多路叠加（听不清）+ seq 统计互相污染（假 40% 丢包）。
 * 这里显式覆盖回来，收到即退出（OS 清理其余资源）。
 */
#include <signal.h>
static void on_fatal_signal(int sig)
{
    (void)sig;
    _exit(0);
}

int main(int argc, char **argv)
{
    sip_osal_set_log_level(SIP_LOG_INFO);
    signal(SIGTERM, on_fatal_signal);
    signal(SIGINT, on_fatal_signal);

    const char *call_ip = NULL;
    uint16_t sig_port = SIG_PORT_DEFAULT;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-w") == 0) {
            /* 守候模式 */
        } else if (strcmp(argv[i], "-m") == 0) {
            s_manual = 1;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            call_ip = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            s_vidsrc_dir = argv[++i];
        } else if (strcmp(argv[i], "-A") == 0) {
            s_audio_only = 1;
        } else if (strcmp(argv[i], "-W") == 0) {
            peer_vidsink_show_window(1);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            s_v4l2_dev = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            sig_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            s_audio_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            s_video_port = (uint16_t)atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 0;
        }
    }
    if (!call_ip && !(argc >= 2 && strcmp(argv[1], "-w") == 0)) {
        usage(argv[0]);
        return 1;
    }

    sip_ua_cfg_t cfg = {
        .sig_port = sig_port,
        .audio_port = s_audio_port,
        .video_port = s_video_port,
        .offer_video = !s_audio_only,
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
