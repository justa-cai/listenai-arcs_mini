#ifndef __SYSHEAP_H__
#define __SYSHEAP_H__

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

void sysheap_init(void);
void exram_free(void *ptr);
void *exram_calloc(size_t align, size_t num, size_t size);
void *exram_malloc(size_t align, size_t size);
void *exram_realloc(void *ptr, size_t size);
void inram_free(void *ptr);
void *inram_calloc(size_t align, size_t num, size_t size);
void *inram_malloc(size_t align, size_t size);
void *inram_realloc(void *ptr, size_t size);
void heap_summary_info(void);

void *psram_calloc(size_t num, size_t size);
void *psram_malloc(size_t size);
void *psram_calloc_align(size_t align, size_t num, size_t size);
void *psram_malloc_align(size_t align, size_t size);
void *psram_realloc(void *ptr, size_t size);
void psram_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
