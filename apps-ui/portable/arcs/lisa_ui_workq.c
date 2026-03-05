#include "lisa_ui_workq.h"
#include "lisa_ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
    lisa_ui_workq_worker_t worker;
    void *arg;
    uint32_t arg_len;
    uint64_t execute_time_ms;
} lisa_ui_workq_item_t;

typedef struct {
    TaskHandle_t task;
    QueueHandle_t queue;
} lisa_ui_workq_impl_t;

static uint64_t get_current_time_ms(void)
{
    return (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void lisa_ui_workq_task(void *param)
{
    lisa_ui_workq_impl_t *impl = (lisa_ui_workq_impl_t *)param;
    lisa_ui_workq_item_t *item;

    while (1) {
        if (xQueueReceive(impl->queue, &item, portMAX_DELAY) == pdTRUE) {
            if (item) {
                uint64_t now = get_current_time_ms();

                if (item->execute_time_ms > 0 && item->execute_time_ms > now) {
                    if (xQueueSend(impl->queue, &item, 0) != pdTRUE) {
                        if (item->arg) {
                            lisa_ui_free(item->arg);
                        }
                        lisa_ui_free(item);
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                }

                if (item->worker) {
                    item->worker(item->arg, item->arg_len);
                }

                if (item->arg) {
                    lisa_ui_free(item->arg);
                }
                lisa_ui_free(item);
            }
        }
    }
}

lisa_ui_workq_t lisa_ui_workq_create(const char *name, uint32_t stack_size, uint32_t prio, uint32_t q_cnt)
{
    lisa_ui_workq_impl_t *impl = lisa_ui_malloc(sizeof(lisa_ui_workq_impl_t));
    if (!impl) {
        return NULL;
    }

    impl->queue = xQueueCreate(q_cnt, sizeof(lisa_ui_workq_item_t *));
    if (!impl->queue) {
        lisa_ui_free(impl);
        return NULL;
    }

    BaseType_t ret = xTaskCreate(lisa_ui_workq_task, name, stack_size, impl, prio, &impl->task);
    if (ret != pdPASS) {
        vQueueDelete(impl->queue);
        lisa_ui_free(impl);
        return NULL;
    }

    return (lisa_ui_workq_t)impl;
}

int lisa_ui_workq_submit(lisa_ui_workq_t workq, lisa_ui_workq_worker_t worker, void *arg, uint32_t arg_len)
{
    return lisa_ui_workq_submit_delay_ms(workq, worker, arg, arg_len, 0);
}

int lisa_ui_workq_submit_delay_ms(lisa_ui_workq_t workq, lisa_ui_workq_worker_t worker, void *arg, uint32_t arg_len, uint32_t delay_ms)
{
    if (!workq || !worker) {
        return -1;
    }

    lisa_ui_workq_impl_t *impl = (lisa_ui_workq_impl_t *)workq;

    lisa_ui_workq_item_t *item = lisa_ui_malloc(sizeof(lisa_ui_workq_item_t));
    if (!item) {
        return -1;
    }

    item->worker = worker;
    item->arg_len = arg_len;
    item->execute_time_ms = get_current_time_ms() + delay_ms;

    if (arg && arg_len > 0) {
        item->arg = lisa_ui_malloc(arg_len);
        if (!item->arg) {
            lisa_ui_free(item);
            return -1;
        }
        memcpy(item->arg, arg, arg_len);
    } else {
        item->arg = NULL;
    }

    if (xQueueSend(impl->queue, &item, 0) != pdTRUE) {
        if (item->arg) {
            lisa_ui_free(item->arg);
        }
        lisa_ui_free(item);
        assert(0);
        return -1;
    }

    return 0;
}
