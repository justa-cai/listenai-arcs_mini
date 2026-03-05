/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_sdmmc.h
 * @brief LISA SDMMC 设备驱动接口
 *
 * ## 驱动能力
 *
 * 本驱动提供以下功能：
 * - 基本操作：read/write 扇区数据
 * - 状态查询：获取SD/MMC 设备状态
 * - 参数查询：获取扇区数量、扇区大小、擦除块大小
 *
 * ## 典型使用流程
 *
 * @code
 * // 1. 获取设备
 * lisa_device_t *sdmmc = lisa_device_get_by_name("sdmmc0");
 *
 * // 2. 探测并初始化设备
 * lisa_sdmmc_probe(sdmmc);
 *
 * // 3. 检查状态
 * if (lisa_sdmmc_status(sdmmc) == LISA_SDMMC_STATUS_OK) {
 *     // SD/MMC 设备就绪
 * }
 *
 * // 4. 获取SD/MMC 设备信息
 * uint32_t sector_count, sector_size;
 * lisa_sdmmc_ioctl(sdmmc, LISA_SDMMC_IOCTL_GET_SECTOR_COUNT, &sector_count);
 * lisa_sdmmc_ioctl(sdmmc, LISA_SDMMC_IOCTL_GET_SECTOR_SIZE, &sector_size);
 *
 * // 5. 读写操作
 * uint8_t buffer[512];
 * lisa_sdmmc_read(sdmmc, buffer, 0, 1);  // 读取扇区 0
 * lisa_sdmmc_write(sdmmc, buffer, 0, 1); // 写入扇区 0
 * @endcode
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SDMMC 类型定义
 * ======================================================================== */

/**
 * @brief SD/MMC 设备状态枚举
 */
typedef enum {
    LISA_SDMMC_STATUS_OK = 0,       /* SD/MMC 设备就绪 */
    LISA_SDMMC_STATUS_UNINIT = 1,   /* SD/MMC 设备未初始化 */
    LISA_SDMMC_STATUS_BUSY = 2,     /* SD/MMC 设备忙 */
    LISA_SDMMC_STATUS_NO_MEDIA = 3, /* 无存储介质 */
} lisa_sdmmc_status_t;

/**
 * @brief SD/MMC 设备 IOCTL 命令枚举
 */
typedef enum {
    LISA_SDMMC_IOCTL_CTRL_SYNC = 0,        /* 同步缓存数据到SD/MMC 设备 */
    LISA_SDMMC_IOCTL_GET_SECTOR_COUNT = 1, /* 获取扇区总数 */
    LISA_SDMMC_IOCTL_GET_SECTOR_SIZE = 2,  /* 获取扇区大小（字节） */
    LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ = 3, /* 获取擦除块大小（扇区数） */
} lisa_sdmmc_ioctl_cmd_t;

/* ========================================================================
 * SDMMC 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*probe)(lisa_device_t *dev);
    int (*status)(lisa_device_t *dev);
    int (*read)(lisa_device_t *dev, uint8_t *buff, uint32_t sector, uint32_t count);
    int (*write)(lisa_device_t *dev, const uint8_t *buff, uint32_t sector, uint32_t count);
    int (*ioctl)(lisa_device_t *dev, uint8_t cmd, void *buff);
} lisa_sdmmc_api_t;

/* ========================================================================
 * SDMMC 对外接口函数
 * ======================================================================== */

/**
 * @brief 探测 SD/MMC 卡
 *
 * 执行纯探测操作，检测卡是否插入并配置总线参数。
 * 电源和时钟初始化已在设备注册阶段自动完成。
 *
 * @param dev SD/MMC 设备设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO 探测失败（卡未插入或初始化失败）
 * @return <0 其他错误
 *
 * @note 必须在读写操作之前调用
 */
