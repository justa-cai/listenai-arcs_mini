#ifndef __ADB_UTILS_H__
#define __ADB_UTILS_H__

#if CONFIG_LOG
#include "elog.h"
#define ADB_LOGE(fmt, args...) log_e(fmt, ##args)
#define ADB_LOGW(fmt, args...) log_w(fmt, ##args)
#define ADB_LOGI(fmt, args...) log_i(fmt, ##args)
#define ADB_LOGD(fmt, args...) log_d(fmt, ##args)
#else
#define ADB_LOGE(fmt, args...) printk(fmt, ##args)
#define ADB_LOGW(fmt, args...) printk(fmt, ##args)
#define ADB_LOGI(fmt, args...) printk(fmt, ##args)
#define ADB_LOGD(fmt, args...) printk(fmt, ##args)
#endif

#include <stdint.h>

#include "sysheap.h"
#define ADB_MEM_TRACE 0

#if defined(CONFIG_BOOT_ADB)
static inline void *adb_boot_malloc(uint32_t size)
{
    void *ptr = NULL;

#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
    if (size > 4096U) {
        ptr = psram_malloc_align(32, size);
        if (ptr != NULL) {
            return ptr;
        }
    }
#endif

    ptr = exram_malloc(4, size);

#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
    if (ptr == NULL && size <= 4096U) {
        ptr = psram_malloc_align(32, size);
    }
#endif

    return ptr;
}
#endif

#if ADB_MEM_TRACE
static inline void* adb_malloc_trace(uint32_t size)
{
    void *ptr;

#if defined(CONFIG_BOOT_ADB)
    ptr = adb_boot_malloc(size);
#else
    ptr = exram_malloc(4, size);
#endif
    ADB_LOGI("adb malloc, size:%d, ptr:%p, caller:%p\n", size, ptr, __builtin_return_address(0));
    return ptr;
}

static inline void adb_free_trace(void *ptr)
{
    ADB_LOGI("adb free, ptr:%p, caller:%p\n", ptr, __builtin_return_address(0));
    exram_free(ptr);
}

#define ADB_MALLOC(size) adb_malloc_trace(size)
#define ADB_FREE(ptr) adb_free_trace(ptr)
#else
#if defined(CONFIG_BOOT_ADB)
#define ADB_MALLOC(size) adb_boot_malloc(size)
#else
#define ADB_MALLOC(size) exram_malloc(4, size)
#endif
#define ADB_FREE(ptr) exram_free(ptr)
#endif

#endif
