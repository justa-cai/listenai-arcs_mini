#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "chip.h"
#include "spiflash.h"
#include "arcs_ap.h"
#include "stdio.h"
#include "string.h"

#include "boot_config.h"
#include "boot_flash.h"

static struct boot_config cfg_cache = {0};
static uint32_t cfg_next_save_addr = BOOT_CONFIG_AREA_BASE;


void boot_config_init(void)
{
    uint8_t *base;
    const struct boot_cfg_hr *cfg;
    cfg = boot_config_load(&base);

    cfg_next_save_addr = (uint32_t)base;

    if (cfg == NULL) {
        printf("boot config: cfg not found, use default\n");
        memset(&cfg_cache, 0, sizeof(struct boot_config));
        cfg_cache.hr.tag = BOOT_CONFIG_TAG;
        cfg_cache.hr.version = BOOT_CONFIG_VERSION;
        cfg_cache.hr.size = sizeof(struct boot_config) - sizeof(struct boot_cfg_hr);
        cfg_cache.hr.sum = 0;
    } else {
        memcpy(&cfg_cache, cfg, sizeof(struct boot_config));
    }

    printf("boot config: init, recover = %d\n", cfg_cache.recover);
}

void boot_config_set_recover(uint8_t mode)
{
    cfg_cache.recover = mode;
}

int boot_config_save(void)
{
    uint8_t *base = (uint8_t *)cfg_next_save_addr;
    uint8_t *end = base + BOOT_CONFIG_AREA_SIZE;
    bool need_erase = false;
    int r;

    printf("boot config: save\n");

    if (memcmp(base, &cfg_cache, sizeof(struct boot_config)) == 0) {
        printf("boot config save, same config\n");
        return 0;
    }

    printf("boot config: save, base = 0x%08x, size = %d\n", base, sizeof(struct boot_config));

#if FLASH_ERASE_EVERY_TIME
    need_erase = true;
#else
    if ((base + sizeof(struct boot_config)) >= end) {
        /* erase flash */
        need_erase = true;
    }
#endif
    if (need_erase) {
        base = (uint8_t *)BOOT_CONFIG_AREA_BASE;
        r = boot_flash_erase(base, BOOT_CONFIG_AREA_SIZE);
        if (r != 0) {
            printf("boot config: flash erase failed, r = %d\n", r);
            return r;
        }
    }

    /* save */
    r = boot_flash_write(base, (uint8_t *)&cfg_cache, sizeof(struct boot_config));
    if (r != 0) {
        printf("boot config: flash write failed, r = %d\n", r);
        return r;
    }
    cfg_next_save_addr = (uint32_t)base + sizeof(struct boot_config);

    return 0;
}

void boot_recovery_software_enter(void)
{
    /* set recovery request */
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    info->req = 1;
}

int boot_recovery_hardware_enter(void)
{
    boot_config_set_recover(true);
    boot_flash_lock_clear();
    int r = boot_config_save();
    boot_flash_lock_resume();
    if (r != 0) {
        boot_config_set_recover(false);
    }

    return r;
}

int boot_recovery_hardware_exit(void)
{
    boot_config_set_recover(false);
    boot_flash_lock_clear();
    int r = boot_config_save();
    boot_flash_lock_resume();
    if (r != 0) {
        boot_config_set_recover(true);
    }

    return r;
}
