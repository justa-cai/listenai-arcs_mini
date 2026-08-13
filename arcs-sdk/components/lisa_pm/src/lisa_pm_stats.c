/**
 * @file lisa_pm_stats.c
 * @brief LISA PM 睡眠统计实现
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_pm_internal.h"

static lisa_pm_stats_t s_stats;
static uint64_t s_last_active_start;

static uint64_t lisa_pm_get_time_us(void)
{
    return (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS * 1000U;
}

static int32_t lisa_pm_stats_hook_enter(uint32_t sleep_time_us, void *arg)
{
    uint64_t now;

    (void)sleep_time_us;
    (void)arg;

    now = lisa_pm_get_time_us();

    taskENTER_CRITICAL();
    if (s_last_active_start > 0U) {
        s_stats.total_active_us += now - s_last_active_start;
    }
    taskEXIT_CRITICAL();

    return 0;
}

static int32_t lisa_pm_stats_hook_exit(uint32_t sleep_time_us, void *arg)
{
    lisa_pm_wakeup_cause_t cause;
    uint64_t now;

    cause = lisa_pm_porting_map_wakeup_cause((uint32_t)(uintptr_t)arg);
    now = lisa_pm_get_time_us();

    taskENTER_CRITICAL();
    s_stats.sleep_count++;
    s_stats.last_sleep_us = sleep_time_us;
    s_stats.total_sleep_us += sleep_time_us;
    if (sleep_time_us > s_stats.max_sleep_us) {
        s_stats.max_sleep_us = sleep_time_us;
    }

    s_stats.wakeup_cause_count[cause]++;

    s_last_active_start = now;
    taskEXIT_CRITICAL();

    lisa_pm_internal_update_wakeup_cause(cause);

    return 0;
}

int32_t lisa_pm_stats_init(void)
{
    taskENTER_CRITICAL();
    memset(&s_stats, 0, sizeof(s_stats));
    s_last_active_start = lisa_pm_get_time_us();
    taskEXIT_CRITICAL();

    return pm_hook_register(PM_HOOK_ID_0, lisa_pm_stats_hook_enter, lisa_pm_stats_hook_exit);
}

int32_t lisa_pm_get_stats(lisa_pm_stats_t *stats)
{
    if (stats == NULL) {
        return -1;
    }

    taskENTER_CRITICAL();
    *stats = s_stats;
    taskEXIT_CRITICAL();

    return 0;
}

int32_t lisa_pm_reset_stats(void)
{
    taskENTER_CRITICAL();
    memset(&s_stats, 0, sizeof(s_stats));
    s_last_active_start = lisa_pm_get_time_us();
    taskEXIT_CRITICAL();

    return 0;
}

uint32_t lisa_pm_get_sleep_ratio(void)
{
    uint64_t total_time = s_stats.total_sleep_us + s_stats.total_active_us;

    if (total_time == 0U) {
        return 0;
    }

    return (uint32_t)((s_stats.total_sleep_us * 10000U) / total_time);
}
