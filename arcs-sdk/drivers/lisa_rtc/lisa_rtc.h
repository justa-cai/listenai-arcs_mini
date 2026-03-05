/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_rtc.h
 * @brief LISA RTC 实时时钟设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RTC 类型定义
 * ======================================================================== */

/**
 * @brief RTC 时间结构体
 */
typedef struct {
    uint16_t year;       /* 年份 (0-127,起始年份2000年,即0表示2000年,127表示2127年) */
    uint8_t month;       /* 月份 (1-12) */
    uint8_t day;         /* 日期 (1-31) */
    uint8_t weekday;     /* 星期 (0-6, 0=Sunday) */
    uint8_t hour;        /* 小时 (0-23) */
    uint8_t minute;      /* 分钟 (0-59) */
    uint8_t second;      /* 秒 (0-59) */
} lisa_rtc_time_t;

/**
 * @brief RTC 闹钟结构体
 */
typedef struct {
    uint16_t year;       /* 年份 (0-127,起始年份2000年,即0表示2000年,127表示2127年) */
    uint8_t month;       /* 月份 (1-12) */
    uint8_t day;         /* 日期 (1-31) */
    uint8_t weekday;     /* 星期 (0-6, 0=Sunday) */
    uint8_t hour;        /* 小时 (0-23) */
    uint8_t minute;      /* 分钟 (0-59) */
    uint8_t second;      /* 秒 (0-59) */
} lisa_rtc_alarm_t;

/**
 * @brief RTC 事件类型
 */
typedef enum {
    LISA_RTC_EVENT_ALARM = (1 << 0),     /* 闹钟事件 */
    LISA_RTC_EVENT_SECOND = (1 << 1),    /* 秒中断事件 */
    LISA_RTC_EVENT_MINUTE = (1 << 2),    /* 分钟中断事件 */
    LISA_RTC_EVENT_HOUR = (1 << 3),      /* 小时中断事件 */
} lisa_rtc_event_t;

/**
 * @brief RTC 能力结构体
 */
typedef struct {
    bool has_alarm;              /* 是否支持闹钟 */
    uint8_t alarm_count;         /* 支持的闹钟数量 */
    uint16_t min_year;           /* 最小年份 */
    uint16_t max_year;           /* 最大年份 */
} lisa_rtc_capabilities_t;

/**
 * @brief RTC 事件回调函数类型
 *
 * @param event 事件类型位掩码
 * @param user_data 用户数据指针
 */
typedef void (*lisa_rtc_callback_t)(uint32_t event, void *user_data);

/* ========================================================================
 * RTC 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*set_time)(lisa_device_t *dev, const lisa_rtc_time_t *time);
    int (*get_time)(lisa_device_t *dev, lisa_rtc_time_t *time);
    int (*set_alarm)(lisa_device_t *dev, uint8_t alarm_id, const lisa_rtc_alarm_t *alarm);
    int (*get_alarm)(lisa_device_t *dev, uint8_t alarm_id, lisa_rtc_alarm_t *alarm);
    int (*enable_alarm)(lisa_device_t *dev, uint8_t alarm_id, bool enable);
    int (*set_periodic_int)(lisa_device_t *dev, lisa_rtc_event_t event, bool enable);
    int (*get_capabilities)(lisa_device_t *dev, lisa_rtc_capabilities_t *caps);
    int (*set_callback)(lisa_device_t *dev, lisa_rtc_callback_t callback, void *user_data);
} lisa_rtc_api_t;

/* ========================================================================
 * RTC 对外接口函数
 * ======================================================================== */

