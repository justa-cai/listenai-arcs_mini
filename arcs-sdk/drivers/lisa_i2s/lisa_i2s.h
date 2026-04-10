/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_i2s.h
 * @brief LISA I2S 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x)                 (1<<x)
#endif

/* ========================================================================
 * I2S 类型定义
 * ======================================================================== */

/**
 * @brief I2S 工作模式
 */
typedef enum {
    LISA_I2S_MODE_MASTER = 0,  /* 主模式 */
    LISA_I2S_MODE_SLAVE = 1,   /* 从模式 */
} lisa_i2s_mode_t;

/**
 * @brief I2S 协议类型
 */
typedef enum {
    LISA_I2S_PROTOCOL_PHILIPS,     /* 标准 I2S 协议 */
    LISA_I2S_PROTOCOL_LEFT_JUSTIFIED,   /* 左对齐 */
    LISA_I2S_PROTOCOL_RIGHT_JUSTIFIED,  /* 右对齐 */
    LISA_I2S_PROTOCOL_PCM_SHORT_MODE_0,        /* PCM 短帧MODE 0 */
    LISA_I2S_PROTOCOL_PCM_SHORT_MODE_1,        /* PCM 短帧MODE 1 */
    LISA_I2S_PROTOCOL_PCM_LONG,         /* PCM 长帧，目前暂不支持 */
} lisa_i2s_protocol_t;

/**
 * @brief I2S 数据位宽
 */
typedef enum {
    LISA_I2S_DATA_WIDTH_16BIT,                      /* 16位数据宽度，一个通道占用2个字节 */
    LISA_I2S_DATA_WIDTH_20BIT_HIGH,                 /* 20位数据宽度，一个通道占用4个字节，对于发送：高20位有效，低12位实际传输为0 */
    LISA_I2S_DATA_WIDTH_24BIT_HIGH,                 /* 24位数据宽度，一个通道占用4个字节，对于发送：高24位有效，低8位实际传输为0 */
    LISA_I2S_DATA_WIDTH_24BIT_LOW,                  /* 24位数据宽度，一个通道占用4个字节，对于发送：低24位有效，高8位实际传输为0 */
    LISA_I2S_DATA_WIDTH_32BIT,                      /* 32位数据宽度，一个通道占用4个字节 */
} lisa_i2s_data_width_t;

typedef enum {
    LISA_I2S_BIT_ORDER_MSB,
    LISA_I2S_BIT_ORDER_LSB,
} lisa_i2s_bit_order_t;

/**
 * @brief I2S 采样率
 */
typedef enum {
    LISA_I2S_SAMPLE_RATE_8K = 8000,
    LISA_I2S_SAMPLE_RATE_16K = 16000,
    LISA_I2S_SAMPLE_RATE_24K = 24000,
    LISA_I2S_SAMPLE_RATE_32K = 32000,
    LISA_I2S_SAMPLE_RATE_48K = 48000,
    LISA_I2S_SAMPLE_RATE_96K = 96000,
} lisa_i2s_sample_rate_t;

/**
 * @brief I2S slot mask
 */
typedef enum {
    LISA_I2S_SLOT_LEFT = BIT(0),                         /* 只有左声道有数据 */
    LISA_I2S_SLOT_RIGHT = BIT(1),                        /* 只有右声道有数据 */
    LISA_I2S_SLOT_STEREO = BIT(0) | BIT(1),             /* 左右声道均有数据 */
}lisa_i2s_slot_mask_t;

/**
 * @brief I2S slot mode
 */
typedef enum {
    LISA_I2S_SLOT_MODE_MONO,                    /* 对于tx不同声道传输相同数据， 对于rx只接受第一个声道的数据 */
    LISA_I2S_SLOT_MODE_STEREO,                  /* 对于tx不同声道传输不同数据， 对于rx接收所有声道数据 */
}lisa_i2s_slot_mode_t;

/**
 * I2S 传输方向
 */
typedef enum {
    LISA_I2S_DIRECTION_TX = BIT(0),
    LISA_I2S_DIRECTION_RX = BIT(1),
    LISA_I2S_DIRECTION_BOTH = BIT(0) | BIT(1),
} lisa_i2s_direction_t;

/* I2S Echo */
typedef struct {
    lisa_i2s_slot_mask_t slot_mask;
    bool enable;
}lisa_i2s_echo_t;

/**
 * @brief I2S 配置结构体
 */
