#include <lisa_semaphore.h>
#include <lisa_mem.h>
#include <lisa_log.h>
#include <lisa_typedef.h>
#include <FreeRTOS.h>
#include <semphr.h>

#define TAG "lisa_semaphore"
/**
 * @brief 信号量创建
 *
 * @param count
 * @return lisa_semaphore_t*
 */
lisa_semaphore_t *lisa_semaphore_create(uint32_t count)
{
	lisa_semaphore_t *lisa_sem = (lisa_semaphore_t *)lisa_mem_alloc(sizeof(lisa_semaphore_t));
	if (!lisa_sem) {
		LISA_LOGE(TAG, "create semaphore handle fail");
		return NULL;
	}

	QueueHandle_t sem = xSemaphoreCreateCounting(count, 0);
	if (sem == NULL) {
		LISA_LOGE(TAG, "Semaphore Create Counting failed");
		lisa_mem_free(lisa_sem);
		return NULL;
	}

	lisa_sem->handle = sem;
	return lisa_sem;
}

/**
 * @brief 信号量获取
 *
 * @param sem
 * @param block_time
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_take(lisa_semaphore_t *sem, int32_t block_time)
{
	if (sem == NULL || sem->handle == NULL) {
		LISA_LOGE(TAG, "lisa_semaphore_take err");
		return LISA_FAIL;
	}
	BaseType_t ret;
	if (xPortIsInsideInterrupt()) {
		BaseType_t higher_priority_task_woken = pdFALSE;
		ret = xSemaphoreTakeFromISR(sem->handle, &higher_priority_task_woken);
		if (ret == pdPASS && higher_priority_task_woken == pdTRUE) {
			portYIELD_FROM_ISR(higher_priority_task_woken);
		}
	} else {
		TickType_t wait_tick;
		if (block_time == LISA_OS_WAIT_FOREVER) {
			wait_tick = portMAX_DELAY;
		} else {
			wait_tick = pdMS_TO_TICKS(block_time);
		}
		ret = xSemaphoreTake(sem->handle, wait_tick);
	}
	if (ret != pdPASS) {
		// LISA_LOGE(TAG, "[%d]%s() fail @ %d", xTaskGetTickCount(), __func__, __LINE__);
		return LISA_FAIL;
	}

	return LISA_OK;
}

/**
 * @brief 信号量释放
 *
 * @param sem
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_give(lisa_semaphore_t *sem)
{
	if (sem == NULL || sem->handle == NULL) {
		LISA_LOGE(TAG, "lisa_semaphore_give err");
		return LISA_FAIL;
	}
	if (!xPortIsInsideInterrupt()) {
		return xSemaphoreGive(sem->handle) == pdTRUE ? LISA_OK : LISA_FAIL;
	}

	BaseType_t yield = pdFALSE;
	BaseType_t ret = xSemaphoreGiveFromISR(sem->handle, &yield);
	if (yield == pdTRUE) {
		portYIELD_FROM_ISR(yield);
	}
	return ret == pdTRUE ? LISA_OK : LISA_FAIL;
}

/**
 * @brief 信号量销毁
 *
 * @param sem
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_delete(lisa_semaphore_t *sem)
{
	if (sem != NULL) {
		if (sem->handle != NULL) {
			vSemaphoreDelete(sem->handle);
		}
		lisa_mem_free(sem);
	}
	return LISA_OK;
}

lisa_err_t lisa_semaphore_reset(lisa_semaphore_t *sem)
{
    if (sem == NULL || sem->handle == NULL) {
        LISA_LOGE(TAG, "lisa_semaphore_reset err");
        return LISA_FAIL;
    }

    xQueueReset(sem->handle);

    return LISA_OK;
}
/**
 * @brief 信号量清空
 *
 * @param sem
 * @return lisa_err_t
 */
lisa_err_t lisa_semaphore_clear(lisa_semaphore_t *sem)
{
	if (sem == NULL || sem->handle == NULL) {
		LISA_LOGE(TAG, "lisa_semaphore_clear err");
		return LISA_FAIL;
	}

	// 清空信号量：不断take直到失败（不阻塞）
	while (xSemaphoreTake(sem->handle, 0) == pdPASS) {
		// 继续清空
	}

	return LISA_OK;
}
