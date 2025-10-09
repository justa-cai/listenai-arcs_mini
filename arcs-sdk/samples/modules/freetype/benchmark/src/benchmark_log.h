#ifndef BENCHMARK_LOG_H
#define BENCHMARK_LOG_H

#include "log_print.h"
#include <stdlib.h>

// 日志级别定义
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} benchmark_log_level_t;

// 默认使用CLOG系列函数，可以通过重定义这些宏来更换日志实现
#ifndef BENCHMARK_LOG_DEBUG
#define BENCHMARK_LOG_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef BENCHMARK_LOG_INFO
#define BENCHMARK_LOG_INFO(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef BENCHMARK_LOG_WARN
#define BENCHMARK_LOG_WARN(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef BENCHMARK_LOG_ERROR
#define BENCHMARK_LOG_ERROR(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

// 表格边框字符
#define TABLE_TOP_LEFT      "+"
#define TABLE_TOP_RIGHT     "+"
#define TABLE_BOTTOM_LEFT   "+"
#define TABLE_BOTTOM_RIGHT  "+"
#define TABLE_HORIZONTAL    "-"
#define TABLE_VERTICAL      "|"
#define TABLE_T_DOWN        "+"
#define TABLE_T_UP          "+"
#define TABLE_T_RIGHT       "+"
#define TABLE_T_LEFT        "+"
#define TABLE_CROSS         "+"

// 日志格式化宏
#define TABLE_WIDTH         80
#define TEST_NAME_WIDTH     40
#define DURATION_WIDTH      15
#define MEMORY_WIDTH        15

// 表格格式字符串
#define TABLE_HEADER_FORMAT "%s%-*s%s|%s%-*s%s|%s%-*s%s\n"
#define TABLE_ROW_FORMAT    "%s%-*s%s|%s%*u%s|%s%*zu%s\n"

#define BENCHMARK_LOG_CATEGORY_START "\n=== %s ===\n"
#define BENCHMARK_LOG_CATEGORY_END   "\n=== End of %s ===\n"

// 辅助宏，用于生成分隔线
#define MAKE_HORIZONTAL_LINE(ch, connector, end_ch) \
    ch \
    "----------------------------------------" \
    connector \
    "---------------" \
    connector \
    "---------------" \
    end_ch "\n"

#endif // BENCHMARK_LOG_H
