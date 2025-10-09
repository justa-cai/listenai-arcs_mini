/**
 * @version     0.1
 * @date        2022-12-19
 * @author      mokee
 * 
 * Copyright (C) 2022 ANHUI LISTENAI Co., LTD All Rights Reserved
 */

#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include "listen_system.h"

int __wrap_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (!tv) return -1;

    return ls_sys_get_time(tv);
}

long int __wrap_time(long int *_timer)
{
    struct timeval tv;
    ls_sys_get_time(&tv);

    return (tv.tv_sec * 1000);
}

struct tm *__wrap_gmtime_r(const long int *tv_sec, struct tm *__tm)
{
    return ls_sys_get_tmtime(tv_sec, __tm);
}