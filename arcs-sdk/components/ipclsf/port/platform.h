#ifndef _DISK_MEM_PLATFORM_H_
#define _DISK_MEM_PLATFORM_H_
#include "cache.h"

#define DISK_MEM_CACHE_FLUSH(addr, size) HAL_FlushInvalidateDCache_by_Addr(addr, size)
#define DISK_MEM_CACHE_CLEAN(addr, size) HAL_FlushDCache_by_Addr(addr, size)
#define DISK_MEM_CACHE_INVALID(addr, size) HAL_InvalidateDCache_by_Addr(addr, size)
#define DISK_MEM_ENTER_CRITCAL()  (0)
#define DISK_MEM_EXIT_CRITCAL(x) 


#endif

