#include "syslog.h"
#include "lisa_log.h"

#include <string.h>
#include <stddef.h>

#ifndef CONFIG_LOG_BACKEND_SYS_NAME
#define CONFIG_LOG_BACKEND_SYS_NAME "sys.log"
#endif

void lisa_log_backend_sys_log_output(const uint8_t *log, uint32_t len, void *data)
{
    syslog_write(log, len);
}

void lisa_log_backend_sys_log_output_direct(const uint8_t *log, uint32_t len, void *data)
{
    syslog_write(log, len);
}

int lisa_log_backend_sys_init(void)
{
    lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_SYS_NAME, lisa_log_backend_sys_log_output,
                            lisa_log_backend_sys_log_output_direct, NULL);

    return 0;
}
