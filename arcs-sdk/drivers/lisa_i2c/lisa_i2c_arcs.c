/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_i2c_arcs.c
 * @brief LISA I2C ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 I2C 硬件适配
 */

#include "lisa_i2c.h"
#include "Driver_I2C.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <lisa_mutex.h>
#include <lisa_time.h>
#include "pinmux.h"

#define LOG_TAG "lisa_i2c_arcs"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

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

/* ===== I2C 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                    /* HAL I2C 句柄 */
    lisa_mutex_t *mutex;                 /* 互斥锁 */
    lisa_i2c_config_t config;            /* 当前配置 */
    volatile uint32_t event_flags;        /* 事件标志（用于同步） */
    SemaphoreHandle_t xfer_sem;          /* 传输完成信号量（独立于调用者的 task notification） */
    volatile bool xfer_in_flight;         /* PM busy 标志：API 入口置 1、退出/错误路径清 0 */
} lisa_i2c_priv_t;

/* ===== I2C 设备静态实例 ===== */
#if CONFIG_LISA_I2C0
static lisa_i2c_priv_t i2c0_priv;
#endif

#if CONFIG_LISA_I2C1
static lisa_i2c_priv_t i2c1_priv;
#endif

/* ===== HAL 事件回调函数 ===== */
#if CONFIG_LISA_I2C0
static void i2c0_event_callback(uint32_t event, void *workspace)
{
    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)workspace;
    if (priv) {
        priv->event_flags |= event;
        if (priv->xfer_sem) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(priv->xfer_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
#endif

#if CONFIG_LISA_I2C1
static void i2c1_event_callback(uint32_t event, void *workspace)
{
    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)workspace;
    if (priv) {
        priv->event_flags |= event;
        if (priv->xfer_sem) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(priv->xfer_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
#endif

/* ===== 内部辅助函数 ===== */

/**
 * @brief 验证I2C速度是否在支持范围内
 * @param speed 速度值（Hz）
 * @return true 速度有效, false 速度无效
 */
static bool is_valid_speed(uint32_t speed)
{
    /* 支持的速度范围：100kHz 到 1MHz */
    return (speed >= LISA_I2C_SPEED_STANDARD && speed <= LISA_I2C_SPEED_FAST_PLUS);
}

/**
 * @brief 将 LISA I2C 速度转换为 HAL 速度常量
 * @param speed 速度值（Hz）
 * @return HAL速度常量，如果速度无效返回0
 */
static uint32_t speed_to_hal_speed(uint32_t speed)
{
    if (speed <= 100000) {
        return CSK_I2C_BUS_SPEED_STANDARD;
    } else if (speed <= 400000) {
        return CSK_I2C_BUS_SPEED_FAST;
    } else if (speed <= 1000000) {
        return CSK_I2C_BUS_SPEED_FAST_PLUS;
    } else {
        /* 超出支持范围 */
        return 0;
    }
}

/**
 * @brief 检查传输事件并返回结果
 * @return LISA_DEVICE_OK / ERR_NACK / ERR_IO 表示传输已结束, -1 表示尚未完成
 */
static inline int check_transfer_events(lisa_i2c_priv_t *priv, bool is_probe)
{
    uint32_t events = priv->event_flags;

    if (events & CSK_I2C_EVENT_TRANSFER_DONE) {
        if (is_probe) {
            priv->event_flags = 0;
            return (events & CSK_I2C_EVENT_ADDRESS_ACK) ?
                   LISA_DEVICE_OK : LISA_DEVICE_ERR_NACK;
        }
        priv->event_flags = 0;
        return LISA_DEVICE_OK;
    }

    if (events & (CSK_I2C_EVENT_ADDRESS_NACK | CSK_I2C_EVENT_ARBITRATION_LOST | CSK_I2C_EVENT_BUS_ERROR)) {
        priv->event_flags = 0;
        if (events & CSK_I2C_EVENT_ADDRESS_NACK) {
            return LISA_DEVICE_ERR_NACK;
        }
        return LISA_DEVICE_ERR_IO;
    }

    return -1; /* 尚未完成 */
}

/**
 * @brief 等待传输完成或错误
 *
 * 基于 FreeRTOS 任务通知：ISR 回调中 vTaskNotifyGiveFromISR 唤醒本任务，
 * 零延迟、零 CPU 空转。
 */
static int wait_for_transfer(lisa_i2c_priv_t *priv, uint32_t timeout_ms, bool is_probe)
{
    int ret;

    /* 快速路径：事件可能在 I2C_MasterTransmit 返回前已由 ISR 置位 */
    ret = check_transfer_events(priv, is_probe);
    if (ret != -1) {
        return ret;
    }

    /* 阻塞等待 ISR 通过信号量通知，带超时
     * 使用独立信号量而非 task notification，避免与调用方任务的其他
     * notification 来源（如 GPIO 中断）冲突导致假唤醒。
     */
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1;
    }
    xSemaphoreTake(priv->xfer_sem, ticks);

    /* 检查最终事件 */
    ret = check_transfer_events(priv, is_probe);
    if (ret != -1) {
        return ret;
    }

    /* 超时 */
    priv->event_flags = 0;
    return LISA_DEVICE_ERR_TIMEOUT;
}

/* ===== ARCS平台I2C实现函数 ===== */

/**
 * @brief 配置I2C总线
 */
static int arcs_i2c_configure(lisa_device_t *dev, const lisa_i2c_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 验证速度范围 */
    if (!is_valid_speed(config->speed)) {
        LISA_LOGE(LOG_TAG, "Invalid I2C speed: %lu Hz (supported range: %u-%u Hz)", 
                  config->speed, LISA_I2C_SPEED_STANDARD, LISA_I2C_SPEED_FAST_PLUS);
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 设置总线速度 */
    uint32_t hal_speed = speed_to_hal_speed(config->speed);
    if (hal_speed == 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Invalid I2C speed: %lu Hz (supported range: %u-%u Hz)", 
                  config->speed, LISA_I2C_SPEED_STANDARD, LISA_I2C_SPEED_FAST_PLUS);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (I2C_Control(priv->hal_handler, CSK_I2C_BUS_SPEED, hal_speed) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to set bus speed");
        return LISA_DEVICE_ERR_IO;
    }

    /* 保存配置（仅在成功设置后） */
    memcpy(&priv->config, config, sizeof(lisa_i2c_config_t));

    /* 如果是从机模式，设置从机地址 */
    if (!config->master_mode) {
        if (I2C_Control(priv->hal_handler, CSK_I2C_OWN_ADDRESS, config->slave_addr) != 0) {
            DEVICE_UNLOCK(priv);
            LISA_LOGE(LOG_TAG, "Failed to set slave address");
            return LISA_DEVICE_ERR_IO;
        }
    }

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "I2C configured: speed=%lu Hz, mode=%s", config->speed,
              config->master_mode ? "master" : "slave");

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取I2C总线当前配置
 */
static int arcs_i2c_get_config(lisa_device_t *dev, lisa_i2c_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    memcpy(config, &priv->config, sizeof(lisa_i2c_config_t));
    DEVICE_UNLOCK(priv);

    /* 验证配置的有效性
     * 如果发现配置无效（可能是之前保存的无效值），返回错误
     * 调用者应该重新调用 configure 来设置有效配置
     */
    if (!is_valid_speed(config->speed)) {
        LISA_LOGE(LOG_TAG, "Current I2C speed is invalid: %lu Hz (supported range: %u-%u Hz). Please reconfigure with a valid speed.", 
                  config->speed, LISA_I2C_SPEED_STANDARD, LISA_I2C_SPEED_FAST_PLUS);
        return LISA_DEVICE_ERR_INVALID;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief I2C通用传输接口
 */
static int arcs_i2c_transfer(lisa_device_t *dev, lisa_i2c_msg_t *msgs, uint32_t num_msgs)
{
    if (!lisa_device_is_initialized(dev) || !msgs || num_msgs == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查是否为主机模式 */
    if (!priv->config.master_mode) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Transfer only supported in master mode");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    priv->xfer_in_flight = true;

    for (uint32_t i = 0; i < num_msgs; i++) {
        lisa_i2c_msg_t *msg = &msgs[i];

        /* 检查消息有效性
         * 允许 len == 0 的情况（用于地址探测）
         * 当 len > 0 时，buf 必须有效
         */
        if (msg->len > 0 && !msg->buf) {
            priv->xfer_in_flight = false;
            DEVICE_UNLOCK(priv);
            return LISA_DEVICE_ERR_INVALID;
        }

        /* 准备地址（支持10位地址） */
        uint32_t addr = msg->addr;
        if (msg->flags & LISA_I2C_FLAG_10BIT_ADDR) {
            addr |= CSK_I2C_ADDRESS_10BIT;
        }

        /* 判断是否为读操作（通过 LISA_I2C_FLAG_READ 标志） */
        bool is_write = !(msg->flags & LISA_I2C_FLAG_READ);

        /* 清除事件标志，并排空上一条消息可能残留的信号量
         * 场景：上一条消息的 ISR 在快速路径检查前已完成，
         * wait_for_transfer 通过快速路径返回但未 take 信号量，
         * 导致信号量处于 "given" 状态。若不排空，下一条消息的
         * xSemaphoreTake 会立即返回，造成假超时。
         */
        priv->event_flags = 0;
        xSemaphoreTake(priv->xfer_sem, 0);

        /* 判断是否发送 STOP 条件 */
        bool xfer_pending = (msg->flags & LISA_I2C_FLAG_NO_STOP) ? true : false;

        /* 如果是第一个消息且设置了 NO_START，需要特殊处理 */
        /* 但 HAL 接口不支持 NO_START，所以忽略该标志 */
        if (i == 0 && (msg->flags & LISA_I2C_FLAG_NO_START)) {
            LISA_LOGW(LOG_TAG, "NO_START flag not supported, ignored");
        }

        /* 执行传输
         * HAL 层支持 0 字节传输（用于地址探测/ACK polling）
         * 当 len == 0 时，buf 可以为 NULL
         */
        int32_t hal_ret;
        if (is_write) {
            hal_ret = I2C_MasterTransmit(priv->hal_handler, addr, 
                                        msg->len > 0 ? msg->buf : NULL, 
                                        msg->len, xfer_pending);
        } else {
            hal_ret = I2C_MasterReceive(priv->hal_handler, addr, 
                                       msg->len > 0 ? msg->buf : NULL, 
                                       msg->len, xfer_pending);
        }

        if (hal_ret != 0) {
            /* HAL 启动失败，尝试等待短时间检查是否有事件标志（如 NACK） */
            bool is_probe = (msg->len == 0);  /* 0字节传输为设备探测 */
            int ret = wait_for_transfer(priv, 100, is_probe); /* 100ms 短超时 */
            priv->xfer_in_flight = false;
            DEVICE_UNLOCK(priv);
            if (ret == LISA_DEVICE_ERR_NACK) {
                return LISA_DEVICE_ERR_NACK; /* 明确返回 NACK */
            }
            LISA_LOGE(LOG_TAG, "HAL transfer failed: %d", hal_ret);
            return LISA_DEVICE_ERR_IO;
        }

        /* 等待传输完成
         * 对于组合传输（写后读），即使设置了 NO_STOP，也需要等待第一个传输完成
         * 才能启动第二个传输，否则 HAL 会返回 BUSY 错误
         * 只有在最后一个消息时才发送 STOP 条件
         */
        bool is_probe = (msg->len == 0);  /* 0字节传输为设备探测 */
        uint32_t timeout_ms = is_probe ? 50 : 1000;
        int ret = wait_for_transfer(priv, timeout_ms, is_probe);
            if (ret != LISA_DEVICE_OK) {
                priv->xfer_in_flight = false;
                DEVICE_UNLOCK(priv);
                return ret;
        }
    }

    priv->xfer_in_flight = false;
    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

/**
 * @brief I2C写数据
 */
static int arcs_i2c_write(lisa_device_t *dev, uint16_t addr, const uint8_t *buf, uint32_t len)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查是否为主机模式 */
    if (!priv->config.master_mode) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Write only supported in master mode");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    /* 清除事件标志 */
    priv->event_flags = 0;

    priv->xfer_in_flight = true;

    /* 执行写操作 */
    int32_t hal_ret = I2C_MasterTransmit(priv->hal_handler, addr, (uint8_t *)buf, len, false);
    if (hal_ret != 0) {
        /* HAL 启动失败，尝试等待短时间检查是否有事件标志（如 NACK） */
        int ret = wait_for_transfer(priv, 100, false); /* 100ms 短超时 */
        priv->xfer_in_flight = false;
        DEVICE_UNLOCK(priv);
        if (ret == LISA_DEVICE_ERR_NACK) {
            return LISA_DEVICE_ERR_NACK; /* 明确返回 NACK */
        }
        LISA_LOGE(LOG_TAG, "HAL write failed: %d", hal_ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 等待传输完成（write 不是设备探测） */
    int ret = wait_for_transfer(priv, 1000, false); /* 1秒超时 */

    priv->xfer_in_flight = false;
    DEVICE_UNLOCK(priv);
    return ret;
}

/**
 * @brief I2C读数据
 */
static int arcs_i2c_read(lisa_device_t *dev, uint16_t addr, uint8_t *buf, uint32_t len)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查是否为主机模式 */
    if (!priv->config.master_mode) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Read only supported in master mode");
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    /* 清除事件标志 */
    priv->event_flags = 0;

    priv->xfer_in_flight = true;

    /* 执行读操作 */
    int32_t hal_ret = I2C_MasterReceive(priv->hal_handler, addr, buf, len, false);
    if (hal_ret != 0) {
        /* HAL 启动失败，尝试等待短时间检查是否有事件标志（如 NACK） */
        int ret = wait_for_transfer(priv, 100, false); /* 100ms 短超时 */
        priv->xfer_in_flight = false;
        DEVICE_UNLOCK(priv);
        if (ret == LISA_DEVICE_ERR_NACK) {
            return LISA_DEVICE_ERR_NACK; /* 明确返回 NACK */
        }
        LISA_LOGE(LOG_TAG, "HAL read failed: %d", hal_ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 等待传输完成（read 不是设备探测） */
    int ret = wait_for_transfer(priv, 1000, false); /* 1秒超时 */

    priv->xfer_in_flight = false;
    DEVICE_UNLOCK(priv);
    return ret;
}

/* ===== ARCS I2C API 实例 ===== */
static const lisa_i2c_api_t arcs_i2c_api = {
    .configure = arcs_i2c_configure,
    .get_config = arcs_i2c_get_config,
    .transfer = arcs_i2c_transfer,
    .write = arcs_i2c_write,
    .read = arcs_i2c_read,
};

/* ===== 设备初始化函数 ===== */

/**
 * @brief OS 资源初始化（mutex / 信号量），仅 _init 阶段调用一次，跨 suspend/resume 保留
 */
static int arcs_i2c_init_resources(lisa_i2c_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    priv->xfer_sem = xSemaphoreCreateBinary();
    if (!priv->xfer_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create I2C transfer semaphore");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 HAL 硬件初始化
 *
 * 由 _init 调用；只动 HAL / pinmux，不分配 mutex / sem / 堆内存。
 *
 * 唤醒后经 reinit 重新走 _init 路径时，destroy 阶段已先
 * `I2C_PowerControl(OFF) + I2C_Uninitialize` 清掉 HAL 状态；启动期首次调用时
 * HAL 内部状态为零，重复 Initialize 无副作用，故本函数保持幂等。
 */
static int arcs_i2c_init_hw(lisa_i2c_priv_t *priv, int instance)
{
    void (*event_cb)(uint32_t, void *) = NULL;
    void (*pinmux_fn)(void) = NULL;

    switch (instance) {
#if CONFIG_LISA_I2C0
    case 0:
        priv->hal_handler = I2C0();
        event_cb = i2c0_event_callback;
        pinmux_fn = lisa_i2c0_pinmux;
        break;
#endif
#if CONFIG_LISA_I2C1
    case 1:
        priv->hal_handler = I2C1();
        event_cb = i2c1_event_callback;
        pinmux_fn = lisa_i2c1_pinmux;
        break;
#endif
    default:
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (!priv->hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get I2C%d handler", instance);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (I2C_Initialize(priv->hal_handler, event_cb, priv) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize I2C%d", instance);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (I2C_PowerControl(priv->hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on I2C%d", instance);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (I2C_Control(priv->hal_handler, CSK_I2C_TRANSMIT_MODE, 0) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C%d transmit mode", instance);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (I2C_Control(priv->hal_handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C%d bus speed", instance);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    I2C_Control(priv->hal_handler, CSK_I2C_BUS_CLEAR, 0);

    pinmux_fn();

    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_I2C0
static int arcs_i2c0_init(void)
{
    /* 清空私有数据 */
    memset(&i2c0_priv, 0, sizeof(lisa_i2c_priv_t));

    int ret = arcs_i2c_init_resources(&i2c0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_i2c_init_hw(&i2c0_priv, 0);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    /* 设置默认配置 */
    i2c0_priv.config.speed = LISA_I2C_SPEED_STANDARD;
    i2c0_priv.config.master_mode = true;
    i2c0_priv.config.slave_addr = 0;

    LISA_LOGI(LOG_TAG, "I2C0 initialized successfully");

    return LISA_DEVICE_OK;
}
#endif

#if CONFIG_LISA_I2C1
static int arcs_i2c1_init(void)
{
    /* 清空私有数据 */
    memset(&i2c1_priv, 0, sizeof(lisa_i2c_priv_t));

    int ret = arcs_i2c_init_resources(&i2c1_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_i2c_init_hw(&i2c1_priv, 1);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    /* 设置默认配置 */
    i2c1_priv.config.speed = LISA_I2C_SPEED_STANDARD;
    i2c1_priv.config.master_mode = true;
    i2c1_priv.config.slave_addr = 0;

    LISA_LOGI(LOG_TAG, "I2C1 initialized successfully");

    return LISA_DEVICE_OK;
}
#endif

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放单个 I2C 实例的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 经各实例 deinit 包装调用。释放顺序与 _init 申请相反：
 *   1) HAL 下电：先 I2C_PowerControl(OFF) 再 I2C_Uninitialize（PowerControl(OFF)
 *      内部会读 INITIALIZED 状态，反向调用会失败，故顺序固定）；
 *   2) 释放 OS 资源 xfer_sem / mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值。
 *
 * 约定：调用方需保证此时无传输在途、无并发业务在使用本设备。
 */
static int arcs_i2c_deinit_instance(lisa_i2c_priv_t *priv)
{
    if (priv == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (priv->hal_handler) {
        I2C_PowerControl(priv->hal_handler, CSK_POWER_OFF);
        I2C_Uninitialize(priv->hal_handler);
    }

    if (priv->xfer_sem) {
        vSemaphoreDelete(priv->xfer_sem);
    }
    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(priv, 0, sizeof(*priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_I2C0
static int arcs_i2c0_deinit(void)
{
    return arcs_i2c_deinit_instance(&i2c0_priv);
}
#endif

#if CONFIG_LISA_I2C1
static int arcs_i2c1_deinit(void)
{
    return arcs_i2c_deinit_instance(&i2c1_priv);
}
#endif

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(i2cN) 释放全部软硬件资源（HAL 下电 + mutex/sem），唤醒后在
 * PM after_wake 回调中调 lisa_device_reinit(i2cN) 重建到 _init 后的状态，并由业务
 * 重新 configure()。因此 prepare_suspend / resume_restore 不再需要（原先它们只做
 * HAL 拆卸 / 字段清零，已被 destroy/reinit 覆盖，且二者运行于 PM 临界区无法做重活）。
 *
 * 仅保留 check_idle：只读 priv->xfer_in_flight，为 true 时占用总线，禁止
 * AUTO_LIGHT_SLEEP。不取 mutex / 不读 HAL，避免在 PM 临界区阻塞或递归。
 */
static int32_t arcs_i2c_pm_check_idle(void *ctx)
{
    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    return priv->xfer_in_flight ? 0 : 1;
}

#if CONFIG_LISA_I2C0
static const lisa_pm_system_ops_t arcs_i2c0_pm_ops = {
    .check_idle      = arcs_i2c_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif

#if CONFIG_LISA_I2C1
static const lisa_pm_system_ops_t arcs_i2c1_pm_ops = {
    .check_idle      = arcs_i2c_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif
#endif /* CONFIG_LISA_PM */

/* ===== 设备注册 ===== */


#if CONFIG_LISA_I2C0
LISA_DEVICE_REGISTER_DEINIT(i2c0, &arcs_i2c_api, &i2c0_priv, NULL, arcs_i2c0_init,
                            arcs_i2c0_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(i2c0, &arcs_i2c0_pm_ops, NULL, &i2c0_priv);
#endif
#endif

#if CONFIG_LISA_I2C1
LISA_DEVICE_REGISTER_DEINIT(i2c1, &arcs_i2c_api, &i2c1_priv, NULL, arcs_i2c1_init,
                            arcs_i2c1_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(i2c1, &arcs_i2c1_pm_ops, NULL, &i2c1_priv);
#endif
#endif
