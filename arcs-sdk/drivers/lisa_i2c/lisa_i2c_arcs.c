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
#include <stddef.h>
#include <string.h>
#include <lisa_mutex.h>
#include <lisa_time.h>
#include "pinmux.h"

#define LOG_TAG "lisa_i2c_arcs"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"

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
    }
}
#endif

#if CONFIG_LISA_I2C1
static void i2c1_event_callback(uint32_t event, void *workspace)
{
    lisa_i2c_priv_t *priv = (lisa_i2c_priv_t *)workspace;
    if (priv) {
        priv->event_flags |= event;
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
 * @brief 等待传输完成或错误
 * @param priv I2C设备私有数据指针
 * @param timeout_ms 超时时间（毫秒）
 * @param is_probe 是否为设备探测（0字节传输）
 * @return LISA_DEVICE_OK 传输成功, LISA_DEVICE_ERR_NACK 地址无应答, LISA_DEVICE_ERR_IO I/O错误, LISA_DEVICE_ERR_TIMEOUT 超时
 */
static int wait_for_transfer(lisa_i2c_priv_t *priv, uint32_t timeout_ms, bool is_probe)
{
    uint32_t start_time = lisa_os_get_tick_ms();
    uint32_t end_time = start_time + timeout_ms;

    while (lisa_os_get_tick_ms() < end_time) {
        uint32_t events = priv->event_flags;

        /* 检查传输完成
         * 对于设备探测（0字节传输），必须严格检查 NACK
         * 对于正常数据传输，与原始实现保持一致
         */
        if (events & CSK_I2C_EVENT_TRANSFER_DONE) {
            /* 对于设备探测（0字节传输），严格检查地址应答 */
            if (is_probe) {
                /* 检查是否有地址应答 */
                if (events & CSK_I2C_EVENT_ADDRESS_ACK) {
                    priv->event_flags = 0;
                    return LISA_DEVICE_OK;  /* 设备存在 */
                } else {
                    priv->event_flags = 0;
                    return LISA_DEVICE_ERR_NACK;  /* 设备不存在 */
                }
            }
            
            /* 对于正常传输，传输完成即认为成功（保持与修复前一致，以免影响 camera） */
            priv->event_flags = 0;
            return LISA_DEVICE_OK;
        }

        /* 检查错误事件（仅在传输未完成时） */
        if (events & (CSK_I2C_EVENT_ADDRESS_NACK | CSK_I2C_EVENT_ARBITRATION_LOST | CSK_I2C_EVENT_BUS_ERROR)) {
            priv->event_flags = 0; /* 清除所有事件 */
            if (events & CSK_I2C_EVENT_ADDRESS_NACK) {
                return LISA_DEVICE_ERR_NACK;
            }
            if (events & CSK_I2C_EVENT_BUS_ERROR) {
                return LISA_DEVICE_ERR_IO;
            }
            return LISA_DEVICE_ERR_IO;
        }

        /* 短暂延时，避免 CPU 占用过高 */
        vTaskDelay(pdMS_TO_TICKS(1));
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

    for (uint32_t i = 0; i < num_msgs; i++) {
        lisa_i2c_msg_t *msg = &msgs[i];

        /* 检查消息有效性
         * 允许 len == 0 的情况（用于地址探测）
         * 当 len > 0 时，buf 必须有效
         */
        if (msg->len > 0 && !msg->buf) {
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

        /* 清除事件标志 */
        priv->event_flags = 0;

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
                DEVICE_UNLOCK(priv);
                return ret;
        }
    }

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

    /* 执行写操作 */
    int32_t hal_ret = I2C_MasterTransmit(priv->hal_handler, addr, (uint8_t *)buf, len, false);
    if (hal_ret != 0) {
        /* HAL 启动失败，尝试等待短时间检查是否有事件标志（如 NACK） */
        int ret = wait_for_transfer(priv, 100, false); /* 100ms 短超时 */
        DEVICE_UNLOCK(priv);
        if (ret == LISA_DEVICE_ERR_NACK) {
            return LISA_DEVICE_ERR_NACK; /* 明确返回 NACK */
        }
        LISA_LOGE(LOG_TAG, "HAL write failed: %d", hal_ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 等待传输完成（write 不是设备探测） */
    int ret = wait_for_transfer(priv, 1000, false); /* 1秒超时 */

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

    /* 执行读操作 */
    int32_t hal_ret = I2C_MasterReceive(priv->hal_handler, addr, buf, len, false);
    if (hal_ret != 0) {
        /* HAL 启动失败，尝试等待短时间检查是否有事件标志（如 NACK） */
        int ret = wait_for_transfer(priv, 100, false); /* 100ms 短超时 */
        DEVICE_UNLOCK(priv);
        if (ret == LISA_DEVICE_ERR_NACK) {
            return LISA_DEVICE_ERR_NACK; /* 明确返回 NACK */
        }
        LISA_LOGE(LOG_TAG, "HAL read failed: %d", hal_ret);
        return LISA_DEVICE_ERR_IO;
    }

    /* 等待传输完成（read 不是设备探测） */
    int ret = wait_for_transfer(priv, 1000, false); /* 1秒超时 */

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

#if CONFIG_LISA_I2C0
static int arcs_i2c0_init(void)
{
    /* 清空私有数据 */
    memset(&i2c0_priv, 0, sizeof(lisa_i2c_priv_t));

    /* 获取 HAL I2C0 句柄 */
    i2c0_priv.hal_handler = I2C0();
    if (!i2c0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get I2C0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 创建互斥锁 */
    i2c0_priv.mutex = lisa_mutex_create();
    if (!i2c0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL I2C */
    if (I2C_Initialize(i2c0_priv.hal_handler, i2c0_event_callback, &i2c0_priv) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize I2C0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 使能电源 */
    if (I2C_PowerControl(i2c0_priv.hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on I2C0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置传输模式为中断模式 */
    if (I2C_Control(i2c0_priv.hal_handler, CSK_I2C_TRANSMIT_MODE, 0) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C0 transmit mode");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置默认总线速度为标准模式 */
    if (I2C_Control(i2c0_priv.hal_handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C0 bus speed");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 清除总线 */
    I2C_Control(i2c0_priv.hal_handler, CSK_I2C_BUS_CLEAR, 0);

    /* 配置 I2C0 引脚复用 */
    lisa_i2c0_pinmux();

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

    /* 获取 HAL I2C1 句柄 */
    i2c1_priv.hal_handler = I2C1();
    if (!i2c1_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get I2C1 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 创建互斥锁 */
    i2c1_priv.mutex = lisa_mutex_create();
    if (!i2c1_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL I2C */
    if (I2C_Initialize(i2c1_priv.hal_handler, i2c1_event_callback, &i2c1_priv) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize I2C1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 使能电源 */
    if (I2C_PowerControl(i2c1_priv.hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on I2C1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置传输模式为中断模式 */
    if (I2C_Control(i2c1_priv.hal_handler, CSK_I2C_TRANSMIT_MODE, 0) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C1 transmit mode");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 设置默认总线速度为标准模式 */
    if (I2C_Control(i2c1_priv.hal_handler, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to set I2C1 bus speed");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 清除总线 */
    I2C_Control(i2c1_priv.hal_handler, CSK_I2C_BUS_CLEAR, 0);

    /* 配置 I2C1 引脚复用 */
    lisa_i2c1_pinmux();

    /* 设置默认配置 */
    i2c1_priv.config.speed = LISA_I2C_SPEED_STANDARD;
    i2c1_priv.config.master_mode = true;
    i2c1_priv.config.slave_addr = 0;

    LISA_LOGI(LOG_TAG, "I2C1 initialized successfully");

    return LISA_DEVICE_OK;
}
#endif

/* ===== 设备注册 ===== */

#if CONFIG_LISA_I2C0
LISA_DEVICE_REGISTER(i2c0,                        /* 设备名称 */
                     &arcs_i2c_api,               /* API指针 */
                     &i2c0_priv,                  /* 私有数据指针 */
                     NULL,                        /* 用户数据 */
                     arcs_i2c0_init,              /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL); /* 优先级 */
#endif

#if CONFIG_LISA_I2C1
LISA_DEVICE_REGISTER(i2c1,                        /* 设备名称 */
                     &arcs_i2c_api,               /* API指针 */
                     &i2c1_priv,                  /* 私有数据指针 */
                     NULL,                        /* 用户数据 */
                     arcs_i2c1_init,              /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL); /* 优先级 */
#endif
