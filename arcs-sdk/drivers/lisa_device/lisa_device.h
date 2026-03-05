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
 * 设备优先级定义
 * ========================================================================
 *
 * 优先级范围：0-99（数值越小优先级越高）
 *
 * 推荐使用预定义的优先级常量，如需自定义请确保在 0-99 范围内
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

    /* ===== 链表节点 (用于设备管理) ===== */
    struct lisa_device *next;
} lisa_device_t;

/**
 * @brief 设备注册条目结构
 */
typedef struct {
    lisa_device_t *device; /* 设备指针 */
    int (*init_fn)(void);  /* 可选的初始化函数 */
    uint32_t priority;     /* 初始化优先级 (数字越小越先初始化) */
} lisa_device_registry_entry_t;

/* ===== 段属性定义 ===== */
#define LISA_DEVICE_SECTION(x) __attribute__((used, section(".lisa_device_registry." #x)))

/**
 * @brief 静态设备注册宏 - 统一声明和注册设备实例
 *
 * @param _name 设备名称标识符 (如 gpioa，用于生成变量名和设备名称字符串)
 * @param _api_ptr 设备 API 结构体指针
 * @param _priv_data_ptr 私有数据指针
 * @param _user_data_ptr 用户数据指针 (可选，传 NULL)
 * @param _init_fn 初始化函数（必须提供，返回0表示成功）
 * @param _priority 初始化优先级 (数值越小优先级越高，范围：0-99)
 *
 * @note 优先级必须在 0-99 范围内，超出范围将导致编译错误
 * @note 推荐使用预定义常量：LISA_DEVICE_PRIORITY_CRITICAL/HIGH/NORMAL/LOW/LOWEST
 *
 */
#define LISA_DEVICE_REGISTER(_name, _api_ptr, _priv_data_ptr, _user_data_ptr, _init_fn, _priority)                     \
    typedef char __priority_range_check_##_name                                                                        \
        [((_priority) >= LISA_DEVICE_PRIORITY_CRITICAL && (_priority) <= LISA_DEVICE_PRIORITY_LOWEST) ? 1 : -1];       \
    static lisa_device_t __lisa_device_instance_##_name = {                                                            \
        .name = #_name,                                                                                                \
        .state = LISA_DEVICE_STATE_UNINITIALIZED,                                                                      \
        .stats = {0},                                                                                                  \
        .api = (void *)(_api_ptr),                                                                                     \
        .priv_data = (_priv_data_ptr),                                                                                 \
        .user_data = (_user_data_ptr),                                                                                 \
        .next = NULL,                                                                                                  \
    };                                                                                                                 \
    static const lisa_device_registry_entry_t __lisa_device_registry_##_name LISA_DEVICE_SECTION(_priority) = {        \
        .device = &__lisa_device_instance_##_name,                                                                     \
        .init_fn = (_init_fn),                                                                                         \
        .priority = (_priority),                                                                                       \
    }

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
 * @brief 初始化设备管理器
 *
 * 自动扫描并注册所有通过 LISA_DEVICE_REGISTER 宏定义的设备
 *
 * @return 成功注册的设备数量, 负数表示错误
 * @note 应在系统初始化早期调用，在使用任何设备之前
 */
int lisa_device_init(void);

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
