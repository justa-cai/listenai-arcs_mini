#include "mock_rtos.h"

DEFINE_FAKE_VALUE_FUNC(int, rtos_queue_create, size_t, int, rtos_queue *);
DEFINE_FAKE_VALUE_FUNC(int, rtos_queue_write, rtos_queue, void *, int, int);
DEFINE_FAKE_VALUE_FUNC(int, rtos_queue_read, rtos_queue, void *, int, int);
DEFINE_FAKE_VALUE_FUNC(int, rtos_queue_cnt, rtos_queue);

DEFINE_FAKE_VALUE_FUNC(int, rtos_semaphore_create, rtos_semaphore *, int, int);
DEFINE_FAKE_VOID_FUNC(rtos_semaphore_signal, rtos_semaphore, int);
DEFINE_FAKE_VOID_FUNC(rtos_semaphore_wait, rtos_semaphore, int);

DEFINE_FAKE_VALUE_FUNC(int, rtos_mutex_create, rtos_mutex *);
DEFINE_FAKE_VOID_FUNC(rtos_mutex_lock, rtos_mutex);
DEFINE_FAKE_VOID_FUNC(rtos_mutex_unlock, rtos_mutex);

DEFINE_FAKE_VALUE_FUNC(void *, rtos_malloc, size_t);
DEFINE_FAKE_VOID_FUNC(rtos_free, void *);
DEFINE_FAKE_VALUE_FUNC(void *, rtos_calloc, size_t, size_t);
DEFINE_FAKE_VOID_FUNC(rtos_delay, int);

DEFINE_FAKE_VALUE_FUNC(rtos_timer, rtos_timer_create, void *, int, unsigned int, rtos_timer_callback);
DEFINE_FAKE_VALUE_FUNC(int, rtos_timer_start, rtos_timer);
DEFINE_FAKE_VALUE_FUNC(int, rtos_timer_stop, rtos_timer);
DEFINE_FAKE_VOID_FUNC(rtos_timer_schedule, rtos_timer, unsigned int);

void mock_rtos_reset(void)
{
    RESET_FAKE(rtos_semaphore_create);
    RESET_FAKE(rtos_mutex_create);
    RESET_FAKE(rtos_calloc);
}
