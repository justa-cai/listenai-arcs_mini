/**
 ****************************************************************************************
 * @file btos_al.c
 *
 * @brief  BT Rtos AL source
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */


/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "string.h"
#include "btos_def.h"
#include "btos_al.h"
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static __inline
#endif
#ifndef WIN32
#define BTOS_MEM_TRACE 0
#if BTOS_MEM_TRACE
#define MEM_TRACE 100

uint32_t mem_count = 0;
uint32_t mem_debug_trace[MEM_TRACE]={0,};
#endif
static btos_handle_t btos_list[OS_TASK_ID_TOTAL];
/*
 * FUNCTIONS
 ****************************************************************************************
 */
 __STATIC_INLINE TickType_t btos_timeout_2_tickcount(int timeout_ms)
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
void *btos_malloc(uint32_t size)
{
    void *ptr = pvPortMalloc(size);
#if BTOS_MEM_TRACE
    taskENTER_CRITICAL();
    mem_debug_trace[mem_count] = ptr;
    mem_count++;
    taskEXIT_CRITICAL();
    CLOGD("m:%x,%d,%d", ptr, size, mem_count);
#endif
    if(ptr == NULL)
    {
        btos_heap_status();
    }
    return ptr;
}

void *btos_calloc(uint32_t nb_elt, uint32_t size)
{
    void * ptr = pvPortMalloc(nb_elt * size);
#if BTOS_MEM_TRACE
    taskENTER_CRITICAL();
    mem_debug_trace[mem_count] = ptr;
    mem_count++;
    taskEXIT_CRITICAL();
    CLOGD("c:%x,%d,%d", ptr, size, mem_count);
#endif
    if (ptr)
    {
        memset(ptr, 0, nb_elt * size);
    }
    else
    {
        btos_heap_status();
    }
    return ptr;
}

void btos_free(void *ptr)
{
#if BTOS_MEM_TRACE
    uint16_t i = 0;
    uint16_t found = 0;
    taskENTER_CRITICAL();
    for(i = 0; i < mem_count; i++)
    {
        if(mem_debug_trace[i] == ptr)
        {
            #if 0
            CLOGD("be mem_trace[%d]:%x %x %x %x %x %x %x %x %x %x", i, mem_debug_trace[i], 
                mem_debug_trace[i+1], mem_debug_trace[i+2], mem_debug_trace[i+3], mem_debug_trace[i+4],
                mem_debug_trace[i+5], mem_debug_trace[i+6], mem_debug_trace[i+7], mem_debug_trace[i+8],
                mem_debug_trace[i+9]);
            #endif
            memcpy(&mem_debug_trace[i], &mem_debug_trace[i+1], (mem_count - 1 - i)*4);
            mem_debug_trace[mem_count - 1] = 0;
            #if 0
            CLOGD("af mem_trace[%d]:%x %x %x %x %x %x %x %x %x %x", i, mem_debug_trace[i], 
                mem_debug_trace[i+1], mem_debug_trace[i+2], mem_debug_trace[i+3], mem_debug_trace[i+4],
                mem_debug_trace[i+5], mem_debug_trace[i+6], mem_debug_trace[i+7], mem_debug_trace[i+8],
                mem_debug_trace[i+9]);
            #endif
            found = 1;
            break;
        }
    }
    mem_count--;
    taskEXIT_CRITICAL();
    CLOGD("f:%x,%d,%d,%d", ptr, mem_count, i, found);
    if(found == 0)
    {
        taskENTER_CRITICAL();
        btos_heap_status();
        CLOGD("f err:%x,%d,%d,%d", ptr, mem_count, i, found);
        while(1);
    }
#endif
    vPortFree(ptr);
}

void btos_heap_status(void)
{
    heap_summary_info();
}

int btos_task_create(btos_task_fct func,
                     const char * const name,
                     btos_task_id task_id,
                     const uint16_t stack_depth,
                     void * const params,
                     btos_prio prio,
                     btos_task_handle * const task_handle)
{
    BaseType_t res;
    btos_task_handle handle;
    btos_handle_t *p_task = &(btos_list[task_id]);
    
    p_task->taskid = task_id;
    p_task->func = func;
    p_task->name = (void *)name;
    p_task->stack_size = stack_depth;
    p_task->stack_prio = prio;
    p_task->sole = pdFALSE;
    p_task->queue = xQueueCreate(50, sizeof(btos_event_t));
    CLOGD("btos_task_create,id:%d, q:0x%x",task_id, p_task->queue);
    if(p_task->queue == NULL)
    {
        return pdFALSE;
    }
    res = xTaskCreate(func, name, stack_depth, params, prio, &handle);

    if (res == pdFAIL)
        return pdFALSE;

    p_task->task_handle = handle;

    if (task_handle)
        *task_handle = handle;

    return pdTRUE;
}

void btos_task_delete(btos_task_id task_id)
{
    btos_handle_t *p_task = &(btos_list[task_id]);

    if (p_task->task_handle)
    {
        vTaskDelete(p_task->task_handle);
        vQueueDelete(p_task->task_handle);
        p_task->queue = NULL;
        p_task->task_handle = NULL;
    }
}
#if 0
uint8_t btos_task_wait_notification(int timeout)
{
    return ulTaskNotifyTake(pdTRUE, rtos_timeout_2_tickcount(timeout));
}

