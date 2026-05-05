#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

uint32_t SysTimeMsGet(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}
