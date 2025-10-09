#include <FreeRTOS.h>
#include "lisa_timer.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "lisa_log.h"
#include <timers.h>

#ifdef TAG
#undef TAG
#endif
#define TAG "LISA-TIMER"

static void _freertos_timer_callback(TimerHandle_t xTimer)
{
	lisa_timer_t *timer = (lisa_timer_t *)pvTimerGetTimerID(xTimer);
	if (timer) {
		if (timer->cb) timer->cb(timer->usr_arg);
	}
}

lisa_timer_t *lisa_timer_create(uint32_t period_ms, lisa_timercb_t cb, void *arg)
{
	lisa_timer_t *timer = (lisa_timer_t *)lisa_mem_alloc(sizeof(lisa_timer_t));
	if (!timer) return NULL;

	timer->handle = xTimerCreate("LISA-TIMER",
								 period_ms / portTICK_PERIOD_MS,
								 (UBaseType_t)0,
								 timer,
								 _freertos_timer_callback);
	if(timer->handle == NULL){
		lisa_mem_free(timer);
		return NULL;
	}

	timer->cb = cb;
	timer->usr_arg = arg;
	timer->period_ms = period_ms;

	return timer;
}

lisa_err_t lisa_timer_delete(lisa_timer_t *timer)
{
	if (timer) {
		if (lisa_timer_isactive(timer)) {
			lisa_timer_stop(timer);
		}
		if (xTimerDelete(timer->handle, 0) != pdPASS) {
			LISA_LOGE(TAG, "delete lisa timer fail");
		}
		lisa_mem_free(timer);
	}
	return LISA_OK;
}

lisa_err_t lisa_timer_start(lisa_timer_t *timer)
{
	if (timer) {
		if (pdPASS == xTimerStart(timer->handle, 0)) {
			return LISA_OK;
		} else {
			LISA_LOGE(TAG, "lisa timer start fail");
		}
	}
	return LISA_FAIL;
}

lisa_err_t lisa_timer_stop(lisa_timer_t *timer)
{
	if (timer) {
		if (pdPASS == xTimerStop(timer->handle, 0)) {
			return LISA_OK;
		} else {
			LISA_LOGE(TAG, "lisa timer stop fail");
		}
	}
	return LISA_FAIL;
}

lisa_err_t lisa_timer_change_period(lisa_timer_t *timer, uint32_t period_ms)
{
	if (timer) {
		if (pdPASS == xTimerChangePeriod(timer->handle, (period_ms / portTICK_PERIOD_MS), 0)) {
			return LISA_OK;
		} else {
			LISA_LOGE(TAG, "lisa timer change fail");
		}
	}
	return LISA_FAIL;
}

uint32_t lisa_timer_remain_time(lisa_timer_t *timer)
{
	if (timer) {
		if (pdPASS == xTimerIsTimerActive(timer->handle)) {
			TickType_t expiry_ticks = xTimerGetExpiryTime(timer->handle);
			TickType_t curr_ticks = xTaskGetTickCount();
			if(expiry_ticks > curr_ticks) {
				return (expiry_ticks - curr_ticks) * portTICK_PERIOD_MS;
			}
		}
	}
	return 0;
}

bool lisa_timer_isactive(lisa_timer_t *timer)
{
	if (timer) {
		if (pdPASS == xTimerIsTimerActive(timer->handle)) {
			return true;
		}
	}
	return false;
}

lisa_err_t lisa_timer_reset(lisa_timer_t *timer)
{
	if (timer) {
		if (pdPASS == xTimerReset(timer->handle, portMAX_DELAY)) {
			return LISA_OK;
		}
	}
	return LISA_FAIL;
}