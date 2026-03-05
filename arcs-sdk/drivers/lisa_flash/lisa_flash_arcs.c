/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_flash_arcs.c
 * @brief LISA Flash ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 Flash 硬件适配
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <lisa_mutex.h>

#include "lisa_flash.h"
#include "flash_if.h"
#include "arcs_ap.h"
#include "cache.h"

#define LOG_TAG "lisa_flash_arcs"
#include <lisa_log.h>

/* ========================================================================
 * Flash 常量定义
 * ======================================================================== */

/* Flash 页/扇区/块大小 */
#ifndef LISA_FLASH_SECTOR_SIZE
#define LISA_FLASH_SECTOR_SIZE CONFIG_LISA_FLASH_ARCS_ERASE_SECTOR_SIZE
#endif

#ifndef LISA_FLASH_WRITE_BLOCK_SIZE
#define LISA_FLASH_WRITE_BLOCK_SIZE CONFIG_LISA_FLASH_ARCS_WRITE_BLOCK_SIZE
#endif


/* Flash 物理地址范围配置 */
#define FLASH_PHYS_ADDR_START CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_ADDR
#define FLASH_PHYS_ADDR_END (CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_ADDR + CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_RESERVE_SIZE)

/* Flash 写入缓冲区大小 (4KB) */
#define FLASH_WRITE_BUFFER_SIZE (4 * 1024)

/* ========================================================================
 * Flash 私有数据结构定义
 * ======================================================================== */

/* ===== Flash 设备私有数据 ===== */
typedef struct {
    lisa_mutex_t *mutex;                /* 互斥锁 */
    bool initialized;                   /* 初始化标志 */
    lisa_flash_parameters_t parameters; /* Flash 参数 */
    lisa_flash_pages_layout_t layout;   /* 页面布局（均匀扇区，只需一个布局段） */
} lisa_flash_priv_t;

/* ===== Flash 设备静态实例 ===== */
static lisa_flash_priv_t flash0_priv;

/* ===== 辅助宏定义 ===== */
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

/* ===== 内部辅助函数 ===== */

/**
 * @brief 检查数据指针是否位于 Flash 物理地址区域
 *
 * @param ptr 数据指针
 * @return true 数据位于 Flash 区域
 * @return false 数据不在 Flash 区域
 */
static inline bool is_data_in_flash_region(const void *ptr)
{
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= FLASH_PHYS_ADDR_START && addr < FLASH_PHYS_ADDR_END);
}

/**
 * @brief 根据 JEDEC device ID 计算 Flash 容量
 *
 * Device ID 表示 2^N Bytes，其中 N = device_id
 * 计算公式：capacity_bytes = 2^(device_id)
 *
 * @param device_id JEDEC device ID (RDID 命令返回的第3个字节)
 * @return Flash 容量(字节)，如果 ID 无效则返回默认值 16MB
 */
static size_t calculate_flash_size_from_id(uint32_t device_id)
{
    /* 计算容量：2^(device_id) bytes */
    size_t capacity = 1UL << (device_id);

    return capacity;
}

/**
 * @brief 动态获取 Flash 大小
 *
 * 通过读取 JEDEC ID 并解析容量字节来确定实际容量
 *
 * @return Flash 容量(字节)，失败则返回 0
 */
static size_t get_flash_size_dynamic(void)
{
    uint32_t jedec_id = 0;

    /* 使用 RDID (0x9F) 命令读取完整的 JEDEC ID */
    int ret = flash_if_read_jedec_id(&jedec_id);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Failed to read JEDEC ID: %d, using default size", ret);
        return 0;
    }

    /* JEDEC ID 格式: [Manufacturer(byte2)][Type(byte1)][Capacity(byte0)] */
    uint8_t manufacturer = (jedec_id >> 16) & 0xFF;
    uint8_t memory_type = (jedec_id >> 8) & 0xFF;
    uint8_t capacity_id = jedec_id & 0xFF;

    LISA_LOGD(LOG_TAG, "JEDEC ID: 0x%06X (Manufacturer=0x%02X, Type=0x%02X, Capacity=0x%02X)", jedec_id, manufacturer,
              memory_type, capacity_id);

    /* 根据容量 ID 计算实际容量 */
    size_t capacity = calculate_flash_size_from_id(capacity_id);

    LISA_LOGD(LOG_TAG, "Detected flash size from ID 0x%02X: %zu bytes (%.1f MB)", capacity_id, capacity,
              capacity / (1024.0 * 1024.0));

    return capacity;
}

