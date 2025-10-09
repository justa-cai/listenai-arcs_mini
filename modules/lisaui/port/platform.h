
#ifndef __LISAUI_PORT_PLATFORM_H__
#define __LISAUI_PORT_PLATFORM_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#if CONFIG_LISAUI_ENV_ARCS_SDK
#include "sysheap.h"
#include "FreeRTOS.h"
#include "semphr.h"
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#endif

#define LISAUI_ENV_MAX_DELAY (0xffffffff)

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_GROUP_SECTION_LEVEL1  __attribute__((used, section("._lisaui.group.level1.*")))
#define LISAUI_GROUP_SECTION_LEVEL2  __attribute__((used, section("._lisaui.group.level2.*")))
#define LISAUI_PAGE_SECTION __attribute__((used, section("._lisaui.pages.*")))


#if CONFIG_LISAUI_ENV_ARCS_SDK
    #define LISAUI_OS_SUCCESS pdPASS
    #define LISAUI_OS_ERROR pdFAIL
    
    #define lisaui_malloc(x) exram_malloc(4, x)
    #define lisaui_align_malloc(align,x) exram_malloc(align, x)
    #define lisaui_free(x)   exram_free(x)
    #define lisaui_queue_create(x,y) xQueueCreate(x,y)
    #define lisaui_queue_delete(x) vQueueDelete(x)
    #define lisaui_queue_send(x,y,z) xQueueSend(x,y,z)
    #define lisaui_queue_receive(x,y,z) xQueueReceive(x,y,z)
    #define lisaui_queue_is_empty(x) xQueueIsQueueEmpty(x)
    #define lisaui_queue_is_full(x) xQueueIsQueueFull(x)
    typedef struct
    {
        SemaphoreHandle_t mutex;
    } lisaui_mutex_handle_t;

    static inline int lisaui_mutex_create(lisaui_mutex_handle_t *handle)
    {

        handle->mutex = xSemaphoreCreateRecursiveMutex();
        return 0;
    }

    static inline int lisaui_mutex_lock(lisaui_mutex_handle_t *handle, int timeout_ms)
    {

        int ret;

        if (LISAUI_ENV_MAX_DELAY == timeout_ms)
        {
            ret = xSemaphoreTakeRecursive(handle->mutex, portMAX_DELAY);
        }
        else
        {
            ret = xSemaphoreTakeRecursive(handle->mutex, pdMS_TO_TICKS(timeout_ms));
        }

        return ret;
    }

    static inline int lisaui_mutex_unlock(lisaui_mutex_handle_t *handle)
    {
        int ret;

        ret = xSemaphoreGiveRecursive(handle->mutex);

        return ret;
    }

    static inline int lisaui_mutex_destroy(lisaui_mutex_handle_t *handle)
    {

        vSemaphoreDelete(handle->mutex);

        return 0;
    }


#else
    #define lisaui_malloc malloc
    #define lisaui_free   free
    static inline int lisaui_mutex_create(lisaui_mutex_handle_t *handle)
    {
        pthread_mutexattr_t attr;
        int ret;

        if (handle == NULL)
        {
            return -EINVAL;
        }

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        
        ret = pthread_mutex_init(&handle->mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        
        if (ret == 0)
        {
            handle->initialized = 1;
            return 0;
        }
        
        return -ret;
    }

    static inline int lisaui_mutex_lock(lisaui_mutex_handle_t *handle, int timeout_ms)
    {
        int ret;

        if (handle == NULL || !handle->initialized)
        {
            return -EINVAL;
        }

        if (timeout_ms == LISAUI_ENV_MAX_DELAY)
        {
            // 无限等待
            ret = pthread_mutex_lock(&handle->mutex);
            return (ret == 0) ? 0 : -ret;
        }
        else
        {
            // 有超时的等待
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            
            // 处理纳秒溢出
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }
            
            ret = pthread_mutex_timedlock(&handle->mutex, &ts);
            return (ret == 0) ? 0 : -ret;
        }
    }

    static inline int lisaui_mutex_unlock(lisaui_mutex_handle_t *handle)
    {
        int ret;

        if (handle == NULL || !handle->initialized)
        {
            return -EINVAL;
        }

        ret = pthread_mutex_unlock(&handle->mutex);
        return (ret == 0) ? 0 : -ret;
    }

    static inline int lisaui_mutex_destroy(lisaui_mutex_handle_t *handle)
    {
        int ret;

        if (handle == NULL || !handle->initialized)
        {
            return -EINVAL;
        }

        ret = pthread_mutex_destroy(&handle->mutex);
        if (ret == 0)
        {
            handle->initialized = 0;
        }
        
        return (ret == 0) ? 0 : -ret;
    }
#endif

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_PORT_PLATFORM_H__ */