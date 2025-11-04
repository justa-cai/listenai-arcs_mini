/**
 * @file llm_dialog.h
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-26
 *
 * @copyright Copyright (c) 2021 - 2024 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LS_LLM_DIALOG_H__
#define __LS_LLM_DIALOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include "lvgl.h"
#elif CONFIG_LVGL_ENV_SIMULATOR
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif

LV_FONT_DECLARE(lv_font_notosans_cs_medium_18);

typedef enum {
    LISAUI_LLM_DIALOG_TYPE_INFO,
    LISAUI_LLM_DIALOG_TYPE_WARN,
    LISAUI_LLM_DIALOG_TYPE_ERROR,
    LISAUI_LLM_DIALOG_TYPE_SUCCESS,
    LISAUI_LLM_DIALOG_TYPE_CONFIRM,
    LISAUI_LLM_DIALOG_TYPE_INPUT,
} ls_llm_dialog_type_t;

typedef enum {
    LISAUI_LLM_DIALOG_EMOJI_STATE_STANDBY = 0,
    LISAUI_LLM_DIALOG_EMOJI_STATE_THINKING,
    LISAUI_LLM_DIALOG_EMOJI_STATE_LISTENING,
} ls_llm_dialog_emoji_state_t;

typedef enum _ls_llm_status_t {
    LISAUI_LLM_STATUS_WAITING = 0, // 等待
    LISAUI_LLM_STATUS_PROCESSING,  // 处理中
    LISAUI_LLM_STATUS_SUCCESS,     // 成功
    LISAUI_LLM_STATUS_FAIL,        // 失败
    LISAUI_LLM_STATUS_CLOSE,       // 关闭

    // 强制关闭
    LISAUI_LLM_STATUS_FORCE_CLOSE = 0xFF,
} ls_llm_status_t;

typedef enum _ls_text_mode_t {
    LISAUI_TEXT_MODE_APPEND = 0, // 追加
    LISAUI_TEXT_MODE_OVERWRITE,  // 覆盖
    LISAUI_TEXT_MODE_NONE = 0,   // 无
} ls_text_mode_t;

int ls_llm_dialog_popup(const char *text);
int ls_llm_dialog_set_emoji_state(const ls_llm_dialog_emoji_state_t state);
int ls_llm_dialog_popup_with_mode(const ls_llm_status_t llm_ret_status, const char *text, ls_text_mode_t text_mode);
void ls_llm_dialog_example(void);

#ifdef __cplusplus
}
#endif

#endif // __LS_LLM_DIALOG_H__
