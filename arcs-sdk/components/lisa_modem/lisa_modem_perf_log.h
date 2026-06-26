#ifndef LISA_MODEM_PERF_LOG_H
#define LISA_MODEM_PERF_LOG_H

#include "lisa_log.h"

#ifndef LISA_MODEM_PERF_LOG_ENABLE
#define LISA_MODEM_PERF_LOG_ENABLE 0
#endif

#ifndef LISA_MODEM_AT_CLIENT_DEBUG_ENABLE
#define LISA_MODEM_AT_CLIENT_DEBUG_ENABLE 0
#endif

#if LISA_MODEM_PERF_LOG_ENABLE
#define LISA_MODEM_PERF_LOGI(tag, fmt, ...) \
    LISA_LOGI(tag, "[PERF] " fmt, ##__VA_ARGS__)
#define LISA_MODEM_PERF_LOGW(tag, fmt, ...) \
    LISA_LOGW(tag, "[PERF] " fmt, ##__VA_ARGS__)
#else
#define LISA_MODEM_PERF_LOGI(tag, fmt, ...) ((void)0)
#define LISA_MODEM_PERF_LOGW(tag, fmt, ...) ((void)0)
#endif

#endif /* LISA_MODEM_PERF_LOG_H */
