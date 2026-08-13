/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_device.h
 * @brief LISA 设备框架 - 设备基类定义
 *
 * 提供统一的设备抽象层，支持多种设备类型的注册、管理和操作
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 统一错误码定义 ===== */
#define LISA_DEVICE_OK              0   /* 成功 */
#define LISA_DEVICE_ERR_INVALID     -1  /* 无效参数 */
#define LISA_DEVICE_ERR_NOT_FOUND   -2  /* 设备未找到 */
#define LISA_DEVICE_ERR_EXISTS      -3  /* 设备已存在 */
#define LISA_DEVICE_ERR_NO_MEM      -4  /* 内存不足 */
#define LISA_DEVICE_ERR_INIT_FAIL   -5  /* 初始化失败 */
#define LISA_DEVICE_ERR_NOT_SUPPORT -6  /* 不支持的操作 */
#define LISA_DEVICE_ERR_TIMEOUT     -7  /* 超时 */
#define LISA_DEVICE_ERR_BUSY        -8  /* 设备忙 */
#define LISA_DEVICE_ERR_NOT_READY   -9  /* 设备未就绪 */
#define LISA_DEVICE_ERR_IO          -10 /* IO 错误 */
#define LISA_DEVICE_ERR_RANGE       -11 /* 参数超出范围 */
#define LISA_DEVICE_ERR_OVERFLOW    -12 /* 溢出错误 */
#define LISA_DEVICE_ERR_NACK        -13 /* NACK 错误（I2C等总线协议） */
/* ========================================================================
 * 设备初始化级别定义（与 sys_init.h 保持一致）
 * ========================================================================
 *
 * 定义设备在系统启动流程中的初始化阶段：
 *
 * - EARLY:   在 heap/RTOS 之前初始化（无锁、无日志），适用于日志串口等关键设备
 * - NORMAL:  在 lisa_device_init() 中正常初始化（heap 可用、日志可用）
 * - POST_KERNEL: 在 RTOS scheduler 启动后初始化（可使用任务/IPC worker）
 *
 * ======================================================================== */

#define LISA_DEVICE_LEVEL_EARLY       0  /* 早期初始化 - PRE_SYSTEM_INIT 阶段，无锁无日志 */
#define LISA_DEVICE_LEVEL_NORMAL      1  /* 正常初始化 - PRE_KERNEL 阶段 */
#define LISA_DEVICE_LEVEL_POST_KERNEL 2  /* RTOS 启动后初始化 - POST_KERNEL 阶段 */

/* ========================================================================
 * 设备优先级定义（同一级别内的子优先级）
 * ========================================================================
 *
 * 优先级范围：0-99（数值越小优先级越高）
 * 控制同一初始化级别内的设备初始化顺序
 * ======================================================================== */

#define LISA_DEVICE_PRIORITY_CRITICAL 0  /* 最高优先级 - 关键系统设备 */
#define LISA_DEVICE_PRIORITY_HIGH     10 /* 高优先级 - 重要外设 */
#define LISA_DEVICE_PRIORITY_NORMAL   50 /* 默认优先级 - 普通设备 */
#define LISA_DEVICE_PRIORITY_LOW      90 /* 低优先级 - 非关键设备 */
#define LISA_DEVICE_PRIORITY_LOWEST   99 /* 最低优先级 - 可选功能 */

/**
 * @brief 设备状态枚举
 */
typedef enum {
    LISA_DEVICE_STATE_UNINITIALIZED = 0, /* 未初始化 */
    LISA_DEVICE_STATE_INITIALIZED,       /* 已初始化 */
    LISA_DEVICE_STATE_ERROR,             /* 错误状态 */
} lisa_device_state_t;

/**
 * @brief 设备统计信息
 */
typedef struct {
    uint32_t ref_count;      /* 引用次数 */
    int init_result;         /* 初始化函数返回值 (0=成功, 负数=错误码) */
    uint32_t init_time;      /* 初始化时间 (ms) */
    uint32_t init_timestamp; /* 初始化时间戳 (ms) */
} lisa_device_stats_t;

