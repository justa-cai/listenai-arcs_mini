/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include <diskio.h> /* FatFs lower layer API */
#include <disk/disk_access.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static const char *const pdrv_str[] = {FF_VOLUME_STRS};

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

static const char *disk_get_drv(BYTE pdrv)
{
	return pdrv_str[pdrv];
}

DSTATUS disk_status(BYTE pdrv)
{
	if (pdrv >= ARRAY_SIZE(pdrv_str))
	{
		return RES_PARERR;
	}

	if (disk_access_status(disk_get_drv(pdrv)) != 0)
	{
		return STA_NOINIT;
	}
	else
	{
		return RES_OK;
	}
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv)
{
	if (pdrv >= ARRAY_SIZE(pdrv_str))
	{
		return RES_PARERR;
	}

	if (disk_access_init(disk_get_drv(pdrv)) != 0)
	{
		return STA_NOINIT;
	}
	else
	{
		return RES_OK;
	}
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	if (pdrv >= ARRAY_SIZE(pdrv_str))
	{
		return RES_PARERR;
	}

	if (disk_access_read(disk_get_drv(pdrv), buff, sector, count) != 0)
	{
		return RES_ERROR;
	}
	else
	{
		return RES_OK;
	}
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
	if (pdrv >= ARRAY_SIZE(pdrv_str))
	{
		return RES_PARERR;
	}

	if (disk_access_write(disk_get_drv(pdrv), buff, sector, count) != 0)
	{
		return RES_ERROR;
	}
	else
	{
		return RES_OK;
	}
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
	int ret = RES_OK;
	uint32_t sector_size = 0;

	if (pdrv >= ARRAY_SIZE(pdrv_str))
	{
		return RES_PARERR;
	}

	switch (cmd)
	{
	case CTRL_SYNC:
		if (disk_access_ioctl(disk_get_drv(pdrv),
							  DISK_IOCTL_CTRL_SYNC, buff) != 0)
		{
			ret = RES_ERROR;
		}
		break;

	case GET_SECTOR_COUNT:
		if (disk_access_ioctl(disk_get_drv(pdrv),
							  DISK_IOCTL_GET_SECTOR_COUNT, buff) != 0)
		{
			ret = RES_ERROR;
		}
		break;

	case GET_SECTOR_SIZE:
		/* 
		 * return a 16-bit number.
		 */
		if ((disk_access_ioctl(disk_get_drv(pdrv),
							   DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) == 0) &&
			(sector_size == (uint16_t)sector_size))
		{
			*(uint16_t *)buff = (uint16_t)sector_size;
		}
		else
		{
			ret = RES_ERROR;
		}
		break;

	case GET_BLOCK_SIZE:
		if (disk_access_ioctl(disk_get_drv(pdrv),
							  DISK_IOCTL_GET_ERASE_BLOCK_SZ, buff) != 0)
		{
			ret = RES_ERROR;
		}
		break;

	default:
		ret = RES_PARERR;
		break;
	}
	return ret;
}
