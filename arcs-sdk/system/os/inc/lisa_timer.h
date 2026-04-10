#ifndef __LISA_OS_TIMER__
#define __LISA_OS_TIMER__

#include "lisa_typedef.h"

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_err.h>

typedef enum {
    OS_TIMER_STOP = 0,
    OS_TIMER_START = 1,
} lisa_timerstatus_e;

typedef enum {
    OS_TIMER_ONCE = 0,
    OS_TIMER_PERIODIC = 1,
} lisa_timertype;

struct lisa_timer;
typedef void (*lisa_timercb_t)(struct lisa_timer *timer);

struct lisa_timer {
    void *handle;
    lisa_timerstatus_e status;
    lisa_timertype type;
    uint32_t period_ms;
    void *arg;
    lisa_timercb_t cb;
};

typedef struct lisa_timer lisa_timer_t;
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
 * @brief   定时器是否有效
 * @param   timer       句柄
 * @return
 */
bool lisa_timer_isactive(lisa_timer_t *timer);

#endif //__LISA_OS_TIMER__
