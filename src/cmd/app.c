#include "shell.h"
#include "stdint.h"
#include "string.h"
#include "cmd.h"

#ifdef XIAOZHI_CLOUD
#include "xz_cloud.h"
#include "lisa_log.h"
#endif

static int wakeup(int argc, char **argv)
{
    extern void client_wakeup_test();
    client_wakeup_test();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, wakeup,
                 wakeup, wakeup test cmd);

#ifdef XIAOZHI_CLOUD
static int cmd_xz_activate(int argc, char **argv)
{
    const char *server_url = NULL;

    if (argc > 1) {
        server_url = argv[1];
    }

    LISA_LOGI("cmd", "Starting XiaoZhi OTA activation...");

    if (xz_cloud_activate(server_url) == 0) {
        LISA_LOGI("cmd", "Activation successful!");
        LISA_LOGI("cmd", "Please reboot or reconnect to apply new credentials.");
    } else {
        LISA_LOGE("cmd", "Activation failed!");
        return -1;
    }

    return 0;
}

static int cmd_xz_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (xz_cloud_is_activated()) {
        LISA_LOGI("cmd", "XiaoZhi: Activated");
    } else {
        LISA_LOGI("cmd", "XiaoZhi: Not activated");
    }

    if (xz_cloud_is_connected()) {
        LISA_LOGI("cmd", "XiaoZhi: Connected");
    } else {
        LISA_LOGI("cmd", "XiaoZhi: Not connected");
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 xz_activate, cmd_xz_activate, xz_activate [server_url] - Activate XiaoZhi cloud);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 xz_status, cmd_xz_status, xz_status - Show XiaoZhi status);
#endif
