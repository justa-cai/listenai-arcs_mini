#include <stddef.h>


extern void *exram_malloc(size_t align, size_t size);
extern void *exram_realloc(void *ptr, size_t size);
extern void *exram_calloc(size_t align, size_t num, size_t size);

extern void exram_free(void *ptr);

void *curl_mem_malloc(size_t size)
{
    return exram_malloc(4, size);
}

void *curl_mem_realloc(void *ptr, size_t size)
{
    return exram_realloc(ptr,size);
}

void *curl_mem_calloc(size_t n, size_t size)
{
    return exram_calloc(4, n, size);
}

void curl_mem_free(void *ptr)
{
    return exram_free(ptr);
}

