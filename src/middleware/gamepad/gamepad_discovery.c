/*
 * Gamepad UDP 发现服务: 监听 0.0.0.0:38201, 收到探测后单播应答
 * announce JSON (本机 IP / WS 端口 / 设备名 / 固件版本)。
 *
 * 报文见 doc/gamepad-protocol.md 第 3 节。
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "lisa_log.h"
#include "lwip/sockets.h"
#include "sdk_version.h"

#define TAG "gamepad_disc"

#define DISC_BUF_LEN 256

static int s_sock = -1;
static volatile bool s_running = false;

static int disc_build_announce(char *buf, uint32_t buf_len, const char *ip)
{
    bool running = false;
    uint32_t fps = 0;

    gamepad_get_game_state(&running, &fps);

    return snprintf(buf, buf_len,
                    "{\"t\":\"announce\",\"ver\":%d,\"dev\":\"%s\",\"name\":\"%s\","
                    "\"did\":\"%s\",\"ip\":\"%s\",\"ws_port\":%u,\"disc_port\":%u,"
                    "\"udp_port\":%u,"
                    "\"fw\":\"%s\",\"state\":\"%s\"}",
                    GAMEPAD_PROTO_VERSION, GAMEPAD_DEV_NAME, GAMEPAD_DEV_BRAND,
                    gamepad_util_get_device_id(), ip,
                    (unsigned)CONFIG_GAMEPAD_WS_PORT,
                    (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT,
                    (unsigned)CONFIG_GAMEPAD_UDP_PORT,
                    gamepad_util_get_fw_version(), running ? "running" : "idle");
}

static void disc_task(void *arg)
{
    (void)arg;
    struct sockaddr_in addr;
    int val = 1;

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        LISA_LOGE(TAG, "socket create failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFIG_GAMEPAD_DISCOVERY_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LISA_LOGE(TAG, "bind :%u failed: %d", (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT, errno);
        closesocket(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    LISA_LOGI(TAG, "discovery listening on :%u", (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT);

    while (s_running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        char rx[DISC_BUF_LEN];
        char tx[DISC_BUF_LEN];
        char local_ip[16] = {0};
        int rx_len = recvfrom(s_sock, rx, sizeof(rx) - 1, 0,
                              (struct sockaddr *)&from, &from_len);
        if (rx_len <= 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        rx[rx_len] = '\0';

        /* 应答前取本机 IP (此时路由已知) */
        if (!gamepad_util_get_local_ip(local_ip, sizeof(local_ip))) {
            LISA_LOGW(TAG, "no local ip, skip announce");
            continue;
        }

        int tx_len = disc_build_announce(tx, sizeof(tx), local_ip);
        if (tx_len <= 0 || (uint32_t)tx_len >= sizeof(tx)) {
            continue;
        }

        int sent = sendto(s_sock, tx, tx_len, 0,
                          (struct sockaddr *)&from, from_len);
        if (sent < 0) {
            LISA_LOGW(TAG, "sendto failed: %d", errno);
        } else {
            LISA_LOGI(TAG, "announce -> %s:%d (%.*s)",
                      inet_ntoa(from.sin_addr), (int)ntohs(from.sin_port),
                      tx_len > 80 ? 80 : tx_len, tx);
        }
    }

    closesocket(s_sock);
    s_sock = -1;
    vTaskDelete(NULL);
}

int gamepad_discovery_start(void)
{
    if (s_running) {
        return 0;
    }
    s_running = true;
    if (xTaskCreate(disc_task, "gp_disc", 2048, NULL,
                    configMAX_PRIORITIES - 4, NULL) != pdPASS) {
        s_running = false;
        LISA_LOGE(TAG, "discovery task create failed");
        return -1;
    }
    return 0;
}

void gamepad_discovery_stop(void)
{
    s_running = false;
    if (s_sock >= 0) {
        closesocket(s_sock); /* 解开 recvfrom 阻塞 */
        s_sock = -1;
    }
}
