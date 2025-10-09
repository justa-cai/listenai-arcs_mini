#include <string.h>
#include <lisa_mem.h>
#include <lisa_queue.h>
#include <lisa_typedef.h>
#include <FreeRTOS.h>
#include <queue.h>

#define TICKS_0_MS (0)

/**
 * @brief 队列创建
 * 
 * @param count 
 * @param queue_name 
 * @param item_size 
 * @return lisa_queue_t* 
 */
lisa_queue_t *lisa_queue_create(uint32_t count, uint8_t *queue_name, uint32_t item_size)
{
	lisa_queue_t *lisa_queue = (lisa_queue_t *)lisa_mem_alloc(sizeof(lisa_queue_t));
	if (lisa_queue) {
		QueueHandle_t queue = xQueueCreate(count, item_size);
		if (queue == NULL) {
			lisa_mem_free(lisa_queue);
			return NULL;
		}
		lisa_queue->count = count;
		lisa_queue->item_size = item_size;
		lisa_queue->handle = (lisa_queuehandle_t)queue;
	} else {
	}
	return lisa_queue;
}

/**
 * @brief 队列入队
 * 
 * @param queue 
 * @param item 
 * @param item_size 
 * @param wait 
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_push(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait)
{
	if (queue == NULL || queue->handle == NULL) {
		return LISA_FAIL;
	}
	TickType_t wait_tick;
	if (wait == LISA_OS_WAIT_FOREVER) {
		wait_tick = portMAX_DELAY;
	} else {
		wait_tick = pdMS_TO_TICKS(wait);
	}
	BaseType_t ret = xQueueSend((QueueHandle_t)queue->handle, item, wait_tick);
	if (ret != pdPASS) {
		return LISA_FAIL;
	}
	return LISA_OK;
}

lisa_err_t lisa_queue_push_front(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait)
{
	if (queue == NULL || queue->handle == NULL) return LISA_FAIL;

	TickType_t wait_tick;
	if (wait == LISA_OS_WAIT_FOREVER) {
		wait_tick = portMAX_DELAY;
	} else {
		wait_tick = pdMS_TO_TICKS(wait);
	}

	BaseType_t ret = xQueueSendToFront((QueueHandle_t)queue->handle, item, wait_tick);
	if (ret != pdPASS) {
		return LISA_FAIL;
	}
	return LISA_OK;
}

/**
 * @brief 队列出队
 * 
 * @param queue 
 * @param item 
 * @param item_size 
 * @param wait 
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_pop(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait)
{
	if (queue == NULL || queue->handle == NULL) {
		return LISA_FAIL;
	}
	TickType_t wait_tick;
	if (wait == LISA_OS_WAIT_FOREVER) {
		wait_tick = portMAX_DELAY;
	} else {
		wait_tick = pdMS_TO_TICKS(wait);
	}
	BaseType_t ret = xQueueReceive((QueueHandle_t)queue->handle, item, wait_tick);
	if (ret != pdPASS) {
		return LISA_FAIL;
	}
	return LISA_OK;
}

lisa_err_t lisa_queue_expand(lisa_queue_t *queue, uint32_t expand_count, uint32_t item_size)
{
	if (queue == NULL || queue->handle == NULL) {
		return LISA_FAIL;
	}
	QueueHandle_t new_queue = xQueueCreate(expand_count + queue->count, queue->item_size);
	if (!new_queue) {
		return LISA_FAIL;
	}
	uint8_t *buffer = lisa_mem_alloc(queue->item_size);
	while (xQueueReceive((QueueHandle_t)queue->handle, buffer, TICKS_0_MS) == pdPASS) {
		xQueueSend(new_queue, buffer, TICKS_0_MS);
	}
	lisa_mem_free(buffer);
	vQueueDelete((QueueHandle_t)queue->handle);
	queue->handle = new_queue;
	queue->count += expand_count;
	queue->item_size = item_size;
	return LISA_OK;
}

bool lisa_queue_full(lisa_queue_t *queue)
{
	if (queue == NULL || queue->handle == NULL) {
		return false;
	}
	return (uxQueueMessagesWaiting((const QueueHandle_t)queue->handle) == queue->count);
}

/**
 * @brief 队列销毁
 * 
 * @param queue 
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_delete(lisa_queue_t *queue)
{
	if (queue != NULL) {
		if (queue->handle != NULL) {
			lisa_queue_clear(queue);
			vQueueDelete((QueueHandle_t)queue->handle);
		}
		lisa_mem_free(queue);
	}
	return LISA_OK;
}

/**
 * @brief 队列等待
 * 
 * @param queue 
 * @return uint32_t 
 */
uint32_t lisa_queue_waiting(lisa_queue_t *queue)
{
	if (queue == NULL || queue->handle == NULL) {
		return LISA_FAIL;
	}
	
	return uxQueueMessagesWaiting((const QueueHandle_t)queue->handle);
}

/**
 * @brief 队列清空
 * 
 * @param queue 
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_clear(lisa_queue_t *queue)
{
	if (queue == NULL || queue->handle == NULL) {
		return LISA_FAIL;
	}

	if (lisa_queue_waiting(queue) > 0)
	{
		uint8_t *buffer = lisa_mem_alloc(queue->item_size);
		while (xQueueReceive((QueueHandle_t)queue->handle, buffer, TICKS_0_MS) == pdPASS) {}
		lisa_mem_free(buffer);
	}
	return LISA_OK;
}

uint32_t lisa_queue_size(lisa_queue_t *queue)
{
	if (queue == NULL || queue->handle == NULL) {
		return 0;
	}
	return queue->count;
}