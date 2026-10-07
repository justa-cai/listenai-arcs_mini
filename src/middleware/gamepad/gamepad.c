/*
 * Gamepad 中间件入口: 启动 UDP 发现 + UDP 按键通道 + WebSocket server。
 * 提供 `gamepad` shell 命令诊断 (status/init/mask)。
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

bool gamepad_pad_active(void)
{
    return gamepad_input_ble_active();
}

/* ------------------------------------------------------------------ */
/* shell 诊断命令: gamepad [status|init|mask]                            */
/* ------------------------------------------------------------------ */
static int gamepad_cmd_handler(int argc, char **argv)
{
    const char *sub = (argc > 1) ? argv[1] : "status";

    Shell *shell = shellGetCurrent();
    if (strcmp(sub, "init") == 0) {
        int ret = gamepad_init();
        shellPrint(shell, "gamepad init: %d (err=%d disc=%d udp=%d ws=%d)\r\n", ret, s_last_err,
                   s_disc_started, s_udp_started, s_ws_started);
        return 0;
    }

    if (strcmp(sub, "mask") == 0) {
        /* 没有独立的"当前位图"读取接口 —— 位图只在 gamepad_input 内部维护,
         * 这里回显不出按下集合, 只能看输入是否被让小应用收到 (miniapp 日志)。 */
        shellPrint(shell, "use: miniapp button log / blepad status\r\n");
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
    shellPrint(shell, "inited=%d err=%d disc=%d udp=%d ws=%d\r\n", s_inited, s_last_err,
               s_disc_started, s_udp_started, s_ws_started);
    shellPrint(shell, "ip=%s ws_port=%u disc_port=%u udp_port=%u\r\n", ip,
               (unsigned)CONFIG_GAMEPAD_WS_PORT,
               (unsigned)CONFIG_GAMEPAD_DISCOVERY_PORT,
               (unsigned)CONFIG_GAMEPAD_UDP_PORT);
    shellPrint(shell, "miniapp running=%s fps=%u ble=%s\r\n",
               running ? "true" : "false", (unsigned)fps,
               gamepad_input_ble_active() ? "true" : "false");
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 gamepad, gamepad_cmd_handler, gamepad status cmd);
