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
    return 0;
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