#if CONFIG_LISA_PM
/* forward declaration: lisa_device_t typedef 在文件下方，wakeup_ops callback
 * 需要先看到 struct lisa_device 这个名字才能避免参数列表局部声明。 */
struct lisa_device;

/**
 * @brief Device-side wakeup-source 能力 vtable
 *
 * wakeup_ops 描述“设备如何作为唤醒源”。应用或驱动先通过设备自己的
 * configure API 设置唤醒条件，再由 lisa_device_wakeup_enable() 调用
 * set_enabled() 统一下发或撤销硬件唤醒配置。
 *
 * wakeup_ops 不参与系统是否允许睡眠的决策，也不负责睡前挂起/唤醒后恢复；
 * 这些动作由 system_ops 负责。sub_idx / trigger 由 driver 自行解释
 * （GPIO: sub_idx=pin, trigger=lisa_gpio_wakeup_trigger_t）。
 *
 * 强一致规则：configure / clear 仅更新 driver 内部缓存，不下发硬件。
 * 硬件下发仅在 set_enabled(true) 发生；set_enabled(false) 撤销所有已下发的配置。
 * 在线变更触发条件必须 set_enabled(false) -> configure -> set_enabled(true)。
 */
typedef struct {
    /**
     * @brief 配置 driver 内部 wakeup 缓存
     *
     * @param dev 设备指针
     * @param sub_idx driver 自定义子索引（GPIO: pin）
     * @param trigger driver 自定义触发条件
     *
     * @return 0 成功
     * @return LISA_DEVICE_ERR_RANGE sub_idx 越界
     * @return LISA_DEVICE_ERR_INVALID trigger 参数非法
     * @return LISA_DEVICE_ERR_NOT_SUPPORT 该 trigger 在当前 SoC 上不支持
     */
    int32_t (*configure)(struct lisa_device *dev, uint32_t sub_idx, uint32_t trigger);

    /**
     * @brief 清除 driver 内部某子索引的 wakeup 缓存
     *
     * @param dev 设备指针
     * @param sub_idx driver 自定义子索引
     *
     * @return 0 成功（含幂等清除未配置的 sub_idx）
     * @return LISA_DEVICE_ERR_RANGE sub_idx 越界
     */
    int32_t (*clear)(struct lisa_device *dev, uint32_t sub_idx);

    /**
     * @brief 将 driver 内部缓存一次性下发到硬件，或撤销已下发的配置
     *
     * 缓存为空时 set_enabled(true) 约定返回 0（幂等空启用），不下发任何 HAL 调用。
     *
     * @param dev 设备指针
     * @param enable true=下发并启用 / false=禁用并撤销
     *
     * @return 0 成功（含幂等空启用）
     * @return <0 底层 HAL 调用失败
     */
    int32_t (*set_enabled)(struct lisa_device *dev, bool enable);
} lisa_pm_wakeup_ops_t;

/**
 * @brief 设备 PM 描述
 *
 * 可由兼容 PM 注册宏挂到 lisa_device_t::pm，也可由 LISA_DEVICE_PM_ATTACH
 * 作为独立 PM 能力条目声明。两个 ops 槽位职责独立，NULL 表示不支持该能力。
 */
typedef struct {
    /* System PM: 睡眠决策、睡前挂起、唤醒后基础恢复。NULL 表示设备不参与 system PM 调度。 */
    const lisa_pm_system_ops_t *system_ops;
    /* Wakeup-source: 唤醒条件缓存和硬件唤醒源启停。NULL 表示设备不能作为 wakeup-source。 */
    const lisa_pm_wakeup_ops_t *wakeup_ops;
    /* PM 回调上下文，通常填设备私有数据指针。system_ops 使用该 ctx；wakeup_ops 直接接收 lisa_device_t。 */
    void *ctx;
} lisa_device_pm_t;

/**
 * @brief 独立 PM 能力注册条目
 *
 * 用于把设备本体注册与可选 PM 能力解耦。设备仍通过普通
 * LISA_DEVICE_REGISTER / LISA_DEVICE_REGISTER_DEINIT 注册，PM 能力通过
 * LISA_DEVICE_PM_ATTACH 单独放入链接器段，供 lisa_pm 初始化时发现。
 */
