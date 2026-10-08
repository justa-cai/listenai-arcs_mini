/*
 * sip_peer.c - Ubuntu 主机侧 SIP 对端程序（与设备共用 core/ 的同一份源码）
 *
 * M1：命令行骨架 + OSAL 自检（绑定信令端口即退出），验证主机侧构建链路。
 * M2 起补齐：-w 守候应答 / -c <ip> 呼出、ALSA 全双工、V4L2 MJPEG。
 *
 * 用法：
 *   sip_peer                 打印用法
 *   sip_peer -w              守候模式（等待设备呼入，自动振铃/应答）
 *   sip_peer -c <ip>         主动呼叫设备
 */
#define TAG "sip_peer"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sip_osal.h"
#include "sip_g711.h"

#ifndef SIP_PEER_SIG_PORT
#define SIP_PEER_SIG_PORT 5060
#endif

static void usage(const char *prog)
{
    printf("%s - LAN SIP video-call host peer (shared core with device)\n"
           "\n"
           "  %s -w            wait mode: auto-answer incoming calls\n"
           "  %s -c <ip>       call the device at <ip>\n"
           "  %s -h            this help\n",
           prog, prog, prog, prog);
}

int main(int argc, char **argv)
{
    sip_osal_set_log_level(SIP_LOG_INFO);

    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    /* OSAL 自检：能绑定信令端口说明 socket 层在主机侧可用。 */
    int fd = sip_osal_udp_open(SIP_PEER_SIG_PORT);
    if (fd < 0) {
        sip_osal_log(SIP_LOG_ERR, TAG, "cannot bind UDP %u", SIP_PEER_SIG_PORT);
        return 1;
    }
    sip_osal_udp_close(fd);
    sip_osal_log(SIP_LOG_INFO, TAG, "osal ok (udp %u bind+close)", SIP_PEER_SIG_PORT);

    if (strcmp(argv[1], "-w") == 0) {
        sip_osal_log(SIP_LOG_WARN, TAG, "wait mode: signaling arrives in M2");
        return 0;
    }
    if (strcmp(argv[1], "-c") == 0 && argc >= 3) {
        char local[16];
        if (sip_osal_get_local_ip(argv[2], local, sizeof(local)) == 0) {
            sip_osal_log(SIP_LOG_INFO, TAG, "route to %s via %s", argv[2], local);
        } else {
            sip_osal_log(SIP_LOG_ERR, TAG, "no route to %s", argv[2]);
            return 1;
        }
        sip_osal_log(SIP_LOG_WARN, TAG, "call mode: signaling arrives in M2");
        return 0;
    }

    usage(argv[0]);
    return 0;
}
