/*
 * Gamepad UDP 发现服务: 监听 0.0.0.0:38201, 收到探测后单播应答
 * announce JSON (本机 IP / WS 端口 / 设备名 / 固件版本)。
 *
 * 报文见 doc/gamepad-protocol.md 第 3 节。
 *
 * 除应答外还反向记录探测来源: 谁能在此端口探测到我们, 谁就和设备同网段,
 * 而 pad_gui (PC 端) 在探测里自报身份与 ROM HTTP API 端口, 于是设备不必
 * 依赖 WebSocket 会话就能知道 ROM 库服务在哪台机器上 (见 gamepad_get_server_addr)。
 *
 * 方向二 (设备主动找服务端): gamepad_disc_find_server() 由设备广播
 * {"t":"discover_server"}, PC 端的 ROM 服务监听同一端口并回 {"t":"server",...}。
 * 这样"找 ROM 库地址"不再依赖 pad_gui 先扫描设备, 谁先起来都行。
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "cJSON.h"
#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "lisa_log.h"
#include "lwip/sockets.h"
#include "net_al.h"          /* net_if_get: 取本机 IP/掩码, 算定向广播地址 */
#include "sdk_version.h"

#define TAG "gamepad_disc"

#define DISC_BUF_LEN 256
/* 探测报文用 cJSON 解析 (拿 client 身份 / http_port), 与 gp_ws 任务同量级留足栈 */
#define DISC_TASK_STACK 4096

static int s_sock = -1;
static volatile bool s_running = false;

/* 最近一次探测来源。GUI 槽位只收「自报为桌面 GUI」的探测 (pad_gui 发 "ubuntu-gui"),
 * ANY 槽位收任意来源的探测; GUI 成立时同时更新 ANY。 */
static char s_peer_gui_ip[16] = { 0 };   /* "255.255.255.255" + NUL */
static char s_peer_any_ip[16] = { 0 };
static uint16_t s_peer_gui_http_port = 0;   /* 探测里自报的 ROM HTTP API 端口, 0 = 未自报 */

/* 大小写不敏感的子串查找 (客户端的 "gui" 标记大小写不做保证) */
static bool str_contains_ci(const char *hay, const char *needle)
{
    size_t needle_len;

    if (!hay || !needle) {
        return false;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return false;
    }

    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < needle_len && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == needle_len) {
            return true;
        }
    }
    return false;
}

/* 记录探测来源 IP, 以及 (若自报) 桌面 GUI 身份与 ROM HTTP API 端口。
 * 报文解析失败不视为错误: 协议 §3 规定无法解析的包按 discover 处理,
 * 只是拿不到客户端身份, 就只能落在 ANY 槽位。 */
static void disc_record_peer(const struct sockaddr_in *from, const char *payload)
{
    bool is_gui = false;
    uint16_t http_port = 0;
    cJSON *root;

    if (!from || from->sin_family != AF_INET || !payload) {
        return;
    }

    root = cJSON_Parse(payload);
    if (root) {
        const cJSON *type = cJSON_GetObjectItem(root, "t");
        const cJSON *client = cJSON_GetObjectItem(root, "client");

        /* 能解析出 t 且不是 discover 的包不是探测, 不记 */
        if (cJSON_IsString(type) && type->valuestring &&
            strcmp(type->valuestring, "discover") != 0) {
            cJSON_Delete(root);
            return;
        }
        if (cJSON_IsString(client) && client->valuestring) {
            is_gui = str_contains_ci(client->valuestring, "gui");
        }
        const cJSON *port = cJSON_GetObjectItem(root, "http_port");
        if (cJSON_IsNumber(port)) {
            double v = cJSON_GetNumberValue(port);
            if (v >= 1.0 && v <= 65535.0) {
                http_port = (uint16_t)v;
            }
        }
        cJSON_Delete(root);
    }

    snprintf(s_peer_any_ip, sizeof(s_peer_any_ip), "%s", inet_ntoa(from->sin_addr));
    if (is_gui) {
        snprintf(s_peer_gui_ip, sizeof(s_peer_gui_ip), "%s", inet_ntoa(from->sin_addr));
        s_peer_gui_http_port = http_port;
        LISA_LOGI(TAG, "gui peer %s (rom http port %u)", s_peer_gui_ip,
                  (unsigned)http_port);
    } else {
        LISA_LOGD(TAG, "peer %s (unidentified client)", s_peer_any_ip);
    }
}

bool gamepad_disc_peer_ip(char *buf, uint32_t buf_len, bool *is_gui)
{
    const char *src = (s_peer_gui_ip[0] != '\0') ? s_peer_gui_ip : s_peer_any_ip;

    if (is_gui) {
        *is_gui = (s_peer_gui_ip[0] != '\0');
    }
    if (!buf || buf_len == 0 || src[0] == '\0') {
        return false;
    }
    snprintf(buf, buf_len, "%s", src);
    return true;
}

