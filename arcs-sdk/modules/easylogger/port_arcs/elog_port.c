/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */

#include <elog.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "uart_reg.h"

#include "sys/time.h"

TaskHandle_t elog_task_handle = NULL;
SemaphoreHandle_t elog_async_semphr = NULL;
SemaphoreHandle_t lock = NULL;
int soc_cpu_id_get(void);

static int elog_port_scheduler_suspended(void)
{
#if (INCLUDE_xTaskGetSchedulerState == 1) || (configUSE_TIMERS == 1)
    return xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED;
#else
    return 0;
#endif
}

static int elog_port_scheduler_running(void)
{
#if (INCLUDE_xTaskGetSchedulerState == 1) || (configUSE_TIMERS == 1)
    return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
#else
    return 1;
#endif
}

#if CONFIG_EASYLOGGER_LOG_MODE_ASYNC
#include "esp_heap_caps.h"

#define HEAP_CAPS    (MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM)
static char *elog_async_log_buf = NULL;
    #ifdef ELOG_ASYNC_LINE_OUTPUT
        #define POLL_GET_BUF_SIZE (ELOG_LINE_BUF_SIZE - 4)
        static char *poll_get_buf = NULL;
    #else
        #define POLL_GET_BUF_SIZE (ELOG_ASYNC_OUTPUT_BUF_SIZE - 4)
        static char *poll_get_buf = NULL;
    #endif


void *elog_port_malloc(size_t size)
{
    return heap_caps_malloc(size, HEAP_CAPS);
}

void elog_port_free(void *ptr)
{
    heap_caps_free(ptr);
}

char *elog_port_get_buf_for_elog_async(size_t *out_size)
{
    if (elog_async_log_buf == NULL) {
        elog_async_log_buf = (char *)elog_port_malloc(ELOG_ASYNC_OUTPUT_BUF_SIZE);
    }
    *out_size = ELOG_ASYNC_OUTPUT_BUF_SIZE;
    return elog_async_log_buf;
}

#endif

void elog_entry(void *para);

ElogErrCode elog_port_init(void)
{
    ElogErrCode result = ELOG_NO_ERR;
    lock = xSemaphoreCreateRecursiveMutex();
    if (lock == NULL) {
        return -1;
    }

#if CONFIG_EASYLOGGER_LOG_MODE_ASYNC
    if (poll_get_buf != NULL) {
        elog_port_free(poll_get_buf);
        poll_get_buf = NULL;
    }

    poll_get_buf = elog_port_malloc(POLL_GET_BUF_SIZE);
    if (poll_get_buf == NULL) {
        return -1;
    }

    elog_async_semphr = xSemaphoreCreateBinary();
    if (elog_async_semphr == NULL) {
        return -1;
    }

    xTaskCreate(elog_entry,               /* Task function */
                "elog_async",             /* Task name */
#if defined(CONFIG_LOG_ASYNC_TASK_STACK_SIZE)
                CONFIG_LOG_ASYNC_TASK_STACK_SIZE, /* Stack size */
#else
                CONFIG_EASYLOGGER_LOG_ASYNC_TASK_STACK_SIZE, /* Stack size */
#endif
                NULL,                     /* Parameters */
#if defined(CONFIG_LOG_ASYNC_TASK_PRIORITY)
                CONFIG_LOG_ASYNC_TASK_PRIORITY,         /* Priority */
#else
                CONFIG_EASYLOGGER_LOG_ASYNC_TASK_PRIORITY,         /* Priority */
#endif
                &elog_task_handle);       /* Task handle */

    if (elog_task_handle == NULL) {
        return -1;
    }
#endif
    return result;
}

void elog_port_deinit(void)
{
#if CONFIG_EASYLOGGER_LOG_MODE_ASYNC
    elog_port_free(poll_get_buf);
    poll_get_buf = NULL;

    elog_port_free(elog_async_log_buf);
    elog_async_log_buf = NULL;

    if (elog_task_handle) {
        vTaskDelete(elog_task_handle);
    }
#endif
}

