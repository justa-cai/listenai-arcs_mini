#ifndef __ADB_UTILS_H__
#define __ADB_UTILS_H__

#if CONFIG_LOG
#include "elog.h"
#define ADB_LOGE(fmt, args...) log_e(fmt, ##args)
#define ADB_LOGW(fmt, args...) log_w(fmt, ##args)
#define ADB_LOGI(fmt, args...) log_i(fmt, ##args)
#define ADB_LOGD(fmt, args...) log_d(fmt, ##args)
#else
/*
 * Boot recovery keeps CONFIG_LOG disabled; routing every ADB debug/info print
 * through printk noticeably slows bulk transfers, so keep only hard errors.
 */
#define ADB_LOGE(fmt, args...) printk(fmt, ##args)
#define ADB_LOGW(...) do { } while (0)
#define ADB_LOGI(...) do { } while (0)
#define ADB_LOGD(...) do { } while (0)
#endif

#include <stdbool.h>
#include <stdint.h>

#include "sysheap.h"
#define ADB_MEM_TRACE 0

#if defined(CONFIG_BOOT_ADB)
#include "esp_heap_caps.h"

static inline bool adb_boot_can_alloc_inram(size_t align, size_t size)
{
    size_t required_internal_block = size + align;
    size_t largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);

    return largest_internal_block >= required_internal_block;
}

static inline void *adb_boot_try_inram_malloc(size_t align, size_t size)
{
    if (!adb_boot_can_alloc_inram(align, size)) {
        return NULL;
    }

    return inram_malloc(align, size);
}

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