void btos_task_notify(btos_task_id task_id, bool isr)
{
    if (isr)
    {
        BaseType_t task_woken = pdFALSE;

        vTaskNotifyGiveFromISR(task_id, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        xTaskNotifyGive(task_id);
    }
}
#endif
uint8_t btos_send_event(btos_task_id task_id, btos_event_t *event, TickType_t time_out)
{
    uint8_t status = pdTRUE;
    btos_handle_t *p_task = &(btos_list[task_id]);
    
    //CLOGD("btos_send_event,id:%d,msg_body:0x%x,sta:%d,q:0x%x",task_id, event->msg_body, status, p_task->queue);
    if(!p_task->queue)
    {
        return pdFALSE;
    }
    if (xQueueSend(p_task->queue, (void*)&event->msg_body,  time_out) == pdFALSE)
    {
        status = pdFALSE;
    }
    return status;
}

uint8_t btos_send_event_isr(btos_task_id task_id, btos_event_t *event)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t status = pdTRUE;
    btos_handle_t *p_task = &(btos_list[task_id]);
    
    //CLOGD("btos_send_event_isr,id:%d,msg_body:0x%x,sta:%d,q:0x%x",task_id, event->msg_body, status, p_task->queue);
    if(!p_task->queue)
    {
        return pdFALSE;
    }
    if (xQueueSendFromISR(p_task->queue, (void*)&event->msg_body,  &task_woken) == pdFALSE)
    {
        status = pdFALSE;
    }
    
    portYIELD_FROM_ISR(task_woken);
    return status;
}


uint8_t btos_wait_event(btos_task_id task_id, btos_event_t *event, TickType_t time_out)
{
    uint8_t status = pdTRUE;
    btos_handle_t *p_task = &(btos_list[task_id]);
    //CLOGD("btos_wait_event,q:0x%x",p_task->queue);
    if(!p_task->queue)
    {
        return pdFALSE;
    }
    if (xQueueReceive(p_task->queue, (void*)&event->msg_body,  time_out) == pdFALSE)
    {
        status = pdFALSE;
    }
    
    //CLOGD("btos_wait_event,id:%d,msg_body:0x%x,sta:%d",task_id, event->msg_body, status);
    return status;
}

void btos_task_suspend(int duration)
{
    if (duration <= 0)
        return;
    vTaskDelay(pdMS_TO_TICKS(duration));
}

int btos_semaphore_create(btos_semaphore *semaphore, int max_count, int init_count)
{
    int res = -1;

    if (max_count == 1)
    {
        *semaphore = xSemaphoreCreateBinary();

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

        if (*semaphore != 0)
        {
            res = 0;
        }
    }

    return res;
}

void btos_semaphore_delete(btos_semaphore semaphore)
{
    vSemaphoreDelete(semaphore);
}

int btos_semaphore_get_count(btos_semaphore semaphore)
{
    return uxSemaphoreGetCount(semaphore);
}

int btos_semaphore_wait(btos_semaphore semaphore, int timeout)
{
    BaseType_t res = pdPASS;

    res = xSemaphoreTake(semaphore, btos_timeout_2_tickcount(timeout));

    return (res == errQUEUE_EMPTY);
}

int btos_semaphore_signal(btos_semaphore semaphore, uint8_t isr)
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

int btos_mutex_create(btos_mutex *mutex)
{
    int res = -1;

    *mutex = xSemaphoreCreateMutex();
    if (*mutex != 0)
    {
        res = 0;
    }

    return res;
}

void btos_mutex_delete(btos_mutex mutex)
{
    //ASSERT_ERR(xSemaphoreGetMutexHolder(mutex) == NULL);
    vSemaphoreDelete(mutex);
}

void btos_mutex_lock(btos_mutex mutex)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void btos_mutex_unlock(btos_mutex mutex)
{
    xSemaphoreGive(mutex);
}

uint32_t btos_protect(void)
{
    taskENTER_CRITICAL();
    return 1;
}

void btos_unprotect(uint32_t protect)
{
    (void) protect;
    taskEXIT_CRITICAL();
}

void btos_start_scheduler(void)
{
    CLOGD("rtos schedule!\n");
    vTaskStartScheduler();
}

int btos_init(void)
{
    return 0;
}

btos_task_handle btos_get_task_handle()
{
    return xTaskGetCurrentTaskHandle();
}

void btos_priority_set(btos_task_handle handle, btos_prio priority)
{
    vTaskPrioritySet(handle, priority);
}

int btos_get_time(uint32_t *sec, uint32_t *usec)
{
    TickType_t tickCount;

    tickCount = xTaskGetTickCount();
    *sec  = tickCount / configTICK_RATE_HZ;
    *usec = (tickCount % configTICK_RATE_HZ)*1000;

    return 0;
}

TimerHandle_t btos_timer_creat(timer_type_t timer_type, uint32_t milli_seconds, TimerCallbackFunction_t call_back_func)
{
    
    TimerHandle_t timer_id;
    timer_id = xTimerCreate(NULL, pdMS_TO_TICKS(milli_seconds), timer_type, 0, (TimerCallbackFunction_t)(call_back_func));
    xTimerStart(timer_id, 0);
    return timer_id;
}

int btos_timer_change(TimerHandle_t timer, unsigned int milli_seconds)
{
    xTimerChangePeriod(timer, pdMS_TO_TICKS(milli_seconds), 0);
    return 0;
}

int btos_timer_cancel(TimerHandle_t timer)
{
    xTimerStop(timer, 0);
    xTimerDelete(timer, 0);
    return 0;
}

int btos_timer_stop(TimerHandle_t timer)
{
    xTimerStop(timer, 0);
    return 0;
}

void btos_sleep(unsigned   int  milli_secondes)
{
    vTaskDelay(milli_secondes);
}

#endif
