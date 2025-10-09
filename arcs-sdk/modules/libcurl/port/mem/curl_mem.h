#pragma once

#include <stdlib.h>



void *curl_mem_malloc(size_t size);


void *curl_mem_realloc(void *ptr, size_t size);


void *curl_mem_calloc(size_t n, size_t size);


void curl_mem_free(void *ptr);