typedef struct {
    struct lisa_device *device;
    lisa_device_pm_t pm;
} lisa_device_pm_registry_entry_t;

typedef int (*lisa_device_pm_iterator_cb)(struct lisa_device *dev,
                                          const lisa_device_pm_t *pm,
                                          void *user_data);
#endif

/**
 * @brief 设备结构
 */
typedef struct lisa_device {
    /* ===== 设备标识 ===== */
    const char *name; /* 设备名称 (唯一标识) */

    /* ===== 设备状态 ===== */
    lisa_device_state_t state; /* 当前状态 */
    lisa_device_stats_t stats; /* 统计信息 */

    /* ===== 设备 API ===== */
    void *api; /* 设备专用 API (由具体设备定义) */

    /* ===== 私有数据 ===== */
    void *priv_data; /* 设备私有数据 */
    void *user_data; /* 用户自定义数据 */

#if CONFIG_LISA_PM
    /* ===== 可选 PM 能力 ===== */
    const lisa_device_pm_t *pm; /* 设备 system PM 描述 */
#endif

    /* ===== 链表节点 (用于设备管理) ===== */
    struct lisa_device *next;
} lisa_device_t;

/**
 * @brief 设备注册条目结构
 */
typedef struct {
    lisa_device_t *device;  /* 设备指针 */
    int (*init_fn)(void);   /* 可选的初始化函数 */
    int (*deinit_fn)(void); /* 可选的反初始化函数：停止并释放驱动软硬件资源，恢复芯片上电初始状态 */
    uint8_t init_level;     /* 初始化级别 (LISA_DEVICE_LEVEL_EARLY/NORMAL) */
    uint32_t priority;      /* 同级别内子优先级 (数字越小越先初始化，0-99) */
} lisa_device_registry_entry_t;

/* ===== 段属性定义 ===== */
#define LISA_DEVICE_SECTION(x) __attribute__((used, section(".lisa_device_registry." #x)))
#if CONFIG_LISA_PM
#define LISA_DEVICE_PM_SECTION(_name) __attribute__((used, section(".lisa_device_pm_registry." #_name)))
#endif

#if CONFIG_LISA_PM
#define LISA_DEVICE_PM_INIT(_pm_ptr) .pm = (_pm_ptr),
#else
#define LISA_DEVICE_PM_INIT(_pm_ptr)
#endif

#define LISA_DEVICE_REGISTER_COMMON(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority, _pm_ptr, _deinit_fn) \
    typedef char __priority_range_check_##_name                                                                        \
        [((_priority) >= LISA_DEVICE_PRIORITY_CRITICAL && (_priority) <= LISA_DEVICE_PRIORITY_LOWEST) ? 1 : -1];       \
    static lisa_device_t __lisa_device_instance_##_name = {                                                            \
        .name = #_name,                                                                                                \
        .state = LISA_DEVICE_STATE_UNINITIALIZED,                                                                      \
        .stats = {0},                                                                                                  \
        .api = (void *)(_api_ptr),                                                                                     \
        .priv_data = (_priv_data_ptr),                                                                                 \
        .user_data = (_user_data_ptr),                                                                                 \
        LISA_DEVICE_PM_INIT(_pm_ptr)                                                                                   \
        .next = NULL,                                                                                                  \
    };                                                                                                                 \
    static const lisa_device_registry_entry_t __lisa_device_registry_##_name LISA_DEVICE_SECTION(_priority) = {        \
        .device = &__lisa_device_instance_##_name,                                                                     \
        .init_fn = (_init_fn),                                                                                         \
        .deinit_fn = (_deinit_fn),                                                                                     \
        .init_level = (_level),                                                                                        \
        .priority = (_priority),                                                                                       \
    }

#define LISA_DEVICE_REGISTER(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority)             \
    LISA_DEVICE_REGISTER_COMMON(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority, NULL, NULL)

/* 带 deinit_fn 的注册：deinit_fn 由 lisa_device_destroy() 调用，负责停止并释放该设备
 * 驱动的软硬件资源，使其恢复到芯片上电初始状态。参数顺序为 init_fn 后紧跟 deinit_fn。 */