static inline int check_device_initialized(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;
    if (!priv->initialized) {
        LISA_LOGE(LOG_TAG, "Flash device not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 检查访问范围是否超出 Flash 边界
 *
 * @param priv Flash 私有数据结构
 * @param offset 访问偏移量
 * @param size 访问大小
 * @return LISA_DEVICE_OK 范围有效
 * @return LISA_DEVICE_ERR_INVALID 范围超出边界
 */
static inline int check_flash_boundary(lisa_flash_priv_t *priv, size_t offset, size_t size)
{
    size_t flash_size = priv->layout.pages_count * priv->layout.pages_size;
    
    if (offset >= flash_size) {
        LISA_LOGE(LOG_TAG, "Offset 0x%zx exceeds flash size 0x%zx", offset, flash_size);
        return LISA_DEVICE_ERR_INVALID;
    }
    
    if (offset + size > flash_size) {
        LISA_LOGE(LOG_TAG, "Access range [0x%zx, 0x%zx) exceeds flash boundary 0x%zx", 
                  offset, offset + size, flash_size);
        return LISA_DEVICE_ERR_INVALID;
    }
    
    return LISA_DEVICE_OK;
}

static inline void _dcache_invalidate_range_expand(unsigned long start, unsigned long end){
    uint32_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1;
    
    // 对start向下取整到cache line边界
    unsigned long aligned_start = start & (~line_mask);
    
    // 对end向上取整到cache line边界
    unsigned long aligned_end = (end + line_mask) & (~line_mask);


    // 只有当有完整的cache line需要处理时才进行操作
    if (aligned_start < aligned_end) {
        HAL_InvalidateDCache_by_Addr((uint32_t *)aligned_start, (aligned_end - aligned_start));
    }
}

/* ===== API 实现函数 ===== */

/**
 * @brief 从 Flash 读取数据
 */
static int arcs_flash_read(lisa_device_t *dev, size_t offset, void *data, size_t len)
{
    uint32_t start_phys_addr;

    if (!data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;

    ret = check_flash_boundary(priv, offset, len);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    start_phys_addr = CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_ADDR + offset;
    _dcache_invalidate_range_expand(start_phys_addr ,start_phys_addr + len);
    memcpy(data,start_phys_addr,len);

    return LISA_DEVICE_OK;
}

/**
 * @brief 向 Flash 写入数据
 *
 * 如果数据位于 flash 区域上，需要先将数据拷贝进内存中，然后再进行写入，
 * 避免在操作 flash 时进行 flash 读取操作。如果数据长度超过 4K，则逐步写入。
 */
static int arcs_flash_write(lisa_device_t *dev, size_t offset, const void *data, size_t len)
{
    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;
    const void *write_data = data;
    uint8_t *temp_buffer = NULL;
    int32_t hal_ret = 0;

    if (!data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = check_flash_boundary(priv, offset, len);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(priv);

    /* 检查数据是否位于 Flash 物理地址区域 */
    if (is_data_in_flash_region(data)) {
        LISA_LOGD(LOG_TAG, "Data is in flash region (0x%p), copying to RAM buffer", data);

        /* 如果数据长度超过 4K，则分块处理 */
        if (len > FLASH_WRITE_BUFFER_SIZE) {
            /* 分配临时缓冲区 */
            temp_buffer = malloc(FLASH_WRITE_BUFFER_SIZE);
            if (!temp_buffer) {
                LISA_LOGE(LOG_TAG, "Failed to allocate temporary buffer (%u bytes)", FLASH_WRITE_BUFFER_SIZE);
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_NO_MEM;
            }

            /* 逐块写入 */
            flash_if_write_protection_set(false);
            size_t remaining = len;
            size_t current_offset = offset;
            const uint8_t *src_ptr = (const uint8_t *)data;

            while (remaining > 0) {
                size_t chunk_size = (remaining > FLASH_WRITE_BUFFER_SIZE) ? FLASH_WRITE_BUFFER_SIZE : remaining;

                /* 将数据从 Flash 区域拷贝到 RAM 缓冲区 */
                memcpy(temp_buffer, src_ptr, chunk_size);

                /* 写入到 Flash */
                hal_ret = flash_if_write(current_offset, temp_buffer, chunk_size);
                if (hal_ret != 0) {
                    LISA_LOGE(LOG_TAG, "Flash write failed at offset 0x%zx, len %zu: %d", current_offset, chunk_size,
                              hal_ret);
                    break;
                }

                remaining -= chunk_size;
                current_offset += chunk_size;
                src_ptr += chunk_size;
            }

            flash_if_write_protection_set(true);
            free(temp_buffer);
        } else {
            /* 数据长度 <= 4K，一次性拷贝并写入 */
            temp_buffer = malloc(len);
            if (!temp_buffer) {
                LISA_LOGE(LOG_TAG, "Failed to allocate temporary buffer (%u bytes)", len);
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_NO_MEM;
            }

            /* 将数据从 Flash 区域拷贝到 RAM 缓冲区 */
            memcpy(temp_buffer, data, len);
            write_data = temp_buffer;

            /* 写入到 Flash */
            flash_if_write_protection_set(false);
            hal_ret = flash_if_write(offset, write_data, len);
            flash_if_write_protection_set(true);

            free(temp_buffer);
        }
    } else {
        /* 数据不在 Flash 区域，直接写入 */
        flash_if_write_protection_set(false);
        hal_ret = flash_if_write(offset, data, len);
        flash_if_write_protection_set(true);
    }

    DEVICE_UNLOCK(priv);

    if (hal_ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash write failed at offset 0x%zx, len %zu: %d", offset, len, hal_ret);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 擦除 Flash 区域
 */
static int arcs_flash_erase(lisa_device_t *dev, size_t offset, size_t size)
{
    int ret;

    if (size == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;

    ret = check_flash_boundary(priv, offset, size);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(priv);

    /* 调用 HAL 擦除 */
    flash_if_write_protection_set(false);
    ret = flash_if_erase(offset, size);
    flash_if_write_protection_set(true);
    DEVICE_UNLOCK(priv);

    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash erase failed at offset 0x%x, size %u: %d", offset, size, ret);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取 Flash 参数
 *
 * @return 返回指向常量参数结构体的指针
 */
static const lisa_flash_parameters_t *arcs_flash_get_parameters(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return NULL;
    }

    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;

    if (!priv->initialized) {
        LISA_LOGE(LOG_TAG, "Flash device not initialized");
        return NULL;
    }

    /* 返回常量参数指针 */
    return &priv->parameters;
}

/**
 * @brief 获取页面布局
 *
 * @return 整个 Flash 的页面布局信息
 */
static const lisa_flash_pages_layout_t *arcs_flash_page_layout(lisa_device_t *dev, size_t *layout_size)
{
    if (!layout_size) {
        return NULL;
    }

    if (!dev || !dev->priv_data) {
        return NULL;
    }

    lisa_flash_priv_t *priv = (lisa_flash_priv_t *)dev->priv_data;

    if (!priv->initialized) {
        LISA_LOGE(LOG_TAG, "Flash device not initialized");
        return NULL;
    }

    /* SPI Flash 通常是均匀扇区布局，只返回一个布局段 */
    *layout_size = 1;
    return &priv->layout;
}

/* ===== API 实例 ===== */
static const lisa_flash_api_t arcs_flash_api = {
    .read = arcs_flash_read,
    .write = arcs_flash_write,
    .erase = arcs_flash_erase,
    .get_parameters = arcs_flash_get_parameters,
    .page_layout = arcs_flash_page_layout,
};

/* ===== 设备初始化函数 ===== */
static int arcs_flash0_init(void)
{
    FLASH_DEV hal_flash_dev = {0};
    /* 初始化私有数据 */
    memset(&flash0_priv, 0, sizeof(flash0_priv));

    /* 创建互斥锁 */
    flash0_priv.mutex = lisa_mutex_create();
    if (!flash0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 配置 HAL Flash 设备参数 */
    hal_flash_dev.base_addr = CONFIG_LISA_FLASH_ARCS_CONTROLLER_BASE_ADDR;
    hal_flash_dev.d_width = CONFIG_LISA_FLASH_ARCS_DATA_WIDTH;
    hal_flash_dev.sclk_div = CONFIG_LISA_FLASH_ARCS_SCLK_DIV;
    hal_flash_dev.addr_bytes = CONFIG_LISA_FLASH_ARCS_ADDR_BYTES;
    hal_flash_dev.w_protect = CONFIG_LISA_FLASH_ARCS_WRITE_PROTECT_ENABLE ? 1 : 0;
#if CONFIG_LISA_FLASH_ARCS_DISABLE_INTERRUPTS
    hal_flash_dev.run_mod = RUN_WITHOUT_INT; /* 默认不使用中断模式 */
#else
    hal_flash_dev.run_mod = RUN_WITH_INT;
#endif
    hal_flash_dev.timeout = FLASH_RETRY_TIMES;
    hal_flash_dev.addr_auto = 0;
    hal_flash_dev.dualflash_mode = 0;
    hal_flash_dev.interrupt_enable = NULL;
    hal_flash_dev.interrupt_disable = NULL;

    /* 调用 HAL 初始化 */
    int32_t ret = flash_if_init(&hal_flash_dev, 0, 0);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash HAL init failed: %d", ret);
        lisa_mutex_delete(flash0_priv.mutex);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 填充 Flash 参数（仅写操作相关的基本参数） */
    flash0_priv.parameters.write_block_size = LISA_FLASH_WRITE_BLOCK_SIZE;
    flash0_priv.parameters.caps.no_explicit_erase = false; /* SPI Flash 需要显式擦除 */
    flash0_priv.parameters.erase_value = 0xFF;             /* Flash 擦除后的值 */

    /* 填充页面布局（SPI Flash 通常是均匀的扇区布局） */
    flash0_priv.layout.pages_size = LISA_FLASH_SECTOR_SIZE; /* 扇区大小: 4KB */

    /* 动态获取 Flash 总容量（通过读取 Flash ID） */
    size_t total_size = get_flash_size_dynamic();
    flash0_priv.layout.pages_count = total_size / flash0_priv.layout.pages_size; /* 扇区数量 */

    flash0_priv.initialized = true;

    LISA_LOGD(LOG_TAG, "Flash initialized successfully (layout: %zu pages × %zu bytes, total: %zu bytes)",
              flash0_priv.layout.pages_count, flash0_priv.layout.pages_size,
              flash0_priv.layout.pages_count * flash0_priv.layout.pages_size);

    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(flash0,                           /* 设备名称 */
                     &arcs_flash_api,                  /* API 指针 */
                     &flash0_priv,                     /* 私有数据指针 */
                     NULL,                             /* 用户数据 */
                     arcs_flash0_init,                 /* 初始化函数 */
                     CONFIG_LISA_FLASH_INIT_PRIORITY); /* 优先级 */
