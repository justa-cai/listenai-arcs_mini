/* net_kb —— 网络键盘（UDP）+ 设备发现 实现 */
#define TAG "net.kb"

#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#include "lisa_thread.h"
#include "lisa_log.h"

#include "voice_msg.h"
#include "net_kb.h"

#define NET_KB_PORT       38001
#define NET_KB_MAGIC      "LLMKB1:"
#define NET_KB_MAGIC_LEN  7
#define NET_KB_KEY_Q_LEN  64
#define NET_KB_LINE_MAX   128         /* 整行包内容上限 */
#define NET_KB_LINE_Q_LEN 4
#define NET_KB_PEER_TIMEOUT_MS 30000

static struct {
    uint8_t started;                       /* UDP 任务已启动 */
    char ip_str[16];                       /* 本机 IP 点分字符串 */
    TickType_t last_peer_tick;             /* 最近一次 PC 包 */
    QueueHandle_t key_q;
    QueueHandle_t line_q;                  /* 整行输入（item = char[NET_KB_LINE_MAX]） */
} S;

static void kb_thread(void *arg);

/* voice.ebus 线程回调：只做启动动作 */
static void on_wifi_ip_got(void *unused, uint32_t msg_id, void *data,
                           uint32_t len, void *user_data)
{
    (void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;
    if (S.started) return;
    S.started = 1;

    lisa_thread_attr_t att = {
        .name = "net.kb",
        .stack_size = 4096,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };
    if (lisa_thread_create(&att, kb_thread, NULL) == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        S.started = 0;
    }
}

int net_kb_init(void)
{
    if (S.key_q == NULL) {
        S.key_q = xQueueCreate(NET_KB_KEY_Q_LEN, sizeof(uint8_t));
        if (!S.key_q) return -1;
    }
    if (S.line_q == NULL) {
        S.line_q = xQueueCreate(NET_KB_LINE_Q_LEN, NET_KB_LINE_MAX);
        if (!S.line_q) return -1;
    }
    /* WiFi 重连会多次发 IP_GOT，订阅一次即可（回调幂等） */
    voice_msg_sub(VOICE_MSG_WIFI_IP_GOT, on_wifi_ip_got, NULL);
    return 0;
}

static void update_ip_str(void)
{
    S.ip_str[0] = '\0';
    /* lwip 默认网口（net_ip.c 已把 STA 设为 default） */
    const char *ip = ip4addr_ntoa(netif_ip4_addr(netif_default));
    if (ip && ip[0]) snprintf(S.ip_str, sizeof S.ip_str, "%s", ip);
}

static void kb_thread(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        LISA_LOGE(TAG, "socket failed: %d", sock);
        S.started = 0;
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500 * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    struct sockaddr_in bind_addr = { 0 };
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(NET_KB_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof bind_addr) != 0) {
        LISA_LOGE(TAG, "bind %d failed", NET_KB_PORT);
        closesocket(sock);
        S.started = 0;
        return;
    }

    update_ip_str();
    LISA_LOGI(TAG, "listening on :%d ip=%s", NET_KB_PORT, S.ip_str);

    char buf[128];
    TickType_t now;
    while (1) {
        struct sockaddr_in src = { 0 };
        socklen_t slen = sizeof src;
        int n = recvfrom(sock, buf, sizeof buf - 1, 0,
                         (struct sockaddr *)&src, &slen);
        now = xTaskGetTickCount();
        if (n <= 0) {
            /* RCVTIMEO 轮询空转：顺带判定 PC 离线 */
            if (S.last_peer_tick &&
                now - S.last_peer_tick > pdMS_TO_TICKS(NET_KB_PEER_TIMEOUT_MS)) {
                S.last_peer_tick = 0;
                LISA_LOGI(TAG, "peer offline (30s no packet)");
            }
            continue;
        }

        S.last_peer_tick = now;            /* 收到任何包都刷新 PC 在线 */
        if (n < NET_KB_MAGIC_LEN + 1) continue;
        if (memcmp(buf, NET_KB_MAGIC, NET_KB_MAGIC_LEN) != 0) continue;

        char type = buf[NET_KB_MAGIC_LEN];
        if (type == 'D') {
            /* 发现探测：单播应答（PC 从应答源地址学到设备 IP） */
            char reply[48];
            update_ip_str();
            int m = snprintf(reply, sizeof reply,
                             NET_KB_MAGIC "d arcs-mini %s", S.ip_str);
            sendto(sock, reply, m, 0, (struct sockaddr *)&src, sizeof src);
        } else if (type == 'L' && n > NET_KB_MAGIC_LEN + 1) {
            /* 整行输入：一行一包（PC 回车时发送） */
            char line[NET_KB_LINE_MAX];
            int body = n - NET_KB_MAGIC_LEN - 1;
            if (body >= NET_KB_LINE_MAX) body = NET_KB_LINE_MAX - 1;
            memcpy(line, buf + NET_KB_MAGIC_LEN + 1, body);
            line[body] = '\0';
            xQueueSend(S.line_q, line, 0);  /* 满则丢行（终端 busy 丢弃） */
        } else if (type == 'K' && n >= NET_KB_MAGIC_LEN + 2) {
            uint8_t key = (uint8_t)buf[NET_KB_MAGIC_LEN + 1];
            xQueueSend(S.key_q, &key, 0);   /* 满则丢键（人眼重按） */
        } else if (type == 'H') {
            /* 心跳，无需处理：peer_alive 已刷新 */
        }
    }
}

int net_kb_getkey(void)
{
    uint8_t key = 0;
    if (!S.key_q || xQueueReceive(S.key_q, &key, 0) != pdPASS) return -1;
    return key;
}

int net_kb_getline(char *buf, unsigned int len)
{
    char line[NET_KB_LINE_MAX];
    if (!S.line_q || !buf || len == 0) return -1;
    if (xQueueReceive(S.line_q, line, 0) != pdPASS) return -1;
    unsigned int n = strlen(line);
    if (n >= len) n = len - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';
    return 0;
}

int net_kb_test_inject(const char *str)
{
    if (!S.key_q || !str) return -1;
    while (*str) {
        uint8_t k = (uint8_t)*str++;
        xQueueSend(S.key_q, &k, 0);
    }
    uint8_t nl = '\n';
    xQueueSend(S.key_q, &nl, 0);
    return 0;
}

int net_kb_line_push(const char *line)
{
    char buf[NET_KB_LINE_MAX];
    if (!S.line_q || !line || !line[0]) return -1;
    unsigned int n = strlen(line);
    if (n >= NET_KB_LINE_MAX) n = NET_KB_LINE_MAX - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';
    return xQueueSend(S.line_q, buf, 0) == pdPASS ? 0 : -1;
}

uint8_t net_kb_peer_alive(void)
{
    return S.last_peer_tick != 0;
}

const char *net_kb_ip_str(void)
{
    return S.ip_str[0] ? S.ip_str : "no-ip";
}
