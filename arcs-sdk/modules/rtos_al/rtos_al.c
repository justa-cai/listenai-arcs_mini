/**
 ****************************************************************************************
 *
 * @file rtos_al.c
 *
 * @brief Implementation of the FreeRTOS abstraction layer.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <FreeRTOSConfig.h>
#include "rtos_def.h"
#include "rtos_al.h"
#include "semphr.h"
#include "dbg_assert.h"
#include "arcs_ap.h"
#include "timers.h"
//TODO：AP/CP两个工程依赖的heap头文件不一致
#if (CONFIG_HARTID == 1)
#include "sysheap.h"
#include "esp_heap_caps.h"
#else
#include "xutils.h"
#endif

#include "assert.h"

#if NX_TRACE

#define TRACE_FILE_ID (0xFFFFFF >> TRACE_FILE_ID_OFT)

/// conversion table between task handles and task ID (for trace purpose)
static rtos_task_handle task_table[MAX_TASK];
/// ID of the task that is currently being created.
/// (Needed as traceTASK_CREATE hook is called before task_table is update)
static enum rtos_task_id creating_task_id = UNDEF_TASK;
#endif

// #if configAPPLICATION_ALLOCATED_HEAP
// #define __MHEAP __attribute__ ((section("MHEAP")))
// uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] __MHEAP;
// #endif
static bool __os_started = false;

rtos_stack_type wpa_task_stack_buf[1024];
rtos_static_task_tcb wpa_task_control;

/*
 * FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief get task name by handle
 *
 * @param[in] ptr pointer of taskhandle
 * @return task name
 *
 ***************************************************************************************
 */
char *rtos_get_name_by_handle(TaskHandle_t ptr)
{
    return pcTaskGetName(ptr);
}

/**
 ****************************************************************************************
 * @brief Convert ms to ticks
 *
 * @param[in] timeout_ms Timeout value in ms (use -1 for no timeout).
 * @return number of ticks for the specified timeout value.
 *
 ****************************************************************************************
 */

__STATIC_INLINE TickType_t rtos_timeout_2_tickcount(int timeout_ms)
{
    if (timeout_ms < 0)
    {
        return portMAX_DELAY;
    }
    else
    {
        return pdMS_TO_TICKS(timeout_ms);
    }
}

uint32_t rtos_now(bool isr)
{
    if (isr)
    {
        return xTaskGetTickCountFromISR();
    }
    else
    {
        return xTaskGetTickCount();
    }
}

void rtos_delay(uint32_t duration_ms)
{
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
}

void *rtos_malloc(uint32_t size)
{
    void *res = exram_malloc(32, size);

    return res;
}

void *rtos_calloc(uint32_t nb_elt, uint32_t size)
{
    void *res = exram_calloc(32, nb_elt, size);
    assert(res != NULL);

    return res;
}

void *rtos_realloc(void *ptr, uint32_t new_size)
{
    return exram_realloc(ptr, new_size);
}

void rtos_free(void *ptr)
{
    exram_free(ptr);
}

// void *_malloc_r(struct _reent *reent, size_t size)
// {
//     return rtos_malloc(size);
// }

// void *_calloc_r(struct _reent *reent, size_t num, size_t size)
// {
//     return rtos_calloc(num, size);
// }

// void *_realloc_r(struct _reent *reent, void *ptr, size_t new_size)
// {
//     return rtos_realloc(ptr, new_size);
// }

// void _free_r(struct _reent *reent, void *ptr)
// {
//     rtos_free(ptr);
// }

void *rtos_aligned_malloc(uint32_t size, uint32_t alignment)
{
    void *res = exram_malloc(alignment, size);
    assert(res != NULL);

    return res;
}

void rtos_aligned_free(void *ptr)
{
    if (ptr)
    {
        exram_free(ptr);
    }
}

void rtos_heap_info(int *total_size, int *free_size, int *min_free_size)
{
#if (CONFIG_HARTID == 1)
    multi_heap_info_t heap_info;

    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);

    *total_size = heap_info.total_free_bytes + heap_info.total_allocated_bytes;
    *free_size = heap_info.total_free_bytes;
    *min_free_size = heap_info.minimum_free_bytes;
#else
    *total_size = configTOTAL_HEAP_SIZE;
    *free_size = xPortGetFreeHeapSize();
    *min_free_size = xPortGetMinimumEverFreeHeapSize();
#endif // (CONFIG_HARTID == 1)
}

