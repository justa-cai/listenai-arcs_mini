/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include "fs_env/fs_env.h"
#include "disk/disk.h"
#include "sdmmc_arcs.h"
#include "disk/iocache.h"

static struct disk_info sdmmc_disk_raw;
static struct disk_info sdmmc_disk;

static int disk_sdmmc_access_init(struct disk_info *disk)
{
    int ret;

    ret = sdmmc_init();

    if (ret != 0)
    {
        return -EIO;
    }

    return 0;
}

static int disk_sdmmc_access_status(struct disk_info *disk)
{
    return DISK_STATUS_OK;
}

#if CONFIG_DISK_SDMMC_ACCESS_BUFFER_ALIGN_ENABLE

#define DISK_SECTOR_SIZE 512
__attribute__((section(".psram.data"),aligned(CONFIG_DISK_SDMMC_ACCESS_BUFFER_ALIGN_SIZE))) 
static uint8_t read_wrap_buffer[CONFIG_DISK_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

__attribute__((section(".psram.data"),aligned(CONFIG_DISK_SDMMC_ACCESS_BUFFER_ALIGN_SIZE))) 
static uint8_t write_wrap_buffer[CONFIG_DISK_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

static int disk_sdmmc_access_read(struct disk_info *disk, uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint32_t sectors_per_wrap = CONFIG_DISK_SDMMC_ACCESS_WRAP_BUFFER_SIZE / DISK_SECTOR_SIZE;
    
    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }

    if ((uint32_t)buff % CONFIG_DISK_SDMMC_ACCESS_BUFFER_ALIGN_SIZE != 0) {
        uint32_t remaining = count;
        
        while (remaining > 0) {
            
            uint32_t sectors_to_read = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;
            ret = gm_sdc_api_sdcard_sector_read(SD_0, sector, sectors_to_read, read_wrap_buffer);
            if (ret != 0) {
                return -EIO;
            }
            
            memcpy(buff, read_wrap_buffer, sectors_to_read * DISK_SECTOR_SIZE);
            buff += sectors_to_read * DISK_SECTOR_SIZE;
            sector += sectors_to_read;
            remaining -= sectors_to_read;
        }

    } else {
        ret = gm_sdc_api_sdcard_sector_read(SD_0, sector, count, buff);
        {volatile uint32_t vdata;vdata = *(volatile uint32_t*)buff;}
        if (ret != 0) {
            return -EIO;
        }
    }
    return 0;
}

static int disk_sdmmc_access_write(struct disk_info *disk, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint32_t sectors_per_wrap = CONFIG_DISK_SDMMC_ACCESS_WRAP_BUFFER_SIZE / DISK_SECTOR_SIZE;

    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }

    if ((uint32_t)buff % CONFIG_DISK_SDMMC_ACCESS_BUFFER_ALIGN_SIZE != 0) {
        uint32_t remaining = count;
        
        while (remaining > 0) {
            uint32_t sectors_to_write = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;
            
            memcpy(write_wrap_buffer, buff, sectors_to_write * DISK_SECTOR_SIZE);
            ret = gm_sdc_api_sdcard_sector_write(SD_0, sector, sectors_to_write, write_wrap_buffer);
            if (ret != 0) {
                return -EIO;
            }
            
            buff += sectors_to_write * DISK_SECTOR_SIZE;
            sector += sectors_to_write;
            remaining -= sectors_to_write;
        }

    } else {
        {volatile uint32_t vdata;vdata = *(volatile uint32_t*)buff;}
        ret = gm_sdc_api_sdcard_sector_write(SD_0, sector, count, (void*)buff);
        if (ret != 0)
        {

            return -EIO;
        }
    }

    return 0;
}

#else

static int disk_sdmmc_access_read(struct disk_info *disk, uint8_t *buff,
                                  uint32_t sector, uint32_t count)
{
    int ret;

    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }
    ret = gm_sdc_api_sdcard_sector_read(SD_0, sector, count, buff);
    if (ret != 0)
    {
        return -EIO;
    }
    return 0;
}

static int disk_sdmmc_access_write(struct disk_info *disk, const uint8_t *buff,
                                   uint32_t sector, uint32_t count)
{
    int ret;

    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }
    ret = gm_sdc_api_sdcard_sector_write(SD_0, sector, count, (void*)buff);
    if (ret != 0)
    {

        return -EIO;
    }
    return 0;
}
#endif

static int disk_sdmmc_access_ioctl(struct disk_info *disk, uint8_t cmd, void *buff)
{
    uint32_t blk_len, blk_num, erase_size;
    uint32_t err;

    err = lib_sdc_get_card_info(SD_0, &blk_len, &blk_num, &erase_size);
    if(err != 0){
        return -EIO;
    }

    if (disk != &sdmmc_disk_raw) {
        blk_num -= CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }
    switch (cmd)
    {
    case DISK_IOCTL_CTRL_SYNC:
        return 0;
    case DISK_IOCTL_GET_SECTOR_COUNT:
        *(uint32_t *)buff = blk_num;
        return 0;
    case DISK_IOCTL_GET_SECTOR_SIZE:
        *(uint32_t *)buff = blk_len;
        return 0;
    case DISK_IOCTL_GET_ERASE_BLOCK_SZ: /* in sectors */
        *(uint32_t *)buff = erase_size;
        return 0;
    default:
        break;
    }

    return -EINVAL;
}

static const struct disk_operations sdmmc_disk_ops = {
    .init = disk_sdmmc_access_init,
    .status = disk_sdmmc_access_status,
    .read = disk_sdmmc_access_read,
    .write = disk_sdmmc_access_write,
    .ioctl = disk_sdmmc_access_ioctl,
};

static struct disk_info sdmmc_disk = {
    .name = CONFIG_DISK_SDMMC_VOLUME_NAME,
    .ops = &sdmmc_disk_ops,
};

static const struct disk_operations sdmmc_disk_raw_ops = {
    .init = disk_sdmmc_access_init,
    .status = disk_sdmmc_access_status,
    .read = disk_sdmmc_access_read,
    .write = disk_sdmmc_access_write,
    .ioctl = disk_sdmmc_access_ioctl,
};

static struct disk_info sdmmc_disk_raw = {
    .name = "SDRAW",
    .ops = &sdmmc_disk_raw_ops,
};

int disk_sdmmc_init(const void *dev)
{
    int r;
    ARG_UNUSED(dev);

    r = disk_access_register(&sdmmc_disk);
    if (r) {
        return r;
    }

    r = disk_access_register(&sdmmc_disk_raw);
    if (r) {
        return r;
    }

#if CONFIG_DISK_SDMMC_IOCACHE_ENABLE
    struct io_cache_config cache_config;
    io_cache_handle_t cache_handle;
    cache_config.entry_count = CONFIG_DISK_SDMMC_IOCACHE_ENTRY_COUNT;        /* 8 cache entries */
    cache_config.max_block_size = CONFIG_DISK_SDMMC_IOCACHE_MAX_BLOCK_SIZE;  /* 4 sectors max per entry */
    cache_config.sector_size = CONFIG_DISK_SDMMC_IOCACHE_SECTOR_SIZE;      /* Standard sector size */

    cache_handle = io_cache_create(&sdmmc_disk, &cache_config);
    io_cache_enable(cache_handle, &sdmmc_disk);
#endif
    return 0;
}
