#include "flash_if.h"
#include "arcs_ap.h"
#include <stdint.h>

#define SECTOR_SIZE CONFIG_DISK_FLASH_SECTOR_SIZE
static uint8_t _s_flash_wrap_buf[CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE] __attribute__((section(".psram.data"),aligned(4))) ;
#define IS_IN_FLASH_REGION(addr) \
    ((uintptr_t)(addr) >= (uintptr_t)(CONFIG_DISK_FLASH_CHIP_BASE_ADDR) && \
     (uintptr_t)(addr) <  (uintptr_t)(CONFIG_DISK_FLASH_CHIP_BASE_ADDR) + (uintptr_t)(CONFIG_DISK_FLASH_CHIP_SIZE))

static FLASH_DEV arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF,        // divider is 1
    .run_mod = RUN_WITHOUT_INT, // RUN_WITH_INT//RUN_WITHOUT_INT
    .timeout = 2000000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

typedef struct
{
    void *flash_dev;
} sflash_dev_t;

static inline int sflash_init(sflash_dev_t *dev, unsigned char ud0, unsigned char ud1)
{
    int ret;

    ret = flash_if_init(&arcs_flash_dev, 0, 0);
    if (ret != 0)
    {
        DISK_LOG("Flash init failed,ret:%d", ret);
        return ret;
    }

    dev->flash_dev = &arcs_flash_dev;

    return 0;
}

static inline int sflash_write(sflash_dev_t *dev, off_t offset, const void *data, size_t len)
{
    int ret;
    size_t num;
    size_t remain = len;

    /*如果原始数据在flash中，需要将数据搬运到内存中再进行写入*/
    if (!IS_IN_FLASH_REGION(data))
    {
        flash_if_write_protection_set(false);
        do{
            num = remain > CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE ? CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE : remain;
            memcpy(_s_flash_wrap_buf, data, num);
            ret = flash_if_write(offset, _s_flash_wrap_buf, num);
            if(ret != 0){
                break;
            }
            offset += num;
            data += num;
            remain -= num;
        }while(remain > 0);
        flash_if_write_protection_set(true);
    }
    else{
        flash_if_write_protection_set(false);
        ret = flash_if_write(offset, data, len);
        flash_if_write_protection_set(true);
        
    }

    return ret;
}

static inline int sflash_read(sflash_dev_t *dev, off_t offset, void *buffer, size_t size)
{
    HAL_InvalidateDCache_by_Addr((uint8_t*)CONFIG_DISK_FLASH_CHIP_BASE_ADDR + offset, size);
    memcpy(buffer, (uint8_t*)CONFIG_DISK_FLASH_CHIP_BASE_ADDR + offset, size);
    return 0;
}

static int sflash_erase(sflash_dev_t *dev, off_t offset, size_t size)
{
    int ret;
    flash_if_write_protection_set( false);
    ret = flash_if_erase(offset, size);
    flash_if_write_protection_set( true);
    return ret;
}