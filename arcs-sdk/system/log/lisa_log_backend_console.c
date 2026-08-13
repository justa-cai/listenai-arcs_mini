#include "lisa_log.h"
#include "console.h"

#include <stddef.h>

#ifndef CONFIG_LOG_BACKEND_SYS_NAME
#define CONFIG_LOG_BACKEND_SYS_NAME "sys.log"
#endif

static void log_console_output(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

    console_write((const char *)log, len);
}

int lisa_log_backend_sys_init(void)
{
    lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_SYS_NAME, log_console_output, log_console_output, NULL);

    return 0;
}