int rtos_task_create(rtos_task_fct func,
                     const char * const name,
                     enum rtos_task_id task_id,
                     const uint16_t stack_depth,
                     void * const params,
                     rtos_prio prio,
                     rtos_task_handle * const task_handle)
{
    BaseType_t res;
    rtos_task_handle handle;

    #if NX_TRACE
    creating_task_id = task_id;
    #endif
    // 临时 * 4，避免栈溢出
    res = xTaskCreate(func, name, stack_depth * 4, params, prio, &handle);

    if (res == pdFAIL){
        assert(0);
        return 1;
    }

    #if ( configUSE_TRACE_FACILITY == 1 )
    vTaskSetTaskNumber(handle, task_id);
    #endif

    if (task_handle) {
        *task_handle = handle;
    }

    return 0;
}

void rtos_task_delete(rtos_task_handle task_handle)
{
    if (!task_handle)
        task_handle = xTaskGetCurrentTaskHandle();
    if (eTaskGetState(task_handle) != eDeleted)
        vTaskDelete(task_handle);
}

int rtos_task_create_static(rtos_task_fct func,
                     const char * const name,
                     enum rtos_task_id task_id,
                     const uint16_t stack_depth,
                     void * const params,
                     rtos_prio prio,
                     rtos_task_handle * const task_handle,
                     rtos_stack_type * task_stack_buf,
                     rtos_static_task_tcb * task_stask_tcb)
{
    rtos_task_handle handle;

#if NX_TRACE
    creating_task_id = task_id;
#endif

    handle = xTaskCreateStatic(func, name, stack_depth, params, prio, task_stack_buf, task_stask_tcb);

    if (handle == NULL){
        assert(0);
        return 1;
    }

#if ( configUSE_TRACE_FACILITY == 1 )
    vTaskSetTaskNumber(handle, task_id);
#endif

    if (task_handle) {
        *task_handle = handle;
    }

    return 0;
}

void rtos_task_suspend(int duration)
{
    if (duration <= 0)
        return;
    vTaskDelay(pdMS_TO_TICKS(duration));
}

int rtos_task_init_notification(rtos_task_handle task)
{
    return 0;
}

int rtos_task_wait_notification(int timeout)
{
    return ulTaskNotifyTake(pdTRUE, rtos_timeout_2_tickcount(timeout));
}

