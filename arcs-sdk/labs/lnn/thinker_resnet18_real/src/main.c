/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lnn_real_cp"

#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lnn_resnet18_real_cp.h"

static void log_boot_banner(void)
{
    LOGI("Thinker ResNet18 real sample CP start");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    log_boot_banner();

    if (lnn_mbox_init() != 0) {
        return -1;
    }

    if (lnn_display_touch_init() != 0) {
        return -1;
    }

    if (lnn_camera_init() != 0) {
        return -1;
    }

    if (lnn_ipc_mutex_init() != 0) {
        return -1;
    }

    if (xTaskCreate(lnn_task_ui, "lnn_ui", 4096, NULL, configMAX_PRIORITIES - 2, NULL) != pdPASS) {
        LOGE("create UI task failed");
        return -1;
    }

    if (xTaskCreate(lnn_task_camera, "lnn_camera", 4096, NULL, configMAX_PRIORITIES - 3, NULL) != pdPASS) {
        LOGE("create camera task failed");
        return -1;
    }

    /* ADB shell + reboot recovery：demo 下也能 adb reboot recovery 进 BOOT 烧录 */
    if (lnn_usb_adb_init() != 0) {
        LOGW("usb adb init failed (flash via burn_serial.sh)");
    }

    return 0;
}
