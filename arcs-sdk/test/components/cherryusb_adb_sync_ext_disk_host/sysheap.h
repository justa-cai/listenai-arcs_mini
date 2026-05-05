#ifndef SYSHEAP_H
#define SYSHEAP_H

#include <stddef.h>

void *exram_malloc(size_t align, size_t size);
void exram_free(void *ptr);
void *inram_malloc(size_t align, size_t size);
void inram_free(void *ptr);
void *psram_malloc_align(size_t align, size_t size);
void psram_free(void *ptr);
int printk(const char *fmt, ...);

#endif
