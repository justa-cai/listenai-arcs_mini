#ifndef __EBUS_PORT_PLATFORM_H_
#define __EBUS_PORT_PLATFORM_H_

#if CONFIG_EBUS_ENV_OS_FREERTOS
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"
#elif CONFIG_EBUS_ENV_OS_POSIX
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if CONFIG_EBUS_ENV_OS_FREERTOS

#define EBUS_ENV_MAX_DELAY 0xffffffff
// #define EBUS_LOG(fmt, ...)
// #define EBUS_ERR(fmt, ...)
#define EBUS_LOG(fmt, ...) printf("[EBUS] " fmt "\n", ##__VA_ARGS__)
#define EBUS_ERR(fmt, ...) printf("[EBUS ERROR] " fmt "\n", ##__VA_ARGS__)


    void *platform_malloc(uint32_t size)
    {
        return malloc(size);
    }

    void platform_free(void *ptr)
    {
        free(ptr);
    }

    typedef struct
    {
        SemaphoreHandle_t mutex;
    } ebus_env_mutex_handle_t;

    static inline int ebus_env_mutex_create(ebus_env_mutex_handle_t *handle)
    {

        handle->mutex = xSemaphoreCreateRecursiveMutex();
        return 0;
    }

    static inline int ebus_env_mutex_lock(ebus_env_mutex_handle_t *handle, int timeout_ms)
    {

        int ret;

        if (EBUS_ENV_MAX_DELAY == timeout_ms)
        {
            ret = xSemaphoreTakeRecursive(handle->mutex, portMAX_DELAY);
        }
        else
        {
            ret = xSemaphoreTakeRecursive(handle->mutex, pdMS_TO_TICKS(timeout_ms));
        }

        return ret;
    }

    static inline int ebus_env_mutex_unlock(ebus_env_mutex_handle_t *handle)
    {
        int ret;

        ret = xSemaphoreGiveRecursive(handle->mutex);

        return ret;
    }

    static inline int ebus_env_mutex_destroy(ebus_env_mutex_handle_t *handle)
    {

        vSemaphoreDelete(handle->mutex);

        return 0;
    }

    static inline int ebus_env_thread_delay_ms(uint32_t delay_ms)
    {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        return 0;
    }

#elif CONFIG_EBUS_ENV_OS_POSIX

#define EBUS_ENV_MAX_DELAY -1
#define EBUS_LOG(fmt, ...) printf("[EBUS] " fmt "\n", ##__VA_ARGS__)
#define EBUS_ERR(fmt, ...) fprintf(stderr, "[EBUS ERROR] " fmt "\n", ##__VA_ARGS__)

    typedef struct
    {
        pthread_mutex_t mutex;
        int initialized;
    } ebus_env_mutex_handle_t;

    void *platform_malloc(uint32_t size)
    {
        return malloc(size);
    }

    void platform_free(void *ptr)
    {
        free(ptr);
    }

    static inline int ebus_env_mutex_create(ebus_env_mutex_handle_t *handle)
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

    static inline int ebus_env_mutex_lock(ebus_env_mutex_handle_t *handle, int timeout_ms)
    {
        int ret;

        if (handle == NULL || !handle->initialized)
        {
            return -EINVAL;
        }

        if (timeout_ms == EBUS_ENV_MAX_DELAY)
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

    static inline int ebus_env_mutex_unlock(ebus_env_mutex_handle_t *handle)
    {
        int ret;

        if (handle == NULL || !handle->initialized)
        {
            return -EINVAL;
        }

        ret = pthread_mutex_unlock(&handle->mutex);
        return (ret == 0) ? 0 : -ret;
    }

    static inline int ebus_env_mutex_destroy(ebus_env_mutex_handle_t *handle)
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

    static inline int ebus_env_thread_delay_ms(uint32_t delay_ms)
    {
        struct timespec ts;
        ts.tv_sec = delay_ms / 1000;
        ts.tv_nsec = (delay_ms % 1000) * 1000000;
        
        return nanosleep(&ts, NULL);
    }

#endif

#ifdef __cplusplus
}
#endif

#endif
