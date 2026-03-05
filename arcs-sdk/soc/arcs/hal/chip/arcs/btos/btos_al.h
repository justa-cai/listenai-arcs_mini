/**
 ****************************************************************************************
 * @file btos_al.h
 *
 * @brief  BT Rtos AL Header
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */


#ifndef BTOS_AL_H_
#define BTOS_AL_H_

#include "btos_def.h"
#include "stdbool.h"
#include "log_print.h"



/*
 * FUNCTIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Get the current btos time, in ms.
 *
 * @param[in] isr  Indicate if this is called from ISR.
 *
 * @return The current btos time (in ms)
 ****************************************************************************************
 */
uint32_t btos_now(bool isr);

/**
 ****************************************************************************************
 * @brief Allocate memory.
 *
 * @param[in] size Size, in bytes, to allocate.
 *
 * @return Address of allocated memory on success and NULL if error occurred.
 ****************************************************************************************
 */
void *btos_malloc(uint32_t size);

/**
 ****************************************************************************************
 * @brief Allocate memory and initialize it to 0.
 *
 * @param[in] nb_elt  Number of element to allocate.
 * @param[in] size    Size, in bytes, of each element allocate.
 *
 * @return Address of allocated and initialized memory on success and NULL if error
 * occurred.
 ****************************************************************************************
 */
void *btos_calloc(uint32_t nb_elt, uint32_t size);

/**
 ****************************************************************************************
 * @brief Free memory.
 *
 * @param[in] ptr Memory buffer to free. MUST have been allocated with @ref btos_malloc
 ****************************************************************************************
 */
void btos_free(void *ptr);

/**
 ****************************************************************************************
 * @brief Create a btos task.
 *
 * @param[in] func Pointer to the task function
 * @param[in] name Name of the task
 * @param[in] task_id ID of the task
 * @param[in] stack_depth Required stack depth for the task
 * @param[in] params Pointer to private parameters of the task function, if any
 * @param[in] prio Priority of the task
 * @param[out] task_handle Handle of the task, that might be used in subsequent btos
 *                         function calls
 *
 * @return 0 on success and != 0 if error occurred.
 ****************************************************************************************
 */
int btos_task_create(btos_task_fct func,
                     const char * const name,
                     btos_task_id task_id,
                     const uint16_t stack_depth,
                     void * const params,
                     btos_prio prio,
                     btos_task_handle * const task_handle);
/**
 ****************************************************************************************
 * @brief Delete a btos task.
 *
 * @param[in] task_handle Handle of the task to delete.
 ****************************************************************************************
 */
void btos_task_delete(btos_task_id task_id);

/**
 ****************************************************************************************
 * @brief btos task suspends itself for a specific duration.
 *
 * @param[in] duration Duration in ms.
 ****************************************************************************************
 */
void btos_task_suspend(int duration);

/**
 ****************************************************************************************
 * @brief Create a btos message queue.
 *
 * @param[in]  elt_size Size, in bytes, of one queue element
 * @param[in]  nb_elt   Number of element to allocate for the queue
 * @param[out] queue    Update with queue handle on success
 *
 * @return 0 on success and != 0 if error occurred.
 ****************************************************************************************
 */
int btos_queue_create(int elt_size, int nb_elt, btos_queue *queue);

/**
 ****************************************************************************************
 * @brief Delete a queue previously created by @ref btos_queue_create.
 * This function does not verify if the queue is empty or not before deleting it.
 *
 * @param[in]  queue   Queue handle
 ****************************************************************************************
 */
void btos_queue_delete(btos_queue queue);

/**
 ****************************************************************************************
 * @brief Check if a btos message queue is empty or not.
 * This function can be called both from an ISR and a task.
 *
 * @param[in]  queue   Queue handle
 *
 * @return true if queue is empty, false otherwise.
 ****************************************************************************************
 */
bool btos_queue_is_empty(btos_queue queue);

