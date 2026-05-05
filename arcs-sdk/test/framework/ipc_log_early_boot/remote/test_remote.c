/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "ap_main"

#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "lisa_log.h"

static void boot_cp(void)
{
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30080000;
    __asm__ volatile ("fence iorw,iorw" : : : "memory");
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int main(void)
{
    uint32_t heartbeat = 0;

    LISA_LOGI(TAG, "AP early boot IPC log begin");
    LISA_LOGI(TAG, "AP early boot IPC log passed");
    log_flush();
    boot_cp();

    while (1) {
        LISA_LOGI(TAG, "AP heartbeat %lu", (unsigned long)heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}