#define LISA_DEVICE_REGISTER_DEINIT(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _deinit_fn, _level, _priority) \
    LISA_DEVICE_REGISTER_COMMON(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority, NULL, _deinit_fn)

#if CONFIG_LISA_PM
#define LISA_DEVICE_PM_ATTACH(_name, _system_ops, _wakeup_ops, _ctx)                                                   \
    static const lisa_device_pm_registry_entry_t __lisa_device_pm_registry_##_name LISA_DEVICE_PM_SECTION(_name) = {   \
        .device = &__lisa_device_instance_##_name,                                                                     \
        .pm = {                                                                                                        \
            .system_ops = (_system_ops),                                                                               \
            .wakeup_ops = (_wakeup_ops),                                                                               \
            .ctx = (_ctx),                                                                                             \
        },                                                                                                             \
    }

#define LISA_DEVICE_REGISTER_PM(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority,           \
                                _system_ops, _wakeup_ops)                                                              \
    static const lisa_device_pm_t __lisa_device_pm_##_name = {                                                         \
        .system_ops = (_system_ops),                                                                                   \
        .wakeup_ops = (_wakeup_ops),                                                                                   \
        .ctx        = (_priv_data_ptr),                                                                                \
    };                                                                                                                 \
    LISA_DEVICE_REGISTER_COMMON(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority, &__lisa_device_pm_##_name, NULL)

#define LISA_DEVICE_REGISTER_PM_DEINIT(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _deinit_fn, _level,   \
                                       _priority, _system_ops, _wakeup_ops)                                            \
    static const lisa_device_pm_t __lisa_device_pm_##_name = {                                                         \
        .system_ops = (_system_ops),                                                                                   \
        .wakeup_ops = (_wakeup_ops),                                                                                   \
        .ctx        = (_priv_data_ptr),                                                                                \
    };                                                                                                                 \
    LISA_DEVICE_REGISTER_COMMON(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority, &__lisa_device_pm_##_name, _deinit_fn)
#else
#define LISA_DEVICE_PM_ATTACH(_name, _system_ops, _wakeup_ops, _ctx)

#define LISA_DEVICE_REGISTER_PM(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority,           \
                                _system_ops, _wakeup_ops)                                                              \
    LISA_DEVICE_REGISTER(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _level, _priority)

#define LISA_DEVICE_REGISTER_PM_DEINIT(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _deinit_fn, _level,   \
                                       _priority, _system_ops, _wakeup_ops)                                            \
    LISA_DEVICE_REGISTER_DEINIT(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _deinit_fn, _level, _priority)
#endif

/* ===== 设备操作辅助函数 ===== */

/**
 * @brief 获取设备状态
 * @param dev 设备指针
 * @return 设备状态
 */
static inline lisa_device_state_t lisa_device_get_state(const lisa_device_t *dev)
{
    return dev ? dev->state : LISA_DEVICE_STATE_UNINITIALIZED;
}

/**
 * @brief 增加引用计数
 * @param dev 设备指针
 */
static inline void lisa_device_inc_ref_count(lisa_device_t *dev)
{
    if (dev) {
        dev->stats.ref_count++;
    }
}

/**
 * @brief 检查设备是否已初始化
 * @param dev 设备指针
 * @return true 已初始化, false 未初始化或错误
 */
static inline bool lisa_device_is_initialized(const lisa_device_t *dev)
{
    return dev && (dev->state == LISA_DEVICE_STATE_INITIALIZED);
}

/* ========================================================================
 * 设备管理接口
 * ======================================================================== */

/**
 * @brief 早期设备初始化（无锁，在 heap/RTOS 之前调用）
 *
 * 仅初始化 LISA_DEVICE_LEVEL_EARLY 级别的设备，不创建 mutex，不使用日志系统。
 * 适用于需要在系统早期就绑定到设备框架的关键设备（如日志串口）。
 *
 * @return 成功初始化的设备数量, 负数表示错误
 * @note 必须在 lisa_device_init() 之前调用
 */
int lisa_device_early_init(void);

