/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief 系统堆管理基础示例
 *
 * 本示例演示如何使用系统堆管理组件分配和释放内存：
 * 1. 从内部 SRAM 分配内存
 * 2. 从外部 PSRAM 分配内存
 * 3. 使用 calloc 分配并清零内存
 * 4. 使用 realloc 重新分配内存
 * 5. 查看堆使用统计信息
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include "sysheap.h"

#include "FreeRTOS.h"
#include "task.h"

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== System Heap Example ===");

    /* 示例 1: 内部 SRAM 内存分配 */
    LISA_LOGI(LOG_TAG, "\n1. SRAM allocation:");
    uint8_t *sram_buf = inram_malloc(4, 256);
    if (sram_buf) {
        strcpy((char *)sram_buf, "Hello from SRAM");
        LISA_LOGI(LOG_TAG, "   Allocated 256 bytes at: %p", sram_buf);
        LISA_LOGI(LOG_TAG, "   Data: %s", sram_buf);
        inram_free(sram_buf);
    } else {
        LISA_LOGE(LOG_TAG, "Error: SRAM allocation failed");
    }

    /* 示例 2: PSRAM 内存分配 */
    LISA_LOGI(LOG_TAG, "\n2. PSRAM allocation:");
    uint8_t *psram_buf = psram_malloc(1024);
    if (psram_buf) {
        strcpy((char *)psram_buf, "Hello from PSRAM");
        LISA_LOGI(LOG_TAG, "   Allocated 1024 bytes at: %p", psram_buf);
        LISA_LOGI(LOG_TAG, "   Data: %s", psram_buf);
        psram_free(psram_buf);
    } else {
        LISA_LOGE(LOG_TAG, "Error: PSRAM allocation failed");
    }

    /* 示例 3: PSRAM calloc - 分配并清零 */
    LISA_LOGI(LOG_TAG, "\n3. PSRAM calloc (zeroed):");
    uint32_t *array = psram_calloc(10, sizeof(uint32_t));
    if (array) {
        LISA_LOGI(LOG_TAG, "   Allocated 10 uint32_t at: %p", array);
        LISA_LOGI(LOG_TAG, "   First element (0): %u", array[0]);
        array[0] = 12345;
        LISA_LOGI(LOG_TAG, "   Modified to: %u", array[0]);
        psram_free(array);
    } else {
        LISA_LOGE(LOG_TAG, "Error: calloc failed");
    }

    /* 示例 4: 内存重新分配 */
    LISA_LOGI(LOG_TAG, "\n4. Memory reallocation:");
    char *buffer = psram_malloc(64);
    if (buffer) {
        strcpy(buffer, "Initial data");
        LISA_LOGI(LOG_TAG, "   64 bytes: %s", buffer);

        char *new_buffer = psram_realloc(buffer, 128);
        if (new_buffer) {
            LISA_LOGI(LOG_TAG, "   Reallocated to 128 bytes at: %p", new_buffer);
            LISA_LOGI(LOG_TAG, "   Data preserved: %s", new_buffer);
            psram_free(new_buffer);
        } else {
            LISA_LOGE(LOG_TAG, "Error: realloc failed");
            psram_free(buffer);
        }
    } else {
        LISA_LOGE(LOG_TAG, "Error: initial malloc failed");
    }

    /* 示例 5: 堆统计信息 */
    LISA_LOGI(LOG_TAG, "\n5. Heap summary:");
    heap_summary_info();

    LISA_LOGI(LOG_TAG, "\n=== Example completed ===");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
