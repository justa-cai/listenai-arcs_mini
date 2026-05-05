#ifndef TEST_TASK_H
#define TEST_TASK_H

#include "FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *arg);

BaseType_t xTaskCreate(TaskFunction_t task_func, const char *name,
                       uint16_t stack_depth, void *params,
                       UBaseType_t priority, TaskHandle_t *task_handle);
void vTaskDelete(TaskHandle_t task_handle);
void vTaskNotifyGiveFromISR(TaskHandle_t task_handle, BaseType_t *woken);
void xTaskNotifyGive(TaskHandle_t task_handle);
void xTaskNotifyStateClear(TaskHandle_t task_handle);
uint32_t ulTaskNotifyTake(BaseType_t clear_count_on_exit, uint32_t ticks_to_wait);

#endif
