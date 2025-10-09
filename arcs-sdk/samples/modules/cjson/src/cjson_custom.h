#ifndef __CSJON_CUSTOM_H__
#define __CSJON_CUSTOM_H__

#include "stddef.h"

void *cjson_custom_malloc(size_t size);
void cjson_custom_free(void *ptr);
void *cjson_custom_realloc(void *ptr, size_t new_size);

#define CJSON_CUSTOM_MALLOC cjson_custom_malloc
#define CJSON_CUSTOM_FREE   cjson_custom_free
#define CJSON_CUSTOM_REALLOC cjson_custom_realloc

#endif
