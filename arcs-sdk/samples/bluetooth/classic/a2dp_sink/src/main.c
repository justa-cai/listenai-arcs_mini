/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"

#define TAG "app-bt"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "shell_passthrough.h"
#include "lisa_bluetooth.h"
#include "lisa_bt_classic_api.h"
#include "fs/user_fs.h"

static int cmd_bt_scan(int argc, char **argv)
{
    char *end = NULL;
    unsigned long value;
    int ret;

    if (argc != 2) {
        LOGE("Usage: bt_scan <0|1|2|3>\n");
        return -1;
    }

    value = strtoul(argv[1], &end, 0);
    if ((argv[1][0] == '\0') || (end == NULL) || (*end != '\0') || (value > 3)) {
        LOGE("Invalid argument: %s\n", argv[1]);
        LOGE("Usage: bt_scan <0|1|2|3>\n");
        return -1;
    }

    ret = lisa_bt_scan((uint8_t)value);
    if (ret != 0) {
        LOGE("bt_scan failed: %d\n", ret);
        return ret;
    }

    LOGI("bt_scan %lu\n", value);
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;

    extern uint8_t _sshram[], _eshram[];
    memset(_sshram, 0, (_eshram - _sshram));

    atcmd_init();

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("Failed to initialize shell (error: %d)\n", ret);
    }

    /* Initialize filesystem for BT pairing info storage */
    user_fs_init();
    lisa_kv_init();

    lisa_bluetooth_init(NULL);

    ret = bt_audio_framework_init();
    if (ret != 0) {
        LOGE("Failed to initialize BT audio framework: %d", ret);
        return -1;
    }

    ret = bt_sink_init();
    if (ret != 0) {
        LOGE("Failed to initialize BT SINK: %d", ret);
        return -1;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

static int at_passthrough_handler(char *data, unsigned short len)
{
    atcmd_handler(data, len);

    return 0;
}

// Register AT passthrough mode command
// Usage:
//   Interactive mode: AT (then input AT+?, no space needed)
//   Single-line mode: AT AT+? (execute at once)
SHELL_EXPORT_PASSTROUGH(SHELL_CMD_PERMISSION(0), AT, AT>>, at_passthrough_handler, AT command passthrough mode);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_scan, cmd_bt_scan, enable or disable bt classic scan);
