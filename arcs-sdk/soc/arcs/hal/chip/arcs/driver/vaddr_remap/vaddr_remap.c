/*
 * vaddr_remap.c
 *
 * Virtual Address Remap — Cipher Region hardware abstraction.
 */

#include "vaddr_remap.h"

#include <string.h>

#include "arcs_ap.h"

#define FLASH_XIP_BASE  0x30000000UL
#define PSRAM_BASE      0x28000000UL

/* Device base address for the selected target */
static uint32_t device_base(vaddr_target_t target)
{
    return (target == VADDR_TARGET_FLASH) ? FLASH_XIP_BASE : PSRAM_BASE;
}

/* Pending configuration state */
static struct {
    vaddr_target_t target;
    uint32_t       offset[VADDR_REGION_MAX];
    bool           encrypt[VADDR_REGION_MAX];
    bool           configured[VADDR_REGION_MAX];
    bool           applied;
} s_cfg;

void vaddr_remap_init(vaddr_target_t target)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.target = target;
}

int32_t vaddr_remap_map(vaddr_region_t region, uint32_t offset)
{
    if (region >= VADDR_REGION_MAX)
        return CSK_DRIVER_ERROR_PARAMETER;

    /* Region A: 64KB alignment (SLV_BASE_ADDR is upper 16 bits) */
    if (region == VADDR_REGION_A && (offset & 0xFFFF) != 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    /* Region B/C/D: 4KB alignment (DEV_OFFSET is 4KB granularity) */
    if (region != VADDR_REGION_A && (offset & 0xFFF) != 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    s_cfg.offset[region]     = offset;
    s_cfg.configured[region] = true;
    s_cfg.applied            = false;

    return CSK_DRIVER_OK;
}

void vaddr_remap_encrypt(vaddr_region_t region, bool enable)
{
    if (region < VADDR_REGION_MAX) {
        s_cfg.encrypt[region] = enable;
        s_cfg.applied = false;
    }
}

int32_t vaddr_remap_apply(void)
{
    volatile CMN_SYSCFG_RegDef *sys = IP_SYSCTRL;
    uint32_t base = device_base(s_cfg.target);

    /* Region A: compute SLV_BASE_ADDR */
    uint32_t slv_base_phys = base;

    if (s_cfg.configured[VADDR_REGION_A])
        slv_base_phys = base + s_cfg.offset[VADDR_REGION_A];

    uint16_t slv_base = (uint16_t)(slv_base_phys >> 16);

    /* Region B/C/D: compute DEV_OFFSET relative to SLV_BASE */
    uint16_t dev_offset[VADDR_REGION_MAX] = {0};

    for (int i = VADDR_REGION_B; i < VADDR_REGION_MAX; i++) {
        if (!s_cfg.configured[i])
            continue;
        uint32_t phys = base + s_cfg.offset[i];
        dev_offset[i] = (uint16_t)((phys - slv_base_phys) >> 12);
        if (dev_offset[i] > 0x7FFF)
            return CSK_DRIVER_ERROR;
    }

    /* Write hardware registers */
    sys->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = (uint32_t)s_cfg.target;

    if (s_cfg.target == VADDR_TARGET_FLASH)
        sys->REG_CIPHER_CTRL0.bit.CIPHER_SLV1_BASE_ADDR = slv_base;
    else
        sys->REG_CIPHER_CTRL0.bit.CIPHER_SLV0_BASE_ADDR = slv_base;

    sys->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = dev_offset[VADDR_REGION_B];
    sys->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_C = dev_offset[VADDR_REGION_C];
    sys->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = dev_offset[VADDR_REGION_D];

    /* Encryption bits */
    sys->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = s_cfg.encrypt[VADDR_REGION_A];
    sys->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = s_cfg.encrypt[VADDR_REGION_B];
    sys->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_C = s_cfg.encrypt[VADDR_REGION_C];
    sys->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = s_cfg.encrypt[VADDR_REGION_D];

    /*
     * Do NOT invalidate DCache here. DCache invalidation destroys
     * PSRAM-resident data (FreeRTOS stacks, log buffers), crashing the
     * system if the caller is running from PSRAM. The caller is
     * responsible for invalidating DCache before jumping to new firmware.
     */
    __asm__ volatile("fence" ::: "memory");

    s_cfg.applied = true;
    return CSK_DRIVER_OK;
}


