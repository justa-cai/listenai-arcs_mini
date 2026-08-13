/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_wdt_venusa.c
 * @brief LISA WDT Venusa 平台适配层
 *
 * 此文件实现 Venusa 芯片平台的看门狗定时器硬件适配
 */

#include "lisa_wdt.h"
#include "Driver_WDT.h"
#include "venusa_ap.h"
#include "PowerManager.h"
#include <stddef.h>
#include <string.h>
#include <lisa_mutex.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "lisa_wdt_venusa"
#include <lisa_log.h>

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                                                        \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_unlock(priv->mutex);                                                                            \
        }                                                                                                              \
    } while (0)

/* ===== WDT 时钟频率定义 ===== */
#define WDT_CLK_32K_HZ 32000UL /* 32K 时钟频率 (Hz) */
#define WDT_WRITE_UNLOCK_KEY 0x5AA5U

#if (BOOT_HARTID == 0)
#define VENUSA_WDT_RESET_CMN_ENABLE() __HAL_PMU_WDT0_RESET_CMN_ENABLE()
#else
#define VENUSA_WDT_RESET_CMN_ENABLE() __HAL_PMU_WDT1_RESET_CMN_ENABLE()
#endif

/* ===== WDT 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                    /* HAL WDT 句柄 */
    lisa_mutex_t *mutex;                  /* 互斥锁 */
    lisa_wdt_config_t config;             /* 当前配置 */
    lisa_wdt_state_t state;               /* 当前状态 */
    lisa_wdt_callback_t callback;         /* 超时回调函数 */
    void *user_data;                      /* 用户数据 */
    HAL_DRIVER_WDT_Clk_Src_t clk_src;     /* 时钟源 */
    HAL_DRIVER_WDT_Int_Time_t int_time;   /* 中断时间配置 */
    HAL_DRIVER_WDT_Rst_Time_t rst_time;   /* 复位时间配置 */
    uint32_t configured_timeout_ms;       /* 已配置的超时时间（毫秒）*/
} lisa_wdt_priv_t;

/* 仅使用 HAL 资源结构的前两个字段（与 HAL 内部定义保持一致） */
typedef struct {
    WDT_RegDef *reg;
    uint32_t irq_num;
} lisa_wdt_hal_res_t;

/* ===== WDT 设备静态实例 ===== */
static lisa_wdt_priv_t wdt0_priv;

/* ===== 内部辅助函数 ===== */

/**
 * @brief HAL中断回调函数
 * 
 * 注意：此函数在中断上下文中执行，应尽量简短快速
 * 不要在中断回调中调用可能阻塞的函数（如mutex操作）
 */
static void wdt_hal_irq_callback(void *workspace)
{
    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)workspace;
    if (!priv) {
        LISA_LOGE(LOG_TAG, "[IRQ] WDT callback: invalid workspace");
        return;
    }

    /* 更新状态为已超时（在中断上下文中，直接赋值是安全的） */
    priv->state = LISA_WDT_STATE_EXPIRED;

    /*
     * 在 LISA 层直接控制寄存器来收敛中断：
     * 1) 清除 WDT 控制寄存器中的 INTEN，避免超时中断重复进入
     * 2) 清除 ECLIC pending，防止当前中断残留再次调度
     * 保留 EN/RSTEN，不影响后续复位阶段。
     */
    lisa_wdt_hal_res_t *hal_res = (lisa_wdt_hal_res_t *)priv->hal_handler;
    if (hal_res && hal_res->reg) {
        uint32_t ctrl = hal_res->reg->REG_CTRL.all;

        hal_res->reg->REG_WREN.all = WDT_WRITE_UNLOCK_KEY;
        hal_res->reg->REG_CTRL.all = (ctrl & ~WDT_CTRL_INTEN_Msk);

        /*
         * 尝试按 W1C 语义清中断状态位；若硬件为只读则该写入会被忽略。
         * 无论是否支持 W1C，后续都会清 ECLIC pending。
         */
        hal_res->reg->REG_ST.all = WDT_ST_INTEXPIRED_Msk;
        (void)hal_res->reg->REG_ST.all;
        clear_IRQ(hal_res->irq_num);
    } else {
        LISA_LOGE(LOG_TAG, "[IRQ] WDT resources unavailable, failed to mask interrupt");
    }

    /* 调用用户回调函数 */
    if (priv->callback) {
        LISA_LOGD(LOG_TAG, "[IRQ] Calling user callback");
        priv->callback(priv->user_data);
        LISA_LOGD(LOG_TAG, "[IRQ] User callback returned");
    } else {
        LISA_LOGW(LOG_TAG, "[IRQ] WDT interrupt triggered but no user callback registered");
    }
}

