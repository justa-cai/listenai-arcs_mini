#include <lisa_mutex.h>
#include <lisa_mem.h>
#include <lisa_log.h>
#include <lisa_typedef.h>
#include <FreeRTOS.h>
#include <semphr.h>

#define TAG "lisa_mutex"

/**
 * @brief 创建互斥锁
 * 
 * @return lisa_mutex_t* 
 */
lisa_mutex_t *lisa_mutex_create(void)
{
	void *mutex = NULL;
	mutex = xSemaphoreCreateMutex();
	if (mutex == NULL) {
		LISA_LOGE(TAG, "lisa_mutex_create failed");
		return NULL;
	}
	lisa_mutex_t *lisa_mutex = (lisa_mutex_t *)lisa_mem_alloc(sizeof(lisa_mutex_t));
	if (lisa_mutex) {
		lisa_mutex->handle = mutex;
	} else {
		vSemaphoreDelete(mutex);
		LISA_LOGE(TAG, "lisa_mutex_create mem calloc failed");
	}
	return lisa_mutex;
}

/**
 * @brief 互斥锁加锁
 * 
 * @param mutex 
 * @param block_time 
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time)
{
	if (mutex == NULL || mutex->handle == NULL) {
		LISA_LOGE(TAG, "lisa_mutex_lock error!!!\n");
		return LISA_FAIL;
	}
	BaseType_t ret;
	TickType_t wait_tick;
	if (block_time == LISA_OS_WAIT_FOREVER) {
		wait_tick = portMAX_DELAY;
	} else {
		wait_tick = pdMS_TO_TICKS(block_time);
	}
	ret = xSemaphoreTake(mutex->handle, wait_tick);
	if (ret != pdPASS) {
		LISA_LOGE(TAG, "%s() fail @ %d", __func__, __LINE__);
		return LISA_FAIL;
	}

	return LISA_OK;
}

/**
 * @brief 互斥锁解锁
 * 
 * @param mutex 
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_unlock(lisa_mutex_t *mutex)
{
	if (mutex == NULL || mutex->handle == NULL) {
		LISA_LOGE(TAG, "lisa_mutex_unlock error!!!\n");
		return LISA_FAIL;
	}
	BaseType_t ret;

	ret = xSemaphoreGive(mutex->handle);
	if (ret != pdPASS) {
		// LISA_LOGE(TAG, "%s() fail @ %d\n", __func__, __LINE__);
		return LISA_FAIL;
	}

	return LISA_OK;
}

/**
 * @brief 销毁互斥锁
 * 
 * @param mutex 
 * @return lisa_err_t 
 */
lisa_err_t lisa_mutex_delete(lisa_mutex_t *mutex)
{
	if (mutex != NULL) {
		if (mutex->handle != NULL) {
			vSemaphoreDelete(mutex->handle);
		}
		lisa_mem_free(mutex);
	}
	return LISA_OK;
}
