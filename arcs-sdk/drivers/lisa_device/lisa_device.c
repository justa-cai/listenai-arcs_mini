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
#if CONFIG_LISA_PM
extern lisa_device_pm_registry_entry_t __lisa_device_pm_registry_start[];
extern lisa_device_pm_registry_entry_t __lisa_device_pm_registry_end[];
#endif

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

#if CONFIG_LISA_PM
static const lisa_device_pm_t *find_attached_pm(const lisa_device_t *device)
{
    lisa_device_pm_registry_entry_t *entry_start = __lisa_device_pm_registry_start;
    lisa_device_pm_registry_entry_t *entry_end = __lisa_device_pm_registry_end;

    for (lisa_device_pm_registry_entry_t *entry = entry_start; entry < entry_end; ++entry) {
        if (entry->device == device &&
            (entry->pm.system_ops != NULL || entry->pm.wakeup_ops != NULL)) {
            return &entry->pm;
        }
    }

    return NULL;
}

static void bind_attached_pm(lisa_device_t *device)
{
    const lisa_device_pm_t *attached_pm = find_attached_pm(device);

    if (attached_pm != NULL) {
        device->pm = attached_pm;
    }
}
#endif

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

#if CONFIG_LISA_PM
    /* 让 LISA_DEVICE_PM_ATTACH 声明的 wakeup_ops 对设备快速路径可见。 */
    bind_attached_pm(device);
#endif

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
static bool post_kernel_initialized = false;

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

        if (entry->init_level != LISA_DEVICE_LEVEL_NORMAL) {
            continue;
        }

        if (init_device_entry(entry, true) == 0) {
            registered++;
        }
    }

    manager_initialized = true;
    return device_count;
}

