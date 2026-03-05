/**
 * @file utils_log.h
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2023-04-17
 *
 * @copyright Copyright (c) 2023
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __UTILS_LOG_H__
#define __UTILS_LOG_H__

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
typedef enum _tisilicon_log_level
{
    LOG_NONE,       /*!< No log output */
    LOG_ERROR,      /*!< Critical errors, software module can not recover on its own */
    LOG_WARN,       /*!< Error conditions from which recovery measures have been taken */
    LOG_INFO,       /*!< Information messages which describe normal flow of events */
    LOG_DEBUG,      /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
    LOG_VERBOSE     /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */
} tisilicon_log_level_t ;
/* clang-format on */

/* clang-format off */
#if CONFIG_LOG_COLORS
#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_BROWN   "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_PURPLE  "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"
#define LOG_COLOR_E       LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W       LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I       LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V
#else /* CONFIG_LOG_COLORS */
#define LOG_COLOR_E
#define LOG_COLOR_W
#define LOG_COLOR_I
#define LOG_COLOR_D
#define LOG_COLOR_V
#define LOG_RESET_COLOR
#endif /* CONFIG_LOG_COLORS */
/* clang-format on */

#ifdef LOG_LEVEL
#undef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL LOG_LEVEL
#endif

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL LOG_DEBUG
#endif

#ifdef LOG_KERNEL
#define LOG_PRINTF printk
#else
#define LOG_PRINTF printf
#endif

#if defined(_WIN32) || defined(_WIN64)
/* Windows code */
#elif defined(__linux__)
// /* Linux code */
#include <time.h>
#include <sys/time.h>
#include <string.h>
#define TIMESTAMP_MAX              30
static char timestamp[51];
static char time_string[9];
static struct timeval tv;
static char *get_timestamp(void)
{

    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    memset(timestamp, 0, TIMESTAMP_MAX);

    strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
    snprintf(timestamp, TIMESTAMP_MAX, "%s.%03ld", time_string, tv.tv_usec / 1000);
    return timestamp;
}
#define read_cycle()               get_timestamp()
#define LOG_FORMAT(letter, format) LOG_COLOR_##letter #letter " (%s) %s: " format LOG_RESET_COLOR "\n"
#elif defined(__APPLE__)
/* Mac OS code */
#include <time.h>
#define read_cycle()               (uint64_t) clock()
#define LOG_FORMAT(letter, format) LOG_COLOR_##letter #letter " (%llu) %s: " format LOG_RESET_COLOR "\n"
#else
/* Other code */
#endif

#ifndef TAG
#define TAG "LISA_UI"
#endif

#define LISA_UI_LOGE(format, ...)                                                                                      \
    do {                                                                                                               \
        LOG_PRINTF(LOG_FORMAT(E, format), read_cycle(), TAG, ##__VA_ARGS__);                                           \
    } while (0)
#define LISA_UI_LOGW(format, ...)                                                                                      \
    do {                                                                                                               \
        LOG_PRINTF(LOG_FORMAT(W, format), read_cycle(), TAG, ##__VA_ARGS__);                                           \
    } while (0)
#define LISA_UI_LOGI(format, ...)                                                                                      \
    do {                                                                                                               \
        LOG_PRINTF(LOG_FORMAT(I, format), read_cycle(), TAG, ##__VA_ARGS__);                                           \
    } while (0)
#define LISA_UI_LOGD(format, ...)                                                                                      \
    do {                                                                                                               \
        LOG_PRINTF(LOG_FORMAT(D, format), read_cycle(), TAG, ##__VA_ARGS__);                                           \
    } while (0)
#define LISA_UI_LOGV(format, ...)                                                                                      \
    do {                                                                                                               \
        LOG_PRINTF(LOG_FORMAT(V, format), read_cycle(), TAG, ##__VA_ARGS__);                                           \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_LOG_H__ */