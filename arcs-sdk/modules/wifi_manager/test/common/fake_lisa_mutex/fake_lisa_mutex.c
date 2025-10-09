/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fake_lisa_mutex.h"

#include "fff.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

DEFINE_FAKE_VALUE_FUNC(lisa_mutex_t*, lisa_mutex_create);
DEFINE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_lock, lisa_mutex_t *, int32_t);
DEFINE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_unlock, lisa_mutex_t *);
DEFINE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_delete, lisa_mutex_t *);

static lisa_mutex_t* custom_lisa_mutex_create(void)
{
    lisa_mutex_t* mutex = (lisa_mutex_t*)malloc(sizeof(lisa_mutex_t));
    if (mutex == NULL) {
        return NULL;
    }
    
    pthread_mutex_t* pmutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (pmutex == NULL) {
        free(mutex);
        return NULL;
    }
    
    if (pthread_mutex_init(pmutex, NULL) != 0) {
        free(pmutex);
        free(mutex);
        return NULL;
    }
    
    mutex->handle = pmutex;
    mutex->locked = false;
    return mutex;
}

static lisa_err_t custom_lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time)
{
    pthread_mutex_t* pmutex = (pthread_mutex_t*)mutex->handle;
    
    int result;
    if (block_time < 0) {
        // 阻塞式等待
        result = pthread_mutex_lock(pmutex);
    } else if (block_time == 0) {
        // 非阻塞尝试
        result = pthread_mutex_trylock(pmutex);
    } else {
        // 超时等待 (Linux不直接支持超时互斥锁，这里简化处理)
        result = pthread_mutex_lock(pmutex);
    }
    
    if (result == 0) {
        mutex->locked = true;
        return LISA_OK;
    }
    
    return LISA_FAIL;
}

static lisa_err_t custom_lisa_mutex_unlock(lisa_mutex_t *mutex)
{
    pthread_mutex_t* pmutex = (pthread_mutex_t*)mutex->handle;
    
    if (pthread_mutex_unlock(pmutex) == 0) {
        mutex->locked = false;
        return LISA_OK;
    }
    
    return LISA_FAIL;
}

static lisa_err_t custom_lisa_mutex_delete(lisa_mutex_t *mutex)
{
    if (mutex && mutex->handle) {
        pthread_mutex_t* pmutex = (pthread_mutex_t*)mutex->handle;
        pthread_mutex_destroy(pmutex);
        free(pmutex);
        free(mutex);
        return LISA_OK;
    }
    
    return LISA_FAIL;
}

void fake_lisa_mutex_init(void)
{
    RESET_FAKE(lisa_mutex_create);
    RESET_FAKE(lisa_mutex_lock);
    RESET_FAKE(lisa_mutex_unlock);
    RESET_FAKE(lisa_mutex_delete);

    lisa_mutex_create_fake.custom_fake = custom_lisa_mutex_create;
    lisa_mutex_lock_fake.custom_fake = custom_lisa_mutex_lock;
    lisa_mutex_unlock_fake.custom_fake = custom_lisa_mutex_unlock;
    lisa_mutex_delete_fake.custom_fake = custom_lisa_mutex_delete;
}


void fake_lisa_mutex_reset(void)
{
    fake_lisa_mutex_init();
}