/**
 * @brief 将毫秒转换为中断时间枚举值
 *
 * @param timeout_ms 超时时间（毫秒）
 * @param clk_src 时钟源
 * @param int_time 输出参数，中断时间枚举值
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_RANGE 超时时间超出范围
 */
static int ms_to_int_time(uint32_t timeout_ms, HAL_DRIVER_WDT_Clk_Src_t clk_src, HAL_DRIVER_WDT_Int_Time_t *int_time)
{
    (void)clk_src;
    uint32_t clk_freq = WDT_CLK_32K_HZ;
    uint64_t clk_period_ns = 1000000000ULL / clk_freq;  /* 时钟周期（纳秒）*/
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;  /* 超时时间（纳秒）*/
    uint64_t clk_count = timeout_ns / clk_period_ns;  /* 需要的时钟周期数 */

    /* 查找大于等于所需周期数的最小 2^N 值（向上取整） */
    /* 这样可以确保实际超时时间不会小于配置的时间 */
    if (clk_count <= (1ULL << 6)) {
        *int_time = hal_driver_wdt_int_time_6;
    } else if (clk_count <= (1ULL << 8)) {
        *int_time = hal_driver_wdt_int_time_8;
    } else if (clk_count <= (1ULL << 10)) {
        *int_time = hal_driver_wdt_int_time_10;
    } else if (clk_count <= (1ULL << 11)) {
        *int_time = hal_driver_wdt_int_time_11;
    } else if (clk_count <= (1ULL << 12)) {
        *int_time = hal_driver_wdt_int_time_12;
    } else if (clk_count <= (1ULL << 13)) {
        *int_time = hal_driver_wdt_int_time_13;
    } else if (clk_count <= (1ULL << 14)) {
        *int_time = hal_driver_wdt_int_time_14;
    } else if (clk_count <= (1ULL << 15)) {
        *int_time = hal_driver_wdt_int_time_15;
    } else if (clk_count <= (1ULL << 17)) {
        *int_time = hal_driver_wdt_int_time_17;
    } else if (clk_count <= (1ULL << 19)) {
        *int_time = hal_driver_wdt_int_time_19;
    } else if (clk_count <= (1ULL << 21)) {
        *int_time = hal_driver_wdt_int_time_21;
    } else if (clk_count <= (1ULL << 23)) {
        *int_time = hal_driver_wdt_int_time_23;
    } else if (clk_count <= (1ULL << 25)) {
        *int_time = hal_driver_wdt_int_time_25;
    } else if (clk_count <= (1ULL << 27)) {
        *int_time = hal_driver_wdt_int_time_27;
    } else if (clk_count <= (1ULL << 29)) {
        *int_time = hal_driver_wdt_int_time_29;
    } else if (clk_count <= (1ULL << 31)) {
        *int_time = hal_driver_wdt_int_time_31;
    } else {
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 计算实际超时时间用于日志 - 需要根据枚举值映射到实际的2^N值 */
    uint64_t actual_clk_count;
    int exp;
    switch ((int)*int_time) {
        case 0: exp = 6; break;   /* int_time_6 */
        case 1: exp = 8; break;   /* int_time_8 */
        case 2: exp = 10; break;  /* int_time_10 */
        case 3: exp = 11; break;  /* int_time_11 */
        case 4: exp = 12; break;  /* int_time_12 */
        case 5: exp = 13; break;  /* int_time_13 */
        case 6: exp = 14; break;  /* int_time_14 */
        case 7: exp = 15; break;  /* int_time_15 */
        case 8: exp = 17; break;  /* int_time_17 */
        case 9: exp = 19; break;  /* int_time_19 */
        case 10: exp = 21; break; /* int_time_21 */
        case 11: exp = 23; break; /* int_time_23 */
        case 12: exp = 25; break; /* int_time_25 */
        case 13: exp = 27; break; /* int_time_27 */
        case 14: exp = 29; break; /* int_time_29 */
        case 15: exp = 31; break; /* int_time_31 */
        default: exp = 6; break;
    }
    actual_clk_count = (1ULL << exp);
    uint64_t actual_timeout_ms = (actual_clk_count * clk_period_ns) / 1000000ULL;
    LISA_LOGD(LOG_TAG, "ms_to_int_time: requested=%lu ms, clk_freq=%lu Hz, clk_count=%llu, selected=int_time_%d (2^%d=%llu cycles, ≈%llu ms)",
              timeout_ms, clk_freq, clk_count, (int)*int_time, exp, actual_clk_count, actual_timeout_ms);

    return LISA_DEVICE_OK;
}

/**
 * @brief 将毫秒转换为复位时间枚举值
 *
 * @param timeout_ms 超时时间（毫秒）
 * @param clk_src 时钟源
 * @param rst_time 输出参数，复位时间枚举值
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_RANGE 超时时间超出范围
 */
static int ms_to_rst_time(uint32_t timeout_ms, HAL_DRIVER_WDT_Clk_Src_t clk_src, HAL_DRIVER_WDT_Rst_Time_t *rst_time)
{
    (void)clk_src;
    uint32_t clk_freq = WDT_CLK_32K_HZ;
    uint64_t clk_period_ns = 1000000000ULL / clk_freq;  /* 时钟周期（纳秒）*/
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;  /* 超时时间（纳秒）*/
    uint64_t clk_count = timeout_ns / clk_period_ns;  /* 需要的时钟周期数 */

    /* 查找大于等于所需周期数的最小 2^N 值（向上取整） */
    /* 这样可以确保实际超时时间不会小于配置的时间 */
    if (clk_count <= (1ULL << 7)) {
        *rst_time = hal_driver_wdt_rst_time_7;
    } else if (clk_count <= (1ULL << 8)) {
        *rst_time = hal_driver_wdt_rst_time_8;
    } else if (clk_count <= (1ULL << 9)) {
        *rst_time = hal_driver_wdt_rst_time_9;
    } else if (clk_count <= (1ULL << 10)) {
        *rst_time = hal_driver_wdt_rst_time_10;
    } else if (clk_count <= (1ULL << 11)) {
        *rst_time = hal_driver_wdt_rst_time_11;
    } else if (clk_count <= (1ULL << 12)) {
        *rst_time = hal_driver_wdt_rst_time_12;
    } else if (clk_count <= (1ULL << 13)) {
        *rst_time = hal_driver_wdt_rst_time_13;
    } else if (clk_count <= (1ULL << 14)) {
        *rst_time = hal_driver_wdt_rst_time_14;
    } else {
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 计算实际超时时间用于日志 - rst_time枚举值直接对应2^(7+枚举值) */
    int exp = 7 + (int)*rst_time;
    uint64_t actual_clk_count = (1ULL << exp);
    uint64_t actual_timeout_ms = (actual_clk_count * clk_period_ns) / 1000000ULL;
    LISA_LOGD(LOG_TAG, "ms_to_rst_time: requested=%lu ms, clk_freq=%lu Hz, clk_count=%llu, selected=rst_time_%d (2^%d=%llu cycles, ≈%llu ms)",
              timeout_ms, clk_freq, clk_count, (int)*rst_time, exp, actual_clk_count, actual_timeout_ms);

    return LISA_DEVICE_OK;
}

/* ===== Venusa平台WDT实现函数 ===== */

/**
 * @brief 配置看门狗设备
 */
static int venusa_wdt_setup(lisa_device_t *dev, const lisa_wdt_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 验证配置参数 */
    if (config->int_timeout_ms == 0) {
        LISA_LOGE(LOG_TAG, "Invalid int_timeout_ms: %lu ms", config->int_timeout_ms);
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 如果看门狗正在运行，先停止 */
    if (priv->state == LISA_WDT_STATE_RUNNING) {
        if (WDT_Disable(priv->hal_handler) != 0) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to disable WDT before setup");
            return LISA_DEVICE_ERR_IO;
        }
        priv->state = LISA_WDT_STATE_IDLE;
    }

    /* 保存配置 */
    priv->config = *config;

    /* 默认使用 32K 时钟源 */
    priv->clk_src = hal_driver_wdt_clk_src_32K;

    /* 计算中断时间（从启动开始） */
    if (ms_to_int_time(config->int_timeout_ms, priv->clk_src, &priv->int_time) != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Invalid interrupt time for int_timeout_ms: %lu ms", config->int_timeout_ms);
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 计算复位时间（从中断触发后开始计算，到系统复位） */
    /* 注意：根据HAL库注释，rst_time是复位阶段的时间，从中断触发后开始计算 */
    /* HAL库示例：int_time=15(1s), rst_time=14(0.5s)，时间线：启动->[1s]中断->[0.5s]复位 */
    if (ms_to_rst_time(config->rst_timeout_ms, priv->clk_src, &priv->rst_time) != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Invalid reset time for rst_timeout_ms: %lu ms", config->rst_timeout_ms);
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 确保中断回调已注册（在配置之前注册，确保配置时回调已存在） */
    /* 注意：即使没有用户回调，我们也需要注册HAL回调来更新状态 */
    if (WDT_Initialize(priv->hal_handler, wdt_hal_irq_callback, priv) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to register WDT callback before configuration");
        return LISA_DEVICE_ERR_IO;
    }
    LISA_LOGD(LOG_TAG, "WDT callback registered: %p, workspace: %p", wdt_hal_irq_callback, priv);

    /* 配置 HAL WDT */
    HAL_DRIVER_WDT_Cfg_t hal_cfg = {
        .clk_src = priv->clk_src,
        .int_time = priv->int_time,
        .rst_time = priv->rst_time,
    };

    if (WDT_Control(priv->hal_handler, &hal_cfg) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to configure WDT");
        return LISA_DEVICE_ERR_IO;
    }
    LISA_LOGD(LOG_TAG, "WDT configured: int_time=%d, rst_time=%d", priv->int_time, priv->rst_time);

    /* 保存配置的总超时时间（用于剩余时间查询） */
    priv->configured_timeout_ms = config->int_timeout_ms;
    priv->state = LISA_WDT_STATE_IDLE;

    DEVICE_UNLOCK(priv);

    uint32_t total_rst_timeout_ms = config->int_timeout_ms + config->rst_timeout_ms;
    LISA_LOGI(LOG_TAG, "WDT configured: int_timeout=%lu ms, rst_timeout=%lu ms (total=%lu ms), clk_src=%d, int_time=%d, rst_time=%d",
              config->int_timeout_ms, config->rst_timeout_ms, total_rst_timeout_ms,
              priv->clk_src, priv->int_time, priv->rst_time);

    return LISA_DEVICE_OK;
}

/**
 * @brief 启动看门狗
 */
static int venusa_wdt_start(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查是否已配置 */
    if (priv->configured_timeout_ms == 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "WDT not configured, call setup first");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 如果已经在运行，直接返回 */
    if (priv->state == LISA_WDT_STATE_RUNNING) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    /* 确保回调已注册（双重检查） */
    if (WDT_Initialize(priv->hal_handler, wdt_hal_irq_callback, priv) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to register WDT callback before start");
        return LISA_DEVICE_ERR_IO;
    }

    /* Venusa 需要打开当前 core WDT 到 CMN 的复位通路，否则 WDT 超时只产生中断不复位。 */
    VENUSA_WDT_RESET_CMN_ENABLE();

    /* 启动 HAL WDT（这会同时使能中断和复位） */
    if (WDT_Enable(priv->hal_handler) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to enable WDT");
        return LISA_DEVICE_ERR_IO;
    }

    priv->state = LISA_WDT_STATE_RUNNING;

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "WDT started with int_time=%d, rst_time=%d, callback=%p", 
              priv->int_time, priv->rst_time, wdt_hal_irq_callback);
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止看门狗
 */
static int venusa_wdt_stop(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (priv->state != LISA_WDT_STATE_RUNNING) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    /* 停止 HAL WDT */
    if (WDT_Disable(priv->hal_handler) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to disable WDT");
        return LISA_DEVICE_ERR_IO;
    }

    priv->state = LISA_WDT_STATE_IDLE;

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "WDT stopped");
    return LISA_DEVICE_OK;
}

/**
 * @brief 喂狗（重置看门狗计数器）
 */
static int venusa_wdt_feed(lisa_device_t *dev)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (priv->state != LISA_WDT_STATE_RUNNING) {
        DEVICE_UNLOCK(priv);
        LISA_LOGW(LOG_TAG, "WDT not running, feed ignored");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 喂狗 */
    if (WDT_Feed(priv->hal_handler) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to refresh WDT");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    LISA_LOGD(LOG_TAG, "WDT fed");
    return LISA_DEVICE_OK;
}

/**
 * @brief 获取剩余时间
 */
static int venusa_wdt_get_remaining_time(lisa_device_t *dev, uint32_t *remaining_ms)
{
    if (!lisa_device_is_initialized(dev) || !remaining_ms) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (priv->state != LISA_WDT_STATE_RUNNING) {
        DEVICE_UNLOCK(priv);
        *remaining_ms = 0;
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* Venusa WDT HAL 不支持直接读取剩余时间，返回配置的超时时间 */
    *remaining_ms = priv->configured_timeout_ms;

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取看门狗状态
 */
static int venusa_wdt_get_state(lisa_device_t *dev, lisa_wdt_state_t *state)
{
    if (!lisa_device_is_initialized(dev) || !state) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    *state = priv->state;
    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置超时回调函数
 */
static int venusa_wdt_set_callback(lisa_device_t *dev, lisa_wdt_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    priv->callback = callback;
    priv->user_data = user_data;

    /* 注意：HAL回调始终注册，用于更新状态和调用用户回调 */
    /* 即使用户没有设置回调，我们也需要HAL回调来更新状态 */
    if (WDT_Initialize(priv->hal_handler, wdt_hal_irq_callback, priv) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to update WDT callback");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    LISA_LOGD(LOG_TAG, "WDT callback %s", callback ? "set" : "cleared");
    return LISA_DEVICE_OK;
}

/* ===== Venusa WDT API 实例 ===== */
static const lisa_wdt_api_t venusa_wdt_api = {
    .setup = venusa_wdt_setup,
    .start = venusa_wdt_start,
    .stop = venusa_wdt_stop,
    .feed = venusa_wdt_feed,
    .get_remaining_time = venusa_wdt_get_remaining_time,
    .get_state = venusa_wdt_get_state,
    .set_callback = venusa_wdt_set_callback,
};

/* ===== 设备初始化函数 ===== */

/**
 * @brief 创建 WDT 的 OS 资源（仅一次，启动期调用）
 *
 * 仅做 mutex 创建之类的一次性 OS 资源分配，不触碰 HAL。
 * resume_restore 路径不会调用此函数。
 */
static int venusa_wdt_init_resources(lisa_wdt_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 WDT0 HAL 硬件初始化
 *
 * 由 _init 调用；只动 HAL，不分配 mutex / 堆内存。唤醒后经 reinit 重新走 _init
 * 路径时，destroy 阶段已先 WDT_Disable + WDT_PowerControl(OFF) + WDT_Uninitialize；
 * 启动期首次调用时 HAL 状态本就为零，重新 Initialize + PowerControl(FULL) 即可把
 * HAL 拉回出口形态。
 */
static int venusa_wdt0_init_hw(lisa_wdt_priv_t *priv)
{
    /* 获取 HAL WDT 句柄 */
    priv->hal_handler = WDT();
    if (!priv->hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get WDT handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL WDT，注册中断回调 */
    if (WDT_Initialize(priv->hal_handler, wdt_hal_irq_callback, priv) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize WDT");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 上电 WDT */
    if (WDT_PowerControl(priv->hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on WDT");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* HW 刚上电；唤醒路径需把 app 可见状态重置为 IDLE，强制应用重新
     * setup() + start()，避免上层根据陈旧 state 继续 feed 已掉电的 WDT。 */
    priv->state = LISA_WDT_STATE_IDLE;
    memset(&priv->config, 0, sizeof(priv->config));
    priv->configured_timeout_ms = 0;

    return LISA_DEVICE_OK;
}

static int venusa_wdt0_init(void)
{
    /* 清空私有数据 */
    memset(&wdt0_priv, 0, sizeof(lisa_wdt_priv_t));

    int ret = venusa_wdt_init_resources(&wdt0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = venusa_wdt0_init_hw(&wdt0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "WDT0 initialized successfully");

    return LISA_DEVICE_OK;
}

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放 WDT0 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 venusa_wdt0_init 申请相反：
 *   1) HAL 下电：WDT_Disable 停计数 → WDT_PowerControl(OFF) → WDT_Uninitialize；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（含 state / config /
 *      configured_timeout_ms 归零，强制唤醒后业务侧重新 setup() + start()）。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int venusa_wdt0_deinit(void)
{
    lisa_wdt_priv_t *priv = &wdt0_priv;

    if (priv->hal_handler) {
        WDT_Disable(priv->hal_handler);
        WDT_PowerControl(priv->hal_handler, CSK_POWER_OFF);
        WDT_Uninitialize(priv->hal_handler);
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&wdt0_priv, 0, sizeof(lisa_wdt_priv_t));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(wdt0) 释放全部软硬件资源（HAL 下电 + mutex），唤醒后在
 * PM after_wake 回调中调 lisa_device_reinit(wdt0) 重建到 _init 后的状态，并由业务
 * 重新 setup() + start()。因此 prepare_suspend / resume_restore 不再需要（原先它们只做
 * HAL 拆卸 / 字段清零，已被 destroy/reinit 覆盖，且二者运行于 PM 临界区无法做重活）。
 *
 * 仅保留 check_idle：WDT 处于 RUNNING 时阻塞 AUTO_LIGHT_SLEEP，避免睡眠期间硬件继续
 * 倒计时触发误 reset。只读 priv->state，不取 mutex / 不读 HAL。
 */
static int32_t venusa_wdt_pm_check_idle(void *ctx)
{
    lisa_wdt_priv_t *priv = (lisa_wdt_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    /* WDT 处于 RUNNING 时阻塞 AUTO_LIGHT_SLEEP，避免误触 reset */
    return (priv->state == LISA_WDT_STATE_RUNNING) ? 0 : 1;
}

static const lisa_pm_system_ops_t venusa_wdt0_pm_ops = {
    .check_idle      = venusa_wdt_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */

/* ===== 设备注册 ===== */


LISA_DEVICE_REGISTER_DEINIT(wdt0, &venusa_wdt_api, &wdt0_priv, NULL, venusa_wdt0_init,
                            venusa_wdt0_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(wdt0, &venusa_wdt0_pm_ops, NULL, &wdt0_priv);
#endif
