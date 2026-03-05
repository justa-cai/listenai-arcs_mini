#include "stdlib.h"
#include <stdint.h>

#include "sysheap.h"

void *lisa_ui_malloc(uint32_t size)
{
    return psram_malloc(size);
}

void lisa_ui_free(void *p)
{
    return psram_free(p);
}
