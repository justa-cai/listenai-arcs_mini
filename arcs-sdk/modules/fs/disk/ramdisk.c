/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include "disk/disk.h"
#include "fs_env/fs_env.h"

static uint8_t __attribute__((section(CONFIG_DISK_RAM_MEMORY_SECTION_NAME)))
memory_pool[CONFIG_DISK_RAM_SECTOR_SIZE * CONFIG_DISK_RAM_SECTOR_COUNT];

static void *lba_to_address(uint32_t lba)
{
	return &memory_pool[lba * CONFIG_DISK_RAM_SECTOR_SIZE];
}

static int disk_ram_access_status(struct disk_info *disk)
{
	return DISK_STATUS_OK;
}

static int disk_ram_access_read(struct disk_info *disk, uint8_t *buff,
								uint32_t sector, uint32_t count)
{
	uint32_t last_sector = sector + count;

	if (last_sector < sector || last_sector > CONFIG_DISK_RAM_SECTOR_COUNT)
	{
		DISK_LOG("Sector %" PRIu32 " is outside the range %zu",
				 last_sector, CONFIG_DISK_RAM_SECTOR_COUNT);
		return -EIO;
	}

	memcpy(buff, lba_to_address(sector), count * CONFIG_DISK_RAM_SECTOR_SIZE);

	return 0;
}

static int disk_ram_access_write(struct disk_info *disk, const uint8_t *buff,
								 uint32_t sector, uint32_t count)
{
	uint32_t last_sector = sector + count;

	if (last_sector < sector || last_sector > CONFIG_DISK_RAM_SECTOR_COUNT)
	{
		DISK_LOG("Sector %" PRIu32 " is outside the range %zu",
				 last_sector, CONFIG_DISK_RAM_SECTOR_COUNT);
		return -EIO;
	}

	memcpy(lba_to_address(sector), buff, count * CONFIG_DISK_RAM_SECTOR_SIZE);

	return 0;
}

static int disk_ram_access_ioctl(struct disk_info *disk, uint8_t cmd, void *buff)
{
	switch (cmd)
	{
	case DISK_IOCTL_CTRL_SYNC:
		return 0;
	case DISK_IOCTL_GET_SECTOR_COUNT:
		*(uint32_t *)buff = CONFIG_DISK_RAM_SECTOR_COUNT;
		return 0;
	case DISK_IOCTL_GET_SECTOR_SIZE:
		*(uint32_t *)buff = CONFIG_DISK_RAM_SECTOR_SIZE;
		return 0;
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ: /* in sectors */
		*(uint32_t *)buff = 1U;
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

static int disk_ram_access_init(struct disk_info *disk)
{
	return 0;
}

static const struct disk_operations ram_disk_ops = {
	.init = disk_ram_access_init,
	.status = disk_ram_access_status,
	.read = disk_ram_access_read,
	.write = disk_ram_access_write,
	.ioctl = disk_ram_access_ioctl,
};

static struct disk_info ram_disk = {
	.name = CONFIG_DISK_RAM_VOLUME_NAME,
	.ops = &ram_disk_ops,
};

int disk_ram_init(const void *dev)
{
	ARG_UNUSED(dev);

	return disk_access_register(&ram_disk);
}
