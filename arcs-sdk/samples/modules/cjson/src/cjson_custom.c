#include "stdio.h"
#include "stddef.h"
#include "stdlib.h"

void *cjson_custom_malloc(size_t size)
{
    printf("cjson_custom_malloc\n");
    return malloc(size);
}

void cjson_custom_free(void *ptr)
{
    printf("cjson_custom_free\n");
    free(ptr);
}

void *cjson_custom_realloc(void *ptr, size_t new_size)
{
    printf("cjson_custom_realloc\n");
    return realloc(ptr, new_size);
}