/**
 ****************************************************************************************
 * @brief Check if a btos message queue is full or not.
 * This function can be called both from an ISR and a task.
 *
 * @param[in]  queue   Queue handle
 *
 * @return true if queue is full, false otherwise.
 ****************************************************************************************
 */
bool btos_queue_is_full(btos_queue queue);

/**
 ****************************************************************************************
 * @brief Get the number of messages pending a queue.
 * This function can be called both from an ISR and a task.
 *
 * @param[in]  queue   Queue handle
 *
 * @return The number of messages pending in the queue.
 ****************************************************************************************
 */
int btos_queue_cnt(btos_queue queue);

/**
 ****************************************************************************************
 * @brief Write a message at the end of a btos message queue.
 *
 * @param[in]  queue   Queue handle
 * @param[in]  msg     Message to copy in the queue. (It is assume that buffer is of the
 *                     size specified in @ref btos_queue_create)
 * @param[in]  timeout Maximum duration to wait, in ms, if queue is full. 0 means do not
 *                     wait and -1 means wait indefinitely.
 * @param[in]  isr     Indicate if this is called from ISR. If set, @p timeout parameter
 *                     is ignored.
 *
 * @return 0 on success and != 0 if error occurred (i.e queue was full and maximum
 * duration has been reached).
 ****************************************************************************************
 */
int btos_queue_write(btos_queue queue, void *msg, int timeout, bool isr);

/**
 ****************************************************************************************
 * @brief Read a message from a btos message queue.
 *
 * @param[in]  queue   Queue handle
 * @param[in]  msg     Buffer to copy into. (It is assume that buffer is of the
 *                     size specified in @ref btos_queue_create)
 * @param[in]  timeout Maximum duration to wait, in ms, if queue is empty. 0 means do not
 *                     wait and -1 means wait indefinitely.
 * @param[in]  isr     Indicate if this is called from ISR. If set, @p timeout parameter
 *                     is ignored.
 *
 * @return 0 on success and != 0 if error occurred (i.e queue was empty and maximum
 * duration has been reached).
 ****************************************************************************************
 */
int btos_queue_read(btos_queue queue, void *msg, int timeout, bool isr);

/**
 ****************************************************************************************
 * @brief Creates and returns a new semaphore.
 *
 * @param[out] semaphore Semaphore handle returned by the function
 * @param[in]  max_count The maximum count value that can be reached by the semaphore.
 *             When the semaphore reaches this value it can no longer be 'given'.
 * @param[in]  init_count The count value assigned to the semaphore when it is created.
 *
 * @return 0 on success and != 0 otherwise.
 ****************************************************************************************
 */
int btos_semaphore_create(btos_semaphore *semaphore, int max_count, int init_count);

/**
 ****************************************************************************************
 * @brief Return a semaphore count.
 *
 * @param[in]  semaphore Semaphore handle
 *
 * @return Semaphore count.
 ****************************************************************************************
 */
int btos_semaphore_get_count(btos_semaphore semaphore);

/**
 ****************************************************************************************
 * @brief Delete a semaphore previously created by @ref btos_semaphore_create.
 *
 * @param[in]  semaphore Semaphore handle
 ****************************************************************************************
 */
void btos_semaphore_delete(btos_semaphore semaphore);

/**
 ****************************************************************************************
 * @brief Wait for a semaphore to be available.
 *
 * @param[in]  semaphore Semaphore handle
 * @param[in]  timeout   Maximum duration to wait, in ms. 0 means do not wait and -1 means
 *                       wait indefinitely.
 *
 * @return 0 on success and != 0 if timeout occurred.
 ****************************************************************************************
 */
int btos_semaphore_wait(btos_semaphore semaphore, int timeout);

/**
 ****************************************************************************************
 * @brief Signal the semaphore the handle of which is passed as parameter.
 *
 * @param[in]  semaphore Semaphore handle
 * @param[in]  isr       Indicate if this is called from ISR
 *
 * @return 0 on success and != 0 otherwise.
 ****************************************************************************************
 */
int btos_semaphore_signal(btos_semaphore semaphore, uint8_t isr);

