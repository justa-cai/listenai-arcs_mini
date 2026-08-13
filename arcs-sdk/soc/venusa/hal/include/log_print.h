/*
 * log_print.h
 *
 *  Created on: 2022
 *      Author: USER
 *
 *  @brief Debug logging system header file providing multi-level logging capabilities
 *         with task filtering support. Includes UART output functions and data dumping utilities.
 */

#ifndef INCLUDE_DEBUG_LOG_PRINT_H_
#define INCLUDE_DEBUG_LOG_PRINT_H_

#include <stdint.h>
#include <stdarg.h>

/** Global log severity level control */
extern uint32_t cloglvl;
/** Task mask for selective logging enablement (bitmask per task ID) */
extern uint32_t taskmask;

/**
 * @brief Initialize logging system
 *
 * @param[in] dbg       Debug mode flag
 * @param[in] baudrate  Baud rate configuration for underlying transport
 * @return Returns 0 on success, non-zero on failure
 */
extern int logInit(int dbg, uint32_t baudrate);

/**
 * @brief Flush remaining log messages in buffer
 *
 * Forces immediate transmission of buffered log data through the configured transport
 */
extern void log_flush(void);

/**
 * @brief Low-level character writer callback
 *
 * @param[in] unused   Ignored parameter (placeholder for standard stream interface)
 * @param[in] c        Character to write to log output
 * @note Must be implemented by specific transport layer (e.g. UART driver)
 */
extern void log_write(void *unused, char c);

/**
 * @brief Formatted debug message output
 *
 * @param[in] format   Format string (printf style)
 * @param[in] ...      Variable arguments matching format specifiers
 * @note Automatically appends newline character
 */
extern void logDbg(const char* format, ...);

/**
 * @brief Hexadecimal data dump utility
 *
 * @param[in] data    Pointer to data buffer
 * @param[in] len     Number of bytes to dump
 * @note Displays both ASCII and hex representations
 */
extern void logDump(uint8_t *data, int len);

/**
 * @brief Log 32-bit value with associated ID via UART
 *
 * @param[in] log_id     Identifier code for message classification
 * @param[in] log_data   32-bit value to log
 * @note Useful for structured logging of numerical values
 */
extern void UART_log32(uint8_t log_id, uint32_t log_data);

/**
 * @brief Log byte array with length via UART
 *
 * @param[in] log_id          Message identifier
 * @param[in] log_data_ptr    Pointer to data buffer
 * @param[in] log_data_length Number of bytes to log
 * @note Allows efficient binary data transmission
 */
extern void UART_logN(uint8_t log_id, uint8_t *log_data_ptr, uint8_t log_data_length);

#ifndef CONTROL_LOG_OVER_TLOG
#define CONTROL_LOG_OVER_TLOG  0  /*!< Alternative log control mechanism flag */
#endif

/**
 * @brief Basic debug logging macro (with automatic newline)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Shortcut for logDbg() with added newline
 */
#define CLOG(fmt, ...)   logDbg(fmt"\n", ##__VA_ARGS__)

/** Log severity level definitions */
#define CLOG_LEVEL_NONE     0  /*!< Suppress all log messages */
#define CLOG_LEVEL_ERROR    1  /*!< Error messages only */
#define CLOG_LEVEL_WARN     2  /*!< Error + warning messages */
#define CLOG_LEVEL_INFO     3  /*!< Add informational messages */
#define CLOG_LEVEL_DEBUG    4  /*!< Include debug messages */
#define CLOG_LEVEL_VERBOSE  5  /*!< Most detailed logging level */

/**
 * @brief Error message logging (with error tag)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= CLOG_LEVEL_ERROR
 */
#define CLOGE(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR)  { CLOG("ERR:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Warning message logging (with warning tag)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= CLOG_LEVEL_WARN
 */
#define CLOGW(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN)   { CLOG("WRN:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Informational message logging (with info tag)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= CLOG_LEVEL_INFO
 */
#define CLOGI(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO)   { CLOG("INF:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Debug message logging (with debug tag)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= CLOG_LEVEL_DEBUG
 */
#define CLOGD(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG)  { CLOG("DBG:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Verbose message logging (with verbose tag)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= CLOG_LEVEL_VERBOSE
 */
#define CLOGV(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_VERBOSE){ CLOG("VBS:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Data dump trigger (when debug level enabled)
 *
 * @param[in] data    Pointer to data buffer
 * @param[in] len     Number of bytes to dump
 * @note Only triggers when cloglvl >= CLOG_LEVEL_DEBUG
 */
#define CDUMP(data, len)    do {if (cloglvl >= CLOG_LEVEL_DEBUG) { logDump(data, len);}} while(0)