typedef struct {
    lisa_i2s_mode_t mode;                       /* 主/从模式 */
    lisa_i2s_protocol_t protocol;               /* 协议类型 */
    lisa_i2s_data_width_t data_width;           /* 数据位宽 */
    lisa_i2s_bit_order_t bit_order;             /* 大尾端/小尾端 */
    lisa_i2s_sample_rate_t sample_rate;         /* 采样率 */

    lisa_i2s_slot_mask_t slot_mask;             /* 声道配置(在非TDM模式下面有效) */

    lisa_i2s_direction_t direction;             /* 传输方向 */
    lisa_i2s_echo_t echo;                       /* ARCS芯片的I2S Echo 配置 (目前暂不支持)*/

    uint32_t tdm_slots;                         /* TDM 时隙数量 (2-16)，仅在TDM模式有效 */
    bool use_tdm;                               /* 是否使用TDM模式 */

    /* 设置I2S驱动单次传输的大小（单位：字节）
     *
     *  注意：block_size必须是32字节的倍数
     *
     *  对于RX，驱动会每次按照这个block_size来接收数据，应用层通过lisa_i2s_read读取数据时，读取的大小也固定是这个block_size
     *
     *  对于TX，应用层每次总共传输的字节数按照 <= 这个block_size来lisa_i2s_write数据，但是强烈建议按照这个block_size来发送数据
     *
     *  如果同一个I2S驱动TX和RX同时进行，建议lisa_i2s_write的总共传输的字节数等于这个block_size
     */
    uint32_t block_size;
} lisa_i2s_config_t;

/**
 * I2S 命令类型
 */
typedef enum {
    LISA_I2S_CMD_START,
    LISA_I2S_CMD_STOP,
    LISA_I2S_CMD_PAUSE,
    LISA_I2S_CMD_RESUME,
} lisa_i2s_cmd_t;

/**
 * @brief I2S 事件类型
 */
typedef enum {
    LISA_I2S_EVENT_TX_DONE = BIT(0),        /* 发送完成 */
    LISA_I2S_EVENT_RX_DONE = BIT(1),        /* 接收完成 */
    LISA_I2S_EVENT_TX_UNDERRUN = BIT(2),    /* 发送下溢 */
    LISA_I2S_EVENT_RX_OVERRUN = BIT(3),     /* 接收溢出 */
    LISA_I2S_EVENT_ERROR = BIT(4),          /* 错误 */
} lisa_i2s_event_t;

/**
 * @brief I2S 事件回调函数类型
 * 
 * @param event 事件类型
 * @param user_data 用户数据
 */
typedef void (*lisa_i2s_event_callback_t)(lisa_i2s_event_t event, void *user_data);

/* ========================================================================
 * I2S 配置宏
 * ======================================================================== */

/**
 * @brief I2S 默认配置（主模式，标准 I2S，16位，16kHz，立体声，tx, 单次传输16ms音频数据）
 */
#define LISA_I2S_DEFAULT_CONFIG_TX() \
    { \
        .mode = LISA_I2S_MODE_MASTER, \
        .protocol = LISA_I2S_PROTOCOL_PHILIPS, \
        .data_width = LISA_I2S_DATA_WIDTH_16BIT, \
        .bit_order = LISA_I2S_BIT_ORDER_MSB, \
        .sample_rate = LISA_I2S_SAMPLE_RATE_16K, \
        .slot_mask = LISA_I2S_SLOT_STEREO, \
        .direction = LISA_I2S_DIRECTION_TX, \
        .echo = { \
            .enable = false, \
        }, \
        .use_tdm = false, \
        .block_size = 1024, \
    };

/**
 * @brief I2S 默认配置（主模式，标准 I2S，16位，16kHz，立体声，rx, 单次传输16ms音频数据）
 */
#define LISA_I2S_DEFAULT_CONFIG_RX() \
    { \
        .mode = LISA_I2S_MODE_MASTER, \
        .protocol = LISA_I2S_PROTOCOL_PHILIPS, \
        .data_width = LISA_I2S_DATA_WIDTH_16BIT, \
        .bit_order = LISA_I2S_BIT_ORDER_MSB, \
        .sample_rate = LISA_I2S_SAMPLE_RATE_16K, \
        .slot_mask = LISA_I2S_SLOT_STEREO, \
        .direction = LISA_I2S_DIRECTION_RX, \
        .echo = { \
            .enable = false, \
        }, \
        .use_tdm = false, \
        .block_size = 1024, \
    };

/* ========================================================================
 * I2S 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*configure)(lisa_device_t *dev, const lisa_i2s_config_t *config);
    int (*get_config)(lisa_device_t *dev, lisa_i2s_config_t *config);

    int (*set_callback)(lisa_device_t *dev, lisa_i2s_event_callback_t callback, void *user_data);

    int (*write)(lisa_device_t *dev, uint32_t *data, uint32_t cnt, uint32_t timeout_ms);
    int (*read)(lisa_device_t *dev, uint8_t **data, uint32_t *len, uint32_t timeout_ms);

    int (*trigger)(lisa_device_t *dev, lisa_i2s_direction_t dir, lisa_i2s_cmd_t cmd);
} lisa_i2s_api_t;

/* ========================================================================
 * I2S 对外接口函数
 * ======================================================================== */

