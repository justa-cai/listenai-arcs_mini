/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include <stddef.h>


#include "fs_env/fs_env.h"
#include "disk/disk.h"

/* list of mounted file systems */
static sys_dlist_t disk_access_list;

static fs_env_mutex_handle_t list_mutex_handle = {
	.mutex = NULL,
};

struct disk_info *disk_access_get_di(const char *name)
{
	struct disk_info *disk = NULL, *itr;
	size_t name_len = strlen(name);
	sys_dnode_t *node;

	fs_env_mutex_lock(&list_mutex_handle,FS_ENV_MAX_DELAY);;
	SYS_DLIST_FOR_EACH_NODE(&disk_access_list, node)
	{
		itr = CONTAINER_OF(node, struct disk_info, node);

		/*
		 * Move to next node if mount point length is
		 * shorter than longest_match match or if path
		 * name is shorter than the mount point name.
		 */
		if (strlen(itr->name) != name_len)
		{
			continue;
		}

		/* Check for disk name match */
		if (strncmp(name, itr->name, name_len) == 0)
		{
			disk = itr;
			break;
		}
	}
	fs_env_mutex_unlock(&list_mutex_handle);

	return disk;
}

int disk_access_init(const char *pdrv)
{
	struct disk_info *disk = disk_access_get_di(pdrv);
	int rc = -EINVAL;

	if ((disk != NULL) && (disk->ops != NULL) &&
		(disk->ops->init != NULL))
	{
		rc = disk->ops->init(disk);
	}

	return rc;
}

int disk_access_status(const char *pdrv)
{
	struct disk_info *disk = disk_access_get_di(pdrv);
	int rc = -EINVAL;

	if ((disk != NULL) && (disk->ops != NULL) &&
		(disk->ops->status != NULL))
	{
		rc = disk->ops->status(disk);
	}

	return rc;
}

int disk_access_read(const char *pdrv, uint8_t *data_buf,
					 uint32_t start_sector, uint32_t num_sector)
{
	struct disk_info *disk = disk_access_get_di(pdrv);
	int rc = -EINVAL;

	if ((disk != NULL) && (disk->ops != NULL) &&
		(disk->ops->read != NULL))
	{
		rc = disk->ops->read(disk, data_buf, start_sector, num_sector);
	}

	return rc;
}

int disk_access_write(const char *pdrv, const uint8_t *data_buf,
					  uint32_t start_sector, uint32_t num_sector)
{
	struct disk_info *disk = disk_access_get_di(pdrv);
	int rc = -EINVAL;

	if ((disk != NULL) && (disk->ops != NULL) &&
		(disk->ops->write != NULL))
	{
		rc = disk->ops->write(disk, data_buf, start_sector, num_sector);
	}

	return rc;
}

int disk_access_ioctl(const char *pdrv, uint8_t cmd, void *buf)
{
	struct disk_info *disk = disk_access_get_di(pdrv);
	int rc = -EINVAL;

	if ((disk != NULL) && (disk->ops != NULL) &&
		(disk->ops->ioctl != NULL))
	{
		rc = disk->ops->ioctl(disk, cmd, buf);
	}

	return rc;
}

int disk_access_register(struct disk_info *disk)
{

	if ((disk == NULL) || (disk->name == NULL))
	{
		DISK_LOG("invalid disk interface!!");
		return -EINVAL;
	}

	if (disk_access_get_di(disk->name) != NULL)
	{
		DISK_LOG("disk interface already registered!!");
		return -EINVAL;
	}
	
	/*  append to the disk list */
	fs_env_mutex_lock(&list_mutex_handle,FS_ENV_MAX_DELAY);
	sys_dlist_append(&disk_access_list, &disk->node);
	fs_env_mutex_unlock(&list_mutex_handle);

	DISK_LOG("disk interface(%s) registered", disk->name);
	
	return 0;
}

int disk_access_unregister(struct disk_info *disk)
{
	if ((disk == NULL) || (disk->name == NULL))
	{
		DISK_LOG("invalid disk interface!!");
		return -EINVAL;
	}

	if (disk_access_get_di(disk->name) == NULL)
	{
		DISK_LOG("disk interface not registered!!");
		return -EINVAL;
	}
	
	/* remove disk node from the list */
	fs_env_mutex_lock(&list_mutex_handle,FS_ENV_MAX_DELAY);
	sys_dlist_remove(&disk->node);
	fs_env_mutex_unlock(&list_mutex_handle);

	DISK_LOG("disk interface(%s) unregistered", disk->name);

	return 0;
}

int disk_init(const void *dev)
{
	ARG_UNUSED(dev);
	fs_env_mutex_create(&list_mutex_handle);
	sys_dlist_init(&disk_access_list);

#if (CONFIG_DISK_DRIVER_FLASH)
	extern int disk_flash_init(void *dev);
	disk_flash_init(NULL);
#endif

#if (CONFIG_DISK_DRIVER_RAM)
	extern int disk_ram_init(void *dev);
	disk_ram_init(NULL);
#endif

#if (CONFIG_DISK_DRIVER_SDMMC)
	extern int disk_sdmmc_init(void *dev);
	disk_sdmmc_init(NULL);
#endif

	return 0;
}
