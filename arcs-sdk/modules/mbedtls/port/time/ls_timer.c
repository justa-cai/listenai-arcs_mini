#include "mbedtls/platform_time.h"
#include "ls_timer.h"
#include "FreeRTOS.h"
#include "task.h"

#define UNIX_TIMESTAMP_OFFSET 1735660800  // 系统启动时的 UNIX 时间戳（2025年1月1日）

mbedtls_time_t ls_time(mbedtls_time_t *timer)
{
    TickType_t ticks = xTaskGetTickCount();
    mbedtls_time_t current_time = UNIX_TIMESTAMP_OFFSET + ticks / configTICK_RATE_HZ;
    if (timer != NULL) {
        *timer = current_time;
    }

    return current_time;
}