/**
 ****************************************************************************************
 * @brief Creates and returns a new mutex.
 *
 * @param[out] mutex Mutex handle returned by the function
 *
 * @return 0 on success and != 0 otherwise.
 ****************************************************************************************
 */
int btos_mutex_create(btos_mutex *mutex);

/**
 ****************************************************************************************
 * @brief Delete a mutex previously created by @ref btos_mutex_create.
 *
 * @param[in]  mutex Mutex handle
 ****************************************************************************************
 */
void btos_mutex_delete(btos_mutex mutex);

/**
 ****************************************************************************************
 * @brief Lock a mutex.
 *
 * @param[in]  mutex Mutex handle
 ****************************************************************************************
 */
void btos_mutex_lock(btos_mutex mutex);

/**
 ****************************************************************************************
 * @brief Unlock a mutex.
 *
 * @param[in]  mutex Mutex handle
 ****************************************************************************************
 */
void btos_mutex_unlock(btos_mutex mutex);

/**
 ****************************************************************************************
 * @brief Enter a critical section.
 * This function returns the previous protection level that is then used in the
 * @ref btos_unprotect function call in order to put back the correct protection level
 * when exiting the critical section. This allows nesting the critical sections.
 *
 * @return  The previous protection level
 ****************************************************************************************
 */
uint32_t btos_protect(void);

/**
 ****************************************************************************************
 * @brief Exit a critical section.
 * This function restores the previous protection level.
 *
 * @param[in]  protect The protection level to restore.
 ****************************************************************************************
 */
void btos_unprotect(uint32_t protect);

/**
 ****************************************************************************************
 * @brief Launch the btos scheduler.
 * This function is supposed not to return as btos will switch the context to the highest
 * priority task inside this function.
 *
 ****************************************************************************************
 */
void btos_start_scheduler(void);

/**
 ****************************************************************************************
 * @brief Init btos
 *
 * Initialize btos layers before start.
 *
 * @return 0 on success and != 0 if error occurred
 ****************************************************************************************
 */
int btos_init(void);

/**
 ****************************************************************************************
 * @brief Change the priority of a task
 * This function cannot be called from an ISR.
 *
 * @param[in] handle Task handle
 * @param[in] priority New priority to set to the task
 *
 ****************************************************************************************
 */
void btos_priority_set(btos_task_handle handle, btos_prio priority);

/**
 ****************************************************************************************
 * @brief Return btos task handle
 *
 * @return current task handle
 ****************************************************************************************
 */
btos_task_handle btos_get_task_handle();

/**
 ****************************************************************************************
 * @brief Wait task event
 *
 * @return Status of wait event fuction
 ****************************************************************************************
 */
uint8_t btos_wait_event(btos_task_id task_id, btos_event_t *event, TickType_t time_out);

/**
 ****************************************************************************************
 * @brief Return btos time
 *
 * @return current task handle
 ****************************************************************************************
 */
int btos_get_time(uint32_t *sec, uint32_t *usec);

/**
 ****************************************************************************************
 * @brief Send btos event
 *
 ****************************************************************************************
 */
uint8_t btos_send_event(btos_task_id task_id, btos_event_t *event, TickType_t time_out);

/**
 ****************************************************************************************
 * @brief Send btos event in interrupt
 *
 ****************************************************************************************
 */
uint8_t btos_send_event_isr(btos_task_id task_id, btos_event_t *event);

/**
 ****************************************************************************************
 * @brief Btos timer creat
 *
 ****************************************************************************************
 */
TimerHandle_t btos_timer_creat(timer_type_t timer_type, uint32_t milli_seconds, TimerCallbackFunction_t call_back_func);

/**
 ****************************************************************************************
 * @brief Btos timer cancel
 *
 ****************************************************************************************
 */
int btos_timer_cancel(TimerHandle_t timer);

/**
 ****************************************************************************************
 * @brief Print btos heap status
 *
 ****************************************************************************************
 */
void btos_heap_status(void);

#endif // btos_H_

/**
 * @}
 */
