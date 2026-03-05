#include "lisa_flash.h"
#include "arcs_ap.h"
#include "cache.h"
#include <stdint.h>

#define FLASH_DEVICE "flash0"
#define SECTOR_SIZE CONFIG_DISK_FLASH_SECTOR_SIZE
static uint8_t _s_flash_wrap_buf[CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE] __attribute__((section(".psram.data"),aligned(4))) ;
#define IS_IN_FLASH_REGION(addr) \
    ((uintptr_t)(addr) >= (uintptr_t)(CONFIG_DISK_FLASH_CHIP_BASE_ADDR) && \
     (uintptr_t)(addr) <  (uintptr_t)(CONFIG_DISK_FLASH_CHIP_BASE_ADDR) + (uintptr_t)(CONFIG_DISK_FLASH_CHIP_SIZE))

typedef struct
{
    void *flash_dev;
} sflash_dev_t;

static inline int sflash_init(sflash_dev_t *dev, unsigned char ud0, unsigned char ud1)
{
    int ret;

    dev->flash_dev = lisa_device_get(FLASH_DEVICE);

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

        do{
            num = remain > CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE ? CONFIG_DISK_FLASH_ERASE_BLOCK_SIZE : remain;
            memcpy(_s_flash_wrap_buf, data, num);
            ret = lisa_flash_write(dev->flash_dev, offset, _s_flash_wrap_buf, num);
            if(ret != 0){
                break;
            }
            offset += num;
            data += num;
            remain -= num;
        }while(remain > 0);

    }
    else{

        ret = lisa_flash_write(dev->flash_dev, offset, data, len);
        
    }

    return ret;
}

static inline int sflash_read(sflash_dev_t *dev, off_t offset, void *buffer, size_t size)
{
    int ret;
    ret = lisa_flash_read(dev->flash_dev, offset, buffer, size);
    return ret;
}

static int sflash_erase(sflash_dev_t *dev, off_t offset, size_t size)
{
    int ret;

    ret = lisa_flash_erase(dev->flash_dev, offset, size);

    return ret;
}