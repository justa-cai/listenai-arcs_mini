#include "sysheap.h"
#include "stdio.h"

void *cc_custom_malloc(size_t size)
{
    printf("cc_custom_malloc(%zu)\n", size);
    return exram_malloc(4, size);
}

void cc_custom_free(void *ptr)
{
    printf("cc_custom_free(%p)\n", ptr);
    exram_free(ptr);
}

void *cc_custom_calloc(size_t nmemb, size_t size)
{
    printf("cc_custom_calloc(%zu,%zu)\n", nmemb, size);
    return exram_calloc(4, nmemb, size);
}
