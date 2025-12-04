#include "syslog.h"
#include "lisa_log.h"

#include <string.h>
#include <stddef.h>

void lisa_log_backend_sys_log_output(const uint8_t *log, uint32_t len, void *data)
{
    syslog_write(log, len);
}

int lisa_log_backend_sys_init(void)
{
    lisa_log_backend_add("sys.log", lisa_log_backend_sys_log_output, NULL);

    return 0;
}
