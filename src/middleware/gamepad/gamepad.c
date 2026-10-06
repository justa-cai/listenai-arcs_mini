/*
 * Gamepad 中间件入口: 启动 UDP 发现 + WebSocket server。
 * 提供 `gamepad` shell 命令诊断 (status/init/heap/mask)。
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"
#include "lisa_log.h"
#include "shell.h"

extern int gamepad_discovery_start(void);
extern void gamepad_discovery_stop(void);
extern int gamepad_udp_start(void);
extern void gamepad_udp_stop(void);
extern int gamepad_ws_server_start(void);
extern void gamepad_ws_server_stop(void);

#define TAG "gamepad"

static bool s_inited = false;
static int s_last_err = 0;
static bool s_disc_started = false;
static bool s_udp_started = false;
static bool s_ws_started = false;

int gamepad_init(void)
{
    if (s_inited) {
        return 0;
    }

    s_last_err = 0;

    if (gamepad_discovery_start() != 0) {
        s_last_err = -1; /* 任务创建失败 (FreeRTOS 堆不足?) */
        LISA_LOGE(TAG, "discovery start failed");
        return -1;
    }
    s_disc_started = true;

    if (gamepad_udp_start() != 0) {
        s_last_err = -3;
        LISA_LOGE(TAG, "udp key channel start failed");
        return -1;
    }
    s_udp_started = true;

    if (gamepad_ws_server_start() != 0) {
        s_last_err = -2;
        LISA_LOGE(TAG, "ws server start failed");
        return -1;
    }
    s_ws_started = true;

    s_inited = true;
    LISA_LOGI(TAG, "init ok (ws:%u disc:%u udp:%u)",
              (unsigned)CONFIG_GAMEPAD_WS_PORT,
              (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT,
              (unsigned)CONFIG_GAMEPAD_UDP_PORT);

#if CONFIG_GAMEPAD_BLE && CONFIG_GAMEPAD_BLE_AUTOSTART
    /* 开机自启 BLE 手柄直连：扫描 → 识别 CodexPad → 连接 → 订阅。
     * 此处 BLE 栈可能尚未使能（ipc 就绪后才 lisa_bluetooth_init），
     * 驱动内部会按 CPAD_INIT_RETRY_MS 重试 client init。 */
    extern int ble_pad_start(void);
    if (ble_pad_start() == 0) {
        LISA_LOGI(TAG, "ble pad autostart requested");
    } else {
        LISA_LOGW(TAG, "ble pad autostart failed");
    }
#endif
    return 0;
}

/* ------------------------------------------------------------------ */
/* 对端 (PC 端 pad_gui) 地址判定                                        */
/* ------------------------------------------------------------------ */

bool gamepad_get_server_addr(char *ip_buf, uint32_t ip_len, uint16_t *http_port)
{
    char ip[32];
    bool is_gui = false;

    if (http_port) {
        *http_port = 0;
    }
    if (!ip_buf || ip_len == 0) {
        return false;
    }
    ip_buf[0] = '\0';

    /* ① UDP 发现里自报为桌面 GUI 的来源: 最可信 —— 它主动声明了身份和
     *    ROM API 端口, 且不依赖 WebSocket 会话是否建立过 */
    if (gamepad_disc_peer_ip(ip, sizeof(ip), &is_gui) && is_gui) {
        snprintf(ip_buf, ip_len, "%s", ip);
        if (http_port) {
            *http_port = gamepad_disc_gui_http_port();
        }
        return true;
    }

    /* ② WebSocket 手柄会话对端: 正常流程里与 ① 同一台 PC (pad_gui 先扫描再连接) */
    if (gamepad_ws_peer_ip(ip, sizeof(ip))) {
        snprintf(ip_buf, ip_len, "%s", ip);
        return true;
    }

    /* ③ 兜底: 任意 UDP 发现来源 (客户端身份未声明, 可能是脚本/第三方工具) */
    if (gamepad_disc_peer_ip(ip, sizeof(ip), NULL)) {
        snprintf(ip_buf, ip_len, "%s", ip);
        return true;
    }

    ip_buf[0] = '\0';
    return false;
}

bool gamepad_find_server_addr(char *ip_buf, uint32_t ip_len, uint16_t *http_port)
{
    if (http_port) {
        *http_port = 0;
    }

    /* 先看已经确认过的 (UDP 探测来源 / WS 会话对端), 命中就不用广播 */
    if (gamepad_get_server_addr(ip_buf, ip_len, http_port)) {
        return true;
    }

    /* 都没有: 设备主动广播找服务端 (不依赖 pad_gui 先扫描过设备)。
     * 结果写进同一个槽位, 后续调用走上面的缓存分支。 */
    if (!gamepad_disc_find_server(0U)) {
        return false;
    }
    return gamepad_get_server_addr(ip_buf, ip_len, http_port);
}

/* ------------------------------------------------------------------ */
/* shell 诊断命令: gamepad [status|init|heap|mask]                       */
/* ------------------------------------------------------------------ */
static int gamepad_cmd_handler(int argc, char **argv)
{
    const char *sub = (argc > 1) ? argv[1] : "status";

    Shell *shell = shellGetCurrent();
    if (strcmp(sub, "init") == 0) {
        int ret = gamepad_init();
        shellPrint(shell, "gamepad init: %d (err=%d disc=%d ws=%d)\r\n", ret, s_last_err,
                   s_disc_started, s_ws_started);
        return 0;
    }

    if (strcmp(sub, "mask") == 0) {
        shellPrint(shell, "joypad mask: 0x%04X\r\n", gamepad_get_joypad_mask());
        return 0;
    }

    /* status */
    bool running = false;
    uint32_t fps = 0;
    char ip[16] = "none";

    gamepad_get_game_state(&running, &fps);
    if (!gamepad_util_get_local_ip(ip, sizeof(ip))) {
        snprintf(ip, sizeof(ip), "none");
    }
    shellPrint(shell, "inited=%d err=%d disc=%d ws=%d\r\n", s_inited, s_last_err,
               s_disc_started, s_ws_started);
    shellPrint(shell, "ip=%s ws_port=%u disc_port=%u\r\n", ip,
               (unsigned)CONFIG_GAMEPAD_WS_PORT,
               (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT);
    shellPrint(shell, "game running=%s fps=%u\r\n", running ? "true" : "false", (unsigned)fps);
    shellPrint(shell, "joypad mask=0x%04X\r\n", gamepad_get_joypad_mask());
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 gamepad, gamepad_cmd_handler, gamepad status cmd);
