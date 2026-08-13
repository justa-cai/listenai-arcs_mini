#include "lisa_log.h"
#include "SEGGER_RTT.h"
#include "sys_init.h"

#include <string.h>
#include <stddef.h>

#ifndef CONFIG_LOG_BACKEND_SEGGER_RTT_NAME
#define CONFIG_LOG_BACKEND_SEGGER_RTT_NAME "rtt.log"
#endif

void lisa_log_backend_rtt_output(const uint8_t *log, uint32_t len, void *data)
{
    /* Use Write (length-counted) — NOT printf: `log` is a raw, non-NUL-
     * terminated easylogger buffer of `len` bytes that may contain '%'.
     * SEGGER_RTT_printf(0, log, len) would treat it as a format string and emit
     * garbage past the first message. */
    SEGGER_RTT_Write(0, log, len);
}

void lisa_log_backend_rtt_panic_output(const uint8_t *log, uint32_t len, void *data)
{
    SEGGER_RTT_WriteSkipNoLock(0,log, len);
}

int lisa_log_backend_rtt_init(void)
{
    SEGGER_RTT_Init();
    lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_SEGGER_RTT_NAME, lisa_log_backend_rtt_output,
            lisa_log_backend_rtt_panic_output, NULL);

    return 0;
}
SYS_INIT(lisa_log_backend_rtt_init, SYS_INIT_LEVEL_PRE_KERNEL, 0);