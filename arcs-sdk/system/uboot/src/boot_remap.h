#ifndef __BOOT_REMAP_H__
#define __BOOT_REMAP_H__

#include <stdint.h>

/*
 * Flash地址映射
 * 
 * 只使用 Region B 进行地址映射：
 * - Region B: 0x10000000 - 0x17FFFFFF (128MB)
 * 
 * 只映射 APP 分区的起始地址，其他分区（CP、RES、TONE等）基于 APP 分区偏移，
 * 映射后都在 Region B 上。
 */

/*
 * Region B 虚拟地址范围定义
 */
#define REMAP_REGION_B_BASE     0x10000000UL
#define REMAP_REGION_B_SIZE     0x08000000UL  /* 128MB */

/*
 * Flash物理基址
 */
#define FLASH_PHYSICAL_BASE     0x30000000UL

/*
 * 映射 APP 分区到 Region B
 * @param app_physical_addr APP 分区的物理地址（base字段）
 * @return 0成功，-1失败
 */
int boot_remap_app(uint32_t app_physical_addr);

/*
 * 禁用 Region B 的映射
 */
void boot_remap_disable(void);


#endif /* __BOOT_REMAP_H__ */
