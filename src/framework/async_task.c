#include "async_task.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

#define DEFAULT_TASK_NAME "task.async"

struct async_task {
    char *name;
    async_task_func_t func;
    void *user_data;
    async_task_complete_callback_t callback;
    uint32_t stack_size;
    uint32_t priority;
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    volatile bool should_stop;
    volatile bool is_running;
};

static void async_task_thread(void *param)
{
    async_task_t *task = (async_task_t *)param;

    if (!task || !task->func) {
        LOGE("Invalid task");
        vTaskDelete(NULL);
        return;
    }

    xSemaphoreTake(task->mutex, portMAX_DELAY);
    task->is_running = true;
    task->should_stop = false;
    xSemaphoreGive(task->mutex);

    LOGI("Async task started");

    task->func(task->user_data, (bool *)&task->should_stop);

    xSemaphoreTake(task->mutex, portMAX_DELAY);
    bool was_stopped = task->should_stop;
    task->is_running = false;
    task->task_handle = NULL;
    xSemaphoreGive(task->mutex);

    LOGI("Async task completed, interrupted: %d", was_stopped);

    if (task->callback) {
        task->callback(task->user_data, !was_stopped, was_stopped);
    }

    async_task_destroy(task);

    vTaskDelete(NULL);
}

async_task_t *async_task_create(const char *name, uint32_t stack_size, uint32_t priority, async_task_func_t func,
                                async_task_complete_callback_t callback, void *user_data)
{
    if (!func) {
        LOGE("Invalid function pointer");
        return NULL;
    }

    async_task_t *task = lisa_mem_calloc(1, sizeof(async_task_t));
    if (!task) {
        LOGE("Failed to allocate task");
        return NULL;
    }

    task->mutex = xSemaphoreCreateMutex();
    if (!task->mutex) {
        LOGE("Failed to create mutex");
        lisa_mem_free(task);
        return NULL;
    }

    const char *task_name = name ? name : DEFAULT_TASK_NAME;
    task->name = lisa_mem_calloc(1, strlen(task_name) + 1);
    if (!task->name) {
        LOGE("Failed to allocate task name");
        vSemaphoreDelete(task->mutex);
        lisa_mem_free(task);
        return NULL;
    }
    strcpy(task->name, task_name);

    task->func = func;
    task->user_data = user_data;
    task->callback = callback;
    task->stack_size = stack_size > 0 ? stack_size : 2048;
    task->priority = priority;
    task->task_handle = NULL;
    task->should_stop = false;
    task->is_running = false;

    LOGI("Async task created: %s", task->name);

    return task;
}

int async_task_start(async_task_t *task)
{
    if (!task) {
        LOGE("Invalid task");
        return -1;
    }

    xSemaphoreTake(task->mutex, portMAX_DELAY);

    if (task->is_running) {
        LOGW("Task is already running");
        xSemaphoreGive(task->mutex);
        return -1;
    }

    BaseType_t ret =
        xTaskCreate(async_task_thread, task->name, task->stack_size, task, task->priority, &task->task_handle);

    xSemaphoreGive(task->mutex);

    if (ret != pdPASS) {
        LOGE("Failed to create task thread");
        return -1;
    }

    LOGI("Async task started");
    return 0;
}

int async_task_stop(async_task_t *task)
{
    if (!task) {
        LOGE("Invalid task");
        return -1;
    }

    xSemaphoreTake(task->mutex, portMAX_DELAY);

    if (!task->is_running) {
        LOGW("Task is not running");
        xSemaphoreGive(task->mutex);
        return -1;
    }

    task->should_stop = true;
    LOGI("Stop signal sent to task");

    xSemaphoreGive(task->mutex);

    return 0;
}

bool async_task_is_running(async_task_t *task)
{
    if (!task) {
        return false;
    }

    xSemaphoreTake(task->mutex, portMAX_DELAY);
    bool running = task->is_running;
    xSemaphoreGive(task->mutex);

    return running;
}

void async_task_destroy(async_task_t *task)
{
    if (!task) {
        return;
    }

    if (task->is_running) {
        LOGW("Destroying running task, stopping first");
        async_task_stop(task);

        TickType_t start_tick = xTaskGetTickCount();
        const uint32_t timeout_ms = 5000;
        bool timeout_logged = false;

        while (task->is_running) {
            vTaskDelay(pdMS_TO_TICKS(10));

            if (!timeout_logged) {
                TickType_t current_tick = xTaskGetTickCount();
                uint32_t elapsed_ms = (current_tick - start_tick) * portTICK_PERIOD_MS;

                if (elapsed_ms >= timeout_ms) {
                    LOGE("Task %s (func: %p) failed to stop after %u ms, still waiting...",
                         task->name ? task->name : "unknown", task->func, timeout_ms);
                    timeout_logged = true;
                }
            }
        }
    }

    if (task->mutex) {
        vSemaphoreDelete(task->mutex);
    }

    if (task->name) {
        lisa_mem_free(task->name);
    }

    lisa_mem_free(task);

    LOGI("Async task destroyed");
}
