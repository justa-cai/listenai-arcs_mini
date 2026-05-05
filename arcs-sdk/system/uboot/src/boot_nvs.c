#include "boot_nvs.h"
#include "boot_flash.h"
#include "chip.h"
#include <string.h>

#define __boot_nvs_ramcode__ __attribute__((section(".boot_f.ramcode")))

#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define ALIGN_UP(x, a)      (((x) + (a) - 1) & ~((a) - 1))

int boot_nvs_init(void)
{
    return boot_flash_init();
}

__boot_nvs_ramcode__ void boot_nvs_read(void *dst, const void *src, size_t len)
{
    boot_flash_read((uint8_t *)src, (uint8_t *)dst, len);
}

__boot_nvs_ramcode__ int boot_nvs_erase(uint32_t addr, size_t size)
{
    if (size == 0) {
        return 0;
    }
    
    /* 对齐到sector */
    uint32_t aligned_addr = ALIGN_DOWN(addr, NVS_SECTOR_SIZE);
    size_t aligned_size = ALIGN_UP(size + (addr - aligned_addr), NVS_SECTOR_SIZE);
    
    /* 使用新芯片的flash擦除接口 */
    return boot_flash_erase((uint8_t *)aligned_addr, aligned_size);
}

__boot_nvs_ramcode__ int boot_nvs_write(uint32_t addr, const void *data, size_t size)
{
    if (data == NULL || size == 0) {
        return 0;
    }
    
    /* 使用新芯片的flash写入接口 */
    return boot_flash_write((uint8_t *)addr, (uint8_t *)data, size);
}

int boot_nvs_erase_write(uint32_t addr, const void *data, size_t size)
{
    int ret;
    
    if (data == NULL || size == 0) {
        return 0;
    }
    
    ret = boot_nvs_erase(addr, size);
    if (ret != 0) {
        return ret;
    }
    
    ret = boot_nvs_write(addr, data, size);
    return ret;
}
