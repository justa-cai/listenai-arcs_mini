/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "coex"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_kv.h"
#include "lisa_shell.h"

#include "coex_bt.h"
#include "coex_bt_audio.h"
#include "coex_iperf.h"
#include "coex_shell.h"
#include "coex_wifi.h"
#include "fs/user_fs.h"

int main(int argc, char **argv)
{
    int ret;

    (void)argc;
    (void)argv;

    LOGI("WiFi+A2DP iperf coex sample booting");

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("shell init failed: %d", ret);
    }

    ret = user_fs_init();
    if (ret != 0) {
        LOGW("user fs init failed: %d", ret);
    }

    ret = lisa_kv_init();
    if (ret != 0) {
        LOGW("kv init failed: %d", ret);
    }

    ret = coex_wifi_init();
    if (ret != 0) {
        LOGE("coex wifi init failed: %d", ret);
    }

    ret = coex_iperf_init();
    if (ret != 0) {
        LOGE("coex iperf init failed: %d", ret);
    }

    ret = coex_bt_init();
    if (ret != 0) {
        LOGE("coex bt init failed: %d", ret);
    }

    ret = coex_bt_audio_init();
    if (ret != 0) {
        LOGE("coex bt audio init failed: %d", ret);
    }

    ret = coex_shell_register();
    if (ret != 0) {
        LOGE("coex shell register failed: %d", ret);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
