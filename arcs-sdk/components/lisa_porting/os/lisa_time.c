#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <lisa_time.h>
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 获取系统ticks数
 * 
 * @return uint64_t 
 */
uint64_t lisa_os_get_tick_ms(void)
{
	return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint64_t lisa_os_get_ticks(void)
{
	return (uint64_t)(xTaskGetTickCount());
}

/**
 * @brief 获取系统时间
 * 
 * @return uint32_t 
 */
uint32_t lisa_os_get_time(void)
{
	return (uint32_t)(lisa_os_get_tick_ms() / 1000U);
}

/**
 * @brief 随机数生成器
 * 
 * @return uint32_t 
 */
uint32_t lisa_rand32(void)
{
	return (uint32_t)(((*((volatile uint32_t *)0xE000E018)) & 0xffffff) |
	                  (xTaskGetTickCount() << 24));
}

uint64_t lisa_os_get_time_ssl(void *arg)
{
	return (uint64_t)(lisa_os_get_tick_ms() / 1000U);
}
