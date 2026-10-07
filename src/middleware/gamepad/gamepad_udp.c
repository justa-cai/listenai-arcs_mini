/*
 * Gamepad UDP 按键通道: 低延迟输入路径 (协议 §4.6)。
 *
 * 端口由 WS welcome / UDP announce 协商下发 (字段 "udp_port");
 * WS 通道保留给状态 / 心跳, 按键可走 UDP。
 *
 * 帧 (8 字节, 大端):
 *   [0] magic 0xA5   [1] ver 0x01   [2..5] seq(u32)   [6..7] mask(u16)
 *
 * 应用层弱网机制 (无重传, 靠状态幂等自愈):
 *   - 客户端: 按键状态一变立即发 + 连接期间 20Hz 周期重发当前位图
 *     (GAMEPAD_UDP_REFRESH_MS 为设备端建议值, 客户端常量与之对齐);
 *   - 设备端: seq 单调去重丢弃乱序/陈旧包; mask 全量覆盖按下集合
 *     (上升沿/下降沿与 WS 差分事件等价);
 *   - 防粘键: 收到过有效帧后, 若 GAMEPAD_UDP_SILENCE_RELEASE_MS 内
 *     无任何帧, 视为发送端失联, 自动松开全部按键。
 */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "lisa_log.h"
#include "lwip/sockets.h"

#define TAG "gamepad_udp"

#define UDP_TASK_STACK      (1536)
#define UDP_RECV_POLL_MS    (100)   /* recvfrom 超时粒度 (兼做静默检查周期) */

/* 客户端建议重发周期: 20Hz (帧仅 8B, 空口开销可忽略) */
#define GAMEPAD_UDP_REFRESH_MS        (50)
/* 静默松键阈值: 这么久没有收到任何 UDP 帧 (且通道曾活跃) 则松开全部按键 */
#define GAMEPAD_UDP_SILENCE_RELEASE_MS (500)

static int s_sock = -1;
static volatile bool s_running = false;

/* 统计 (供 WS 周期上报, PC 端据此算丢包率): 当前 UDP 会话累计 */
static volatile uint32_t s_stat_seq = 0;   /* 已应用的最大 seq */
static volatile uint32_t s_stat_rx = 0;    /* 按 seq 去重后接受的帧数 */
static volatile bool s_stat_active = false;/* 是否有活跃 peer */

static inline uint32_t udp_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* WS 线程调用: 取当前会话统计; 无活跃 peer 返回 false */
bool gamepad_udp_get_stats(uint32_t *last_seq, uint32_t *rx_frames)
{
    if (!s_stat_active) {
        return false;
    }
    if (last_seq) {
        *last_seq = s_stat_seq;
    }
    if (rx_frames) {
        *rx_frames = s_stat_rx;
    }
    return true;
}

static void udp_task(void *arg)
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

    /* recvfrom 带超时, 兼做周期性的静默检查 */
    struct timeval tv;
    tv.tv_sec = UDP_RECV_POLL_MS / 1000;
    tv.tv_usec = (UDP_RECV_POLL_MS % 1000) * 1000;
    setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFIG_GAMEPAD_UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LISA_LOGE(TAG, "bind :%u failed: %d", (unsigned)CONFIG_GAMEPAD_UDP_PORT, errno);
        closesocket(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    LISA_LOGI(TAG, "udp key channel listening on :%u", (unsigned)CONFIG_GAMEPAD_UDP_PORT);

    bool have_peer = false;      /* 收到过有效帧 (通道被使用过) */
    uint32_t last_seq = 0;       /* 已应用的最大 seq (去重基准) */
    uint32_t last_rx_ms = 0;

    while (s_running) {
        uint8_t frame[GAMEPAD_UDP_FRAME_LEN];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);

        int rx_len = recvfrom(s_sock, frame, sizeof(frame), 0,
                              (struct sockaddr *)&from, &from_len);
        if (rx_len < 0) {
            /* 超时: 检查静默松键 */
            if (have_peer &&
                udp_now_ms() - last_rx_ms > GAMEPAD_UDP_SILENCE_RELEASE_MS) {
                LISA_LOGW(TAG, "udp peer silent %ums, release all keys",
                          (unsigned)GAMEPAD_UDP_SILENCE_RELEASE_MS);
                /* 网络入口: BLE 手柄占用时不清键 (BLE 是事件上报, 清了它写不回来) */
                gamepad_input_net_set_mask(0);
                have_peer = false;
                /* 会话结束: 清零统计, PC 端按非单调检测自动重定基线 */
                s_stat_active = false;
                s_stat_seq = 0;
                s_stat_rx = 0;
            }
            continue;
        }
        if (rx_len != GAMEPAD_UDP_FRAME_LEN ||
            frame[0] != GAMEPAD_UDP_FRAME_MAGIC ||
            frame[1] != GAMEPAD_UDP_FRAME_VER) {
            continue;
        }

        const uint32_t seq = ((uint32_t)frame[2] << 24) | ((uint32_t)frame[3] << 16) |
                             ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];
        const uint16_t mask = (uint16_t)((uint16_t)frame[6] << 8 | (uint16_t)frame[7]);

        last_rx_ms = udp_now_ms();

        if (!have_peer) {
            have_peer = true;
            last_seq = seq;
            LISA_LOGI(TAG, "udp peer %s:%d active (seq=%u mask=0x%04X)",
                      inet_ntoa(from.sin_addr), (int)ntohs(from.sin_port),
                      (unsigned)seq, (unsigned)mask);
        } else if ((int32_t)(seq - last_seq) <= 0) {
            continue;   /* 乱序/陈旧/重复包, 丢弃 */
        }
        last_seq = seq;

        /* 统计: 按 seq 去重后接受的帧 (供 PC 端计算丢包率) */
        s_stat_seq = seq;
        s_stat_rx++;
        s_stat_active = true;

        /* 网络入口: BLE 手柄占用时忽略整帧 (统计仍照常更新, PC 端丢包率不受影响) */
        gamepad_input_net_set_mask(mask);
    }

    closesocket(s_sock);
    s_sock = -1;
    vTaskDelete(NULL);
}

int gamepad_udp_start(void)
{
    if (s_running) {
        return 0;
    }
    s_running = true;
    if (xTaskCreate(udp_task, "gp_udp", UDP_TASK_STACK, NULL,
                    configMAX_PRIORITIES - 4, NULL) != pdPASS) {
        s_running = false;
        LISA_LOGE(TAG, "udp task create failed");
        return -1;
    }
    return 0;
}

void gamepad_udp_stop(void)
{
    s_running = false;
    if (s_sock >= 0) {
        closesocket(s_sock); /* 解开 recvfrom 阻塞 */
        s_sock = -1;
    }
}
