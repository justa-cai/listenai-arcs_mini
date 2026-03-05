#ifndef __LISA_OS_MUTEX__
#define __LISA_OS_MUTEX__

#include "FreeRTOS.h"
#include <lisa_err.h>

typedef void *lisa_mutexhandle_t;

typedef struct {
	void *handle;
} lisa_mutex_t;

/**
 * @brief create
 * @return lisa_mutex_t* 
 */
lisa_mutex_t *lisa_mutex_create(void);

/**
 * @brief lock
 * @param  mutex            
 * @param  block_time       
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time);

/**
 * @brief unlock
 * @param  mutex            
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_unlock(lisa_mutex_t *mutex);

/**
 * @brief delete
 * @param  mutex            
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_delete(lisa_mutex_t *mutex);

#endif  //__LISA_OS_MUTEX__
