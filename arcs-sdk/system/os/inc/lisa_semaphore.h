#ifndef __LISA_OS_SEMAPHORE__
#define __LISA_OS_SEMAPHORE__

#include <lisa_err.h>

typedef void *lisa_semaphorehandle_t;

typedef struct {
	lisa_semaphorehandle_t handle;
} lisa_semaphore_t;

/**
 * @brief create
 * @param  count            
 * @return lisa_semaphore_t* 
 */
lisa_semaphore_t *lisa_semaphore_create(uint32_t count);

/**
 * @brief take
 * @param  sem              
 * @param  block_time       
 * @return lisa_err_t 
 */
lisa_err_t lisa_semaphore_take(lisa_semaphore_t *sem, int32_t block_time);

/**
 * @brief give
 * @param  sem              
 * @return lisa_err_t 
 */
lisa_err_t lisa_semaphore_give(lisa_semaphore_t *sem);

/**
 * @brief delete
 * @param  sem
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_delete(lisa_semaphore_t *sem);

/**
 * @brief clear
 * @param  sem
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_clear(lisa_semaphore_t *sem);

#endif  //__LISA_OS_SEMAPHORE__
