/*
 * log_print.h
 *
 *  Created on: 2022
 *      Author: USER
 */

#ifndef INCLUDE_DEBUG_LOG_PRINT_H_
#define INCLUDE_DEBUG_LOG_PRINT_H_

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
 extern "C" {
#endif

extern uint32_t cloglvl;
extern uint32_t taskmask;

extern int logInit(int dbg, uint32_t baudrate);
extern void log_flush(void);
extern void log_write(void *unused, char c);
extern void logDbg(const char* format, ...);
extern int log_print_hook_set(void (*hook)(const char *, va_list));
extern void log_print_level_set(uint32_t level);
extern uint32_t log_print_level_get(void);

extern void logDump(uint8_t *data, int len);

extern void UART_log32(uint8_t log_id, uint32_t log_data);
extern void UART_logN(uint8_t log_id, uint8_t *log_data_ptr, uint8_t log_data_length);

#ifndef CONTROL_LOG_OVER_TLOG
#define CONTROL_LOG_OVER_TLOG  0
#endif

#define CLOG(fmt, ...)   logDbg(fmt"\n", ##__VA_ARGS__)

#define CLOG_LEVEL_NONE     0
#define CLOG_LEVEL_ERROR    1
#define CLOG_LEVEL_WARN     2
#define CLOG_LEVEL_INFO     3
#define CLOG_LEVEL_DEBUG    4
#define CLOG_LEVEL_VERBOSE  5

//#define TASKMSK_NONE				0xFFFFFFFF
//
//#ifndef CLOG_LEVEL
//#define CLOG_LEVEL   CLOG_LEVEL_DEBUG
//#endif
//
//#ifndef TASKMSK
//#define TASKMSK   TASKMSK_NONE
//#endif

#define CLOGE(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR)  { CLOG("ERR:"fmt,##__VA_ARGS__);}} while(0)
#define CLOGW(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN)   { CLOG("WRN:"fmt,##__VA_ARGS__);}} while(0)
#define CLOGI(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO)   { CLOG("INF:"fmt,##__VA_ARGS__);}} while(0)
#define CLOGD(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG)  { CLOG("DBG:"fmt,##__VA_ARGS__);}} while(0)
#define CLOGV(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_VERBOSE){ CLOG("VBS:"fmt,##__VA_ARGS__);}} while(0)
#define CDUMP(data, len)    do {if (cloglvl >= CLOG_LEVEL_DEBUG) { logDump(data, len);}} while(0)

#define TASK_CLOGE(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR){if((1 << taskid) & taskmask)   CLOG("ERR:"fmt,##__VA_ARGS__);}} while(0)
#define TASK_CLOGW(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN){if((1 << taskid) & taskmask)    CLOG("WRN:"fmt,##__VA_ARGS__);}} while(0)
#define TASK_CLOGI(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO){if((1 << taskid) & taskmask)    CLOG("INF:"fmt,##__VA_ARGS__);}} while(0)
#define TASK_CLOGD(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG){if((1 << taskid) & taskmask)   CLOG("DBG:"fmt,##__VA_ARGS__);}} while(0)
#define TASK_CLOGV(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_VERBOSE){if((1 << taskid) & taskmask) CLOG("VBS:"fmt,##__VA_ARGS__);}} while(0)
#define TASK_CDUMP(taskid, data, len)    do {if (cloglvl >= CLOG_LEVEL_DEBUG){if((1 << taskid) & taskmask)   logDump(data, len);}} while(0)

#define CLOG_FLUSH()        log_flush()

////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(__i386__) && !defined(__x86_64__)
#define TLOG(fmt, ...)    PW_TOKENIZE_TO_GLOBAL_HANDLER(fmt, ##__VA_ARGS__)

#define TLOG_LEVEL_NONE     0
#define TLOG_LEVEL_ERROR    1
#define TLOG_LEVEL_WARN     2
#define TLOG_LEVEL_INFO     3
#define TLOG_LEVEL_DEBUG    4
#define TLOG_LEVEL_VERBOSE  5

#ifndef TLOG_LEVEL
#define TLOG_LEVEL   TLOG_LEVEL_DEBUG
#endif

#define TLOGE(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_ERROR)   TLOG("ERR:"fmt, ##__VA_ARGS__);} while(0)
#define TLOGW(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_WARN)    TLOG("WRN:"fmt, ##__VA_ARGS__);} while(0)
#define TLOGI(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_INFO)    TLOG("INF:"fmt, ##__VA_ARGS__);} while(0)
#define TLOGD(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_DEBUG)   TLOG("DBG:"fmt, ##__VA_ARGS__);} while(0)
#define TLOGV(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_VERBOSE) TLOG("VBS:"fmt, ##__VA_ARGS__);} while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DEBUG_LOG_PRINT_H_ */