/**
 * @brief 配置 I2S 设备
 * 
 * @param [in] dev I2S 设备
 * @param [in] config 配置参数
 * @return 0 成功，负数错误码
 */
static inline int lisa_i2s_configure(lisa_device_t *dev, const lisa_i2s_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->configure ? api->configure(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取I2S当前配置
 *
 * @note 此函数需在lisa_i2s_configure成功返回之后调用才有效
 *
 * @param [in] dev I2S设备指针
 * @param [out] config 输出参数，用于接收配置信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_i2s_get_config(lisa_device_t *dev, lisa_i2s_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置 I2S 事件回调函数
 * 
 * @param [in] dev I2S 设备
 * @param [in] callback 回调函数
 * @param [in] user_data 用户数据
 * @return 0 成功，负数错误码
 *
 * @note  此函数需要在lisa_i2s_configure()调用成功之后使用
 */
static inline int lisa_i2s_set_callback(lisa_device_t *dev, lisa_i2s_event_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 发送数据 (非阻塞， 暂时写到临时缓存里面等待发送)，返回LISA_DEVICE_OK表示全部发送到临时缓存，其他返回值表示数据没有发送
 *
 * @note  发送的总字节数 = sizeof(uint32_t) * cnt
 *
 * @note  发送的总字节数: 建议等于 lisa_i2s_configure()配置的config->block_size的大小。也支持小于config->block_size
 *
 * @note  举例： 
 *              对于data_width = 16bit，需要每2个通道数据填充一个uint32_t类型，cnt = 要发送的总字节数 / sizeof(uint32_t)
 *              对于其他data_width，则1个通道数据填充一个uint32_t类型，cnt = 要发送的总字节数 / sizeof(uint32_t)
 *
 * @param [in] dev I2S 设备
 * @param [in] data 发送数据缓冲区，注意这里是uint32_t类型
 * @param [in] cnt 发送数据点数，一个点类型为uint32_t，即4个字节
 * @param [in] timeout_ms 超时时间（毫秒）
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_NOT_READY 发送还没有初始化或配置
 * @return LISA_DEVICE_ERR_TIMEOUT 超时，没有发送数据，或者没有发完数据
 * @return LISA_DEVICE_ERR_BUSY 超时，没有发送数据，或者没有发完数据
 *
 * @note  此函数需要在lisa_i2s_configure()调用成功之后使用
 */
static inline int lisa_i2s_write(lisa_device_t *dev, uint32_t *data, uint32_t cnt, uint32_t timeout_ms)
{
    if (!dev || !dev->api || !data || cnt == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->write ? api->write(dev, data, cnt, timeout_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 接收数据，返回LISA_DEVICE_OK表示已经收到数据（此时len值有效），其他返回值表示没有收到数据（此时len值无效）
 *
 * @note data是uint8_t *类型，返回的len是读取的总字节数
 *
 * @note 此函数返回的是内部的接收缓存的指针，所以函数返回成功后，用户需要及时拷贝数据，防止数据被覆盖
 *
 * @param [in] dev I2S 设备
 * @param [inout] data 用于存储接收到数据的缓冲区起始地址
 * @param [inout] len 实际读到的数据长度(单位为字节)
 * @param [in] timeout_ms 超时时间（毫秒）
 * @return LISA_DEVICE_OK 成功
 * @return LISA_DEVICE_ERR_TIMEOUT 超时，没有接收到数据
 * @return LISA_DEVICE_ERR_NOT_READY 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 * @note timeout_ms 为 0 表示不等待
 */
static inline int lisa_i2s_read(lisa_device_t *dev, uint8_t **data, uint32_t *len, uint32_t timeout_ms)
{
    if (!dev || !dev->api || !data || len == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->read ? api->read(dev, data, len, timeout_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief I2S 命令
 * 
 * @param [in] dev I2S 设备
 * @param [in] dir I2S 方向
 * @param [in] cmd 命令类型
 * @return int 0成功，负数错误码
 *
 * @note  此函数需要在lisa_i2s_configure()调用成功之后使用
 */
static inline int lisa_i2s_trigger(lisa_device_t *dev, lisa_i2s_direction_t dir, lisa_i2s_cmd_t cmd)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_i2s_api_t *api = (lisa_i2s_api_t *)dev->api;
    return api->trigger ? api->trigger(dev, dir, cmd) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

#ifdef __cplusplus
}
#endif
