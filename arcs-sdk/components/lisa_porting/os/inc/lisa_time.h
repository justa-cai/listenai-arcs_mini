#ifndef __LISA_OS_TIME__
#define __LISA_OS_TIME__

#include <stddef.h>
#include <stdint.h>
#define LISA_WAIT_FOREVER 0xffffffffU /* Wait forever timeout value */
#define LISA_NO_WAIT      0           /* wait 0 second timeout value */

/**
 * @brief get ticks
 * @return uint32_t 秒
 */
uint64_t lisa_os_get_ticks(void);

/**
 * @brief gettime
 * @return uint32_t 秒
 */
uint32_t lisa_os_get_time(void);

/**
 * @brief rand
 * @return uint32_t
 */
uint32_t lisa_rand32(void);

uint64_t lisa_os_get_time_ssl(void *arg);

uint64_t lisa_os_get_tick_ms();

#endif //__LISA_OS_TIME__
