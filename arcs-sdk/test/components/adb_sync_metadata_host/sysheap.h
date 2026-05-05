#ifndef TEST_ADB_SYNC_METADATA_SYSHEAP_H
#define TEST_ADB_SYNC_METADATA_SYSHEAP_H

#include <stddef.h>

void *exram_malloc(int align, size_t size);
void exram_free(void *ptr);

#endif
