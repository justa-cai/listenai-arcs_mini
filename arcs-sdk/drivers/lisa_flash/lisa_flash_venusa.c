/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_flash_venusa.c
 * @brief LISA Flash VENUSA platform adaptation layer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <lisa_mutex.h>

#include "cache.h"
#include "lisa_flash.h"
#include "spiflash.h"
#include "venusa_ap.h"

#define LOG_TAG "lisa_flash_venusa"
#include <lisa_log.h>

#define VENUSA_FLASH_WRITE_BUFFER_SIZE (4 * 1024)

typedef struct {
    lisa_mutex_t *mutex;
    bool initialized;
    FLASH_DEV hal_dev;
    lisa_flash_parameters_t parameters;
    lisa_flash_pages_layout_t layout;
} venusa_flash_priv_t;

static venusa_flash_priv_t flash0_priv;

#define DEVICE_LOCK(priv)                                      \
    do {                                                       \
        if ((priv)->mutex) {                                   \
            lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER); \
        }                                                      \
    } while (0)

#define DEVICE_UNLOCK(priv)                    \
    do {                                       \
        if ((priv)->mutex) {                   \
            lisa_mutex_unlock((priv)->mutex);  \
        }                                      \
    } while (0)

static inline bool is_buffer_in_flash(const void *ptr)
{
    uintptr_t addr = (uintptr_t)ptr;

    return addr >= (uintptr_t)CONFIG_LISA_FLASH_VENUSA_FLASH_PHYS_ADDR &&
           addr < ((uintptr_t)CONFIG_LISA_FLASH_VENUSA_FLASH_PHYS_ADDR +
                   (uintptr_t)CONFIG_LISA_FLASH_VENUSA_FLASH_SIZE);
}

static size_t calculate_flash_size_from_jedec(uint32_t jedec_id)
{
    uint8_t capacity_id = jedec_id & 0xFF;

    if (capacity_id >= (sizeof(size_t) * 8U)) {
        return 0;
    }

    return (size_t)1U << capacity_id;
}

static size_t get_flash_size(FLASH_DEV *hal_dev)
{
    uint32_t jedec_id = 0;
    size_t configured_size = (size_t)CONFIG_LISA_FLASH_VENUSA_FLASH_SIZE;

    int ret = flash_get_jedec_id(hal_dev, &jedec_id);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Failed to read JEDEC ID: %d, using configured size %zu", ret, configured_size);
        return configured_size;
    }

    size_t detected_size = calculate_flash_size_from_jedec(jedec_id);
    if (detected_size == 0) {
        LISA_LOGW(LOG_TAG, "Invalid JEDEC ID 0x%06x, using configured size %zu", jedec_id, configured_size);
        return configured_size;
    }

    if (detected_size > configured_size) {
        LISA_LOGI(LOG_TAG, "JEDEC size %zu exceeds configured range %zu, capped", detected_size, configured_size);
        return configured_size;
    }

    LISA_LOGI(LOG_TAG, "JEDEC ID: 0x%06x, flash size: %zu", jedec_id, detected_size);
    return detected_size;
}

