#include "boot_control_store.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arcs_ap.h"
#include "boot_config.h"
#include "boot_env.h"
#include "boot_flash.h"

#ifndef CONFIG_MEM_FLASH_BASE
#define CONFIG_MEM_FLASH_BASE 0x30000000UL
#endif

#define BOOT_CONTROL_STORE_SECTOR_SIZE 0x1000

/* RSVD4 bit 16 起：BOOT_MODE 1 bit（NORMAL/UPDATE）+ UPGRADE_STATUS 2 bit
 * （NONE/IN_PROGRESS/SUCCESS/FAILED），mask 严格按枚举范围，以免覆盖
 * boot_config.h 里在更高 bit 上后加的具名字段。 */
#define BOOT_INFO_BOOT_MODE_SHIFT       0
#define BOOT_INFO_BOOT_MODE_MASK        0x1
#define BOOT_INFO_UPGRADE_STATUS_SHIFT  2
#define BOOT_INFO_UPGRADE_STATUS_MASK   0x3

#define BOOT_CONTROL_STORE_RESERVED_SIZE                                                   \
    (BOOT_CONTROL_STORE_SECTOR_SIZE -                                                     \
     (sizeof(uint32_t) * 3 + sizeof(boot_ota_request_record_t) + sizeof(uboot_ota_failure_info_t)))

typedef enum {
    UPGRADE_STATUS_NONE = 0,
    UPGRADE_STATUS_IN_PROGRESS = 1,
    UPGRADE_STATUS_SUCCESS = 2,
    UPGRADE_STATUS_FAILED = 3,
} upgrade_status_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    boot_ota_request_record_t ota_request;
    uboot_ota_failure_info_t ota_failure_info;
    uint32_t crc32;
    uint8_t reserved[BOOT_CONTROL_STORE_RESERVED_SIZE];
} __attribute__((packed)) boot_control_store_flash_t;

_Static_assert(sizeof(boot_control_store_flash_t) == BOOT_CONTROL_STORE_SECTOR_SIZE,
               "boot_control_store_flash_t must be 4KB");

static uint32_t calculate_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

static uint8_t *boot_control_store_flash_addr(void)
{
    return (uint8_t *)CONFIG_BOOT_CONTROL_STORE_BASE_ADDR;
}

static void set_failure_none(uboot_ota_failure_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->reason = UBOOT_OTA_FAILURE_NONE;
    info->detail = UBOOT_OTA_FAILURE_DETAIL_NONE;
}

static void write_failure_info(boot_control_store_flash_t *store,
                               const uboot_ota_failure_info_t *info)
{
    memcpy((uint8_t *)store + offsetof(boot_control_store_flash_t, ota_failure_info),
           info,
           sizeof(*info));
}

static void read_failure_info(const boot_control_store_flash_t *store, uboot_ota_failure_info_t *info)
{
    memcpy(info,
           (const uint8_t *)store + offsetof(boot_control_store_flash_t, ota_failure_info),
           sizeof(*info));
}

static void init_store(boot_control_store_flash_t *store)
{
    uboot_ota_failure_info_t info;

    memset(store, 0xFF, sizeof(*store));
    store->magic = BOOT_CONTROL_STORE_MAGIC;
    store->version = BOOT_CONTROL_STORE_VERSION;
    set_failure_none(&info);
    write_failure_info(store, &info);
}

static int read_store(boot_control_store_flash_t *store)
{
    uint8_t *flash_addr = boot_control_store_flash_addr();
    uint32_t crc;

    boot_flash_read(flash_addr, (uint8_t *)store, sizeof(*store));

    if (store->magic != BOOT_CONTROL_STORE_MAGIC || store->version != BOOT_CONTROL_STORE_VERSION) {
        return -1;
    }

    crc = calculate_crc32(store, offsetof(boot_control_store_flash_t, crc32));
    if (crc != store->crc32) {
        return -1;
    }

    return 0;
}

static int write_store(boot_control_store_flash_t *store)
{
    uint8_t *flash_addr = boot_control_store_flash_addr();

    store->crc32 = calculate_crc32(store, offsetof(boot_control_store_flash_t, crc32));

    if (boot_flash_erase(flash_addr, BOOT_CONTROL_STORE_SECTOR_SIZE) != 0) {
        return -1;
    }

    if (boot_flash_write(flash_addr, (uint8_t *)store, sizeof(*store)) != 0) {
        return -1;
    }

    return 0;
}

