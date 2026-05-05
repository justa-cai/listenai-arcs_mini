/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"

#define TAG "cp_main"

int main(int argc, char **argv)
{
    uint32_t heartbeat = 0;

    printf("\n========================================\n");
    printf("=== Dual-Core IPC Log Sample (CP)    ===\n");
    printf("========================================\n");

    while (1) {
        LISA_LOGI(TAG, "CP heartbeat %lu", (unsigned long)heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    return 0;
}
