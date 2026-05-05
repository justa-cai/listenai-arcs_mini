#include <stddef.h>

#include "lisa_log.h"
#include "syslog.h"

#ifndef CONFIG_LOG_BACKEND_SYS_NAME
#define CONFIG_LOG_BACKEND_SYS_NAME "sys.log"
#endif

static void boot_log_output(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;
    syslog_write((const char *)log, (int)len);
}

int lisa_log_backend_sys_init(void)
{
    return lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_SYS_NAME, boot_log_output, boot_log_output, NULL);
}
