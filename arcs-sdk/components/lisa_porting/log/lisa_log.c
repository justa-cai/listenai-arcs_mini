#include <stdarg.h>
#include <stdio.h>
#include "lisa_log.h"
#include "syslog.h"

#if CONFIG_LOG_FRONTEND_EASYLOGGER
#include "elog.h"
#endif

extern int arcs_uart_put(const char *data, int len);

static void (*lisa_log_output_handle)(const char *, int) = NULL;

void lisa_log_output_handle_set(void (*handle)(const char *, int))
{
    lisa_log_output_handle = handle;
}

#if CONFIG_LOG_FRONTEND_EASYLOGGER
uint32_t elog_time_ms_get(void)
{
    extern uint32_t SysTimeMsGet(void);
    return SysTimeMsGet();
}
#endif

int lisa_log_init(void)
{
    static uint8_t init_done = 0;
    if (init_done) {
        return 0;
    }
    init_done = 1;

#if CONFIG_LOG_FRONTEND_EASYLOGGER
    int ret = elog_init();
    if (ret < 0) {
        printf("[ERR]: elog_init error: %d\n", ret);
        return ret;
    }
    /* set EasyLogger log format */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL);
    /* start EasyLogger */
    elog_start();
    syslog_hook_set(elog_raw_output_v);
#endif

    return 0;
}

#if CONFIG_LOG_FRONTEND_EASYLOGGER
void elog_port_output_log(const char *log, size_t size)
{
    if (lisa_log_output_handle) {
        lisa_log_output_handle(log, size);
    } else {
        syslog_write(log, size);
    }
}
#endif

void log_flush(void)
{
#if defined(CONFIG_LOG_FRONTEND_EASYLOGGER)
    #if defined(CONFIG_EASYLOGGER_LOG_MODE_ASYNC)
        extern size_t elog_port_read_log_then_output(void);
        while (1) {
            if (elog_port_read_log_then_output() == 0) {
                break;
            }
        }
    #endif
#endif
}

void logDump(uint8_t *data, int len)
{
#if CONFIG_LOG_FRONTEND_EASYLOGGER
    elog_hexdump("logDump", 16, data, len);
#endif
}

void logHexDump(char *name, uint8_t width, uint8_t *data, int len)
{
#if CONFIG_LOG_FRONTEND_EASYLOGGER
    elog_hexdump(name, width, data, len);
#endif
}

void log_write(void *unused, char c)
{
}
#if CONFIG_LOG_FRONTEND_EASYLOGGER

static inline uint8_t lisa_log_lvl_to_elog_lvl(uint8_t lvl)
{
    return lvl;
}
#endif

void lisa_log_set_level(uint8_t lvl)
{
#if CONFIG_LOG_FRONTEND_EASYLOGGER
    elog_set_filter_lvl(lisa_log_lvl_to_elog_lvl(lvl));
#endif
}
