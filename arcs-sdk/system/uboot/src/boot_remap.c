/*
 * Flash地址映射实现
 * 
 * 只使用 Region B 映射 APP 分区起始地址
 * 其他分区（CP、RES、TONE等）基于 APP 分区偏移，映射后都在 Region B 上
 */

#include "boot_remap.h"
#include "chip.h"
#include "syslog.h"

/*
 * 计算偏移量 (单位转换为4KB页)
 */
static uint32_t calc_offset(uint32_t physical_addr)
{
    if (physical_addr < FLASH_PHYSICAL_BASE) {
        return 0;
    }
    return (physical_addr - FLASH_PHYSICAL_BASE) / 4096;
}

/*
 * 映射 APP 分区到 Region B
 */
int boot_remap_app(uint32_t app_physical_addr)
{
    uint32_t offset = calc_offset(app_physical_addr);
    if (offset > 0x7FFF) {
        printk("remap: offset too large 0x%x (max 0x7FFF)\n", offset);
        return -1;
    }
    
    printk("remap: Region B: virtual=0x%08x -> physical=0x%08x (offset=0x%x)\n",
           REMAP_REGION_B_BASE, app_physical_addr, offset);
    
    /* 配置 Region B 的偏移并启用 */
    IP_SYSCTRL->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = offset & 0x7FFF;
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 1;
    
    /* 确保映射生效 */
    __DSB();
    __ISB();
    
    return 0;
}

/*
 * 禁用 Region B 的映射
 */
void boot_remap_disable(void)
{
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 0;
    
    __DSB();
    __ISB();
}