static inline int check_device_initialized(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;
    if (!priv->initialized) {
        LISA_LOGE(LOG_TAG, "Flash device not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    return LISA_DEVICE_OK;
}

static inline int check_flash_boundary(venusa_flash_priv_t *priv, size_t offset, size_t size)
{
    size_t flash_size = priv->layout.pages_count * priv->layout.pages_size;

    if (offset >= flash_size) {
        LISA_LOGE(LOG_TAG, "Offset 0x%zx exceeds flash size 0x%zx", offset, flash_size);
        return LISA_DEVICE_ERR_INVALID;
    }

    if (size > flash_size - offset) {
        LISA_LOGE(LOG_TAG, "Access range [0x%zx, 0x%zx) exceeds flash boundary 0x%zx",
                  offset, offset + size, flash_size);
        return LISA_DEVICE_ERR_INVALID;
    }

    return LISA_DEVICE_OK;
}

static inline int check_erase_alignment(size_t offset, size_t size)
{
    if ((offset % CONFIG_LISA_FLASH_VENUSA_ERASE_SECTOR_SIZE) != 0 ||
        (size % CONFIG_LISA_FLASH_VENUSA_ERASE_SECTOR_SIZE) != 0) {
        LISA_LOGE(LOG_TAG, "Erase range must be %u-byte aligned: offset=0x%zx, size=%zu",
                  CONFIG_LISA_FLASH_VENUSA_ERASE_SECTOR_SIZE, offset, size);
        return LISA_DEVICE_ERR_INVALID;
    }

    return LISA_DEVICE_OK;
}

static inline int set_write_protection(venusa_flash_priv_t *priv, bool enable)
{
    int ret = flash_write_protection_set(&priv->hal_dev, enable);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash write protection %s failed: %d", enable ? "enable" : "disable", ret);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

static void venusa_flash_dcache_flush(const void *addr, size_t len)
{
    if (!addr || len == 0U) {
        return;
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    uintptr_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1U;
    uintptr_t aligned_start = start & ~line_mask;
    uintptr_t aligned_end = (end + line_mask) & ~line_mask;

    HAL_FlushDCache_by_Addr((uint32_t *)aligned_start, (uint32_t)(aligned_end - aligned_start));
}

static void venusa_flash_dcache_invalidate(void *addr, size_t len)
{
    if (!addr || len == 0U) {
        return;
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    uintptr_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1U;
    uintptr_t aligned_start = start & ~line_mask;
    uintptr_t aligned_end = (end + line_mask) & ~line_mask;

    HAL_InvalidateDCache_by_Addr((uint32_t *)aligned_start, (uint32_t)(aligned_end - aligned_start));
}

static void venusa_flash_dcache_flush_invalidate(void *addr, size_t len)
{
    if (!addr || len == 0U) {
        return;
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    uintptr_t line_mask = HAL_DCACHE_CFG_LINE_SIZE - 1U;
    uintptr_t aligned_start = start & ~line_mask;
    uintptr_t aligned_end = (end + line_mask) & ~line_mask;

    HAL_FlushInvalidateDCache_by_Addr((uint32_t *)aligned_start, (uint32_t)(aligned_end - aligned_start));
}

__attribute__((weak)) int lisa_flash_hook_init(void)
{
    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_init_hook(FLASH_DEV *hal_dev)
{
    (void)hal_dev;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_init_done_hook(FLASH_DEV *hal_dev, int result)
{
    (void)hal_dev;
    (void)result;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_read_hook(FLASH_DEV *hal_dev, size_t offset, size_t len)
{
    (void)hal_dev;
    (void)offset;
    (void)len;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_read_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t len, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)len;
    (void)result;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_write_hook(FLASH_DEV *hal_dev, size_t offset, size_t len)
{
    (void)hal_dev;
    (void)offset;
    (void)len;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_write_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t len, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)len;
    (void)result;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_erase_hook(FLASH_DEV *hal_dev, size_t offset, size_t size)
{
    (void)hal_dev;
    (void)offset;
    (void)size;

    return LISA_DEVICE_OK;
}

__attribute__((weak)) int lisa_flash_erase_done_hook(FLASH_DEV *hal_dev, size_t offset, size_t size, int result)
{
    (void)hal_dev;
    (void)offset;
    (void)size;
    (void)result;

    return LISA_DEVICE_OK;
}

static int venusa_flash_hal_read_wrap(venusa_flash_priv_t *priv, size_t offset, void *data, size_t len)
{
    int ret = lisa_flash_read_hook(&priv->hal_dev, offset, len);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_dcache_flush_invalidate(data, len);
    ret = flash_read(&priv->hal_dev, (off_t)offset, data, len);
    venusa_flash_dcache_invalidate(data, len);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash read failed at offset 0x%zx, len %zu: %d", offset, len, ret);
        ret = LISA_DEVICE_ERR_IO;
    } else {
        ret = LISA_DEVICE_OK;
    }

    int done_ret = lisa_flash_read_done_hook(&priv->hal_dev, offset, len, ret);
    if (ret == LISA_DEVICE_OK && done_ret != LISA_DEVICE_OK) {
        ret = done_ret;
    }

    return ret;
}

static int venusa_flash_hal_write_raw(venusa_flash_priv_t *priv, size_t offset, const void *data, size_t len)
{
    venusa_flash_dcache_flush(data, len);
    int ret = flash_write(&priv->hal_dev, (off_t)offset, data, len);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash write failed at offset 0x%zx, len %zu: %d", offset, len, ret);
        ret = LISA_DEVICE_ERR_IO;
    } else {
        ret = LISA_DEVICE_OK;
    }

    return ret;
}

static int venusa_flash_hal_erase_raw(venusa_flash_priv_t *priv, size_t offset, size_t size)
{
    int ret = flash_erase(&priv->hal_dev, (off_t)offset, size);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash erase failed at offset 0x%zx, size %zu: %d", offset, size, ret);
        ret = LISA_DEVICE_ERR_IO;
    } else {
        ret = LISA_DEVICE_OK;
    }

    return ret;
}

static int venusa_flash_read(lisa_device_t *dev, size_t offset, void *data, size_t len)
{
    if (!data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;

    ret = check_flash_boundary(priv, offset, len);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(priv);
    ret = venusa_flash_hal_read_wrap(priv, offset, data, len);
    DEVICE_UNLOCK(priv);

    return ret;
}

static int venusa_flash_write(lisa_device_t *dev, size_t offset, const void *data, size_t len)
{
    if (!data || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;

    ret = check_flash_boundary(priv, offset, len);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(priv);

    ret = lisa_flash_write_hook(&priv->hal_dev, offset, len);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    ret = set_write_protection(priv, false);
    if (ret != LISA_DEVICE_OK) {
        goto out_done;
    }

    if (is_buffer_in_flash(data)) {
        uint8_t *buffer = malloc(VENUSA_FLASH_WRITE_BUFFER_SIZE);
        if (!buffer) {
            LISA_LOGE(LOG_TAG, "Failed to allocate %u-byte write buffer", VENUSA_FLASH_WRITE_BUFFER_SIZE);
            ret = LISA_DEVICE_ERR_NO_MEM;
            goto out_protect;
        }

        size_t remaining = len;
        size_t current_offset = offset;
        const uint8_t *src = (const uint8_t *)data;

        while (remaining > 0) {
            size_t chunk_size = remaining > VENUSA_FLASH_WRITE_BUFFER_SIZE ? VENUSA_FLASH_WRITE_BUFFER_SIZE : remaining;
            memcpy(buffer, src, chunk_size);

            ret = venusa_flash_hal_write_raw(priv, current_offset, buffer, chunk_size);
            if (ret != LISA_DEVICE_OK) {
                break;
            }

            remaining -= chunk_size;
            current_offset += chunk_size;
            src += chunk_size;
        }

        free(buffer);
    } else {
        ret = venusa_flash_hal_write_raw(priv, offset, data, len);
    }

out_protect:
#if CONFIG_LISA_FLASH_VENUSA_WRITE_PROTECT_ENABLE
    {
        int protect_ret = set_write_protection(priv, true);
        if (ret == LISA_DEVICE_OK && protect_ret != LISA_DEVICE_OK) {
            ret = protect_ret;
        }
    }
#endif

out_done:
    {
        int done_ret = lisa_flash_write_done_hook(&priv->hal_dev, offset, len, ret);
        if (ret == LISA_DEVICE_OK && done_ret != LISA_DEVICE_OK) {
            ret = done_ret;
        }
    }

    DEVICE_UNLOCK(priv);
    return ret;
}

static int venusa_flash_erase(lisa_device_t *dev, size_t offset, size_t size)
{
    if (size == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;

    ret = check_flash_boundary(priv, offset, size);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = check_erase_alignment(offset, size);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    DEVICE_LOCK(priv);

    ret = lisa_flash_erase_hook(&priv->hal_dev, offset, size);
    if (ret != LISA_DEVICE_OK) {
        DEVICE_UNLOCK(priv);
        return ret;
    }

    ret = set_write_protection(priv, false);
    if (ret == LISA_DEVICE_OK) {
        ret = venusa_flash_hal_erase_raw(priv, offset, size);
    }

#if CONFIG_LISA_FLASH_VENUSA_WRITE_PROTECT_ENABLE
    {
        int protect_ret = set_write_protection(priv, true);
        if (ret == LISA_DEVICE_OK && protect_ret != LISA_DEVICE_OK) {
            ret = protect_ret;
        }
    }
#endif

    {
        int done_ret = lisa_flash_erase_done_hook(&priv->hal_dev, offset, size, ret);
        if (ret == LISA_DEVICE_OK && done_ret != LISA_DEVICE_OK) {
            ret = done_ret;
        }
    }

    DEVICE_UNLOCK(priv);
    return ret;
}

static int venusa_flash_sr_read(lisa_device_t *dev, uint32_t id, uint32_t *value)
{
    if (!value || id < 1 || id > 3) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    ret = flash_status_register_get(&priv->hal_dev, (unsigned char)id, value);
    DEVICE_UNLOCK(priv);

    return ret == 0 ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static int venusa_flash_sr_write(lisa_device_t *dev, uint32_t id, uint32_t value)
{
    if (id < 1 || id > 3) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;
    uint32_t out = 0;

    DEVICE_LOCK(priv);
    ret = set_write_protection(priv, false);
    if (ret == LISA_DEVICE_OK) {
        int hal_ret = flash_status_register_set(&priv->hal_dev, (unsigned char)id, value, &out);
        ret = hal_ret == 0 ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
    }
#if CONFIG_LISA_FLASH_VENUSA_WRITE_PROTECT_ENABLE
    int protect_ret = set_write_protection(priv, true);
    if (ret == LISA_DEVICE_OK && protect_ret != LISA_DEVICE_OK) {
        ret = protect_ret;
    }
#endif
    DEVICE_UNLOCK(priv);

    return ret;
}

static const lisa_flash_parameters_t *venusa_flash_get_parameters(lisa_device_t *dev)
{
    if (check_device_initialized(dev) != LISA_DEVICE_OK) {
        return NULL;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;
    return &priv->parameters;
}

static const lisa_flash_pages_layout_t *venusa_flash_page_layout(lisa_device_t *dev, size_t *layout_size)
{
    if (!layout_size || check_device_initialized(dev) != LISA_DEVICE_OK) {
        return NULL;
    }

    venusa_flash_priv_t *priv = (venusa_flash_priv_t *)dev->priv_data;
    *layout_size = 1;
    return &priv->layout;
}

static const lisa_flash_api_t venusa_flash_api = {
    .read = venusa_flash_read,
    .write = venusa_flash_write,
    .erase = venusa_flash_erase,
    .sr_read = venusa_flash_sr_read,
    .sr_write = venusa_flash_sr_write,
    .get_parameters = venusa_flash_get_parameters,
    .page_layout = venusa_flash_page_layout,
};

static int venusa_flash0_init(void)
{
    memset(&flash0_priv, 0, sizeof(flash0_priv));

#if CONFIG_LISA_FLASH_HOOK_WAITER_ONLY
    /*
     * This image only waits for peer Flash operations. Do not touch the Flash
     * HAL or QSPI state here; the peer core owns Flash initialization/access.
     */
    return LISA_DEVICE_ERR_NOT_READY;
#endif

    flash0_priv.mutex = lisa_mutex_create();
    if (!flash0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

#if CONFIG_LISA_FLASH_VENUSA_DISABLE_DELAY_LINE
    uint32_t delay_cfg = inw(CONFIG_LISA_FLASH_VENUSA_DELAY_LINE_BASE_ADDR);
    delay_cfg &= ~(1UL << 17);
    outw(CONFIG_LISA_FLASH_VENUSA_DELAY_LINE_BASE_ADDR, delay_cfg);
#endif

    flash0_priv.hal_dev.base_addr = CONFIG_LISA_FLASH_VENUSA_CONTROLLER_BASE_ADDR;
    flash0_priv.hal_dev.d_width = CONFIG_LISA_FLASH_VENUSA_DATA_WIDTH;
    flash0_priv.hal_dev.sclk_div = CONFIG_LISA_FLASH_VENUSA_SCLK_DIV;
#if CONFIG_LISA_FLASH_VENUSA_DISABLE_INTERRUPTS
    flash0_priv.hal_dev.run_mod = RUN_WITHOUT_INT;
#else
    flash0_priv.hal_dev.run_mod = RUN_WITH_INT;
#endif
    flash0_priv.hal_dev.timeout = CONFIG_LISA_FLASH_VENUSA_TIMEOUT;
    flash0_priv.hal_dev.addr_bytes = CONFIG_LISA_FLASH_VENUSA_ADDR_BYTES;
#if CONFIG_LISA_FLASH_VENUSA_ADDR_AUTO
    flash0_priv.hal_dev.addr_auto = 1;
#else
    flash0_priv.hal_dev.addr_auto = 0;
#endif

    int ret = lisa_flash_init_hook(&flash0_priv.hal_dev);
    if (ret != LISA_DEVICE_OK) {
        lisa_mutex_delete(flash0_priv.mutex);
        flash0_priv.mutex = NULL;
        return ret;
    }

    ret = flash_init(&flash0_priv.hal_dev, 0, 0);
    int done_ret = lisa_flash_init_done_hook(&flash0_priv.hal_dev,
                                             ret == 0 ? LISA_DEVICE_OK : LISA_DEVICE_ERR_INIT_FAIL);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Flash HAL init failed: %d", ret);
        lisa_mutex_delete(flash0_priv.mutex);
        flash0_priv.mutex = NULL;
        return done_ret != LISA_DEVICE_OK ? done_ret : LISA_DEVICE_ERR_INIT_FAIL;
    }
    if (done_ret != LISA_DEVICE_OK) {
        lisa_mutex_delete(flash0_priv.mutex);
        flash0_priv.mutex = NULL;
        return done_ret;
    }

    size_t flash_size = get_flash_size(&flash0_priv.hal_dev);
    if (flash_size < CONFIG_LISA_FLASH_VENUSA_ERASE_SECTOR_SIZE) {
        LISA_LOGE(LOG_TAG, "Invalid flash size %zu", flash_size);
        lisa_mutex_delete(flash0_priv.mutex);
        flash0_priv.mutex = NULL;
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    flash0_priv.parameters.write_block_size = CONFIG_LISA_FLASH_VENUSA_WRITE_BLOCK_SIZE;
    flash0_priv.parameters.caps.no_explicit_erase = false;
    flash0_priv.parameters.erase_value = 0xFF;

    flash0_priv.layout.pages_size = CONFIG_LISA_FLASH_VENUSA_ERASE_SECTOR_SIZE;
    flash0_priv.layout.pages_count = flash_size / flash0_priv.layout.pages_size;
    flash0_priv.initialized = true;

    LISA_LOGI(LOG_TAG, "Flash0 initialized (layout: %zu pages x %zu bytes, total: %zu bytes)",
              flash0_priv.layout.pages_count, flash0_priv.layout.pages_size,
              flash0_priv.layout.pages_count * flash0_priv.layout.pages_size);

    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(flash0,
                     &venusa_flash_api,
                     &flash0_priv,
                     NULL,
                     venusa_flash0_init,
                     LISA_DEVICE_LEVEL_POST_KERNEL,
                     CONFIG_LISA_FLASH_INIT_PRIORITY);
