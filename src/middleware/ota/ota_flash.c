#define TAG "ota_flash"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "lisa_log.h"
#include "mbedtls/md5.h"
#include "lisa_flash.h"
#include "arcs_ap_base.h"

#include "cache.h"

#include "lisa_uart.h"

#ifdef CONFIG_OTA_MODEM_LOCK
#include "at_client.h"
#endif

#include "ota_flash.h"
#include "ota_api.h"

#define SIZE_K(x) ((x) * 1024)
#define SIZE_M(x) (SIZE_K(x) * 1024)

typedef struct {
    uint32_t addr;
    uint32_t size;
} ota_partition_t;

static const ota_partition_t partition_map[] = {
    [OTA_PART_WAKE_WORD_BIN] =
        {
            .addr = 0x00200000,
            .size = SIZE_K(1536),
        },
    [OTA_PART_PROMPT_TONE_BIN] =
        {
            .addr = 0x00100000,
            .size = SIZE_M(1),
        },
    [OTA_PART_EMOJI_BIN] =
        {
            .addr = 0x00380000,
            .size = SIZE_K(768),
        },
    [OTA_PART_APP_STAGING] =
        {
            .addr = 0x00A00000,
            .size = SIZE_K(5120),
        },
};

int ota_flash_verify(ota_partition_id_e part, const char *md5, uint32_t size)
{
    char calc_md5[OTA_RES_MD5_LEN];
    const ota_partition_t *partition = &partition_map[part];

    if (size > partition->size) {
        LISA_LOGE(TAG, "Partition %d size too large: %u > %u", part, size, partition->size);
        return -1;
    }

    HAL_InvalidateDCache_by_Addr((uint32_t *)(CMN_FLASH_REGION + partition->addr), size);
    mbedtls_md5((const uint8_t *)(CMN_FLASH_REGION + partition->addr), size, calc_md5);
    LISA_LOGI(TAG, "Partition %d MD5 actual: " MD5_PRI, part, MD5_ARG(calc_md5));
    LISA_LOGI(TAG, "Partition %d MD5 expect: " MD5_PRI, part, MD5_ARG(md5));

    int mismatch = memcmp(md5, calc_md5, OTA_RES_MD5_LEN) != 0;
    LISA_LOGI(TAG, "Partition %d %s", part, mismatch ? "mismatch" : "match");

    return mismatch;
}

int ota_flash_get(ota_partition_id_e part, const void **data, uint32_t *size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (data) {
        *data = (const void *)(CMN_FLASH_REGION + partition->addr);
    }

    if (size) {
        *size = partition->size;
    }

    return 0;
}

/* Flash 写以 256 字节（1 页）为单位，间歇 yield 让 ISR 有机会运行 */
#define OTA_FLASH_PAGE_SIZE 256
/* 预擦除块大小：一次性擦 64KB，避免频繁小擦除 */
#define OTA_PRE_ERASE_SIZE (64 * 1024)
/* Erase 前排空 RX：等待 UART 空闲的最大时间（ms）。
 * 仅用于下载开始前的预擦除阶段（modem 空闲）。下载中途用 LOCK_QUICK_DELAY_MS。 */
#define OTA_DRAIN_IDLE_TIMEOUT_MS 5000
/* 下载中途锁 AT 后的短暂延时（ms）：让已在途的 UART 数据落定即可，
 * 不等待 idle——modem 持续发 URC，永远等不到。 */
#define OTA_LOCK_QUICK_DELAY_MS 20
/* 页间 yield 时间（ms） */
#define OTA_PAGE_YIELD_MS 5

static struct {
    bool in_progress;
    SemaphoreHandle_t mutex;
    lisa_device_t *flash;
    ota_partition_id_e part;
    uint32_t offset;
    uint32_t total_size;
    uint32_t buffer_len;
    uint8_t buffer[4096];
    uint32_t erased_start;   /* 当前已预擦区域的起始（绝对 Flash 地址） */
    uint32_t erased_end;     /* 当前已预擦区域的末尾（绝对 Flash 地址，不含） */
} flash_update = {.in_progress = false};

