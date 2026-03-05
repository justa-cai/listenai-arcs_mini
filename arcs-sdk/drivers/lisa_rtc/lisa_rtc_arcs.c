/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_rtc_arcs.c
 * @brief LISA RTC ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 RTC 硬件适配
 *
 * @note ARCS 芯片使用 CALENDAR 外设实现 RTC 功能
 */

#include "lisa_rtc.h"
#include "Driver_CALENDAR.h"
#include <stddef.h>
#include <string.h>
#include <lisa_mutex.h>
#include <stdbool.h>

#define LOG_TAG "lisa_rtc_arcs"
#include <lisa_log.h>

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                                                        \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_unlock(priv->mutex);                                                                            \
        }                                                                                                              \
    } while (0)

/* ===== RTC 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                /* HAL CALENDAR 句柄 */
    lisa_mutex_t *mutex;              /* 互斥锁 */
    lisa_rtc_callback_t callback;     /* 用户回调函数 */
    void *user_data;                  /* 用户数据 */
    uint32_t enabled_events;          /* 已启用的事件掩码 */
} lisa_rtc_priv_t;

/* ===== RTC 设备静态实例 ===== */
static lisa_rtc_priv_t rtc0_priv;

/* ===== 内部辅助函数 ===== */

/**
 * @brief HAL层中断回调
 */
static void rtc_hal_callback(uint32_t event, void *workspace)
{
    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)workspace;
    if (!priv || !priv->callback) {
        return;
    }

    /* 将HAL事件转换为LISA事件 */
    uint32_t lisa_event = 0;
    
    if (event & CSK_CALENDAR_EVENT_ALARM_INT) {
        lisa_event |= LISA_RTC_EVENT_ALARM;
    }
    if (event & CSK_CALENDAR_EVENT_SEC_INT) {
        lisa_event |= LISA_RTC_EVENT_SECOND;
    }
    if (event & CSK_CALENDAR_EVENT_MIN_INT) {
        lisa_event |= LISA_RTC_EVENT_MINUTE;
    }
    if (event & CSK_CALENDAR_EVENT_HOUR_INT) {
        lisa_event |= LISA_RTC_EVENT_HOUR;
    }

    if (lisa_event) {
        priv->callback(lisa_event, priv->user_data);
    }
}

/* ===== 参数校验辅助函数 ===== */

#define RTC_YEAR_MIN 0U
#define RTC_YEAR_MAX 127U

static bool rtc_validate_common_time(uint16_t year,
                                     uint8_t month,
                                     uint8_t day,
                                     uint8_t hour,
                                     uint8_t minute,
                                     uint8_t second)
{
    if (year < RTC_YEAR_MIN || year > RTC_YEAR_MAX) {
        return false;
    }

    if (month < 1U || month > 12U) {
        return false;
    }

    if (day < 1U || day > 31U) {
        return false;
    }

    if (hour > 23U || minute > 59U || second > 59U) {
        return false;
    }

    return true;
}

static bool rtc_validate_time_struct(const lisa_rtc_time_t *time)
{
    if (!time) {
        return false;
    }

    if (!rtc_validate_common_time(time->year,
                                  time->month,
                                  time->day,
                                  time->hour,
                                  time->minute,
                                  time->second)) {
        return false;
    }

    if (time->weekday > 6U) {
        return false;
    }

    return true;
}

static bool rtc_validate_alarm_struct(const lisa_rtc_alarm_t *alarm)
{
    if (!alarm) {
        return false;
    }

    /* CALENDAR 闹钟必须匹配所有字段，不支持忽略 */
    if (!rtc_validate_common_time(alarm->year,
                                  alarm->month,
                                  alarm->day,
                                  alarm->hour,
                                  alarm->minute,
                                  alarm->second)) {
        return false;
    }

    return true;
}

/* ===== ARCS平台RTC实现函数 ===== */

/**
 * @brief 设置RTC时间
 */
