
#ifndef _ELOG_CFG_H_
#define _ELOG_CFG_H_

/* enable log output. default open this macro */
#define ELOG_OUTPUT_ENABLE

/* setting static output log level */
#if CONFIG_EASYLOGGER_LOG_LEVEL_ASSERT
#define ELOG_OUTPUT_LVL ELOG_LVL_ASSERT
#elif CONFIG_EASYLOGGER_LOG_LEVEL_ERROR
#define ELOG_OUTPUT_LVL ELOG_LVL_ERROR
#elif CONFIG_EASYLOGGER_LOG_LEVEL_WARN
#define ELOG_OUTPUT_LVL ELOG_LVL_WARN
#elif CONFIG_EASYLOGGER_LOG_LEVEL_INFO
#define ELOG_OUTPUT_LVL ELOG_LVL_INFO
#elif CONFIG_EASYLOGGER_LOG_LEVEL_DEBUG
#define ELOG_OUTPUT_LVL ELOG_LVL_DEBUG
#elif CONFIG_EASYLOGGER_LOG_LEVEL_VERBOSE
#define ELOG_OUTPUT_LVL ELOG_LVL_VERBOSE
#else
#define ELOG_OUTPUT_LVL ELOG_LVL_INFO
#endif

/* enable assert check */
#define ELOG_ASSERT_ENABLE

/* buffer size for every line's log */
#if defined(CONFIG_LOG_LINE_BUF_SIZE)
#define ELOG_LINE_BUF_SIZE                   CONFIG_LOG_LINE_BUF_SIZE
#else
#define ELOG_LINE_BUF_SIZE                   CONFIG_EASYLOGGER_LINE_BUF_SIZE
#endif
/* output line number max length */
#define ELOG_LINE_NUM_MAX_LEN                5
/* output filter's tag max length */
#define ELOG_FILTER_TAG_MAX_LEN              30
/* output filter's keyword max length */
#define ELOG_FILTER_KW_MAX_LEN               16
/* output filter's tag level max num */
#define ELOG_FILTER_TAG_LVL_MAX_NUM          5
/* output newline sign */
#define ELOG_NEWLINE_SIGN                   "\r\n"
/*---------------------------------------------------------------------------*/
/* enable log color */
#define ELOG_COLOR_ENABLE
/* change the some level logs to not default color if you want */
#define ELOG_COLOR_ASSERT                        (F_MAGENTA B_NULL S_NORMAL)
#define ELOG_COLOR_ERROR                         (F_RED B_NULL S_NORMAL)
#define ELOG_COLOR_WARN                          (F_YELLOW B_NULL S_NORMAL)
#define ELOG_COLOR_INFO                          (F_CYAN B_NULL S_NORMAL)
#define ELOG_COLOR_DEBUG                         (F_GREEN B_NULL S_NORMAL)
#define ELOG_COLOR_VERBOSE                       (F_BLUE B_NULL S_NORMAL)
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_EASYLOGGER_LOG_MODE_ASYNC)
/* enable asynchronous output mode */
#define ELOG_ASYNC_OUTPUT_ENABLE
/* the highest output level for async mode, other level will sync output */
#define ELOG_ASYNC_OUTPUT_LVL                    ELOG_LVL_ASSERT
/* buffer size for asynchronous output mode */
#if defined(CONFIG_LOG_ASYNC_BUF_SIZE)
#define ELOG_ASYNC_OUTPUT_BUF_SIZE               CONFIG_LOG_ASYNC_BUF_SIZE
#else
#define ELOG_ASYNC_OUTPUT_BUF_SIZE               CONFIG_EASYLOGGER_ASYNC_BUF_SIZE
#endif
/* each asynchronous output's log which must end with newline sign */
#define ELOG_ASYNC_LINE_OUTPUT
#else
/* nothing */
#endif

#endif /* _ELOG_CFG_H_ */