static void set_boot_mode_reg(boot_mode_t mode)
{
    uint32_t raw = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    uint32_t reserved = (raw >> 16) & 0x1FFF;

    reserved &= ~(BOOT_INFO_BOOT_MODE_MASK << BOOT_INFO_BOOT_MODE_SHIFT);
    reserved |= ((uint32_t)mode & BOOT_INFO_BOOT_MODE_MASK) << BOOT_INFO_BOOT_MODE_SHIFT;

    raw = (raw & 0xE000FFFF) | (reserved << 16);
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = raw;
}

static boot_mode_t get_boot_mode_reg(void)
{
    uint32_t raw = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    uint32_t reserved = (raw >> 16) & 0x1FFF;
    uint32_t mode = (reserved >> BOOT_INFO_BOOT_MODE_SHIFT) & BOOT_INFO_BOOT_MODE_MASK;

    return (boot_mode_t)mode;
}

static void set_upgrade_status_reg(upgrade_status_t status)
{
    uint32_t raw = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    uint32_t reserved = (raw >> 16) & 0x1FFF;

    reserved &= ~(BOOT_INFO_UPGRADE_STATUS_MASK << BOOT_INFO_UPGRADE_STATUS_SHIFT);
    reserved |= ((uint32_t)status & BOOT_INFO_UPGRADE_STATUS_MASK) << BOOT_INFO_UPGRADE_STATUS_SHIFT;

    raw = (raw & 0xE000FFFF) | (reserved << 16);
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = raw;
}

int boot_control_store_save(const boot_ota_request_record_t *record)
{
    boot_control_store_flash_t store;

    if (record == NULL) {
        return -1;
    }

    if (read_store(&store) != 0) {
        init_store(&store);
    }

    store.ota_request = *record;
    return write_store(&store);
}

int boot_control_store_load(boot_ota_request_record_t *record)
{
    boot_control_store_flash_t store;

    if (record == NULL) {
        return -1;
    }

    if (read_store(&store) != 0) {
        return -1;
    }

    *record = store.ota_request;
    return 0;
}

int boot_control_store_clear(void)
{
    boot_control_store_flash_t store;

    if (read_store(&store) != 0) {
        init_store(&store);
    }

    memset(&store.ota_request, 0, sizeof(store.ota_request));
    if (write_store(&store) != 0) {
        return -1;
    }

    set_boot_mode_reg(BOOT_MODE_NORMAL);
    set_upgrade_status_reg(UPGRADE_STATUS_NONE);
    return 0;
}

int boot_control_store_get_mode(boot_mode_t *mode)
{
    if (mode == NULL) {
        return -1;
    }

    *mode = get_boot_mode_reg();
    return 0;
}

int boot_control_store_set_mode(boot_mode_t mode)
{
    set_boot_mode_reg(mode);

    if (mode == BOOT_MODE_UPDATE) {
        set_upgrade_status_reg(UPGRADE_STATUS_IN_PROGRESS);
    } else {
        set_upgrade_status_reg(UPGRADE_STATUS_NONE);
    }

    return 0;
}

int boot_control_store_get_failure(uboot_ota_failure_info_t *info)
{
    boot_control_store_flash_t store;

    if (info == NULL) {
        return -1;
    }

    if (read_store(&store) != 0) {
        set_failure_none(info);
        return 0;
    }

    read_failure_info(&store, info);
    return 0;
}

int boot_control_store_set_failure(const uboot_ota_failure_info_t *info)
{
    boot_control_store_flash_t store;

    if (info == NULL) {
        return -1;
    }

    if (read_store(&store) != 0) {
        init_store(&store);
    }

    write_failure_info(&store, info);
    return write_store(&store);
}

int boot_control_store_clear_failure(void)
{
    boot_control_store_flash_t store;
    uboot_ota_failure_info_t info;

    if (read_store(&store) != 0) {
        init_store(&store);
    }

    set_failure_none(&info);
    write_failure_info(&store, &info);
    return write_store(&store);
}
