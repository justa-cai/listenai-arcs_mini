/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_device.c
 * @brief LISA 设备框架 - 设备管理核心实现
 */

#include "lisa_device.h"
#include <lisa_mutex.h>
#include <lisa_time.h>
#include <string.h>

#define LOG_TAG "lisa_device"
#include <lisa_log.h>

/* ===== 链接器符号声明 ===== */
extern lisa_device_registry_entry_t __lisa_device_registry_start[];
extern lisa_device_registry_entry_t __lisa_device_registry_end[];

/* ===== 全局变量 ===== */
static lisa_device_t *device_list_head = NULL;
static uint32_t device_count = 0;
static bool manager_initialized = false;
static lisa_mutex_t *device_mutex = NULL;

/* ===== 内部辅助函数 ===== */

/**
 * @brief 验证设备指针有效性
 */
static inline bool is_device_valid(const lisa_device_t *dev)
{
    return (dev != NULL && dev->name != NULL);
}

/**
 * @brief 内部设备查找函数 (不增加引用计数)
 * @note 仅供内部使用
 */
static lisa_device_t *find_device_by_name(const char *name)
{
    if (!name) {
        return NULL;
    }

    if (device_mutex) {
        lisa_mutex_lock(device_mutex, LISA_WAIT_FOREVER);
    }

    lisa_device_t *dev = device_list_head;

    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            if (device_mutex) {
                lisa_mutex_unlock(device_mutex);
            }
            return dev;
        }
        dev = dev->next;
    }

    if (device_mutex) {
        lisa_mutex_unlock(device_mutex);
    }

    return NULL;
}

/**
 * @brief 内部设备注册函数
 * @note 仅在 lisa_device_init 中使用
 */
static int register_device_internal(lisa_device_t *device)
{
    if (!is_device_valid(device)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 检查是否已存在 */
    if (find_device_by_name(device->name)) {
        return LISA_DEVICE_ERR_EXISTS;
    }

    /* 添加到链表头 */
    device->next = device_list_head;
    device_list_head = device;
    device_count++;

    /* 初始化设备状态 */
    device->state = LISA_DEVICE_STATE_UNINITIALIZED;
    memset(&device->stats, 0, sizeof(device->stats));

    return LISA_DEVICE_OK;
}

/* ========================================================================
 * 设备管理接口实现
 * ======================================================================== */

int lisa_device_init(void)
{
    if (manager_initialized) {
        return device_count;
    }

    /* 初始化锁 */
    device_mutex = lisa_mutex_create();
    if (device_mutex == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return -1;
    }

    lisa_device_registry_entry_t *entry_start = __lisa_device_registry_start;
    lisa_device_registry_entry_t *entry_end = __lisa_device_registry_end;

    /* 计算设备数量 */
    size_t count = entry_end - entry_start;

    if (count == 0) {
        manager_initialized = true;
        return 0;
    }

    /* 注册所有设备 */
    int registered = 0;
    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &entry_start[i];

        if (!entry->device) {
            continue;
        }

        /* 初始化函数必须存在 */
        if (!entry->init_fn) {
            /* 没有初始化函数，不符合规范，跳过 */
            continue;
        }

        /* 先注册设备到管理器（状态为 UNINITIALIZED） */
        if (register_device_internal(entry->device) != LISA_DEVICE_OK) {
            continue;
        }

        /* 记录初始化开始时间 */
        uint64_t start_time = lisa_os_get_tick_ms();
        /* 调用初始化函数 */
        int init_ret = entry->init_fn();
        /* 记录初始化结束时间和耗时 */
        uint64_t end_time = lisa_os_get_tick_ms();
        entry->device->stats.init_timestamp = (uint32_t)end_time;
        entry->device->stats.init_time = (uint32_t)(end_time - start_time);

        /* 记录初始化返回值到统计信息 */
        entry->device->stats.init_result = init_ret;

        /* 根据返回值设置状态 */
        if (init_ret == 0) {
            /* 初始化成功 */
            entry->device->state = LISA_DEVICE_STATE_INITIALIZED;
        } else {
            /* 初始化失败，标记为错误状态 */
            entry->device->state = LISA_DEVICE_STATE_ERROR;
        }

        registered++;
    }

    manager_initialized = true;
    return registered;
}

lisa_device_t *lisa_device_get(const char *name)
{
    lisa_device_t *dev = find_device_by_name(name);

    /* 统计引用次数：每次成功获取设备时，引用计数 +1 */
    if (dev) {
        if (device_mutex) {
            lisa_mutex_lock(device_mutex, LISA_WAIT_FOREVER);
        }
        dev->stats.ref_count++;
        if (device_mutex) {
            lisa_mutex_unlock(device_mutex);
        }
    }

    return dev;
}

bool lisa_device_ready(const lisa_device_t *dev)
{
    if (dev == NULL) {
        /* 设备指针无效 */
        return false;
    }

    if (dev->state != LISA_DEVICE_STATE_INITIALIZED) {
        /* 设备未就绪（未初始化或处于错误状态） */
        return false;
    }

    return true;
}

/* ========================================================================
 * 查询接口实现
 * ======================================================================== */

uint32_t lisa_device_get_count(void)
{
    return device_count;
}

void lisa_device_get_stats(const lisa_device_t *dev, lisa_device_stats_t *stats)
{
    if (!is_device_valid(dev) || !stats) {
        return;
    }

    memcpy(stats, &dev->stats, sizeof(lisa_device_stats_t));
}

void lisa_device_reset_stats(lisa_device_t *dev)
{
    if (!is_device_valid(dev)) {
        return;
    }

    memset(&dev->stats, 0, sizeof(lisa_device_stats_t));
}

/* ========================================================================
 * 遍历接口实现
 * ======================================================================== */

int lisa_device_foreach(lisa_device_iterator_cb callback, void *user_data)
{
    if (!callback) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (device_mutex) {
        lisa_mutex_lock(device_mutex, LISA_WAIT_FOREVER);
    }

    lisa_device_t *dev = device_list_head;
    int count = 0;

    while (dev && callback) {
        count++; /* 先计数当前设备 */
        int ret = callback(dev, user_data);
        if (ret != 0) {
            break; /* 回调返回非0，停止遍历 */
        }
        dev = dev->next;
    }

    if (device_mutex) {
        lisa_mutex_unlock(device_mutex);
    }

    return count;
}
