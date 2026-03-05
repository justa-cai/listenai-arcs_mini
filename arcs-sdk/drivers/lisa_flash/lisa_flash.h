/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_flash.h
 * @brief LISA Flash 设备驱动接口
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
 * Flash 类型定义
 * ======================================================================== */
/**
 * @brief Flash 参数结构体
 *
 * 包含 Flash 设备的运行时参数信息
 * 仅包含写操作相关的基本参数，布局信息通过 page_layout 接口获取
 */
typedef struct {
    size_t write_block_size;     /* 最小写对齐和大小(字节) */

    /**
     * @brief 设备能力标志
     *
     * 用户代码应该通过 lisa_flash_params_get_* 辅助函数访问能力标志，
     * 而不是直接访问结构体内容
     */
    struct {
        /**
         * @brief 设备无需显式擦除
         *
         * 当此标志为 true 时，设备满足以下条件之一：
         * - 设备在写入时自动擦除
         * - 设备根本不需要擦除
         * - 设备支持擦除但不强制要求擦除
         *
         * 对于 SPI Flash，通常为 false（需要显式擦除）
         */
        bool no_explicit_erase: 1;
    } caps;

    uint8_t erase_value;         /* 擦除后的值，通常为 0xFF */
} lisa_flash_parameters_t;

/**
 * @brief Flash 页面布局结构体
 *
 * 描述一组连续的、大小相同的页面
 */
typedef struct {
    size_t pages_count;          /* 此布局中的页面数量 */
    size_t pages_size;           /* 每个页面的大小(字节) */
} lisa_flash_pages_layout_t;

/* ========================================================================
 * Flash 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*read)(lisa_device_t *dev, size_t offset, void *data, size_t len);
    int (*write)(lisa_device_t *dev, size_t offset, const void *data, size_t len);
    int (*erase)(lisa_device_t *dev, size_t offset, size_t size);
    const lisa_flash_parameters_t *(*get_parameters)(lisa_device_t *dev);
    const lisa_flash_pages_layout_t *(*page_layout)(lisa_device_t *dev, size_t *layout_size);
} lisa_flash_api_t;

/* ========================================================================
 * Flash 对外接口函数
 * ======================================================================== */
/**
 * @brief 从 Flash 读取数据
 *
 * 从指定偏移地址读取数据到缓冲区。
 *
 * @param dev Flash 设备指针
 * @param offset 读取起始偏移地址(字节对齐)
 * @param data 数据缓冲区指针
 * @param len 读取长度(字节)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note 设备必须已初始化
 * @note offset 和 len 必须在有效范围内
 */
