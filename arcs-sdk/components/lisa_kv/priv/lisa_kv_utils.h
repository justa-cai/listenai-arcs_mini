#ifndef __LISA_KV_UTILS_H__
#define __LISA_KV_UTILS_H__

#include "sysheap.h"

#define LISA_KV_MALLOC(size) exram_malloc(4, size)
#define LISA_KV_FREE(ptr) exram_free(ptr)

#endif