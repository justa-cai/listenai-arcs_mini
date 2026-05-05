/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "ap_main"
#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "sys_init.h"
#include "lisa_log.h"

static void ap_demo_log_apis(void)
{
    LISA_LOGI(TAG, "==== AP log API demo start ====");
    LISA_LOGI(TAG, "AP LISA_LOGI demo");
    LISA_LOGI(TAG, "==== AP log API demo end ====");
    LISA_LOGI(TAG, "AP IPC log sample passed");
    log_flush();
}

static int boot_cp(void)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30080000;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    return 0;
}

SYS_INIT(boot_cp, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 1);

int main(int argc, char **argv)
{
    uint32_t heartbeat = 0;

    ap_demo_log_apis();

    while (1) {
        LISA_LOGI(TAG, "AP heartbeat %lu", (unsigned long)heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