/**
 * @brief 初始化设备管理器
 *
 * 初始化所有 LISA_DEVICE_LEVEL_NORMAL 级别的设备（跳过已由 early_init 处理的）
 *
 * @return 成功注册的设备数量, 负数表示错误
 * @note 应在 sysheap_init() 之后调用
 */
int lisa_device_init(void);

/**
 * @brief 初始化所有 LISA_DEVICE_LEVEL_POST_KERNEL 级别的设备
 *
 * @return 当前设备总数
 *
 * @note 在 RTOS scheduler 启动后调用，用于需要任务/IPC worker 的设备。
 */
int lisa_device_post_kernel_init(void);

/**
 * @brief 通过名称获取设备
 *
 * 获取设备并自动增加引用计数
 *
 * @param name 设备名称
 * @return 设备指针, NULL 表示未找到
 * @note 获取设备后建议使用 lisa_device_ready() 检查设备状态
 */
lisa_device_t *lisa_device_get(const char *name);

/**
 * @brief 检查设备是否就绪可用
 *
 * 检查设备是否已成功初始化（状态为 INITIALIZED）
 *
 * @param dev 设备指针
 * @return true 设备就绪, false 设备未就绪或参数无效
 * @note 推荐在使用设备前调用此接口进行检查
 */
bool lisa_device_ready(const lisa_device_t *dev);

/**
 * @brief 销毁设备：停止并释放该设备驱动的软硬件资源，恢复芯片上电初始状态
 *
 * 这是设备框架的通用运行期生命周期接口，不依赖 CONFIG_LISA_PM。它可用于
 * 低功耗场景，也可用于普通运行期的设备热释放、错误恢复或资源重建。
 *
 * 调用设备注册时提供的 deinit_fn（通过 LISA_DEVICE_REGISTER_DEINIT /
 * LISA_DEVICE_REGISTER_PM_DEINIT 挂载），由驱动负责关闭外设时钟、注销中断、
 * 释放运行期申请的软件资源等，使硬件回到上电初始态。
 *
 * 框架侧行为：
 * - CONFIG_LISA_PM=y 且设备当前作为活跃唤醒源时，先撤销唤醒配置与状态记录；
 * - 调用 deinit_fn（若驱动未提供则仅复位框架状态）；
 * - 将设备状态复位为 LISA_DEVICE_STATE_UNINITIALIZED 并清空统计信息；
 * - 设备仍保留在管理器注册表中，后续可重新初始化。
 *
 * 幂等：设备已处于 UNINITIALIZED 状态时调用为空操作。
 *
 * @param dev 设备指针；为 NULL 或无效时为空操作
 * @note deinit_fn 在不持有设备管理锁的情况下调用，约束与 init_fn 一致。
 */
void lisa_device_destroy(lisa_device_t *dev);

/**
 * @brief 重新初始化设备：再次调用注册时的 init_fn，恢复到 init_fn 执行后的状态
 *
 * 与 lisa_device_destroy() 对称，用于设备被 destroy 后的通用运行期重建。重新执行驱动注册时
 * 提供的 init_fn（重新分配 OS 资源、重配硬件等），并把框架状态恢复为
 * LISA_DEVICE_STATE_INITIALIZED。
 *
 * 典型用法（与 PM 配合）：应用在“正常任务上下文”中睡眠前调用
 * lisa_device_destroy() 释放设备资源；唤醒后在 PM after_wake 回调（同样是正常任务
 * 上下文，例如 lisa_pm 的 after_wake 任务）中调用本接口完成重建。
 *
 * @warning 严禁在关中断 / 调度器停摆的上下文（如 PM system_ops 的
 *          prepare_suspend / resume_restore，二者运行于 HAL __disable_irq() 临界区）
 *          中调用本接口或 lisa_device_destroy()：init_fn / deinit_fn 通常会创建或删除
 *          FreeRTOS 对象与堆内存，在该上下文中会触发断言或死机。
 *
 * 幂等：设备已处于 INITIALIZED 状态时直接返回成功，不重复执行 init_fn。
 *
 * @param dev 设备指针
 *
 * @retval LISA_DEVICE_OK 重新初始化成功（含已初始化的幂等返回）
 * @retval LISA_DEVICE_ERR_INVALID dev 为 NULL 或无效
 * @retval LISA_DEVICE_ERR_NOT_SUPPORT 未找到注册条目或未提供 init_fn
 * @retval LISA_DEVICE_ERR_INIT_FAIL init_fn 返回非 0
 */