static int arcs_rtc_set_time(lisa_device_t *dev, const lisa_rtc_time_t *time)
{
    if (!lisa_device_is_initialized(dev) || !time) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!rtc_validate_time_struct(time)) {
        LISA_LOGE(LOG_TAG, "Invalid RTC time parameters");
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;
    
    /* 将LISA时间格式转换为HAL时间格式 */
    CSK_CALENDAR_TIME hal_time = {
        .year = time->year,
        .month = time->month,
        .weekend = time->weekday,
        .day = time->day,
        .hour = time->hour,
        .min = time->minute,
        .sec = time->second
    };

    DEVICE_LOCK(priv);

    if (CALENDAR_SetTime(priv->hal_handler, &hal_time) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to set RTC time");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取RTC时间
 */
static int arcs_rtc_get_time(lisa_device_t *dev, lisa_rtc_time_t *time)
{
    if (!lisa_device_is_initialized(dev) || !time) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;
    CSK_CALENDAR_TIME hal_time;

    DEVICE_LOCK(priv);

    if (CALENDAR_GetTime(priv->hal_handler, &hal_time) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to get RTC time");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    /* 将HAL时间格式转换为LISA时间格式 */
    time->year = hal_time.year;
    time->month = hal_time.month;
    time->day = hal_time.day;
    time->weekday = hal_time.weekend;
    time->hour = hal_time.hour;
    time->minute = hal_time.min;
    time->second = hal_time.sec;

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置闹钟
 */
static int arcs_rtc_set_alarm(lisa_device_t *dev, uint8_t alarm_id, const lisa_rtc_alarm_t *alarm)
{
    if (!lisa_device_is_initialized(dev) || !alarm) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (alarm_id != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    if (!rtc_validate_alarm_struct(alarm)) {
        LISA_LOGE(LOG_TAG, "Invalid RTC alarm parameters");
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;
    
    /* 将LISA闹钟格式转换为HAL闹钟格式 */
    CSK_CALENDAR_ALARM hal_alarm = {
        .year = alarm->year,
        .month = alarm->month,
        .day = alarm->day,
        .hour = alarm->hour,
        .min = alarm->minute,
        .sec = alarm->second
    };

    DEVICE_LOCK(priv);

    if (CALENDAR_SetAlarm(priv->hal_handler, &hal_alarm) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to set RTC alarm");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取闹钟配置
 */
static int arcs_rtc_get_alarm(lisa_device_t *dev, uint8_t alarm_id, lisa_rtc_alarm_t *alarm)
{
    if (!lisa_device_is_initialized(dev) || !alarm) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (alarm_id != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;
    CSK_CALENDAR_ALARM hal_alarm;

    DEVICE_LOCK(priv);

    if (CALENDAR_GetAlarm(priv->hal_handler, &hal_alarm) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to get RTC alarm");
        return LISA_DEVICE_ERR_IO;
    }

    DEVICE_UNLOCK(priv);

    /* 将HAL闹钟格式转换为LISA闹钟格式 */
    alarm->year = hal_alarm.year;
    alarm->month = hal_alarm.month;
    alarm->day = hal_alarm.day;
    alarm->weekday = 0xFF;  /* CALENDAR 不支持星期匹配 */
    alarm->hour = hal_alarm.hour;
    alarm->minute = hal_alarm.min;
    alarm->second = hal_alarm.sec;

    return LISA_DEVICE_OK;
}

/**
 * @brief 启用或禁用闹钟
 */
static int arcs_rtc_enable_alarm(lisa_device_t *dev, uint8_t alarm_id, bool enable)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (alarm_id != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    if (CALENDAR_Control(priv->hal_handler, CSK_CALENDAR_CTRL_ALARM_EN, enable ? 1 : 0) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to %s alarm", enable ? "enable" : "disable");
        return LISA_DEVICE_ERR_IO;
    }

    if (enable) {
        priv->enabled_events |= LISA_RTC_EVENT_ALARM;
    } else {
        priv->enabled_events &= ~LISA_RTC_EVENT_ALARM;
    }

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置周期性中断
 */
static int arcs_rtc_set_periodic_int(lisa_device_t *dev, lisa_rtc_event_t event, bool enable)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;
    uint32_t control = 0;

    /* 映射LISA事件到HAL控制位 */
    switch (event) {
    case LISA_RTC_EVENT_SECOND:
        control = CSK_CALENDAR_CTRL_SEC_INT;
        break;
    case LISA_RTC_EVENT_MINUTE:
        control = CSK_CALENDAR_CTRL_MIN_INT;
        break;
    case LISA_RTC_EVENT_HOUR:
        control = CSK_CALENDAR_CTRL_HOUR_INT;
        break;
    default:
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    DEVICE_LOCK(priv);

    if (CALENDAR_Control(priv->hal_handler, control, enable ? 1 : 0) != 0) {
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to set periodic interrupt");
        return LISA_DEVICE_ERR_IO;
    }

    if (enable) {
        priv->enabled_events |= event;
    } else {
        priv->enabled_events &= ~event;
    }

    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取RTC设备能力
 */
static int arcs_rtc_get_capabilities(lisa_device_t *dev, lisa_rtc_capabilities_t *caps)
{
    if (!lisa_device_is_initialized(dev) || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* ARCS芯片CALENDAR能力 */
    caps->has_alarm = true;
    caps->alarm_count = 1;
    caps->min_year = 0;
    caps->max_year = 127;  /* CALENDAR 支持年份 0-127,起始年份2000年 */

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置RTC事件回调
 */
static int arcs_rtc_set_callback(lisa_device_t *dev, lisa_rtc_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_rtc_priv_t *priv = (lisa_rtc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);
    priv->callback = callback;
    priv->user_data = user_data;
    DEVICE_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

/* ===== ARCS RTC API 实例 ===== */
static const lisa_rtc_api_t arcs_rtc_api = {
    .set_time = arcs_rtc_set_time,
    .get_time = arcs_rtc_get_time,
    .set_alarm = arcs_rtc_set_alarm,
    .get_alarm = arcs_rtc_get_alarm,
    .enable_alarm = arcs_rtc_enable_alarm,
    .set_periodic_int = arcs_rtc_set_periodic_int,
    .get_capabilities = arcs_rtc_get_capabilities,
    .set_callback = arcs_rtc_set_callback,
};

/* ===== 设备初始化函数 ===== */

static int arcs_rtc0_init(void)
{
    /* 清空私有数据 */
    memset(&rtc0_priv, 0, sizeof(lisa_rtc_priv_t));

    /* 获取 HAL CALENDAR 句柄 */
    rtc0_priv.hal_handler = CALENDAR();
    if (!rtc0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get CALENDAR handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 创建互斥锁 */
    rtc0_priv.mutex = lisa_mutex_create();
    if (!rtc0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL CALENDAR */
    if (CALENDAR_Initialize(rtc0_priv.hal_handler, rtc_hal_callback, &rtc0_priv) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize CALENDAR");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 使能 CALENDAR 电源 */
    if (CALENDAR_PowerControl(rtc0_priv.hal_handler, CSK_POWER_FULL) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to power on CALENDAR");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 启用校准功能（可选） */
    CALENDAR_Control(rtc0_priv.hal_handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);

    LISA_LOGI(LOG_TAG, "RTC0 initialized successfully");

    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(rtc0,                        /* 设备名称 */
                     &arcs_rtc_api,               /* API指针 */
                     &rtc0_priv,                  /* 私有数据指针 */
                     NULL,                        /* 用户数据 */
                     arcs_rtc0_init,              /* 初始化函数 */
                     LISA_DEVICE_PRIORITY_NORMAL); /* 优先级 */
