#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__

#include <stdbool.h>

#include "stdint.h"
#include "stddef.h"

#define BOOT_PROTECT_AREA_SIZE (256 * 1024)

#define BOOT_CONFIG_AREA_BASE 0x3003d000
#define BOOT_CONFIG_AREA_SIZE (4096)

#ifndef CONFIG_MEM_FLASH_BASE
#define CONFIG_MEM_FLASH_BASE 0x30000000UL
#endif

#define APPLICATION_ADDRESS             CONFIG_APPLICATION_ADDRESS
#define APPLICATION_DYNAMIC_ADDRESS     CONFIG_APPLICATION_DYNAMIC_ADDRESS
/* App image starts immediately after the reserved boot flash area. */
#define BOOT_DEFAULT_APP_ADDRESS        (CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE)
#define BOOT_RECOVERY_REQUEST_MAX_COUNT 5

static inline uint32_t boot_default_app_addr_get(void)
{
    return BOOT_DEFAULT_APP_ADDRESS;
}


#if BOOT_CONFIG_AREA_BASE % 4096 != 0
#error "BOOT_CONFIG_AREA_BASE must be aligned to 4096"
#endif

#if BOOT_CONFIG_AREA_SIZE < 4096
#error "BOOT_CONFIG_AREA_SIZE must be greater than 4096"
#endif

#if BOOT_CONFIG_AREA_SIZE % 4096 != 0
#error "BOOT_CONFIG_AREA_SIZE must be aligned to 4096"
#endif

#define BOOT_CONFIG_VERSION 1
#define BOOT_CONFIG_TAG     0x4C534243

#define FLASH_ERASE_EVERY_TIME 1

struct boot_cfg_hr {
    uint32_t tag;
    uint8_t version;
    uint8_t size;
    uint8_t sum;
} __attribute__((packed));

struct boot_config {
    struct boot_cfg_hr hr;
    uint8_t recover;
} __attribute__((packed));

/* RSVD4 layout：bit 16/18-19 与 boot_control_store_*.c 的 BOOT_MODE /
 * UPGRADE_STATUS 共址，新增字段必须避开这两个区域，并相应收紧
 * boot_control_store_*.c 里的 SHIFT/MASK，避免误覆盖具名 bit。 */
struct boot_info {
    uint32_t reboot_cnt: 8;
    uint32_t recover_reason: 8;
    uint32_t ota_pending: 1;            /* bit 16，BOOT_MODE bit 0 别名 */
    uint32_t shutdown_req: 1;           /* bit 17 */
    uint32_t upgrade_status: 2;         /* bits 18-19，boot_control_store UPGRADE_STATUS 共址 */
    uint32_t charging_wait: 1;          /* bit 20 */
    uint32_t resume_normal_boot: 1;     /* bit 21 */
    uint32_t reserved: 7;
    uint32_t handshake_timeout: 1;
    uint32_t boot_wdt: 1;
    uint32_t req: 1;
};

enum {
    RECOVER_REASON_NONE = 0,
    RECOVER_REASON_SOFT_REQ,
    RECOVER_REASON_HARD_REQ,
    RECOVER_REASON_APP_INVALID,
    RECOVER_REASON_AP_WDT_TIMEOUT,
    RECOVER_REASON_BOOT_WDT_TIMEOUT,
    RECOVER_REASON_MAX,
};

static inline struct boot_cfg_hr *boot_config_load_by_addr(uint8_t *addr)
{
    struct boot_cfg_hr *cfg = (struct boot_cfg_hr *)addr;
    if (cfg->tag != BOOT_CONFIG_TAG) {
        return NULL;
    }

    return cfg;
}

static inline const struct boot_cfg_hr *boot_config_load(uint8_t **next_addr)
{
    uint8_t *base = (uint8_t *)BOOT_CONFIG_AREA_BASE;
    uint8_t *end = base + BOOT_CONFIG_AREA_SIZE;
    struct boot_cfg_hr *cfg;
    struct boot_cfg_hr *cfg_next;

    cfg = boot_config_load_by_addr(base);

#if !FLASH_ERASE_EVERY_TIME
    while (cfg != NULL && base < end) {
        base += sizeof(struct boot_cfg_hr);
        base += cfg->size;

        cfg_next = boot_config_load_by_addr(base);
        if (cfg_next == NULL) {
            break;
        }

        cfg = cfg_next;
    }
#endif

    if (next_addr) {
        *next_addr = base;
    }

    return cfg;
}

static inline bool boot_config_is_valid(const struct boot_config *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if (cfg->hr.tag != BOOT_CONFIG_TAG) {
        return false;
    }

    if (cfg->hr.version != BOOT_CONFIG_VERSION) {
        return false;
    }

    if (cfg->hr.size != (sizeof(struct boot_config) - sizeof(struct boot_cfg_hr))) {
        return false;
    }

    return true;
}

static inline const struct boot_config *boot_config_load_valid(uint8_t **next_addr)
{
    const struct boot_config *cfg = (const struct boot_config *)boot_config_load(next_addr);

    if (!boot_config_is_valid(cfg)) {
        return NULL;
    }

    return cfg;
}

static inline bool boot_config_has_recovery_request(const struct boot_config *cfg)
{
    return boot_config_is_valid(cfg) && cfg->recover != 0;
}

#ifdef CONFIG_BOOT_VADDR_REMAP

/* Runtime partition descriptor stored in AON register */
struct boot_partition_info {
    uint32_t region   : 4;   /* vaddr_region_t: 0=A, 1=B, 2=C, 3=D */
    uint32_t target   : 4;   /* vaddr_target_t: 0=PSRAM, 1=Flash */
    uint32_t offset   : 20;  /* flash/psram offset >> 12 (4KB units, max 4GB) */
    uint32_t valid    : 4;   /* 0xA = valid marker */
};

#define BOOT_PARTITION_VALID_MARKER  0xA

#endif

/* 注意: 不要在BOOT第一阶段使用以下API */
void boot_config_init(void);
void boot_config_set_recover(uint8_t mode);
int boot_config_save(void);
int boot_recovery_hardware_enter(void);
int boot_recovery_hardware_exit(void);
void boot_recovery_software_enter(void);

#endif
