#include "lisa_log.h"
#include "elog.h"
#include <stddef.h>

struct lisa_log_frontend_easylog_data {
    void (*output_hook)(const uint8_t *log, uint32_t len);
};

static struct lisa_log_frontend_easylog_data lisa_log_frontend_easylog_data = {
    .output_hook = NULL,
};

uint32_t elog_time_ms_get(void)
{
    extern uint32_t SysTimeMsGet(void);
    return SysTimeMsGet();
}

void elog_port_output_log(const char *log, size_t size)
{
    if (lisa_log_frontend_easylog_data.output_hook) {
        lisa_log_frontend_easylog_data.output_hook((const uint8_t *)log, size);
    }
}

static int lisa_log_frontend_easylog_init(const struct lisa_log_frontend *frontend)
{
    int ret = elog_init();
    if (ret < 0) {
        printf("[ERR]: elog_init error: %d\n", ret);
        return ret;
    }

    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_P_INFO | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL);
    elog_start();

    syslog_hook_set(elog_raw_output_v);

    return 0;
}

static void elog_port_output_hook_set(void (*output_hook)(const uint8_t *log, uint32_t len))
{
    lisa_log_frontend_easylog_data.output_hook = output_hook;
}

static void lisa_log_frontend_flush(const struct lisa_log_frontend *frontend)
{
#if defined(CONFIG_EASYLOGGER_LOG_MODE_ASYNC)
    extern size_t elog_port_read_log_then_output(void);
    while (1) {
        if (elog_port_read_log_then_output() == 0) {
            break;
        }
    }
#endif
}

static void lisa_log_frontend_easylog_level_set(const struct lisa_log_frontend *frontend, int level)
{
    elog_set_filter_lvl(level);
}

const struct lisa_log_frontend lisa_log_frontend_easylog = {
    .name = "easylog",
    .init = lisa_log_frontend_easylog_init,
    .flush = lisa_log_frontend_flush,
    .output_hook_set = elog_port_output_hook_set,
    .level_set = lisa_log_frontend_easylog_level_set,
    .data = &lisa_log_frontend_easylog_data,
};
