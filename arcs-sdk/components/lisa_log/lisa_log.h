/**
 * Copyright (c) 2025, LISTENAI
 *
 *  Created on: 2022
 *      Author: USER
 */

#pragma once
#include <stdint.h>
#include "log_print.h"
#include "syslog.h"

#include "assert.h"

typedef void (*lisa_log_output_t)(const uint8_t *log, uint32_t len, void *data);

int lisa_log_backend_add(const char *name, lisa_log_output_t output, void *data);
/**
 * @brief 添加日志后端
 *
 * @param name 后端名称
 * @param output 正常输出
 * @param output_panic 异常输出, panic模式下使用, 此模式不能使用任何阻塞或系统调用
 * @param data 用户数据
 * @return int
 */
int lisa_log_backend_add_v2(const char *name, lisa_log_output_t output, lisa_log_output_t output_panic,
                            void *data);
int lisa_log_backend_remove(const char *name);
int lisa_log_backend_resume_all(void);
int lisa_log_backend_pause_all(void);
int lisa_log_backend_resume(const char *name);
int lisa_log_backend_pause(const char *name);

struct lisa_log_frontend {
    const char *name;
    int (*init)(const struct lisa_log_frontend *frontend);
    void (*flush)(const struct lisa_log_frontend *frontend);
    void (*output_hook_set)(void (*output_hook)(const uint8_t *log, uint32_t len));
    void (*level_set)(const struct lisa_log_frontend *frontend, int level);
    int (*level_get)(const struct lisa_log_frontend *frontend);
    void *data;
};

#if !defined(LOG_TAG) && defined(TAG)
#define LOG_TAG TAG
#endif

#ifndef LOG_TAG
#define LOG_TAG "NO_TAG"
#endif

#ifndef LISA_LOG_LEVEL_NONE
#define LISA_LOG_LEVEL_NONE    0
#endif
#ifndef LISA_LOG_LEVEL_ERROR
#define LISA_LOG_LEVEL_ERROR   1
#endif
#ifndef LISA_LOG_LEVEL_WARN
#define LISA_LOG_LEVEL_WARN    2
#endif
#ifndef LISA_LOG_LEVEL_INFO
#define LISA_LOG_LEVEL_INFO    3
#endif
#ifndef LISA_LOG_LEVEL_DEBUG
#define LISA_LOG_LEVEL_DEBUG   4
#endif
#ifndef LISA_LOG_LEVEL_VERBOSE
#define LISA_LOG_LEVEL_VERBOSE 5
#endif

#if !defined(LISA_LOG_LEVEL)
#define LISA_LOG_LEVEL LISA_LOG_LEVEL_VERBOSE
#endif

int lisa_log_init(void);
void lisa_log_set_level(uint8_t lvl);

#if CONFIG_LOG_FRONTEND_EASYLOGGER
#include "elog.h"

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_ERROR
#define LOGE(fmt, ...) elog_e(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_WARN
#define LOGW(fmt, ...) elog_w(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_INFO
#define LOGI(fmt, ...) elog_i(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGI(fmt, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_DEBUG
#define LOGD(fmt, ...) elog_d(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_VERBOSE
#define LOGV(fmt, ...) elog_v(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGV(fmt, ...)
#endif

#define LOGH(name, data, len) elog_hexdump(name, 16, data, len)

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_ERROR
#define LISA_LOGE(tag, format, ...) elog_e(tag, format, ##__VA_ARGS__)
#else
#define LISA_LOGE(tag, format, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_WARN
#define LISA_LOGW(tag, format, ...) elog_w(tag, format, ##__VA_ARGS__)
#else
#define LISA_LOGW(tag, format, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_INFO
#define LISA_LOGI(tag, format, ...) elog_i(tag, format, ##__VA_ARGS__)
#else
#define LISA_LOGI(tag, format, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_DEBUG
#define LISA_LOGD(tag, format, ...) elog_d(tag, format, ##__VA_ARGS__)
#else
#define LISA_LOGD(tag, format, ...)
#endif

#if LISA_LOG_LEVEL >= LISA_LOG_LEVEL_VERBOSE
#define LISA_LOGV(tag, format, ...) elog_v(tag, format, ##__VA_ARGS__)
#else
#define LISA_LOGV(tag, format, ...)
#endif

#define LISA_LOGT(tag, format, ...) elog_d(tag, format, ##__VA_ARGS__)
#define LISA_LOG(tag, format, ...)  elog_i(tag, format, ##__VA_ARGS__)
#define LISA_LOG_RAW(...)           elog_raw(__VA_ARGS__)

#define LISA_LOGH(tag, data, len, name) elog_hexdump(name, 16, data, len)

#define LISA_ASSERT(exp, fmt, ...)                                                                                     \
    do {                                                                                                               \
        if (__builtin_expect(!(exp), 0)) {                                                                             \
            printk("assert@%d:%s: " fmt "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__);                                  \
            assert(exp);                                                                                               \
        }                                                                                                              \
    } while (0)

#else

#define __LISA_LOG_RAW(tag, fmt, ...) printk("%s\r\n" fmt, tag, ##__VA_ARGS__)

/* No logger system */
#define LOGE(fmt, ...)                __LISA_LOG_RAW(TAG, "E:" fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)                __LISA_LOG_RAW(TAG, "W:" fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...)                __LISA_LOG_RAW(TAG, "I:" fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)                __LISA_LOG_RAW(TAG, "D:" fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...)                __LISA_LOG_RAW(TAG, "V:" fmt, ##__VA_ARGS__)
#define LOGH(name, data, len)

#define LISA_LOGV(tag, fmt, ...) __LISA_LOG_RAW(tag, "V:" fmt, ##__VA_ARGS__)
#define LISA_LOGD(tag, fmt, ...) __LISA_LOG_RAW(tag, "D:" fmt, ##__VA_ARGS__)
#define LISA_LOGT(tag, fmt, ...) __LISA_LOG_RAW(tag, "T:" fmt, ##__VA_ARGS__)
#define LISA_LOGI(tag, fmt, ...) __LISA_LOG_RAW(tag, "I:" fmt, ##__VA_ARGS__)
#define LISA_LOGW(tag, fmt, ...) __LISA_LOG_RAW(tag, "W:" fmt, ##__VA_ARGS__)
#define LISA_LOGE(tag, fmt, ...) __LISA_LOG_RAW(tag, "E:" fmt, ##__VA_ARGS__)

#define LISA_LOG(tag, fmt, ...) __LISA_LOG_RAW(tag, fmt, ##__VA_ARGS__)
#define LISA_ASSERT(exp, fmt, ...)                                                                                     \
    do {                                                                                                               \
        if (__builtin_expect(!(exp), 0)) {                                                                             \
            printk("assert@%d:%s: " fmt "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__);                                  \
            assert(exp);                                                                                               \
        }                                                                                                              \
    } while (0)
#define LISA_LOGH(tag, data, len, name)

#endif


#define LISA_NLOGD LOGD
#define LISA_NLOGI LOGI
#define LISA_NLOGW LOGW
#define LISA_NLOGE LOGE
#define LISA_NLOGV LOGV
