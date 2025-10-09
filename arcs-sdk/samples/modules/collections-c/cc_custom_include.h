#ifndef __CC_CUSTOM_INCLUDE__
#define __CC_CUSTOM_INCLUDE__

#include <stddef.h>

void *cc_custom_malloc(size_t size);
void cc_custom_free(void *ptr);
void *cc_custom_calloc(size_t nmemb, size_t size);

#define CC_MALLOC cc_custom_malloc
#define CC_FREE   cc_custom_free
#define CC_CALLOC cc_custom_calloc

#endif
