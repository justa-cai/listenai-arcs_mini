/*
 * sip_service.c - 设备侧 SIP 通话服务
 *
 * 职责（按里程碑逐步落地）：
 *   M1  骨架：init/状态查询/shell 命令面
 *   M2  信令任务 sip_sig（5060 收发 + sip_ua_poll）与音频任务 sip_aud
 *       （G.711 双向）、app_wakeup 的 post-AEC mic tap、call 焦点通道
 *   M3  UI 桥（状态回调投递 worq.ui）
 *   M4  视频任务 sip_vid（摄像头采集 → 软件 JPEG → RTP 上行）
 *   M5  下行视频（重组 + jpeg_decoder 硬解 + UI 推帧）
 *
 * 任务/播放器/焦点的写法全部以 apps/arcs-mini/video/video_player.c 为范本。
 */
#define TAG "sip_service"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lisa_log.h"
#include "shell.h"

#include "sip_osal.h"
#include "sip_g711.h"
#include "sip_service.h"

static sip_svc_state_t s_state = SIP_SVC_IDLE;

int sip_service_init(void)
{
    sip_osal_set_log_level(SIP_LOG_INFO);
    s_state = SIP_SVC_IDLE;
    LISA_LOGI(TAG, "sip service ready (signaling task starts in M2)");
    return 0;
}

sip_svc_state_t sip_service_state(void)
{
    return s_state;
}

bool sip_service_in_call(void)
{
    return s_state == SIP_SVC_ACTIVE;
}

const char *sip_service_state_name(void)
{
    switch (s_state) {
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
    return "";
}

int sip_service_start_call(const char *peer_ip, uint16_t peer_port)
{
    (void)peer_ip;
    (void)peer_port;
    LISA_LOGW(TAG, "start_call: signaling not implemented yet (M2)");
    return -1;
}

int sip_service_answer(void)
{
    LISA_LOGW(TAG, "answer: signaling not implemented yet (M2)");
    return -1;
}

int sip_service_reject(void)
{
    LISA_LOGW(TAG, "reject: signaling not implemented yet (M2)");
    return -1;
}

int sip_service_hangup(void)
{
    LISA_LOGW(TAG, "hangup: signaling not implemented yet (M2)");
    return -1;
}

void sip_service_cancel(void)
{
}

void sip_service_on_mic_pcm(const int16_t *pcm, uint32_t samples)
{
    /* M2 起：2:1 抽取到 8k、G.711 编码、写上行环形缓冲。 */
    (void)pcm;
    (void)samples;
}

/* ============================ shell ============================ */

static int sip_shell_cmd(int argc, char **argv)
{
    if (argc < 2) {
        shellPrint(shellGetCurrent(),
                   "usage: sip status | call <ip> [port] | answer | reject | "
                   "hangup | cancel | cfg [ip] | bench | log <0-3>\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        shellPrint(shellGetCurrent(), "sip: state=%s peer=%s g711=ok\n",
                   sip_service_state_name(), sip_service_peer_ip());
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
