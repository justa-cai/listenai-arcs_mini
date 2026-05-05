#ifndef __ADB_UTILS_H__
#define __ADB_UTILS_H__

#include "xutils.h"

#include "usb_config.h"
#include "sysheap.h"

extern void *inram_malloc(size_t align, size_t size);
extern void *inram_realloc(void *ptr, size_t size);
extern void *inram_calloc(size_t align, size_t num, size_t size);
extern void inram_free(void *ptr);

#define ADB_LOGE(fmt, ...)  printf(COR_FG_RED fmt CORDEF CRLF, ##__VA_ARGS__)
#define ADB_LOGW(fmt, ...)  // printf(COR_FG_FUCHSIN fmt CORDEF CRLF, ##__VA_ARGS__)
#define ADB_LOGI(fmt, ...)  // printf(COR_FG_YELLOW fmt CORDEF CRLF, ##__VA_ARGS__)
#define ADB_LOGD(fmt, ...)  // printf(COR_FG_BLUE fmt CORDEF CRLF, ##__VA_ARGS__)
#define ADB_LOGV(fmt, ...)  // printf(COR_FG_GREEN fmt CORDEF CRLF, ##__VA_ARGS__)
#define ADB_ASSERT(cond)    assert(cond)

#define ADB_CNTOF(A)    (sizeof(A)/sizeof(A[0]))

#define ADB_MEM_TRACE 0

#if ADB_MEM_TRACE
static inline void* adb_malloc_trace(uint32_t size)
{
    void *ptr = inram_malloc(4, size);
    ADB_LOGW("adb malloc, size:%d, ptr:%p, caller:%p", size, ptr, __builtin_return_address(0));
    return ptr;
}

static inline void adb_free_trace(void *ptr)
{
    ADB_LOGW("adb free, ptr:%p, caller:%p", ptr, __builtin_return_address(0));
    inram_free(ptr);
}

#define ADB_MALLOC(size) adb_malloc_trace(size)
#define ADB_FREE(ptr) adb_free_trace(ptr)
#else
#define ADB_MALLOC(size) inram_malloc(4, size)
#define ADB_FREE(ptr) inram_free(ptr)
#endif

#endif