static inline int lisa_flash_read(lisa_device_t *dev, size_t offset, void *data, size_t len)
{
    if (!dev || !dev->api || !data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_flash_api_t *api = (lisa_flash_api_t *)dev->api;
    return api->read ? api->read(dev, offset, data, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 向 Flash 写入数据
 *
 * 向指定偏移地址写入数据。
 *
 * @param dev Flash 设备指针
 * @param offset 写入起始偏移地址(字节对齐)
 * @param data 数据缓冲区指针
 * @param len 写入长度(字节)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note 写入前必须先擦除目标区域
 * @note offset 和 len 必须在有效范围内
 */
static inline int lisa_flash_write(lisa_device_t *dev, size_t offset, const void *data, size_t len)
{
    if (!dev || !dev->api || !data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_flash_api_t *api = (lisa_flash_api_t *)dev->api;
    return api->write ? api->write(dev, offset, data, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 擦除 Flash 区域
 *
 * 擦除指定偏移地址和大小的 Flash 区域。
 *
 * @param dev Flash 设备指针
 * @param offset 擦除起始偏移地址
 * @param size 擦除大小(字节)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_IO IO错误
 * @return <0 其他错误
 *
 * @note offset 和 size 必须按照硬件要求对齐(通常为扇区或块大小)
 */
static inline int lisa_flash_erase(lisa_device_t *dev, size_t offset, size_t size)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_flash_api_t *api = (lisa_flash_api_t *)dev->api;
    return api->erase ? api->erase(dev, offset, size) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取 Flash 参数
 *
 * 获取 Flash 设备的运行时参数信息。
 * 返回的指针指向常量结构体，在运行时保持不变。
 *
 * @param dev Flash 设备指针
 *
 * @return 指向 lisa_flash_parameters_t 结构体的指针，成功
 * @return NULL 参数无效或不支持该操作
 *
 * @note 返回的指针指向常量数据，可以缓存该指针或复制其内容
 * @note 必须在设备初始化后调用
 * @note 参数包括：write_block_size（最小写对齐）、caps（能力标志）、erase_value（擦除值）
 * @note 布局信息（页大小、扇区大小、总大小）通过 lisa_flash_page_layout() 获取
 * @note 建议使用辅助函数访问参数：lisa_flash_params_get_*()
 */
static inline const lisa_flash_parameters_t *lisa_flash_get_parameters(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return NULL;
    }
    lisa_flash_api_t *api = (lisa_flash_api_t *)dev->api;
    return api->get_parameters ? api->get_parameters(dev) : NULL;
}

/**
 * @brief 获取 Flash 页面布局
 *
 * 获取 Flash 设备的页面布局数组。
 * 返回的数组描述了 Flash 的完整布局，包含一个或多个布局段。
 *
 * @param dev Flash 设备指针
 * @param layout_size 输出参数，返回布局数组的元素数量
 *
 * @return 指向 lisa_flash_pages_layout_t 数组的指针，成功
 * @return NULL 参数无效或不支持该操作
 *
 * @note 返回的指针指向常量数据，在运行时保持不变
 * @note 必须在设备初始化后调用
 * @note 对于均匀扇区的 Flash，layout_size 通常为 1
 * @note 对于非均匀扇区的 Flash，可能返回多个布局段
 * @note 每个布局段描述一组连续的、大小相同的页面
 *
 * @example
 * // 获取并遍历页面布局
 * size_t layout_count = 0;
 * const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
 * if (layout) {
 *     size_t total_offset = 0;
 *     for (size_t i = 0; i < layout_count; i++) {
 *         printf("Layout %zu: %zu pages × %zu bytes (offset: 0x%zx)\n",
 *                i, layout[i].pages_count, layout[i].pages_size, total_offset);
 *         total_offset += layout[i].pages_count * layout[i].pages_size;
 *     }
 * }
 */
static inline const lisa_flash_pages_layout_t *lisa_flash_page_layout(lisa_device_t *dev, size_t *layout_size)
{
    if (!dev || !dev->api || !layout_size) {
        return NULL;
    }
    lisa_flash_api_t *api = (lisa_flash_api_t *)dev->api;
    return api->page_layout ? api->page_layout(dev, layout_size) : NULL;
}

/**
 * @brief 从页面布局计算 Flash 总大小
 *
 * 遍历布局数组，累加所有布局段的大小。
 *
 * @param layout 页面布局数组指针
 * @param layout_size 布局数组元素数量
 *
 * @return Flash 总大小(字节)
 *
 * @note 对于均匀布局（layout_size=1），返回 pages_count × pages_size
 */
static inline size_t lisa_flash_get_size_from_layout(const lisa_flash_pages_layout_t *layout, size_t layout_size)
{
    size_t total_size = 0;
    if (layout) {
        for (size_t i = 0; i < layout_size; i++) {
            total_size += layout[i].pages_count * layout[i].pages_size;
        }
    }
    return total_size;
}

/**
 * @brief 获取 Flash 参数中的写块大小
 *
 * @param params Flash 参数指针
 *
 * @return 写块大小(字节)
 */
static inline size_t lisa_flash_params_get_write_block_size(const lisa_flash_parameters_t *params)
{
    return params ? params->write_block_size : 0;
}

/**
 * @brief 获取 Flash 参数中的擦除值
 *
 * @param params Flash 参数指针
 *
 * @return 擦除后的值，通常为 0xFF
 */
static inline uint8_t lisa_flash_params_get_erase_value(const lisa_flash_parameters_t *params)
{
    return params ? params->erase_value : 0xFF;
}

/**
 * @brief 检查 Flash 是否需要显式擦除
 *
 * @param params Flash 参数指针
 *
 * @return true 无需显式擦除（设备自动擦除或不需要擦除）
 * @return false 需要显式擦除（如 SPI Flash）
 *
 * @note 对于 SPI Flash，返回 false
 * @note 对于 NOR Flash 等可能返回 true
 */
static inline bool lisa_flash_params_get_no_explicit_erase(const lisa_flash_parameters_t *params)
{
    return params ? params->caps.no_explicit_erase : false;
}

#ifdef __cplusplus
}
#endif
