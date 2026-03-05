#include "stdlib.h"
#include <stdint.h>

void *lisa_ui_malloc(uint32_t size)
{
    return malloc(size);
}

void lisa_ui_free(void *p)
{
    return free(p);
}
