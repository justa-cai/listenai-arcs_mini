#ifndef __DISK_MEM_H__
#define __DISK_MEM_H__

enum {
	DISK_MEM_ERR_NONE = 0,
	DISK_MEM_ERR_BREAKED = -1,
	DISK_MEM_ERR_ACCESS_FAILED = -2,
	DISK_MEM_ERR_DMA_FAILED = -3,
};

void disk_mem_urpc_init(void);

#endif
