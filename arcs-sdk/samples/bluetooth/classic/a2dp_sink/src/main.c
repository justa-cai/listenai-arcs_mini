/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"

#define TAG "app-bt"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "shell_passthrough.h"
#include "lisa_bluetooth.h"
#include "fs/user_fs.h"

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
