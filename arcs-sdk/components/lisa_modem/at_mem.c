/**
 * @file at_mem.c
 * @brief Memory Allocation Abstraction Layer Implementation
 */

#include "at_mem.h"
#include <stdlib.h>
#include <string.h>

static const at_mem_ops_t *s_mem_ops = NULL;

void at_mem_set_ops(const at_mem_ops_t *ops)
{
    s_mem_ops = ops;
}

void *at_mem_alloc(size_t size)
{
    if (s_mem_ops && s_mem_ops->alloc) {
        return s_mem_ops->alloc(size);
    }
    return malloc(size);
}

void *at_mem_calloc(size_t count, size_t size)
{
    if (s_mem_ops && s_mem_ops->calloc) {
        return s_mem_ops->calloc(count, size);
    }
    return calloc(count, size);
}

void *at_mem_realloc(void *ptr, size_t size)
{
    if (s_mem_ops && s_mem_ops->realloc) {
        return s_mem_ops->realloc(ptr, size);
    }
    return realloc(ptr, size);
}

void at_mem_free(void *ptr)
{
    if (s_mem_ops && s_mem_ops->free) {
        s_mem_ops->free(ptr);
        return;
    }
    free(ptr);
}
