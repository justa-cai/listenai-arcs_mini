/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Disk Driver Interface
 *
 * This file contains interface for disk access. Apart from disks, various
 * other storage media like Flash and RAM disks may implement this interface to
 * be used by various higher layers(consumers) like USB Mass storage
 * and Filesystems.
 */

#ifndef __DISK_H_
#define __DISK_H_

/**
 * @brief Disk Driver Interface
 * @defgroup disk_driver_interface Disk Driver Interface
 * @{
 */

#include <stdint.h>
#include "fs_env/dlist.h"

#ifdef __cplusplus
extern "C"
{
#endif


#ifndef DISK_LOG
#define DISK_LOG(fmt, ...)
#endif

#ifndef ROUND_UP
#define ROUND_UP(x, align)                                 \
	(((unsigned long)(x) + ((unsigned long)(align) - 1)) & \
	 ~((unsigned long)(align) - 1))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(x, align) \
	((unsigned long)(x) & ~((unsigned long)(align) - 1))
#endif

/**
 * @brief Possible Cmd Codes for disk_ioctl()
 */

/** Get the number of sectors in the disk  */
#define DISK_IOCTL_GET_SECTOR_COUNT 1
/** Get the size of a disk SECTOR in bytes */
#define DISK_IOCTL_GET_SECTOR_SIZE 2
/** reserved. It used to be DISK_IOCTL_GET_DISK_SIZE */
#define DISK_IOCTL_RESERVED 3
/** How many  sectors constitute a FLASH Erase block */
#define DISK_IOCTL_GET_ERASE_BLOCK_SZ 4
/** Commit any cached read/writes to disk */
#define DISK_IOCTL_CTRL_SYNC 5

#define DISK_IOCTL_BYTE_READ 6

	struct disk_ioctl_byte_read
	{
		void *addr;
		void *datas;
		uint32_t size;
		struct k_sem *break_sem;
	};

/**
 * @brief Possible return bitmasks for disk_status()
 */

/** Disk status okay */
#define DISK_STATUS_OK 0x00
/** Disk status uninitialized */
#define DISK_STATUS_UNINIT 0x01
/** Disk status no media */
#define DISK_STATUS_NOMEDIA 0x02
/** Disk status write protected */
#define DISK_STATUS_WR_PROTECT 0x04

	struct disk_operations;

	/**
	 * @brief Disk info
	 */
	struct disk_info
	{
		/** Internally used list node */
		sys_dnode_t node;
		/** Disk name */
		char *name;
		/** Disk operations */
		const struct disk_operations *ops;
		void *cache_instance;  /* Pointer to cache instance */
	};

	/**
	 * @brief Disk operations
	 */
	struct disk_operations
	{
		int (*init)(struct disk_info *disk);
		int (*status)(struct disk_info *disk);
		int (*read)(struct disk_info *disk, uint8_t *data_buf,
					uint32_t start_sector, uint32_t num_sector);
		int (*write)(struct disk_info *disk, const uint8_t *data_buf,
					 uint32_t start_sector, uint32_t num_sector);
		int (*ioctl)(struct disk_info *disk, uint8_t cmd, void *buff);
	};

	/**
	 * @brief Register disk
	 *
	 * @param[in] disk Pointer to the disk info structure
	 *
	 * @return 0 on success, negative errno code on fail
	 */
	int disk_access_register(struct disk_info *disk);

	/**
	 * @brief Unregister disk
	 *
	 * @param[in] disk Pointer to the disk info structure
	 *
	 * @return 0 on success, negative errno code on fail
	 */
	int disk_access_unregister(struct disk_info *disk);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __DISK_H_ */
