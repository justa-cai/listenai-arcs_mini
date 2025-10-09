#pragma once

#include <stdlib.h>

void *ls_mbedtls_mem_calloc(size_t n, size_t size);
void ls_mbedtls_mem_free(void *ptr);