/**
 * @brief 设置RTC时间
 *
 * @param dev RTC设备指针
 * @param time 时间结构体
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_set_time(lisa_device_t *dev, const lisa_rtc_time_t *time)
{
    if (!dev || !dev->api || !time) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 参数合法性检查 */
    if (time->year > 127U || time->month < 1U || time->month > 12U ||
        time->day < 1U || time->day > 31U || time->weekday > 6U ||
        time->hour > 23U || time->minute > 59U || time->second > 59U) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->set_time ? api->set_time(dev, time) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取RTC时间
 *
 * @param dev RTC设备指针
 * @param time 输出参数，用于接收时间
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_get_time(lisa_device_t *dev, lisa_rtc_time_t *time)
{
    if (!dev || !dev->api || !time) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->get_time ? api->get_time(dev, time) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置闹钟
 *
 * @param dev RTC设备指针
 * @param alarm_id 闹钟ID (0-N)
 * @param alarm 闹钟配置
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_set_alarm(lisa_device_t *dev, uint8_t alarm_id, const lisa_rtc_alarm_t *alarm)
{
    if (!dev || !dev->api || !alarm) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 参数合法性检查 */
    if (alarm->year > 127U || alarm->month < 1U || alarm->month > 12U ||
        alarm->day < 1U || alarm->day > 31U ||
        alarm->hour > 23U || alarm->minute > 59U || alarm->second > 59U) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->set_alarm ? api->set_alarm(dev, alarm_id, alarm) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取闹钟配置
 *
 * @param dev RTC设备指针
 * @param alarm_id 闹钟ID (0-N)
 * @param alarm 输出参数，用于接收闹钟配置
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_get_alarm(lisa_device_t *dev, uint8_t alarm_id, lisa_rtc_alarm_t *alarm)
{
    if (!dev || !dev->api || !alarm) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->get_alarm ? api->get_alarm(dev, alarm_id, alarm) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启用或禁用闹钟
 *
 * @param dev RTC设备指针
 * @param alarm_id 闹钟ID (0-N)
 * @param enable true=启用, false=禁用
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_enable_alarm(lisa_device_t *dev, uint8_t alarm_id, bool enable)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->enable_alarm ? api->enable_alarm(dev, alarm_id, enable) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置周期性中断
 *
 * @param dev RTC设备指针
 * @param event 中断类型 (SECOND/MINUTE/HOUR)
 * @param enable true=启用, false=禁用
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 某些硬件只能同时启用一种周期性中断
 */
static inline int lisa_rtc_set_periodic_int(lisa_device_t *dev, lisa_rtc_event_t event, bool enable)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 参数合法性检查：event必须是有效的周期性中断类型 */
    if (event != LISA_RTC_EVENT_SECOND &&
        event != LISA_RTC_EVENT_MINUTE &&
        event != LISA_RTC_EVENT_HOUR) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->set_periodic_int ? api->set_periodic_int(dev, event, enable) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取RTC设备能力
 *
 * @param dev RTC设备指针
 * @param caps 输出参数，用于接收设备能力信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_get_capabilities(lisa_device_t *dev, lisa_rtc_capabilities_t *caps)
{
    if (!dev || !dev->api || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->get_capabilities ? api->get_capabilities(dev, caps) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置RTC事件回调函数
 *
 * @param dev RTC设备指针
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_rtc_set_callback(lisa_device_t *dev, lisa_rtc_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_rtc_api_t *api = (lisa_rtc_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 星期枚举
 */
#define LISA_RTC_WEEKDAY_SUNDAY    0
#define LISA_RTC_WEEKDAY_MONDAY    1
#define LISA_RTC_WEEKDAY_TUESDAY   2
#define LISA_RTC_WEEKDAY_WEDNESDAY 3
#define LISA_RTC_WEEKDAY_THURSDAY  4
#define LISA_RTC_WEEKDAY_FRIDAY    5
#define LISA_RTC_WEEKDAY_SATURDAY  6

/**
 * @brief 闹钟忽略标志
 */
#define LISA_RTC_ALARM_IGNORE_YEAR    0
#define LISA_RTC_ALARM_IGNORE_MONTH   0
#define LISA_RTC_ALARM_IGNORE_DAY     0
#define LISA_RTC_ALARM_IGNORE_WEEKDAY 0xFF

#ifdef __cplusplus
}
#endif
