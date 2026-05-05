#ifndef TEST_ADB_SYNC_STAGE_SYSHEAP_H_
#define TEST_ADB_SYNC_STAGE_SYSHEAP_H_

#include <stddef.h>

void *exram_malloc(size_t align, size_t size);
void exram_free(void *ptr);

#endif
