/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fake_lisa_thread.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

DEFINE_FAKE_VALUE_FUNC(lisa_thread_t *, lisa_thread_create, const lisa_thread_attr_t *, lisa_thread_entry_t, void *);
DEFINE_FAKE_VALUE_FUNC(lisa_err_t, lisa_thread_delete, lisa_thread_t *);

static void *thread_entry_adapter(void *arg) {
    LisaThreadArg *thread_arg = (LisaThreadArg *)arg;
    if (thread_arg && thread_arg->fn) {
        // printf("func:%s, line:%d, thread_arg->arg: %p\n", __FUNCTION__, __LINE__, thread_arg->arg);
        thread_arg->fn(thread_arg->arg);
    }
    // printf("func:%s, line:%d, free thread_arg: %p\n", __FUNCTION__, __LINE__, thread_arg);
    free(thread_arg);
    return NULL;
}

static lisa_thread_t *custom_fake_thread_create(const lisa_thread_attr_t *attr, lisa_thread_entry_t entry, void *arg)
{
    if (!entry) return NULL;
    lisa_thread_t *thread = (lisa_thread_t *)malloc(sizeof(lisa_thread_t));
    if (!thread) return NULL;

    LisaThreadArg *thread_arg = (LisaThreadArg *)malloc(sizeof(LisaThreadArg));
    if (!thread_arg) {
        free(thread);
        return NULL;
    }
    thread_arg->fn = entry;
    thread_arg->arg = arg;

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    if (attr && attr->stack_size) {
        pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
    }
    // 优先级设置略过，pthread 不直接支持

    int ret = pthread_create(&thread->handle, &pthread_attr, thread_entry_adapter, thread_arg);

    pthread_attr_destroy(&pthread_attr);
    if (ret != 0) {
        printf("pthread_create failed, ret: %d\n", ret);
        free(thread_arg);
        free(thread);
        return NULL;
    }
    return thread;
}

static lisa_err_t custom_fake_thread_delete(lisa_thread_t *thread)
{
    if (!thread) return LISA_OK;
    pthread_join(thread->handle, NULL);
    free(thread);
    return LISA_OK;
}

void fake_lisa_thread_init(void)
{
    RESET_FAKE(lisa_thread_create);
    RESET_FAKE(lisa_thread_delete);


    lisa_thread_create_fake.custom_fake = custom_fake_thread_create;
    lisa_thread_delete_fake.custom_fake = custom_fake_thread_delete;
}

void fake_lisa_thread_reset(void)
{
    fake_lisa_thread_init();
}