__attribute__((weak)) void elog_port_output_log(const char *log, size_t size)
{
}

void elog_port_output(const char *log, size_t size)
{
    elog_port_output_log(log, size);
}

void elog_port_output_lock(void)
{
    if (lock == NULL){
        return;
    }

    if (xPortIsInsideInterrupt()) {
        // 中断中跳过锁定，避免死锁
        return;
    }
    if (xPortIsInsideCritical()) {
        // 临界区内跳过锁定，避免死锁
        return;
    }
    if (elog_port_scheduler_suspended()) {
        return;
    }

    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
}

void elog_port_output_unlock(void)
{
    if (lock == NULL){
        return;
    }

    if (xPortIsInsideInterrupt()) {
        // 中断中跳过解锁
        return;
    }
    if (xPortIsInsideCritical()) {
        // 临界区内跳过解锁
        return;
    }
    if (elog_port_scheduler_suspended()) {
        return;
    }

    xSemaphoreGiveRecursive(lock);
}

__attribute__((weak)) uint32_t elog_time_ms_get(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

const char *elog_port_get_time(void)
{
    static char cur_system_time[15] = "";
#if defined(CONFIG_LOG_USE_POSIX_TIME) || defined(CONFIG_EASYLOGGER_USE_POSIX_TIME)
    struct timeval _tv;
    struct tm _tm = {0};
    gettimeofday(&_tv, NULL);
    gmtime_r(&(_tv.tv_sec), &_tm);

    uint32_t hours = _tm.tm_hour;
    uint32_t minutes = _tm.tm_min;
    uint32_t seconds = _tm.tm_sec;
    uint32_t ms = _tv.tv_usec / 1000;
#else
    uint32_t total_ms = elog_time_ms_get();
    uint32_t ms = total_ms % 1000;
    uint32_t total_seconds = total_ms / 1000;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
#endif

    snprintf(cur_system_time, sizeof(cur_system_time), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds, ms);

    return cur_system_time;
}

const char *elog_port_get_p_info(void)
{
    static char cpu_id_str[12];
    int cpu_id = soc_cpu_id_get();

    if (cpu_id < 0) {
        return "na";
    }

    snprintf(cpu_id_str, sizeof(cpu_id_str), "%d", cpu_id);
    return cpu_id_str;
}

const char *elog_port_get_t_info(void)
{
    if (xPortIsInsideInterrupt()) {
        return "isr";
    } else {
        TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
        if (current_task == NULL) {
            return "none";
        }
        return pcTaskGetName(current_task);
    }
}

void elog_async_output_notice(void)
{
#if CONFIG_EASYLOGGER_LOG_MODE_ASYNC
    if (!elog_port_scheduler_running()) {
        return;
    }

    if (xPortIsInsideInterrupt()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(elog_async_semphr, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else if (!xPortIsInsideCritical()) {
        xSemaphoreGive(elog_async_semphr);
    } else {
        /*do noting*/
    }
#endif
}

#if CONFIG_EASYLOGGER_LOG_MODE_ASYNC
size_t elog_port_read_log_then_output(void)
{
    size_t get_log_size = 0;
#ifdef ELOG_ASYNC_LINE_OUTPUT
    get_log_size = elog_async_get_line_log(poll_get_buf, POLL_GET_BUF_SIZE);
#else
    get_log_size = elog_async_get_log(poll_get_buf, POLL_GET_BUF_SIZE);
#endif
    if (get_log_size) {
        elog_port_output_lock();
        elog_port_output(poll_get_buf, get_log_size);
        elog_port_output_unlock();
    }
    return get_log_size;
}

void elog_entry(void *para)
{
    size_t get_log_size = 0;
    for (;;) {
        /* waiting log */
        xSemaphoreTake(elog_async_semphr, portMAX_DELAY);
        /* polling gets and outputs the log */
        while (1) {
            get_log_size = elog_port_read_log_then_output();
            if (get_log_size == 0) {
                break;
            }
        }
    }
}
#endif
