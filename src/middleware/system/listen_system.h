/**
 * System of ARCS.
 */

#ifndef __LISTENAI_SYSTEM_H__
#define __LISTENAI_SYSTEM_H_

#include <stdint.h>
#include "sys/time.h"

#define TIMEZONE_BERLIN     (1)
#define TIMEZONE_SHANGHAI   (8)
#define TIMEZONE_CHICAGO    (-6)
#define AREA_TIMEZONE       TIMEZONE_SHANGHAI

typedef void (*sntp_synced_callback)(void);

/**
 * @brief   初始化系统模块
 * @param   tz  时区
 */
void ls_sys_init(int tz);

/**
 * @brief   启动LWIP SNTP
 */
void ls_sys_sntp_start(sntp_synced_callback cb);

/**
 * @brief   设置系统时间
 * @param   sec     秒
 * @param   usec    微秒
 */
void ls_sys_set_time(uint32_t sec, uint32_t usec);

/**
 * @brief   设置系统时间
 * @param   val     timeval
 */
void ls_sys_set_timeval(struct timeval *val);

/**
 * @brief   获取系统时间
 * @param   tv  时间
 * @return  0:成功, -1:失败
 */
int ls_sys_get_time(struct timeval *tv);

/**
 * @brief   获取系统时间
 * @param   tv_sec  秒
 * @param   __tm    time
 */
struct tm *ls_sys_get_tmtime(const long int *tv_sec, struct tm *__tm);

#endif