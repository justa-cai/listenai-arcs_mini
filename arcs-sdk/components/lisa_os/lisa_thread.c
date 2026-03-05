#include <lisa_mem.h>
#include <lisa_log.h>
#include <lisa_thread.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#define TAG "lisa_thread"
#include "lisa_log.h"
#define LISA_TASK_MAGIC (0x19911011)

static void __lisa_thread_entry(void *arg)
{
	LisaThreadArg *lisa_arg = (LisaThreadArg *)arg;

	void (*fn)(void *) = lisa_arg->fn;
	void *farg = lisa_arg->arg;
	lisa_mem_free(lisa_arg);

	fn(farg);

	vTaskDelete(NULL);
}

void vPortCleanUpTCB(void *pxTCB)
{
	LisaStaticTask_t *task = (LisaStaticTask_t *)pxTCB;
	LISA_LOGI(TAG, "vPortCleanUpTCB, %p, name:%s, magic:%x", pxTCB, pcTaskGetName((TaskHandle_t)pxTCB), task->magic);

	if (task->magic != LISA_TASK_MAGIC) return;

	if (task->user_stack != 1)
	{
		lisa_mem_free(task->stack);
	}
	lisa_mem_free(task->lisa_thread);

	/* pxTCB由pvMalloc分配, 此函数返回后, pxTCB被kernel释放 */
}

lisa_thread_t *lisa_thread_create(const lisa_thread_attr_t *attr, void (*entry)(void *), void *arg)
{
	if (attr == NULL || attr->stack_size <= 0) {
		LISA_LOGE(TAG, "lisa thread create fail, param error");
		return NULL;
	}

	lisa_thread_t *thread = NULL; // 对外句柄
	LisaStaticTask_t *task = NULL; // 对内句柄
	LisaThreadArg *ls_arg = NULL; // 对内回调函数参数

	thread = lisa_mem_alloc(sizeof(lisa_thread_t));
	if (thread) {
		/* 由freeRTOS分配, 释放也有freeRTOS释放 */
		task = pvPortMalloc(sizeof(LisaStaticTask_t));
		if (task == NULL) goto lisa_thread_create_err;
		uint32_t stack_depth = attr->stack_size / sizeof(StackType_t);
		task->user_stack = 0;
		task->stack = (StackType_t *)lisa_mem_alloc(stack_depth * sizeof(StackType_t));

		if (task->stack == NULL) goto lisa_thread_create_err;

		task->magic = LISA_TASK_MAGIC;
		task->lisa_thread = thread;

		ls_arg = lisa_mem_alloc(sizeof(LisaThreadArg));
		if (ls_arg == NULL) goto lisa_thread_create_err;
		ls_arg->arg = arg;
		ls_arg->fn = entry;

		thread->handle = (lisa_threadhandle_t) xTaskCreateStatic(
				__lisa_thread_entry,
				attr->name,
				stack_depth,
				ls_arg,
				attr->priority,
				task->stack,
				&(task->ltask));
		if (thread->handle == NULL) goto lisa_thread_create_err;

		return thread;
	}

lisa_thread_create_err:
	LISA_LOGE(TAG, "lisa thread create fail");
	if (ls_arg) lisa_mem_free(ls_arg);
	if (task) {
		if (task->stack) lisa_mem_free(task->stack);
		lisa_mem_free(task);
	}
	if (thread) lisa_mem_free(thread);
	return NULL;
}

lisa_thread_t *lisa_thread_create_bymem(const lisa_thread_attr_t *attr, void *task_mem, void (*entry)(void *), void *arg)
{
	if (task_mem == NULL || attr == NULL || attr->stack_size <= 0) {
		LISA_LOGE(TAG, "lisa thread create fail, param error");
		return NULL;
	}

	lisa_thread_t *thread = NULL; // 对外句柄
	LisaStaticTask_t *task = NULL; // 对内句柄
	LisaThreadArg *ls_arg = NULL; // 对内回调函数参数

	thread = lisa_mem_alloc(sizeof(lisa_thread_t));
	if (thread) {
		task = lisa_mem_calloc(1, sizeof(LisaStaticTask_t));
		if (task == NULL) goto lisa_thread_create_err;
		uint32_t stack_depth = attr->stack_size / sizeof(StackType_t);
		task->user_stack = 1;
		task->stack = task_mem;

		task->magic = LISA_TASK_MAGIC;
		task->lisa_thread = thread;

		ls_arg = lisa_mem_alloc(sizeof(LisaThreadArg));
		if (ls_arg == NULL) goto lisa_thread_create_err;
		ls_arg->arg = arg;
		ls_arg->fn = entry;

		thread->handle = (lisa_threadhandle_t) xTaskCreateStatic(
				__lisa_thread_entry,
				attr->name,
				stack_depth,
				ls_arg,
				attr->priority,
				task->stack,
				&(task->ltask));
		if (thread->handle == NULL) goto lisa_thread_create_err;

		return thread;
	}

lisa_thread_create_err:
	LISA_LOGE(TAG, "lisa thread create fail");
	if (ls_arg) lisa_mem_free(ls_arg);
	if (task) lisa_mem_free(task);
	if (thread) lisa_mem_free(thread);
	return NULL;
}

/**
 * @brief 设置线程优先级
 * 
 * @param thread 
 * @param priority 新的优先级
 * @return lisa_err_t 
 */
lisa_err_t lisa_thread_set_priority(lisa_thread_t *thread, uint8_t priority)
{
	lisa_err_t result = LISA_FAIL;
	if (thread != NULL) {
		uint8_t pre_priority = uxTaskPriorityGet(thread->handle);
		if (pre_priority != priority) {
			vTaskPrioritySet(thread->handle, priority);
			LISA_LOGD(TAG, "set priority:%d", priority);
		}
		result = LISA_OK;
	}
	return result;
}

/**
 * @brief 线程销毁
 *
 * @param thread
 * @return lisa_err_t
 */
lisa_err_t lisa_thread_delete(lisa_thread_t *thread)
{
	/**
	 * 此处由Lisa Thread内部去释放线程参数
	 * 外部只需在线程体返回即可
	 * ! 此处会在 __lisa_thread_entry 函数体最后释放 !
	 */
	return LISA_OK;
}
/**
 * @brief 线程休眠 单位：秒
 *
 * @param seconds
 * @return lisa_err_t
 */
lisa_err_t lisa_thread_delay(uint32_t seconds)
{
	lisa_thread_mdelay(seconds * 1000);
	return 0;
}
/**
 * @brief 线程休眠 单位：毫秒
 *
 * @param ms
 * @return lisa_err_t
 */
lisa_err_t lisa_thread_mdelay(uint32_t ms)
{
	vTaskDelay((TickType_t)pdMS_TO_TICKS(ms));
	return 0;
}

lisa_err_t lisa_thread_yield(void)
{
	taskYIELD();
	return LISA_OK;
}

char * lisa_thread_cur_thread_name(void)
{
	TaskHandle_t task = xTaskGetCurrentTaskHandle();
	return pcTaskGetName(task);
}