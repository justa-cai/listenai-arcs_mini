/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lisa_time.h"

#include <stdlib.h>

uint64_t lisa_os_get_ticks(void)
{
    return 0;
}

uint32_t lisa_os_get_time(void)
{
    return 0;
}

uint32_t lisa_rand32(void)
{
    return (uint32_t)rand();
}

uint64_t lisa_os_get_time_ssl(void *arg)
{
    (void)arg;
    return 0;
}

uint64_t lisa_os_get_tick_ms(void)
{
    return 0;
}
