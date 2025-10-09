#include <stddef.h>

#ifndef CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC

extern void *exram_calloc(size_t align, size_t num, size_t size);
extern void *inram_calloc(size_t align, size_t num, size_t size);

extern void inram_free(void *ptr);
extern void exram_free(void *ptr);

void *ls_mbedtls_mem_calloc(size_t n, size_t size)
{
#ifdef CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC
    return inram_calloc(32,n,size);
#elif CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC
    return exram_calloc(32, n, size);
#else
    return calloc(n, size);
#endif
}

void ls_mbedtls_mem_free(void *ptr)
{
#ifdef CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC
    return inram_free(ptr);
#elif CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC
    return exram_free(ptr);
#else
    return free(ptr);
#endif
}

#endif /* !CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC */

