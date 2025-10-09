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

#include "sysheap.h"
#define ADB_MEM_TRACE 0

#if ADB_MEM_TRACE
static inline void* adb_malloc_trace(uint32_t size)
{
    void *ptr = exram_malloc(4, size);
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
#define ADB_MALLOC(size) exram_malloc(4, size)
#define ADB_FREE(ptr) exram_free(ptr)
#endif

#endif