/**
 * @brief Task-specific error logging (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] fmt     Format string
 * @param[in] ...     Variable arguments
 * @note Only logs when both cloglvl >= CLOG_LEVEL_ERROR AND task mask matches
 */
#define TASK_CLOGE(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR){if((1 << taskid) & taskmask)   CLOG("ERR:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Task-specific warning logging (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] fmt     Format string
 * @param[in] ...     Variable arguments
 * @note Only logs when both cloglvl >= CLOG_LEVEL_WARN AND task mask matches
 */
#define TASK_CLOGW(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN){if((1 << taskid) & taskmask)    CLOG("WRN:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Task-specific informational logging (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] fmt     Format string
 * @param[in] ...     Variable arguments
 * @note Only logs when both cloglvl >= CLOG_LEVEL_INFO AND task mask matches
 */
#define TASK_CLOGI(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO){if((1 << taskid) & taskmask)    CLOG("INF:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Task-specific debug logging (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] fmt     Format string
 * @param[in] ...     Variable arguments
 * @note Only logs when both cloglvl >= CLOG_LEVEL_DEBUG AND task mask matches
 */
#define TASK_CLOGD(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG){if((1 << taskid) & taskmask)   CLOG("DBG:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Task-specific verbose logging (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] fmt     Format string
 * @param[in] ...     Variable arguments
 * @note Only logs when both cloglvl >= CLOG_LEVEL_VERBOSE AND task mask matches
 */
#define TASK_CLOGV(taskid, fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_VERBOSE){if((1 << taskid) & taskmask) CLOG("VBS:"fmt,##__VA_ARGS__);}} while(0)

/**
 * @brief Task-specific data dump (checks task mask)
 *
 * @param[in] taskid  Task ID bit position
 * @param[in] data    Pointer to data buffer
 * @param[in] len     Number of bytes to dump
 * @note Only triggers when both cloglvl >= CLOG_LEVEL_DEBUG AND task mask matches
 */
#define TASK_CDUMP(taskid, data, len)    do {if (cloglvl >= CLOG_LEVEL_DEBUG){if((1 << taskid) & taskmask)   logDump(data, len);}} while(0)

/**
 * @brief Manual log buffer flush trigger
 *
 * Forces immediate transmission of buffered log content
 */
#define CLOG_FLUSH()        log_flush()

/////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(__i386__) && !defined(__x86_64__)
/**
 * @brief Alternative logging macro (platform-specific implementation)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Used on non-x86 platforms instead of standard CLOG macro
 */
#define TLOG(fmt, ...)    PW_TOKENIZE_TO_GLOBAL_HANDLER(fmt, ##__VA_ARGS__)

/** TLOG severity level definitions */
#define TLOG_LEVEL_NONE     0  /*!< Suppress all TLOG messages */
#define TLOG_LEVEL_ERROR    1  /*!< Error messages only */
#define TLOG_LEVEL_WARN     2  /*!< Error + warning messages */
#define TLOG_LEVEL_INFO     3  /*!< Add informational messages */
#define TLOG_LEVEL_DEBUG    4  /*!< Include debug messages */
#define TLOG_LEVEL_VERBOSE  5  /*!< Most detailed TLOG level */

#ifndef TLOG_LEVEL
#define TLOG_LEVEL   TLOG_LEVEL_DEBUG  /*!< Default TLOG severity level */
#endif

/**
 * @brief Alternative error logging (uses TLOG instead of CLOG)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= TLOG_LEVEL_ERROR
 */
#define TLOGE(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_ERROR)   TLOG("ERR:"fmt, ##__VA_ARGS__);} while(0)

/**
 * @brief Alternative warning logging (uses TLOG instead of CLOG)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= TLOG_LEVEL_WARN
 */
#define TLOGW(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_WARN)    TLOG("WRN:"fmt, ##__VA_ARGS__);} while(0)

/**
 * @brief Alternative informational logging (uses TLOG instead of CLOG)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= TLOG_LEVEL_INFO
 */
#define TLOGI(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_INFO)    TLOG("INF:"fmt, ##__VA_ARGS__);} while(0)

/**
 * @brief Alternative debug logging (uses TLOG instead of CLOG)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= TLOG_LEVEL_DEBUG
 */
#define TLOGD(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_DEBUG)   TLOG("DBG:"fmt, ##__VA_ARGS__);} while(0)

/**
 * @brief Alternative verbose logging (uses TLOG instead of CLOG)
 *
 * @param[in] fmt    Format string
 * @param[in] ...    Variable arguments
 * @note Only logs when cloglvl >= TLOG_LEVEL_VERBOSE
 */
#define TLOGV(fmt, ...)     do {if (cloglvl >= TLOG_LEVEL_VERBOSE) TLOG("VBS:"fmt, ##__VA_ARGS__);} while(0)
#endif

#endif /* INCLUDE_DEBUG_LOG_PRINT_H_ */