static inline void ota_flash_lock_and_drain(void)
{
#ifdef CONFIG_OTA_MODEM_LOCK
    at_cmd_tx_pause();
    lisa_device_t *uart_dev = lisa_device_get("uart2");
    if (uart_dev) {
        int ret = lisa_uart_rx_wait_idle(uart_dev, OTA_DRAIN_IDLE_TIMEOUT_MS);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGW(TAG, "UART idle drain returned %d, proceeding anyway", ret);
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
#endif
}

/**
 * @brief 下载中途快速锁 AT：暂停 AT 发送 + 短暂延时让在途数据落定。
 *        不等待 UART idle——modem 持续发 URC，永远等不到，
 *        等久了反而导致 modem TCP 缓冲区溢出丢数据。
 */
static inline void ota_flash_lock_quick(void)
{
#ifdef CONFIG_OTA_MODEM_LOCK
    at_cmd_tx_pause();
    vTaskDelay(pdMS_TO_TICKS(OTA_LOCK_QUICK_DELAY_MS));
#endif
}

static inline void ota_flash_unlock(void)
{
#ifdef CONFIG_OTA_MODEM_LOCK
    at_cmd_tx_resume();
#endif
}

/**
 * @brief 确保 Flash 目标区域已预擦。若未擦，执行大块擦除。
 *        调用前需由上层持有 AT 锁（ota_flash_lock_and_drain），
 *        以确保擦除和后续写入期间模组不会产生新的 UART 数据。
 *
 * @param flash      Flash 设备
 * @param flash_addr 需要写入的绝对 Flash 地址
 * @param len        需要写入的长度
 * @return 0 成功，<0 失败
 */
static int ota_flash_ensure_erased(lisa_device_t *flash, uint32_t flash_addr, uint32_t len)
{
    uint32_t need_end = flash_addr + len;

    /* 已在预擦区域内，无需再擦 */
    if (flash_addr >= flash_update.erased_start && need_end <= flash_update.erased_end) {
        return 0;
    }

    /* 计算新的大块擦除范围（64KB 对齐） */
    uint32_t block_start = flash_addr & ~(OTA_PRE_ERASE_SIZE - 1);
    uint32_t block_size = OTA_PRE_ERASE_SIZE;

    LISA_LOGI(TAG, "Pre-erasing 64KB block at 0x%08X", block_start);

    int ret = lisa_flash_erase(flash, block_start, block_size);
    if (ret != 0) {
        LISA_LOGE(TAG, "Pre-erase failed at 0x%08X (%d)", block_start, ret);
        return ret;
    }

    flash_update.erased_start = block_start;
    flash_update.erased_end = block_start + block_size;

    return 0;
}

/**
 * @brief 按页（256B）写入 Flash，每页之间 yield 让 ISR 有机会搬 FIFO。
 *        调用前需确保目标区域已擦除，且由上层持有 AT 锁。
 */
static int ota_flash_write_paged(lisa_device_t *flash, uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t written = 0;
    while (written < len) {
        uint32_t chunk = len - written;
        if (chunk > OTA_FLASH_PAGE_SIZE) {
            chunk = OTA_FLASH_PAGE_SIZE;
        }

        int ret = lisa_flash_write(flash, addr + written, data + written, chunk);
        if (ret != 0) {
            return ret;
        }
        written += chunk;

        if (written < len) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    return 0;
}

int ota_flash_update_begin(ota_partition_id_e part, uint32_t total_size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (total_size != OTA_FLASH_SIZE_UNKNOWN && total_size > partition->size) {
        LISA_LOGE(TAG, "Partition %d size too small: %u < %u", part, partition->size, total_size);
        return -1;
    }

    lisa_device_t *flash = lisa_device_get("flash0");
    if (!flash) {
        LISA_LOGE(TAG, "Flash device not found");
        return -1;
    }

    flash_update.in_progress = true;
    flash_update.mutex = xSemaphoreCreateMutex();
    flash_update.flash = flash;
    flash_update.part = part;
    flash_update.offset = 0;
    flash_update.total_size = total_size;
    flash_update.buffer_len = 0;
    /* 下载开始前预擦除整个分区。此时 modem 空闲，即使 erase 禁用中断
     * ~1.3s 也不会丢数据。下载中途 ensure_erased 全部走快速路径。 */
    {
        uint32_t erase_len = (total_size != OTA_FLASH_SIZE_UNKNOWN) ? total_size : partition->size;
        uint32_t aligned_start = partition->addr & ~(OTA_PRE_ERASE_SIZE - 1);
        uint32_t aligned_end = (partition->addr + erase_len + OTA_PRE_ERASE_SIZE - 1) & ~(OTA_PRE_ERASE_SIZE - 1);

        LISA_LOGI(TAG, "Partition %d: pre-erasing %u bytes from 0x%08X (%u blocks)",
                  part, aligned_end - aligned_start, aligned_start,
                  (aligned_end - aligned_start) / OTA_PRE_ERASE_SIZE);

        ota_flash_lock_and_drain();

        for (uint32_t addr = aligned_start; addr < aligned_end; addr += OTA_PRE_ERASE_SIZE) {
            int ret = lisa_flash_erase(flash, addr, OTA_PRE_ERASE_SIZE);
            if (ret != 0) {
                LISA_LOGE(TAG, "Pre-erase failed at 0x%08X (%d)", addr, ret);
                flash_update.erased_start = aligned_start;
                flash_update.erased_end = (addr > aligned_start) ? addr : 0;
                ota_flash_unlock();
                if (flash_update.erased_end == 0) {
                    flash_update.in_progress = false;
                    vSemaphoreDelete(flash_update.mutex);
                    return -1;
                }
                /* 部分失败：已擦区域仍可用，剩余由 ensure_erased 补齐 */
                LISA_LOGW(TAG, "Pre-erase partial: erased [0x%08X, 0x%08X)",
                          flash_update.erased_start, flash_update.erased_end);
                return 0;
            }
        }

        flash_update.erased_start = aligned_start;
        flash_update.erased_end = aligned_end;
        ota_flash_unlock();
    }

    return 0;
}

int ota_flash_update_step(ota_partition_id_e part, uint32_t offset, const uint8_t *data, uint32_t size)
{
    const ota_partition_t *partition = &partition_map[part];

    if (!flash_update.in_progress || flash_update.part != part) {
        LISA_LOGE(TAG, "Update for partition %d not in progress", part);
        return -1;
    }

    if (offset != flash_update.offset) {
        LISA_LOGE(TAG, "Unexpected offset %u, expected %u", offset, flash_update.offset);
        return -1;
    }

    if (offset + size > partition->size) {
        LISA_LOGE(TAG, "Write exceeds partition %d size: %u + %u > %u", part, offset, size, partition->size);
        return -1;
    }

    if (size > sizeof(flash_update.buffer)) {
        LISA_LOGE(TAG, "Write size too large: %u > %zu", size, sizeof(flash_update.buffer));
        return -1;
    }

    if (flash_update.total_size != OTA_FLASH_SIZE_UNKNOWN && offset + size > flash_update.total_size) {
        LISA_LOGE(TAG, "Write exceeds update size: %u + %u > %u", offset, size, flash_update.total_size);
        return -1;
    }

    LISA_LOGI(TAG, "Updating partition %d at offset %u, size %u, current buffered %u", part, offset, size,
              flash_update.buffer_len);

    const uint8_t *src = data;
    uint32_t remaining = size;

    xSemaphoreTake(flash_update.mutex, portMAX_DELAY);

    while (remaining > 0) {
        uint32_t capacity = sizeof(flash_update.buffer) - flash_update.buffer_len;
        uint32_t chunk = remaining < capacity ? remaining : capacity;

        memcpy(flash_update.buffer + flash_update.buffer_len, src, chunk);
        flash_update.buffer_len += chunk;
        flash_update.offset += chunk;
        src += chunk;
        remaining -= chunk;

        if (flash_update.buffer_len == sizeof(flash_update.buffer)) {
            uint32_t write_offset = flash_update.offset - flash_update.buffer_len;
            uint32_t flash_addr = partition->addr + write_offset;
            int ret;

            /* 暂停 AT 发送，覆盖擦除和写入全过程。
             * 不等待 UART idle——下载中途 modem 持续发 URC 等不到，
             * 等久了反而导致 modem TCP 缓冲区溢出丢数据。 */
            ota_flash_lock_quick();

            ret = ota_flash_ensure_erased(flash_update.flash, flash_addr, sizeof(flash_update.buffer));
            if (ret != 0) {
                LISA_LOGE(TAG, "Pre-erase failed at offset %u (%d)", write_offset, ret);
                ota_flash_unlock();
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            ret = ota_flash_write_paged(flash_update.flash, flash_addr, flash_update.buffer,
                                        sizeof(flash_update.buffer));
            if (ret != 0) {
                LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, ret);
                ota_flash_unlock();
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            ota_flash_unlock();

            flash_update.buffer_len = 0;
            LISA_LOGI(TAG, "Partition %d wrote to offset %u, buffer remaining %u", part,
                      flash_update.offset - flash_update.buffer_len, flash_update.buffer_len);
        }
    }

    xSemaphoreGive(flash_update.mutex);

    return 0;
}

int ota_flash_update_finish(ota_partition_id_e part)
{
    const ota_partition_t *partition = &partition_map[part];

    if (!flash_update.in_progress || flash_update.part != part) {
        LISA_LOGE(TAG, "Update for partition %d not in progress", part);
        return -1;
    }

    if (flash_update.total_size != OTA_FLASH_SIZE_UNKNOWN && flash_update.offset != flash_update.total_size) {
        LISA_LOGE(TAG, "Update incomplete: %u / %u", flash_update.offset, flash_update.total_size);
        return -1;
    }

    if (flash_update.buffer_len > 0) {
        uint32_t write_offset = flash_update.offset - flash_update.buffer_len;
        int ret;

        xSemaphoreTake(flash_update.mutex, portMAX_DELAY);

        {
            uint32_t flash_addr = partition->addr + write_offset;

            /* 暂停 AT 发送，覆盖擦除和写入全过程 */
            ota_flash_lock_quick();

            ret = ota_flash_ensure_erased(flash_update.flash, flash_addr, flash_update.buffer_len);
            if (ret != 0) {
                LISA_LOGE(TAG, "Pre-erase failed at offset %u (%d)", write_offset, ret);
                ota_flash_unlock();
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            ret = ota_flash_write_paged(flash_update.flash, flash_addr, flash_update.buffer,
                                        flash_update.buffer_len);
            if (ret != 0) {
                LISA_LOGE(TAG, "Write partition %d failed at offset %u (%d)", part, write_offset, ret);
                ota_flash_unlock();
                xSemaphoreGive(flash_update.mutex);
                return -1;
            }

            ota_flash_unlock();
        }

        xSemaphoreGive(flash_update.mutex);

        flash_update.buffer_len = 0;
    }

    vSemaphoreDelete(flash_update.mutex);
    flash_update.in_progress = false;

    return (int)flash_update.offset;
}

int ota_flash_update_abort(ota_partition_id_e part)
{
    if (!flash_update.in_progress || flash_update.part != part) {
        LISA_LOGW(TAG, "No update in progress for partition %d to abort", part);
        return -1;
    }

    /* 丢弃缓冲区中的未刷数据，只释放互斥锁和标记会话结束 */
    flash_update.buffer_len = 0;
    if (flash_update.mutex) {
        vSemaphoreDelete(flash_update.mutex);
        flash_update.mutex = NULL;
    }
    flash_update.in_progress = false;

    LISA_LOGI(TAG, "Partition %d update aborted (offset consumed=%u)", part, flash_update.offset);

    return 0;
}
