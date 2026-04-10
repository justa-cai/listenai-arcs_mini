#ifndef MOCK_RTOS_H
#define MOCK_RTOS_H

#include "fff.h"
#include "rtos_al.h"

DECLARE_FAKE_VALUE_FUNC(int, rtos_queue_create, size_t, int, rtos_queue *);
DECLARE_FAKE_VALUE_FUNC(int, rtos_queue_write, rtos_queue, void *, int, int);
DECLARE_FAKE_VALUE_FUNC(int, rtos_queue_read, rtos_queue, void *, int, int);
DECLARE_FAKE_VALUE_FUNC(int, rtos_queue_cnt, rtos_queue);

DECLARE_FAKE_VALUE_FUNC(int, rtos_semaphore_create, rtos_semaphore *, int, int);
DECLARE_FAKE_VOID_FUNC(rtos_semaphore_signal, rtos_semaphore, int);
DECLARE_FAKE_VOID_FUNC(rtos_semaphore_wait, rtos_semaphore, int);

DECLARE_FAKE_VALUE_FUNC(int, rtos_mutex_create, rtos_mutex *);
DECLARE_FAKE_VOID_FUNC(rtos_mutex_lock, rtos_mutex);
DECLARE_FAKE_VOID_FUNC(rtos_mutex_unlock, rtos_mutex);

DECLARE_FAKE_VALUE_FUNC(void *, rtos_malloc, size_t);
DECLARE_FAKE_VOID_FUNC(rtos_free, void *);
DECLARE_FAKE_VALUE_FUNC(void *, rtos_calloc, size_t, size_t);
DECLARE_FAKE_VOID_FUNC(rtos_delay, int);

void mock_rtos_reset(void);

#endif
