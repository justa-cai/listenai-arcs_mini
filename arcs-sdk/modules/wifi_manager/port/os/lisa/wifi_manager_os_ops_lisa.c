/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_thread.h"
#include "lisa_mutex.h"
#include "lisa_queue.h"
#include "wifi_manager/wifi_manager.h"

// 互斥锁操作
static void *mutex_create_func(void)
{
    return lisa_mutex_create();
}

static int mutex_lock_func(void *mutex, uint32_t timeout)
{
    return lisa_mutex_lock((lisa_mutex_t *)mutex, timeout);
}

static int mutex_unlock_func(void *mutex)
{
    return lisa_mutex_unlock((lisa_mutex_t *)mutex);
}

static void mutex_delete_func(void *mutex)
{
    lisa_mutex_delete((lisa_mutex_t *)mutex);
}

// 队列操作
static void *queue_create_func(uint32_t queue_length, const char *name, size_t item_size)
{
    return lisa_queue_create(queue_length, (uint8_t *)name, item_size);
}

static int queue_push_func(void *queue, const void *item, size_t item_size, uint32_t timeout)
{
    return lisa_queue_push((lisa_queue_t *)queue, (void *)item, item_size, timeout);
}

static int queue_pop_func(void *queue, void *item, size_t item_size, uint32_t timeout)
{
    return lisa_queue_pop((lisa_queue_t *)queue, item, item_size, timeout);
}

static void queue_delete_func(void *queue)
{
    lisa_queue_delete((lisa_queue_t *)queue);
}

// 线程操作
static void *thread_create_func(wifi_manager_os_thread_attr_t *attr, wifi_manager_os_thread_entry_t entry, void *arg)
{
    lisa_thread_attr_t lisa_attr;
    lisa_attr.name = attr->name;
    lisa_attr.stack_size = attr->stack_size;
    lisa_attr.priority = attr->priority;

    return lisa_thread_create(&lisa_attr, entry, arg);
}

static void thread_delete_func(void *thread)
{
    lisa_thread_delete((lisa_thread_t *)thread);
}

// 导出全局OS操作结构体
wifi_manager_os_ops_t wifi_manager_os_ops_lisa = {
    .mutex_create = mutex_create_func,
    .mutex_lock = mutex_lock_func,
    .mutex_unlock = mutex_unlock_func,
    .mutex_delete = mutex_delete_func,
    
    .queue_create = queue_create_func,
    .queue_push = queue_push_func,
    .queue_pop = queue_pop_func,
    .queue_delete = queue_delete_func,
    
    .thread_create = thread_create_func,
    .thread_delete = thread_delete_func
};

wifi_manager_os_ops_t* wifi_manager_lisa_os_ops_get(void) {
    return &wifi_manager_os_ops_lisa;
}

