/*
 * vaddr_remap.h
 *
 * Virtual Address Remap — hardware abstraction for Cipher Region mapping.
 *
 * The ARCS chip provides 4 cipher regions (A/B/C/D) that map virtual
 * addresses to Flash or PSRAM physical addresses. This module provides
 * a clean API to configure these mappings.
 *
 * Hardware constraints:
 *   - TGT_SLV_SEL is global: all regions share the same target device.
 *   - Region A uses SLV_BASE_ADDR (64KB granularity), no DEV_OFFSET.
 *   - Region B/C/D use DEV_OFFSET (4KB granularity) relative to SLV_BASE.
 *   - Region A must be configured before B/C/D (sets shared base).
 *
 * Usage:
 *   vaddr_remap_init(VADDR_TARGET_FLASH);
 *   vaddr_remap_map(VADDR_REGION_A, 0x20000);   // flash offset 128KB
 *   vaddr_remap_map(VADDR_REGION_B, 0x40000);   // flash offset 256KB
 *   vaddr_remap_apply();
 */

#ifndef INCLUDE_VADDR_REMAP_H_
#define INCLUDE_VADDR_REMAP_H_

#include <stdbool.h>
#include <stdint.h>

#include "Driver_Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cipher region identifier */
typedef enum {
    VADDR_REGION_A = 0,  /* 128MB, uses SLV_BASE_ADDR */
    VADDR_REGION_B = 1,  /* 128MB, uses DEV_OFFSET */
    VADDR_REGION_C = 2,  /*  64MB, uses DEV_OFFSET */
    VADDR_REGION_D = 3,  /*  64MB, uses DEV_OFFSET */
    VADDR_REGION_MAX,
} vaddr_region_t;

/* Fixed virtual base addresses per region (hardware-defined) */
#define VADDR_REGION_A_BASE  0x08000000UL
#define VADDR_REGION_B_BASE  0x10000000UL
#define VADDR_REGION_C_BASE  0x18000000UL
#define VADDR_REGION_D_BASE  0x1C000000UL

/* Target device for address mapping */
typedef enum {
    VADDR_TARGET_PSRAM = 0,  /* Map to PSRAM (SLV0, 0x28000000) */
    VADDR_TARGET_FLASH = 1,  /* Map to Flash (SLV1, 0x30000000) */
} vaddr_target_t;

/*
 * Initialize the remap module and set the global target device.
 *
 * @param target  Flash or PSRAM (TGT_SLV_SEL)
 */
void vaddr_remap_init(vaddr_target_t target);

/*
 * Configure a region to map to a device offset.
 *
 * The offset is relative to the target device base address:
 *   - Flash:  physical = 0x30000000 + offset
 *   - PSRAM:  physical = 0x28000000 + offset
 *
 * Alignment requirements:
 *   - Region A: 64KB aligned (offset & 0xFFFF must be 0)
 *   - Region B/C/D: 4KB aligned (offset & 0xFFF must be 0)
 *
 * Region A determines the shared SLV_BASE_ADDR. Configure it before B/C/D,
 * or vaddr_remap_apply() will fail if B/C/D offsets fall outside the base range.
 *
 * @param region  Target region
 * @param offset  Byte offset within the target device
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid region/alignment
 */
int32_t vaddr_remap_map(vaddr_region_t region, uint32_t offset);

/*
 * Set encryption for a region.
 *
 * Encryption requires efuse key provisioning. On development boards
 * without keys, EN=0 and EN=1 produce identical data.
 *
 * @param region  Target region
 * @param enable  true to enable AES decryption
 */
void vaddr_remap_encrypt(vaddr_region_t region, bool enable);

/*
 * Commit all pending configurations to hardware registers.
 *
 * Writes TGT_SLV_SEL, SLV_BASE_ADDR, DEV_OFFSET, and EN bits.
 * Does NOT invalidate DCache (to avoid destroying PSRAM-resident data).
 *
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR if DEV_OFFSET exceeds 15-bit range
 */
int32_t vaddr_remap_apply(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_VADDR_REMAP_H_ */
