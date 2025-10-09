/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *name;
    uint32_t stack_size;
    uint32_t priority;
} wifi_manager_os_thread_attr_t;

typedef void (*wifi_manager_os_thread_entry_t)(void *);


/**
 * @brief 定义系统操作结构体
 */
typedef struct {
    void *(*mutex_create)(void);
    int (*mutex_lock)(void *mutex, uint32_t timeout);
    int (*mutex_unlock)(void *mutex);
    void (*mutex_delete)(void *mutex);

    void *(*queue_create)(uint32_t queue_length, const char *name, size_t item_size);
    int (*queue_push)(void *queue, const void *item, size_t item_size, uint32_t timeout);
    int (*queue_pop)(void *queue, void *item, size_t item_size, uint32_t timeout);
    void (*queue_delete)(void *queue);

    void *(*thread_create)(wifi_manager_os_thread_attr_t *attr, wifi_manager_os_thread_entry_t entry, void *arg);
    void (*thread_delete)(void *thread);
} wifi_manager_os_ops_t;


#ifdef __cplusplus
}
#endif
