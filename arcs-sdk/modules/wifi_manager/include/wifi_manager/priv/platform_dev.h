#ifndef _PLATFORM_DEV_H_
#define _PLATFORM_DEV_H_

#include "esp_heap_caps.h"
#include "nvs.h"
#include "FreeRTOS.h"
#include "event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_MEM_MALLOC(sz) heap_caps_malloc((sz), MALLOC_CAP_SPIRAM)
#define PLATFORM_MEM_CALLOC(n,sz) heap_caps_calloc((n), (sz), MALLOC_CAP_SPIRAM)
#define PLATFORM_MEM_ALIGN_MALLOC(align,sz) heap_caps_aligned_alloc((align),(sz),MALLOC_CAP_SPIRAM)
#define PLATFORM_MEM_NOCACHE_MALLOC(sz) heap_caps_malloc((sz), MALLOC_CAP_INTERNAL)
#define PLATFORM_MEM_REALLOC(p,sz) heap_caps_realloc((p), (sz), MALLOC_CAP_SPIRAM)
#define PLATFORM_MEM_FREE(p) heap_caps_free((p))

void* platform_get_flash_dev(void);

#ifdef __cplusplus
}
#endif

#endif
