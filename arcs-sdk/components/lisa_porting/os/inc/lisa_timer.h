#ifndef __LISA_OS_TIMER__
#define __LISA_OS_TIMER__

#include "FreeRTOS.h"
#include <timers.h>
#include <stdbool.h>
#include <lisa_err.h>

typedef void (*lisa_timercb_t)(void *arg);

typedef struct {
	TimerHandle_t handle;
	lisa_timercb_t cb;
	uint32_t period_ms;
    void *usr_arg;
} lisa_timer_t;

/**
 * @brief   定时器创建
 * @param   cb          回调函数
 * @param   arg         回调参数
 * @param   period_ms   时间
 * @return  句柄 
 */
lisa_timer_t *lisa_timer_create(uint32_t period_ms, lisa_timercb_t cb, void *arg);

/**
 * @brief   定时器销毁
 * @param   timer       句柄
 */
lisa_err_t lisa_timer_delete(lisa_timer_t *timer);

/**
 * @brief   定时器开始
 * @param   timer       句柄
 * @return
 */
lisa_err_t lisa_timer_start(lisa_timer_t *timer);

/**
 * @brief   定时器停止
 * @param   timer       句柄
 * @return
 */
lisa_err_t lisa_timer_stop(lisa_timer_t *timer);

/**
 * @brief   定时器改变时间
 * @param   timer       句柄
 * @return
 */
lisa_err_t lisa_timer_change_period(lisa_timer_t *timer, uint32_t period_ms);

/**
 * @brief   定时器超时剩余时间
 * @param   timer       句柄
 * @return	剩余超时时间, ms
 */
uint32_t lisa_timer_remain_time(lisa_timer_t *timer);

/**
 * @brief   定时器是否有效
 * @param   timer       句柄
 * @return
 */
bool lisa_timer_isactive(lisa_timer_t *timer);

/**
 * @brief   定时器重新启动
 * @param   timer       句柄
 * @return
 */
lisa_err_t lisa_timer_reset(lisa_timer_t *timer);

#endif //__LISA_OS_TIMER__