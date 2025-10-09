/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _INCLUDE_FS_ENV_H_
#define _INCLUDE_FS_ENV_H_

#if(CONFIG_FS_ENV_MEMORY_MANAGEMENT_OS)
#include "esp_heap_caps.h"
#endif
#if (CONFIG_FS_ENV_OS_FREERTOS)
#include "FreeRTOS.h"
#include "semphr.h"
#endif


#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#define FS_ENV_MAX_DELAY    (-1)

#define LS_ENV_LOG(fmt, ...)

#if(CONFIG_FS_ENV_MEMORY_MANAGEMENT_OS)
#if(CONFIG_FS_ENV_MEMORY_MANAGEMENT_USE_PSRAM)
#define FS_ENV_MEM_MALLOC(sz) exram_malloc(32, (sz))
#define FS_ENV_MEM_REALLOC(p,sz) exram_realloc((p), (sz))
#define FS_ENV_MEM_FREE(p) exram_free((p))
#else
#define FS_ENV_MEM_MALLOC(sz) heap_caps_malloc((sz), MALLOC_CAP_INTERNAL)
#define FS_ENV_MEM_REALLOC(p,sz) heap_caps_realloc((p), (sz), MALLOC_CAP_INTERNAL)
#define FS_ENV_MEM_FREE(p) heap_caps_free((p))
#endif
#else
#define FS_ENV_MEM_MALLOC(sz) malloc(sz)
#define FS_ENV_MEM_REALLOC(p,sz) realloc((p), (sz))
#define FS_ENV_MEM_FREE(p) free((p))
#endif

typedef struct {
    void* mutex;
}fs_env_mutex_handle_t;


int fs_env_mutex_create(fs_env_mutex_handle_t* handle);
int fs_env_mutex_lock(fs_env_mutex_handle_t* handle,int timeout_ms);
int fs_env_mutex_unlock(fs_env_mutex_handle_t* handle);
int fs_env_mutex_destroy(fs_env_mutex_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE_FS_ENV_H_ */