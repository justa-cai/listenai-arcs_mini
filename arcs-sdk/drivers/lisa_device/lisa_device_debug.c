/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_device_debug.c
 * @brief LISA 设备框架 - 调试维测功能实现
 */

#include "lisa_device_debug.h"
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_LISA_DEVICE_DEBUG

/* ===== 链接器符号声明 ===== */
extern lisa_device_registry_entry_t __lisa_device_registry_start[];
extern lisa_device_registry_entry_t __lisa_device_registry_end[];

/* ===== 内部辅助函数 ===== */

/**
 * @brief 验证设备指针有效性
 */
static inline bool is_device_valid(const lisa_device_t *dev)
{
    return (dev != NULL && dev->name != NULL);
}

/**
 * @brief 打印设备回调函数
 */
static int print_device_callback(lisa_device_t *dev, void *user_data)
{
    (void)user_data;
    lisa_device_print_info(dev);
    printf("\n");
    return 0;
}

/* ===== 辅助工具实现 ===== */

const char *lisa_device_get_state_name(lisa_device_state_t state)
{
    switch (state) {
        case LISA_DEVICE_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case LISA_DEVICE_STATE_INITIALIZED:   return "INITIALIZED";
        case LISA_DEVICE_STATE_ERROR:         return "ERROR";
        default:                               return "UNKNOWN";
    }
}

/* ===== 信息打印实现 ===== */

void lisa_device_print_registry(void)
{
    size_t count = __lisa_device_registry_end - __lisa_device_registry_start;
    
    printf("=== LISA Device Registry ===\n");
    printf("Total entries: %zu\n", count);
    printf("%-4s %-20s %-8s %s\n", "No.", "Name", "Priority", "Init Fn");
    printf("------------------------------------------------------------\n");
    
    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &__lisa_device_registry_start[i];
        if (entry->device) {
            printf("%-4zu %-20s %-8u %p\n",
                   i,
                   entry->device->name,
                   entry->priority,
                   entry->init_fn);
        }
    }
    printf("\n");
}

void lisa_device_print_info(const lisa_device_t *dev)
{
    if (!is_device_valid(dev)) {
        printf("Invalid device\n");
        return;
    }
    
    printf("Device: %s\n", dev->name);
    printf("  State: %s\n", lisa_device_get_state_name(dev->state));
    printf("  Stats:\n");
    printf("    Reference count: %u\n", dev->stats.ref_count);
}

void lisa_device_print_all(void)
{
    uint32_t count = lisa_device_get_count();
    printf("=== Registered Devices (%u) ===\n\n", count);
    lisa_device_foreach(print_device_callback, NULL);
}

/* ===== 验证功能实现 ===== */

int lisa_device_verify_registry(void)
{
    size_t count = __lisa_device_registry_end - __lisa_device_registry_start;
    int errors = 0;
    
    printf("Verifying device registry...\n");
    
    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &__lisa_device_registry_start[i];
        
        if (!entry->device) {
            printf("  [ERROR] Entry %zu: NULL device pointer\n", i);
            errors++;
            continue;
        }
        
        if (!entry->device->name) {
            printf("  [ERROR] Entry %zu: NULL device name\n", i);
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("Registry verification passed\n");
        return LISA_DEVICE_OK;
    } else {
        printf("Registry verification failed with %d errors\n", errors);
        return LISA_DEVICE_ERR_INVALID;
    }
}

#endif /* CONFIG_LISA_DEVICE_DEBUG */
