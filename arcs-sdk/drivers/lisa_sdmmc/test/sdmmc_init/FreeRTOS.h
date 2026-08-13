#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTICKS_TO_MS(ticks) ((uint32_t)(ticks))

TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);

#endif