#include "lisa_log.h"
static inline int lisa_sdmmc_probe(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_sdmmc_api_t *api = (lisa_sdmmc_api_t *)dev->api;
    return api->probe ? api->probe(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取SD/MMC 设备状态
 *
 * 查询SD/MMC 设备当前状态。
 *
 * @param dev SD/MMC 设备设备指针
 *
 * @return LISA_SDMMC_STATUS_OK SD/MMC 设备就绪
 * @return LISA_SDMMC_STATUS_UNINIT SD/MMC 设备未初始化
 * @return LISA_SDMMC_STATUS_BUSY SD/MMC 设备忙
 * @return LISA_SDMMC_STATUS_NO_MEDIA 无存储介质
 * @return <0 错误
 */
static inline int lisa_sdmmc_status(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_sdmmc_api_t *api = (lisa_sdmmc_api_t *)dev->api;
    return api->status ? api->status(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 从SD/MMC 设备读取扇区数据
 *
 * 从指定扇区读取数据到缓冲区。
 *
 * @param dev SD/MMC 设备设备指针
 * @param buff 数据缓冲区指针
 * @param sector 起始扇区号
 * @param count 读取扇区数量
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note 设备必须已初始化
 * @note 缓冲区大小必须 >= count * sector_size
 */
static inline int lisa_sdmmc_read(lisa_device_t *dev, uint8_t *buff, uint32_t sector, uint32_t count)
{
    if (!dev || !dev->api || !buff) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_sdmmc_api_t *api = (lisa_sdmmc_api_t *)dev->api;
    return api->read ? api->read(dev, buff, sector, count) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 向SD/MMC 设备写入扇区数据
 *
 * 将数据从缓冲区写入指定扇区。
 *
 * @param dev SD/MMC 设备设备指针
 * @param buff 数据缓冲区指针
 * @param sector 起始扇区号
 * @param count 写入扇区数量
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note 设备必须已初始化
 * @note 缓冲区大小必须 >= count * sector_size
 */
static inline int lisa_sdmmc_write(lisa_device_t *dev, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    if (!dev || !dev->api || !buff) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_sdmmc_api_t *api = (lisa_sdmmc_api_t *)dev->api;
    return api->write ? api->write(dev, buff, sector, count) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief SD/MMC 设备控制命令
 *
 * 执行SD/MMC 设备控制命令或获取SD/MMC 设备信息。
 *
 * @param dev SD/MMC 设备设备指针
 * @param cmd 控制命令，参见 lisa_sdmmc_ioctl_cmd_t
 * @param buff 命令参数/返回值缓冲区
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效或命令不支持
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note 各命令的 buff 参数说明：
 *       - LISA_SDMMC_IOCTL_CTRL_SYNC: buff 可为 NULL
 *       - LISA_SDMMC_IOCTL_GET_SECTOR_COUNT: buff 指向 uint32_t，返回扇区总数
 *       - LISA_SDMMC_IOCTL_GET_SECTOR_SIZE: buff 指向 uint32_t，返回扇区大小
 *       - LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ: buff 指向 uint32_t，返回擦除块大小
 */
static inline int lisa_sdmmc_ioctl(lisa_device_t *dev, uint8_t cmd, void *buff)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_sdmmc_api_t *api = (lisa_sdmmc_api_t *)dev->api;
    return api->ioctl ? api->ioctl(dev, cmd, buff) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取SD/MMC 设备扇区数量
 *
 * 便捷函数，获取SD/MMC 设备总扇区数。
 *
 * @param dev SD/MMC 设备设备指针
 * @param sector_count 输出参数，扇区总数
 *
 * @return 0 成功
 * @return <0 错误
 */
static inline int lisa_sdmmc_get_sector_count(lisa_device_t *dev, uint32_t *sector_count)
{
    if (!sector_count) {
        return LISA_DEVICE_ERR_INVALID;
    }
    return lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_GET_SECTOR_COUNT, sector_count);
}

/**
 * @brief 获取SD/MMC 设备扇区大小
 *
 * 便捷函数，获取单个扇区的字节大小。
 *
 * @param dev SD/MMC 设备设备指针
 * @param sector_size 输出参数，扇区大小（字节）
 *
 * @return 0 成功
 * @return <0 错误
 */
static inline int lisa_sdmmc_get_sector_size(lisa_device_t *dev, uint32_t *sector_size)
{
    if (!sector_size) {
        return LISA_DEVICE_ERR_INVALID;
    }
    return lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_GET_SECTOR_SIZE, sector_size);
}

/**
 * @brief 同步SD/MMC 设备缓存
 *
 * 便捷函数，将缓存数据同步到SD/MMC 设备。
 *
 * @param dev SD/MMC 设备设备指针
 *
 * @return 0 成功
 * @return <0 错误
 */
static inline int lisa_sdmmc_sync(lisa_device_t *dev)
{
    return lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_CTRL_SYNC, NULL);
}

#ifdef __cplusplus
}
#endif
