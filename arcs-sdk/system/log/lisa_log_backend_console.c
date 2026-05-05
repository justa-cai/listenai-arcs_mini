#include "lisa_log.h"
#include "console.h"

#include <stddef.h>

#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
extern int32_t ipc_slave_print(char *string, int32_t len);
#endif

#ifndef CONFIG_LOG_BACKEND_SYS_NAME
#define CONFIG_LOG_BACKEND_SYS_NAME "sys.log"
#endif

static void log_console_output(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
    if (ipc_slave_print((char *)log, (int32_t)len) == 0)
        return;
#endif

    console_write((const char *)log, len);
}

int lisa_log_backend_sys_init(void)
{
    lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_SYS_NAME, log_console_output, log_console_output, NULL);

    return 0;
}