void rtos_task_notify(rtos_task_handle task, bool isr)
{
    if (isr)
    {
        BaseType_t task_woken = pdFALSE;

        vTaskNotifyGiveFromISR(task, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        xTaskNotifyGive(task);
    }
}

int rtos_queue_create(int elt_size, int nb_elt, rtos_queue *queue)
{
    *queue = xQueueCreate(nb_elt, elt_size);
    assert(*queue != NULL);
    if ( *queue == NULL )
        return -1;

    return 0;
}

void rtos_queue_delete(rtos_queue queue)
{
    vQueueDelete(queue);
}

bool rtos_queue_is_empty(rtos_queue queue)
{
    BaseType_t res;

    GLOBAL_INT_DISABLE();
    res = xQueueIsQueueEmptyFromISR(queue);
    GLOBAL_INT_RESTORE();

    return (res == pdTRUE);
}

bool rtos_queue_is_full(rtos_queue queue)
{
    BaseType_t res;

    GLOBAL_INT_DISABLE();
    res = xQueueIsQueueFullFromISR(queue);
    GLOBAL_INT_RESTORE();

    return (res == pdTRUE);
}

int rtos_queue_cnt(rtos_queue queue)
{
    UBaseType_t res;

    GLOBAL_INT_DISABLE();
    res = uxQueueMessagesWaitingFromISR(queue);
    GLOBAL_INT_RESTORE();

    return ((int)res);
}

int rtos_queue_write(rtos_queue queue, void *msg, int timeout, bool isr)
{
    BaseType_t res;

    if (isr)
    {
        BaseType_t task_woken = pdFALSE;

        res = xQueueSendToBackFromISR(queue, msg, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        res = xQueueSendToBack(queue, msg, rtos_timeout_2_tickcount(timeout));
    }

    return (res == errQUEUE_FULL);
}

int rtos_queue_read(rtos_queue queue, void *msg, int timeout, bool isr)
{
    BaseType_t res = pdPASS;

    if (isr)
    {
        BaseType_t task_woken = pdFALSE;

        res = xQueueReceiveFromISR(queue, msg, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        res = xQueueReceive(queue, msg, rtos_timeout_2_tickcount(timeout));
    }

    return (res == errQUEUE_EMPTY);
}

int rtos_semaphore_create(rtos_semaphore *semaphore, int max_count, int init_count)
{
    int res = -1;

    if (max_count == 1)
    {
        *semaphore = xSemaphoreCreateBinary();
        assert(*semaphore != NULL);

        if (*semaphore != 0)
        {
            if (init_count)
            {
                xSemaphoreGive(*semaphore);
            }
            res = 0;
        }
    }
    else
    {
        *semaphore = xSemaphoreCreateCounting(max_count, init_count);
        assert(*semaphore != NULL);
        if (*semaphore != 0)
        {
            res = 0;
        }
    }

    return res;
}

void rtos_semaphore_delete(rtos_semaphore semaphore)
{
    vSemaphoreDelete(semaphore);
}

int rtos_semaphore_get_count(rtos_semaphore semaphore)
{
    return uxSemaphoreGetCount(semaphore);
}

int rtos_semaphore_wait(rtos_semaphore semaphore, int timeout)
{
    BaseType_t res = pdPASS;

    res = xSemaphoreTake(semaphore, rtos_timeout_2_tickcount(timeout));

    return (res == errQUEUE_EMPTY);
}

int rtos_semaphore_signal(rtos_semaphore semaphore, bool isr)
{
    BaseType_t res;

    if (isr)
    {
        BaseType_t task_woken = pdFALSE;

        res = xSemaphoreGiveFromISR(semaphore, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        res = xSemaphoreGive(semaphore);
    }

    return (res == errQUEUE_FULL);
}

int rtos_mutex_create(rtos_mutex *mutex)
{
    int res = -1;

    *mutex = xSemaphoreCreateMutex();
    assert(*mutex != NULL);
    if (*mutex != 0)
    {
        res = 0;
    }

    return res;
}

void rtos_mutex_delete(rtos_mutex mutex)
{
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )
    ASSERT_ERR(xSemaphoreGetMutexHolder(mutex) == NULL);
#endif
    vSemaphoreDelete(mutex);
}

void rtos_mutex_lock(rtos_mutex mutex)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void rtos_mutex_unlock(rtos_mutex mutex)
{
    xSemaphoreGive(mutex);
}

uint32_t rtos_protect(void)
{
    taskENTER_CRITICAL();
    return 1;
}

void rtos_unprotect(uint32_t protect)
{
    (void) protect;
    taskEXIT_CRITICAL();
}

void rtos_start_scheduler(void)
{
    __os_started = true;
    vTaskStartScheduler();
}

bool rtos_os_started(void)
{
    return __os_started;
}

int rtos_init(void)
{
    #if NX_TRACE
    memset(task_table, 0, sizeof(task_table));
    #endif

    return 0;
}

#if NX_TRACE
/**
 ****************************************************************************************
 * @brief Get task id from task handle
 *
 * @param[in] task Task handle. If NULL use curretn task handle
 * @return id of the task within @ref rtos_task_id.
 ****************************************************************************************
 */
__STATIC_INLINE int rtos_trace_task_id(void *task)
{
    if (!task)
        task = xTaskGetCurrentTaskHandle();
    int i;

    for (i = 0; i < MAX_TASK; i++)
    {
        if (task == task_table[i])
        {
            return i;
        }
    }

    return UNDEF_TASK;
}
#endif

rtos_task_handle rtos_get_task_handle()
{
    return xTaskGetCurrentTaskHandle();
}

void rtos_trace_task(int id, void *task)
{
    #if NX_TRACE
    enum rtos_task_id task_id = rtos_trace_task_id(task);

    if (id == RTOS_TRACE_SWITCH_IN)
    {
        TRACE_RTOS(SWITCH_IN, "Enter Task %rT", task_id);
    }
    else if (id == RTOS_TRACE_SWITCH_OUT)
    {
        TRACE_RTOS(SWITCH_OUT, "Exit Task %rT", task_id);
    }
    else if (id == RTOS_TRACE_DELETE)
    {
        TRACE_RTOS(CREATE, "Delete task %rT", task_id);
    }
    else if (id == RTOS_TRACE_SUSPEND)
    {
        TRACE_RTOS(SUSPEND, "Suspend task %rT", task_id);
    }
    else if (id == RTOS_TRACE_RESUME)
    {
        TRACE_RTOS(SUSPEND, "Resume task %rT", task_id);
    }
    else if (id == RTOS_TRACE_RESUME_FROM_ISR)
    {
        TRACE_RTOS(SUSPEND, "Resume from ISR task %rT", task_id);
    }
    else if (id == RTOS_TRACE_CREATE)
    {
        if ((creating_task_id == UNDEF_TASK) &&
            (task_table[IDLE_TASK] == NULL)
            #if( configSUPPORT_STATIC_ALLOCATION == 0 )
            && (task == xTaskGetIdleTaskHandle())
            #endif
            )
        {
            task_table[IDLE_TASK] = task;
            creating_task_id = IDLE_TASK;
        }
        else if (creating_task_id < MAX_TASK)
        {
            task_table[creating_task_id] = task;
        }

        TRACE_RTOS(CREATE, "Create task %rT", creating_task_id);
        creating_task_id = UNDEF_TASK;
    }
    #endif
}

void rtos_trace_mem(int id, void *ptr, int size, int free_size)
{
    #if NX_TRACE
    enum rtos_task_id task_id = rtos_trace_task_id(NULL);

    if (id == RTOS_TRACE_ALLOC)
    {
        if (ptr == NULL)
        {
            TRACE_RTOS(ERR, "[%rT] Failed to allocate %d bytes. (free_size = %d)",
                  task_id, size, free_size);
        }
        #if RTOS_MALLOC_TRACE_LEVEL > 0
        else
        {
            TRACE_RTOS(ALLOC, "[%rT] Allocate %d bytes at %p. (free_size = %d)",
                       task_id, size, TR_PTR(ptr), free_size);
        }
        #endif
    }
    else if (id == RTOS_TRACE_FREE)
    {

        TRACE_RTOS(FREE, "[%rT] Free %d bytes at %p. (free_size = %d)",
                   task_id, size, TR_PTR(ptr), free_size);
    }
    #endif
}

void rtos_priority_set(rtos_task_handle handle, rtos_prio priority)
{
    vTaskPrioritySet(handle, priority);
}

uint32_t rtos_get_time(void)
{
    return ( xTaskGetTickCount( ) * 1000 / configTICK_RATE_HZ );
}

bool rtos_time_past(uint32_t timestamp_ms, uint32_t timeout_ms)
{
    uint32_t cur_ms = rtos_get_time();
    uint32_t timepast_ms;

    if (cur_ms < timestamp_ms)
    {
        timepast_ms = cur_ms + (portMAX_DELAY - timestamp_ms) + 1;
    }
    else
    {
        timepast_ms = cur_ms - timestamp_ms;
    }

    if (timepast_ms >= timeout_ms)
    {
        return true;
    }

    return false;
}

#ifndef SYS_TIMER_FREQ
#define SYS_TIMER_FREQ    (1000000UL)
#endif
int32_t rtos_get_sys_time(enum time_origin_t origin, uint32_t *sec, uint32_t *usec)
{
    uint64_t count;

    count = SysTimer_GetLoadValue();
    if (sec)
    *sec  = count / SYS_TIMER_FREQ;
    if (usec)
    *usec = (count % SYS_TIMER_FREQ);

    return 0;
}

uint64_t rtos_get_sys_us(void)
{
    return SysTimer_GetLoadValue();
}

int32_t rtos_event_create(rtos_event *evt)
{
    *evt = xEventGroupCreate();
    assert(*evt != NULL);
    return 0;
}

rtos_event_bit rtos_event_wait(rtos_event evt, rtos_event_bit bit, TickType_t timeout_ms)
{
    return xEventGroupWaitBits(evt, bit, pdTRUE, pdFALSE, timeout_ms);
}

int32_t rtos_event_clear(rtos_event evt, rtos_event_bit bit)
{
    xEventGroupClearBits(evt, bit);

    return 0;
}

int32_t rtos_event_set(rtos_event evt, rtos_event_bit bit, bool isr)
{
    rtos_base_type xHigherPriorityTaskWoken = pdFALSE;

    if (isr)
        xEventGroupSetBitsFromISR(evt, bit, &xHigherPriorityTaskWoken);
    else
        xEventGroupSetBits(evt, bit);

    return 0;
}

void rtos_event_delete(rtos_event evt)
{
	vEventGroupDelete(evt);
}

rtos_timer rtos_timer_create(void *id, bool reload, uint32_t period_ms, rtos_timer_callback cb)
{
    rtos_timer timer = xTimerCreate(NULL, pdMS_TO_TICKS(period_ms), reload, id, cb);
    assert(timer != NULL);
    return timer;
}

int32_t rtos_timer_start(rtos_timer timer)
{
    if (xTimerStart(timer, 0) != pdPASS)
        return -1;
    else
        return 0;
}

int32_t rtos_timer_stop(rtos_timer timer)
{
    return xTimerStop(timer, 0);
}

int32_t rtos_timer_delete(rtos_timer timer)
{
    return xTimerDelete(timer, 0);
}

int32_t rtos_timer_reload(rtos_timer timer)
{
    return xTimerReset(timer, 0);
}

void rtos_timer_schedule(rtos_timer timer)
{
    xTimerChangePeriod(timer, 0, 0);
}

void rtos_timer_id_set(rtos_timer timer, void *id)
{
    vTimerSetTimerID(timer, id);
}

void *rtos_timer_id_get(rtos_timer timer)
{
    return pvTimerGetTimerID(timer);
}

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configUSE_TRACE_FACILITY == 1 ) )
// #define snprintf(...) tfp_snprintf(__VA_ARGS__)
struct task_run_time_info
{
    UBaseType_t task_id;
    configRUN_TIME_COUNTER_TYPE run_time;
    uint32_t age;
};

struct task_cpu_usage
{
    uint16_t num;
    uint16_t counter;
    configRUN_TIME_COUNTER_TYPE totalTime;
    struct task_run_time_info *tasks_detail;
};

struct task_cpu_usage rtos_tasks_time_stats;
void rtos_get_cpu_usage( char * pcWriteBuffer, int32_t uxBufferLength )
{
    int32_t first_round = 0;
    int32_t xOutputBufferFull = 0;
    struct task_run_time_info *tmp;
    int32_t x, i, current, iSnprintfReturnValue, uxConsumedBufferLength = 0;
    UBaseType_t uxArraySize;
    TaskStatus_t * pxTaskStatusArray;
    configRUN_TIME_COUNTER_TYPE lastTotalTime, ulTotalTime = 0;
    configRUN_TIME_COUNTER_TYPE ulStatsAsPercentage;

    *pcWriteBuffer = ( char ) 0x00;
    uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = rtos_malloc( uxArraySize * sizeof( TaskStatus_t ) );
    if( pxTaskStatusArray != NULL )
    {
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );
        if (rtos_tasks_time_stats.tasks_detail == NULL)
        {
            rtos_tasks_time_stats.tasks_detail = rtos_malloc( uxArraySize * sizeof(struct task_run_time_info) );
            if (rtos_tasks_time_stats.tasks_detail == NULL)
                goto END;
            memset(rtos_tasks_time_stats.tasks_detail, 0, uxArraySize * sizeof(struct task_run_time_info));
            first_round = 1;
            rtos_tasks_time_stats.counter   = 1;
            rtos_tasks_time_stats.num       = uxArraySize;
            rtos_tasks_time_stats.totalTime = 0;
        }
        else if (rtos_tasks_time_stats.num < uxArraySize)
        {
            tmp = (struct task_run_time_info*)rtos_malloc( uxArraySize * sizeof(struct task_run_time_info) );
            if (tmp != NULL)
            {
                memcpy(tmp, rtos_tasks_time_stats.tasks_detail, rtos_tasks_time_stats.num * sizeof(struct task_run_time_info));
                rtos_free(rtos_tasks_time_stats.tasks_detail);
                rtos_tasks_time_stats.tasks_detail = tmp;
                rtos_tasks_time_stats.num          = uxArraySize;
            }
            else
            {
                goto END;
            }
        }
        else
        {
            ;
        }

        if (ulTotalTime > 0)
        {
            if (first_round == 0)
            {
                for( x = 0; x < uxArraySize; x++ )
                {
                    for (i = 0; i < rtos_tasks_time_stats.num; i++)
                    {
                        if (rtos_tasks_time_stats.tasks_detail[i].task_id == pxTaskStatusArray[x].xTaskNumber)
                        {
                            rtos_tasks_time_stats.tasks_detail[i].age = rtos_tasks_time_stats.counter;
                            break;
                        }
                    }
                }
            }
            lastTotalTime = ulTotalTime;
            if (ulTotalTime >= rtos_tasks_time_stats.totalTime)
                ulTotalTime = (ulTotalTime - rtos_tasks_time_stats.totalTime) / 100;
            else
                ulTotalTime = (((configRUN_TIME_COUNTER_TYPE)-1) - rtos_tasks_time_stats.totalTime + ulTotalTime) / 100;

            for( x = 0; x < uxArraySize; x++ )
            {
                current = -1;
                for (i = 0; i < rtos_tasks_time_stats.num; i++)
                {
                    if (rtos_tasks_time_stats.tasks_detail[i].task_id == pxTaskStatusArray[x].xTaskNumber)
                    {
                        current = i;
                        break;
                    }
                    else if ((current < 0) && (rtos_tasks_time_stats.tasks_detail[i].age != rtos_tasks_time_stats.counter))
                    {
                        current = i;
                    }
                }

                if (current >= 0)
                {
                    rtos_tasks_time_stats.tasks_detail[current].task_id  = pxTaskStatusArray[x].xTaskNumber;
                    rtos_tasks_time_stats.tasks_detail[current].age      = rtos_tasks_time_stats.counter;

                    if (pxTaskStatusArray[ x ].ulRunTimeCounter >= rtos_tasks_time_stats.tasks_detail[current].run_time)
                        ulStatsAsPercentage = (pxTaskStatusArray[ x ].ulRunTimeCounter - rtos_tasks_time_stats.tasks_detail[current].run_time) / ulTotalTime;
                    else
                        ulStatsAsPercentage = (((configRUN_TIME_COUNTER_TYPE)-1) - rtos_tasks_time_stats.tasks_detail[current].run_time + pxTaskStatusArray[ x ].ulRunTimeCounter) / ulTotalTime;
                    if( ( uxConsumedBufferLength + configMAX_TASK_NAME_LEN ) <= uxBufferLength )
                    {
                        pcWriteBuffer += snprintf(pcWriteBuffer, configMAX_TASK_NAME_LEN, "%-15s", pxTaskStatusArray[ x ].pcTaskName);
                        uxConsumedBufferLength = uxConsumedBufferLength + ( configMAX_TASK_NAME_LEN - 1U );

                        if( uxConsumedBufferLength < ( uxBufferLength - 1U ) )
                        {
                            if( ulStatsAsPercentage > 0U )
                            {
                                iSnprintfReturnValue = snprintf( pcWriteBuffer,
                                                                 uxBufferLength - uxConsumedBufferLength,
                                                                 "\t%u\t\t%u\r\n",
                                                                 ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter,
                                                                 ( unsigned int ) ulStatsAsPercentage );
                            }
                            else
                            {
                               iSnprintfReturnValue = snprintf( pcWriteBuffer,
                                                             uxBufferLength - uxConsumedBufferLength,
                                                             "\t%u\t\t<1\r\n",
                                                             ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
                            }

                            uxConsumedBufferLength += iSnprintfReturnValue;
                            pcWriteBuffer += iSnprintfReturnValue;
                        }
                        else
                        {
                            xOutputBufferFull = 1;
                        }
                    }
                    else
                    {
                        xOutputBufferFull = 1;
                    }
                    rtos_tasks_time_stats.tasks_detail[current].run_time = pxTaskStatusArray[x].ulRunTimeCounter;
                }

                if( xOutputBufferFull == 1 )
                    break;
            }

            if (rtos_tasks_time_stats.num > (uxArraySize << 1))
            {
                tmp = rtos_malloc( uxArraySize * sizeof(struct task_run_time_info) );
                if (tmp != NULL)
                {
                    for( i = 0, x = 0; i < rtos_tasks_time_stats.num; i++ )
                    {
                        if (rtos_tasks_time_stats.tasks_detail[i].age == rtos_tasks_time_stats.counter)
                        {
                            tmp[x++] = rtos_tasks_time_stats.tasks_detail[i];
                        }
                    }
                    rtos_free(rtos_tasks_time_stats.tasks_detail);
                    rtos_tasks_time_stats.tasks_detail = tmp;
                    rtos_tasks_time_stats.num          = uxArraySize;
                }
                else
                {
                    goto END;
                }
            }
            rtos_tasks_time_stats.counter++;
            rtos_tasks_time_stats.totalTime = lastTotalTime;
        }

END:
        rtos_free( pxTaskStatusArray );
    }
}

void rtos_get_cpu_usage1( char * pcWriteBuffer, int32_t uxBufferLength )
{
    vTaskGetRunTimeStats(pcWriteBuffer);
}
#else
void rtos_get_cpu_usage( char * pcWriteBuffer, int32_t uxBufferLength )
{
}
void rtos_get_cpu_usage1( char * pcWriteBuffer, int32_t uxBufferLength )
{
}
#endif