int lisa_device_post_kernel_init(void)
{
    if (post_kernel_initialized) {
        return device_count;
    }

    if (!manager_initialized) {
        (void)lisa_device_init();
    }

    lisa_device_registry_entry_t *entry_start = __lisa_device_registry_start;
    lisa_device_registry_entry_t *entry_end = __lisa_device_registry_end;
    size_t count = entry_end - entry_start;

    for (size_t i = 0; i < count; i++) {
        lisa_device_registry_entry_t *entry = &entry_start[i];

        if (entry->init_level != LISA_DEVICE_LEVEL_POST_KERNEL) {
            continue;
        }

        (void)init_device_entry(entry, true);
    }

    post_kernel_initialized = true;
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

/**
 * @brief 在注册段中查找设备对应的注册条目
 * @note 注册段在编译期生成、运行期只读，无需加锁
 */
static lisa_device_registry_entry_t *find_registry_entry(const lisa_device_t *dev)
{
    lisa_device_registry_entry_t *entry_start = __lisa_device_registry_start;
    lisa_device_registry_entry_t *entry_end = __lisa_device_registry_end;

    for (lisa_device_registry_entry_t *entry = entry_start; entry < entry_end; ++entry) {
        if (entry->device == dev) {
            return entry;
        }
    }

    return NULL;
}

void lisa_device_destroy(lisa_device_t *dev)
{
    if (!is_device_valid(dev)) {
        return;
    }

    /* 已处于上电初始状态：幂等空操作，避免对驱动重复执行 deinit */
    if (dev->state == LISA_DEVICE_STATE_UNINITIALIZED) {
        return;
    }

#if CONFIG_LISA_PM
    /* 若该设备当前作为活跃唤醒源，先撤销硬件唤醒配置与框架状态记录 */
    if (lisa_device_wakeup_is_enabled(dev)) {
        lisa_device_wakeup_enable(dev, false);
    }
#endif

    /* 调用驱动自定义 deinit，释放软硬件资源、恢复芯片上电初始状态。
     * 与 init_fn 一致，不持有 device_mutex，避免驱动 deinit 内部访问设备框架时死锁。 */
    lisa_device_registry_entry_t *entry = find_registry_entry(dev);
    if (entry && entry->deinit_fn) {
        int deinit_ret = entry->deinit_fn();
        if (deinit_ret != 0) {
            LISA_LOGW(LOG_TAG, "device %s deinit returned %d", dev->name, deinit_ret);
        }
    }

    /* 复位框架状态：保留注册条目，仅回到未初始化状态并清空统计信息 */
    dev->state = LISA_DEVICE_STATE_UNINITIALIZED;
    memset(&dev->stats, 0, sizeof(dev->stats));
}

int lisa_device_reinit(lisa_device_t *dev)
{
    if (!is_device_valid(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 已初始化：幂等返回，避免对已建资源二次 init 导致泄漏 */
    if (dev->state == LISA_DEVICE_STATE_INITIALIZED) {
        return LISA_DEVICE_OK;
    }

    lisa_device_registry_entry_t *entry = find_registry_entry(dev);
    if (entry == NULL || entry->init_fn == NULL) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    /* 与 init_fn 一致：不持有 device_mutex，避免驱动 init 内部访问设备框架时死锁 */
    uint64_t start_time = lisa_os_get_tick_ms();
    int init_ret = entry->init_fn();
    uint64_t end_time = lisa_os_get_tick_ms();

    dev->stats.init_timestamp = (uint32_t)end_time;
    dev->stats.init_time = (uint32_t)(end_time - start_time);
    dev->stats.init_result = init_ret;

    if (init_ret == 0) {
        dev->state = LISA_DEVICE_STATE_INITIALIZED;
        return LISA_DEVICE_OK;
    }

    dev->state = LISA_DEVICE_STATE_ERROR;
    return LISA_DEVICE_ERR_INIT_FAIL;
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

#if CONFIG_LISA_PM
int lisa_device_pm_foreach(lisa_device_pm_iterator_cb callback, void *user_data)
{
    if (callback == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_device_pm_registry_entry_t *entry_start = __lisa_device_pm_registry_start;
    lisa_device_pm_registry_entry_t *entry_end = __lisa_device_pm_registry_end;
    int count = 0;

    for (lisa_device_pm_registry_entry_t *entry = entry_start; entry < entry_end; ++entry) {
        if (entry->device == NULL ||
            (entry->pm.system_ops == NULL && entry->pm.wakeup_ops == NULL)) {
            continue;
        }

        count++;
        if (callback(entry->device, &entry->pm, user_data) != 0) {
            break;
        }
    }

    return count;
}

/* ========================================================================
 * Device-side wakeup-source 实现
 *
 * 维护 per-device enabled 状态的静态节点池。容量小（典型 1-3 个 device），
 * 用静态池避免 heap 依赖；wakeup 配置是启动期一次性行为，并发场景罕见，
 * 沿用 lisa_device 模块在快速路径外的轻量惯例，不加额外锁。
 * ======================================================================== */

#ifndef CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE
#define CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE 8
#endif

/* used_mask 是 uint32_t 位图，池容量上限受其位宽约束 */
_Static_assert(CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE > 0 &&
               CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE <= 32,
               "CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE must be in [1, 32]");

typedef struct lisa_device_wakeup_state_node {
    lisa_device_t *dev;
    struct lisa_device_wakeup_state_node *next;
} lisa_device_wakeup_state_node_t;

static lisa_device_wakeup_state_node_t s_wakeup_state_pool[CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE];
static uint32_t s_wakeup_state_pool_used_mask = 0;
static lisa_device_wakeup_state_node_t *s_wakeup_enabled_head = NULL;

static lisa_device_wakeup_state_node_t *lisa_device_wakeup_state_alloc(void)
{
    for (uint32_t i = 0; i < CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE; ++i) {
        if (!(s_wakeup_state_pool_used_mask & (1U << i))) {
            s_wakeup_state_pool_used_mask |= (1U << i);
            s_wakeup_state_pool[i].dev = NULL;
            s_wakeup_state_pool[i].next = NULL;
            return &s_wakeup_state_pool[i];
        }
    }
    return NULL;
}

static void lisa_device_wakeup_state_free(lisa_device_wakeup_state_node_t *node)
{
    uint32_t idx = (uint32_t)(node - s_wakeup_state_pool);
    if (idx < CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE) {
        s_wakeup_state_pool_used_mask &= ~(1U << idx);
    }
}

bool lisa_device_wakeup_is_capable(lisa_device_t *dev)
{
    if (dev == NULL) {
        return false;
    }
    if (dev->pm == NULL) {
        return false;
    }
    if (dev->pm->wakeup_ops == NULL) {
        return false;
    }
    return true;
}

bool lisa_device_wakeup_is_enabled(lisa_device_t *dev)
{
    if (!lisa_device_wakeup_is_capable(dev)) {
        return false;
    }
    for (lisa_device_wakeup_state_node_t *p = s_wakeup_enabled_head; p != NULL; p = p->next) {
        if (p->dev == dev) {
            return true;
        }
    }
    return false;
}

int32_t lisa_device_wakeup_enable(lisa_device_t *dev, bool enable)
{
    if (dev == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }
    if (!lisa_device_wakeup_is_capable(dev)) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    const lisa_pm_wakeup_ops_t *ops = dev->pm->wakeup_ops;
    bool currently_enabled = lisa_device_wakeup_is_enabled(dev);

    if (enable && !currently_enabled) {
        int32_t ret = (ops->set_enabled != NULL) ? ops->set_enabled(dev, true) : 0;
        if (ret != 0) {
            return ret;
        }
        lisa_device_wakeup_state_node_t *node = lisa_device_wakeup_state_alloc();
        if (node == NULL) {
            /* 池满：已下发硬件但记不下状态，撤回保持一致 */
            if (ops->set_enabled != NULL) {
                ops->set_enabled(dev, false);
            }
            return LISA_DEVICE_ERR_NO_MEM;
        }
        node->dev = dev;
        node->next = s_wakeup_enabled_head;
        s_wakeup_enabled_head = node;
        return 0;
    }
    if (!enable && currently_enabled) {
        int32_t ret = (ops->set_enabled != NULL) ? ops->set_enabled(dev, false) : 0;
        if (ret != 0) {
            return ret;
        }
        lisa_device_wakeup_state_node_t **pp = &s_wakeup_enabled_head;
        while (*pp != NULL) {
            if ((*pp)->dev == dev) {
                lisa_device_wakeup_state_node_t *victim = *pp;
                *pp = (*pp)->next;
                lisa_device_wakeup_state_free(victim);
                break;
            }
            pp = &(*pp)->next;
        }
        return 0;
    }
    return 0;  /* 目标状态已达成，幂等 */
}
#endif /* CONFIG_LISA_PM */

/* ========================================================================
 * 通过 SYS_INIT 自动注册设备初始化到系统启动流程
 * ======================================================================== */

SYS_INIT(lisa_device_early_init, SYS_INIT_LEVEL_PRE_SYSTEM_INIT, SYS_INIT_SUB_PRIORITY_EARLY);
SYS_INIT(lisa_device_init, SYS_INIT_LEVEL_PRE_KERNEL, SYS_INIT_SUB_PRIORITY_FIRST);
SYS_INIT(lisa_device_post_kernel_init, SYS_INIT_LEVEL_POST_KERNEL, SYS_INIT_SUB_PRIORITY_FIRST);
