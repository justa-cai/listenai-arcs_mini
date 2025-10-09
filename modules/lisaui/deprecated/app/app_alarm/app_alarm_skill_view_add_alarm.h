/**
 * @file app_alarm_skill_view_add_alarm.h
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-24
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __APP_ALARM_SKILL_VIEW_ADD_ALARM_H__
#define __APP_ALARM_SKILL_VIEW_ADD_ALARM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_app_common.h"

lisaui_err_t lisaui_app_alarm_close_skill_alarm_view(void);
lisaui_err_t lisaui_app_alarm_show_skill_alarm_view(const char *alarm_time_text, const char *alarm_date_text);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ALARM_SKILL_VIEW_ADD_ALARM_H__ */