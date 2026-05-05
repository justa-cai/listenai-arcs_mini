/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#define TAG "ap_main"
#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "sys_init.h"
#include "lisa_log.h"

static int boot_cp(void)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30080000;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    return 0;
}

SYS_INIT(boot_cp, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 1);

int main(void)
{
    uint32_t heartbeat = 0;

    LISA_LOGI(TAG, "AP crash test setup done");

    while (heartbeat < 2) {
        LISA_LOGI(TAG, "AP heartbeat %lu", (unsigned long)heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LISA_LOGI(TAG, "AP crash test trigger");
    log_flush();

    __builtin_trap();

    return 0;
}
