#ifndef TEST_ADB_SYNC_STAGE_SYSHEAP_H_
#define TEST_ADB_SYNC_STAGE_SYSHEAP_H_

#include <stddef.h>

void *inram_malloc(size_t align, size_t size);
void inram_free(void *ptr);
void *exram_malloc(size_t align, size_t size);
void exram_free(void *ptr);
void *psram_malloc_align(size_t align, size_t size);
void psram_free(void *ptr);

#endif