uint16_t gamepad_disc_gui_http_port(void)
{
    return s_peer_gui_http_port;
}

/* ------------------------------------------------------------------ */
/* 方向二: 设备主动发现 PC 端 ROM 服务                                   */
/* ------------------------------------------------------------------ */

/* 本网段的定向广播地址 (如 192.168.31.255)。受限广播 255.255.255.255 在部分
 * AP 上不转发, 定向广播可靠得多, 所以两个都发。
 * 字节序换算与 gamepad_util_get_local_ip 保持一致 (低字节 = 第一段)。 */
static bool disc_directed_bcast(struct in_addr *out)
{
    net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
    uint32_t ip = 0, mask = 0, gw = 0;
    char buf[16];

    if (!net_if || net_if_get_ip(net_if, &ip, &mask, &gw) != 0 || ip == 0 || mask == 0) {
        return false;
    }

    uint32_t bcast = (uint32_t)((ip & mask) | ~mask);
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (unsigned)(bcast & 0xFF), (unsigned)((bcast >> 8) & 0xFF),
             (unsigned)((bcast >> 16) & 0xFF), (unsigned)((bcast >> 24) & 0xFF));
    out->s_addr = inet_addr(buf);
    return out->s_addr != INADDR_NONE && out->s_addr != 0;
}

bool gamepad_disc_find_server(uint32_t timeout_ms)
{
    struct sockaddr_in dst;
    char probe[DISC_BUF_LEN];
    char rx[DISC_BUF_LEN];
    int val = 1;
    bool found = false;

    if (timeout_ms == 0) {
        timeout_ms = 800U;
    }

    int n = snprintf(probe, sizeof(probe),
                     "{\"t\":\"discover_server\",\"ver\":%d,\"dev\":\"%s\",\"did\":\"%s\"}",
                     GAMEPAD_PROTO_VERSION, GAMEPAD_DEV_NAME, gamepad_util_get_device_id());
    if (n <= 0 || (uint32_t)n >= sizeof(probe)) {
        return false;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        LISA_LOGW(TAG, "find_server: socket failed: %d", errno);
        return false;
    }
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(CONFIG_GAMEPAD_DISCOVERY_PORT);

    /* 定向广播 + 受限广播各发一次; 来源端口为本地临时端口, 应答单播回这里 */
    if (disc_directed_bcast(&dst.sin_addr)) {
        (void)sendto(sock, probe, (size_t)n, 0, (struct sockaddr *)&dst, sizeof(dst));
    }
    dst.sin_addr.s_addr = INADDR_BROADCAST;
    if (sendto(sock, probe, (size_t)n, 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        LISA_LOGW(TAG, "find_server: broadcast failed: %d", errno);
    }

    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (!found && (int32_t)(xTaskGetTickCount() - deadline) < 0) {
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;

        int r = lwip_select(sock + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) {
            continue;   /* 超时: 重新检查总期限 */
        }

        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int rl = recvfrom(sock, rx, sizeof(rx) - 1, 0, (struct sockaddr *)&from, &from_len);
        if (rl <= 0) {
            continue;
        }
        rx[rl] = '\0';

        cJSON *root = cJSON_Parse(rx);
        if (!root) {
            continue;
        }
        const cJSON *type = cJSON_GetObjectItem(root, "t");
        const bool is_server = cJSON_IsString(type) && type->valuestring &&
                               strcmp(type->valuestring, "server") == 0;
        if (is_server) {
            uint16_t http_port = 0;
            const cJSON *port = cJSON_GetObjectItem(root, "http_port");
            if (cJSON_IsNumber(port)) {
                double v = cJSON_GetNumberValue(port);
                if (v >= 1.0 && v <= 65535.0) {
                    http_port = (uint16_t)v;
                }
            }
            /* 应答来源地址就是服务端地址 (比报文里自报的 ip 更可信) */
            snprintf(s_peer_gui_ip, sizeof(s_peer_gui_ip), "%s", inet_ntoa(from.sin_addr));
            snprintf(s_peer_any_ip, sizeof(s_peer_any_ip), "%s", s_peer_gui_ip);
            s_peer_gui_http_port = http_port;
            found = true;
            LISA_LOGI(TAG, "server found: %s (rom http port %u)",
                      s_peer_gui_ip, (unsigned)http_port);
        }
        cJSON_Delete(root);
    }

    closesocket(sock);
    if (!found) {
        LISA_LOGI(TAG, "find_server: no reply in %ums", (unsigned)timeout_ms);
    }
    return found;
}

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

        /* 反向记录探测来源 (可能是 pad_gui, 也可能是手机 APP); 解析失败也能应答 */
        disc_record_peer(&from, rx);

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
    if (xTaskCreate(disc_task, "gp_disc", DISC_TASK_STACK, NULL,
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
