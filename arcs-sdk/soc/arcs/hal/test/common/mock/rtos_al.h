#ifndef RTOS_AL_H
#define RTOS_AL_H

#include <stdint.h>
#include <stdlib.h>

typedef void *rtos_queue;
typedef void *rtos_semaphore;
typedef void *rtos_mutex;
typedef void *rtos_timer;
typedef void (*rtos_timer_callback)(rtos_timer timer);

int rtos_queue_create(size_t elem_size, int elem_count, rtos_queue *queue);
int rtos_queue_write(rtos_queue queue, void *item, int timeout, int from_isr);
int rtos_queue_read(rtos_queue queue, void *item, int timeout, int from_isr);
int rtos_queue_cnt(rtos_queue queue);

int rtos_semaphore_create(rtos_semaphore *sem, int max, int init);
void rtos_semaphore_signal(rtos_semaphore sem, int from_isr);
void rtos_semaphore_wait(rtos_semaphore sem, int timeout_ms);

int rtos_mutex_create(rtos_mutex *mutex);
void rtos_mutex_lock(rtos_mutex mutex);
void rtos_mutex_unlock(rtos_mutex mutex);

void *rtos_malloc(size_t size);
void rtos_free(void *ptr);
void *rtos_calloc(size_t count, size_t size);
void rtos_delay(int ms);

rtos_timer rtos_timer_create(void *id, int reload, unsigned int period_ms, rtos_timer_callback cb);
int rtos_timer_start(rtos_timer timer);
int rtos_timer_stop(rtos_timer timer);
void rtos_timer_schedule(rtos_timer timer, unsigned int period_ms);

#endif