int lisa_device_reinit(lisa_device_t *dev);

/* ========================================================================
 * 查询接口
 * ======================================================================== */

/**
 * @brief 获取已注册设备总数
 *
 * @return 设备数量
 */
uint32_t lisa_device_get_count(void);

/**
 * @brief 获取设备统计信息
 *
 * @param dev 设备指针
 * @param stats 统计信息输出缓冲区
 */
void lisa_device_get_stats(const lisa_device_t *dev, lisa_device_stats_t *stats);

/**
 * @brief 重置设备统计信息
 *
 * @param dev 设备指针
 */
void lisa_device_reset_stats(lisa_device_t *dev);

#if CONFIG_LISA_PM
/* ========================================================================
 * Device-side wakeup-source API（与源类型无关的总闸）
 * ======================================================================== */

/**
 * @brief 查询某 device 是否硬件支持作唤醒源
 *
 * 实现等价：dev->pm != NULL && dev->pm->wakeup_ops != NULL
 *
 * @param dev 设备指针
 *
 * @retval true 该 device 已挂载 wakeup_ops vtable
 * @retval false dev 为 NULL、未挂载 PM 描述、或未挂载 wakeup_ops
 */
bool lisa_device_wakeup_is_capable(lisa_device_t *dev);

/**
 * @brief 启用或禁用某 device 作为活跃唤醒源
 *
 * 必须先通过 device 自己的 configure API 配置好触发条件（如
 * lisa_gpio_configure_wakeup）。否则 set_enabled(true) 在 driver 内部
 * 会因 cache 为空返回 0（幂等空启用），实际硬件 wakeup 路径不开通。
 *
 * @param dev 设备指针
 * @param enable true=启用 / false=禁用
 *
 * @retval 0 成功
 * @retval LISA_DEVICE_ERR_INVALID dev 为 NULL
 * @retval LISA_DEVICE_ERR_NOT_SUPPORT 该 device 不支持作唤醒源
 * @retval <0 driver set_enabled 透传的错误
 */
int32_t lisa_device_wakeup_enable(lisa_device_t *dev, bool enable);

/**
 * @brief 查询某 device 当前是否处于"已启用 wakeup"状态
 *
 * 反映 device 框架维护的 per-device 状态，不实时向 driver 查询。
 *
 * @param dev 设备指针
 *
 * @retval true 上一次调用是 enable(true) 且未被 enable(false) 撤销
 * @retval false 其他情况（含 dev 为 NULL、不 capable）
 */
bool lisa_device_wakeup_is_enabled(lisa_device_t *dev);

/**
 * @brief 遍历通过 LISA_DEVICE_PM_ATTACH 声明的 PM 能力
 *
 * @param callback 遍历回调；返回非 0 时停止遍历
 * @param user_data 透传给回调的用户数据
 *
 * @return 遍历到的有效 PM attach 条目数量；参数非法时返回 LISA_DEVICE_ERR_INVALID
 */
int lisa_device_pm_foreach(lisa_device_pm_iterator_cb callback, void *user_data);
#endif /* CONFIG_LISA_PM */

/* ========================================================================
 * 遍历接口
 * ======================================================================== */

/**
 * @brief 设备迭代器回调函数类型
 *
 * @param dev 设备指针
 * @param user_data 用户自定义数据
 * @return 0 继续遍历, 非0 停止遍历
 */
typedef int (*lisa_device_iterator_cb)(lisa_device_t *dev, void *user_data);

/**
 * @brief 遍历所有已注册设备
 *
 * @param callback 回调函数
 * @param user_data 用户数据 (传递给回调)
 * @return 遍历的设备数量
 */
int lisa_device_foreach(lisa_device_iterator_cb callback, void *user_data);

#ifdef __cplusplus
}
#endif
