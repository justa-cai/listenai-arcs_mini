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
#include "sys_init.h"
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

static int init_device_entry(lisa_device_registry_entry_t *entry, bool use_timing)
{
    if (!entry->device || !entry->init_fn) {
        return -1;
    }

    /* 跳过已初始化的设备（由 early_init 处理过的） */
    if (entry->device->state == LISA_DEVICE_STATE_INITIALIZED ||
        entry->device->state == LISA_DEVICE_STATE_ERROR) {
        return -1;
    }

    /* 先注册设备到管理器（状态为 UNINITIALIZED） */
    if (register_device_internal(entry->device) != LISA_DEVICE_OK) {
        return -1;
    }

    /* 调用初始化函数 */
    uint64_t start_time = 0;
    if (use_timing) {
        start_time = lisa_os_get_tick_ms();
    }

    int init_ret = entry->init_fn();

    if (use_timing) {
        uint64_t end_time = lisa_os_get_tick_ms();
        entry->device->stats.init_timestamp = (uint32_t)end_time;
        entry->device->stats.init_time = (uint32_t)(end_time - start_time);
    }

    entry->device->stats.init_result = init_ret;

    if (init_ret == 0) {
        entry->device->state = LISA_DEVICE_STATE_INITIALIZED;
    } else {
        entry->device->state = LISA_DEVICE_STATE_ERROR;
    }

    return 0;
}

static bool early_initialized = false;

int lisa_device_early_init(void)
{
    if (early_initialized) {
        return 0;
    }

    lisa_device_registry_entry_t *entry_start = __lisa_device_registry_start;
    lisa_device_registry_entry_t *entry_end = __lisa_device_registry_end;
    size_t count = entry_end - entry_start;

    int registered = 0;
    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &entry_start[i];

        /* 仅初始化 EARLY 级别的设备 */
        if (entry->init_level != LISA_DEVICE_LEVEL_EARLY) {
            continue;
        }

        if (init_device_entry(entry, false) == 0) {
            registered++;
        }
    }

    early_initialized = true;
    return registered;
}

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
    size_t count = entry_end - entry_start;

    if (count == 0) {
        manager_initialized = true;
        return 0;
    }

    /* 注册并初始化剩余设备（跳过已由 early_init 处理的） */
    int registered = 0;
    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &entry_start[i];

        if (init_device_entry(entry, true) == 0) {
            registered++;
        }
    }

    manager_initialized = true;
    return device_count;
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

/* ========================================================================
 * 通过 SYS_INIT 自动注册设备初始化到系统启动流程
 * ======================================================================== */

SYS_INIT(lisa_device_early_init, SYS_INIT_LEVEL_PRE_SYSTEM_INIT, SYS_INIT_SUB_PRIORITY_EARLY);
SYS_INIT(lisa_device_init, SYS_INIT_LEVEL_PRE_KERNEL, SYS_INIT_SUB_PRIORITY_FIRST);
