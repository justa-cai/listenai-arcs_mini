/**
 * @file floating_button.c
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-21
 *
 * @copyright Copyright (c) 2021 - 2024 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ls_llm_dialog.h"
#include "lisaui_log.h"
#include "lv_img_utils.h"
#include "incbin.h"
#include <src/misc/lv_area.h>

static lv_obj_t *llm_dialog;
static lv_obj_t *llm_asr_result_label;
lv_obj_t *emoji_icon;

static lv_anim_t a1;
static lv_anim_t a2;
static lv_anim_timeline_t *anim_timeline = NULL;
static lv_img_dsc_t img_gif_ani_talk;

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/widgets/" path
#else
// #define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/widgets/" path
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "source/view/lisaui/widgets/" path
// app/llm/lisaui/widgets/assets/gif/ani_talk.gif
#endif

UI_RES_IMG_NAME(ani_talk, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/gif/ani_talk-10fps.gif"))
// UI_RES_IMG_NAME(ani_talk, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/gif/ani_talk.gif"))

static void set_width(void *var, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)var, v);
}

static void set_height(void *var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

static void anim_timeline_create(int32_t width, int32_t height)
{
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, llm_dialog);
    lv_anim_set_values(&a1, 0, width);
    lv_anim_set_early_apply(&a1, false);
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)set_width);
    lv_anim_set_path_cb(&a1, lv_anim_path_overshoot);
    lv_anim_set_time(&a1, 300);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, llm_dialog);
    lv_anim_set_values(&a2, 0, height);
    lv_anim_set_early_apply(&a2, false);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)set_height);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_set_time(&a2, 300);

    /* Create anim timeline */
    anim_timeline = lv_anim_timeline_create();
    lv_anim_timeline_add(anim_timeline, 0, &a1);
    lv_anim_timeline_add(anim_timeline, 0, &a2);
}
/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_obj_t *ls_lv_llm_dialog_create(lv_obj_t *parent)
{
    llm_dialog = lv_obj_create(parent);
    // lv_obj_remove_style_all(llm_dialog);
    lv_obj_set_style_pad_all(llm_dialog, 0, 0);
    // lv_obj_set_flex_flow(llm_dialog, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(llm_dialog, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    // bottom_bg = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(bottom_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_size(llm_dialog, LV_PCT(100), LV_DPX(60));
    // lv_obj_set_pos(llm_dialog, lv_pct(10), lv_pct(70));
    lv_obj_align(llm_dialog, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(llm_dialog, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_color(llm_dialog, lv_color_hex(0x454554), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(llm_dialog, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(llm_dialog, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(llm_dialog, lv_color_hex(0x454554), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(llm_dialog, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(llm_dialog, lv_color_hex(0x454554), LV_PART_MAIN | LV_STATE_DEFAULT);

    emoji_icon = lv_gif_create(llm_dialog);
    lv_img_gif_src_init(&img_gif_ani_talk, gfile_ani_talkData, gfile_ani_talkSize);
    lv_gif_set_src(emoji_icon, &img_gif_ani_talk);
    // lv_obj_align(emoji_icon, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align_to(emoji_icon, llm_dialog, LV_ALIGN_LEFT_MID, 0, 0);

    llm_asr_result_label = lv_textarea_create(llm_dialog);
    lv_obj_set_size(llm_asr_result_label, LV_PCT(80), LV_DPX(45));
    lv_obj_align_to(llm_asr_result_label, emoji_icon, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_clear_state(llm_asr_result_label, LV_STATE_FOCUSED);
    // lv_obj_align(llm_asr_result_label, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_add_flag(llm_asr_result_label, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_style_pad_all(llm_asr_result_label, 0, 0);
    lv_obj_set_style_pad_left(llm_asr_result_label, 20, 0);
    lv_obj_set_style_pad_right(llm_asr_result_label, 20, 0);
    lv_obj_set_style_border_width(llm_asr_result_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(llm_asr_result_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(llm_asr_result_label, &lv_font_notosans_cs_medium_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(llm_asr_result_label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(llm_asr_result_label, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(llm_asr_result_label, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (!anim_timeline) {
        anim_timeline_create(LV_PCT(100), LV_DPX(60)); // width: 320 * 0.8 = 256, height: 240 * 0.2 = 48;
    }
    // lv_obj_add_event_cb(llm_asr_result_label, dialog_event_handler, LV_EVENT_ALL, NULL);
    return llm_dialog;
}

int ls_lv_llm_dialog_set_emoji_state(const ls_llm_dialog_emoji_state_t state)
{
    // if (!llm_dialog)
    // {
    //     return -1;
    // }
    // switch (state)
    // {
    // case LLM_DIALOG_EMOJI_STATE_LISTENING:
    //     lv_gif_set_src(emoji_icon, &xiaoling_listening_s);
    //     break;
    // case LLM_DIALOG_EMOJI_STATE_THINKING:
    //     lv_gif_set_src(emoji_icon, &xiaoling_thinking_s);
    //     break;
    // case LLM_DIALOG_EMOJI_STATE_STANDBY:
    // default:
    //     lv_gif_set_src(emoji_icon, &xiaoling_standby_s);
    //     break;
    // }
    return 0;
}

int ls_llm_dialog_popup(const char *text)
{
    if (!llm_dialog) {
        ls_lv_llm_dialog_create(lv_layer_top());
        // ls_lv_llm_dialog_create(lv_layer_sys());
    }
    // lv_label_set_text(llm_asr_result_, text);
    // lv_textarea_set_text(llm_asr_result_label, text);
    // lv_anim_timeline_start(anim_timeline);
    if (strlen(text) == 0) {
        return 0;
    }
    ls_llm_dialog_popup_with_mode(LISAUI_LLM_STATUS_SUCCESS, text, LISAUI_TEXT_MODE_OVERWRITE);
    return 0;
}

int ls_llm_dialog_popup_with_mode(const ls_llm_status_t llm_ret_status, const char *text, ls_text_mode_t text_mode)
{
    static ls_llm_status_t last = LISAUI_LLM_STATUS_CLOSE;
    if (!llm_dialog) {
        if (llm_ret_status == LISAUI_LLM_STATUS_FORCE_CLOSE) {
            return 0;
        }
        ls_lv_llm_dialog_create(lv_layer_top());
    }

    switch (llm_ret_status) {
    case LISAUI_LLM_STATUS_WAITING:
        ls_lv_llm_dialog_set_emoji_state(LISAUI_LLM_DIALOG_EMOJI_STATE_LISTENING);
        // lv_textarea_cursor_down(llm_asr_result_label);
        lv_textarea_set_text(llm_asr_result_label, text);
        // lv_textarea_set_text(llm_asr_result_label, "....");
        lv_anim_timeline_set_reverse(anim_timeline, false);
        lv_anim_timeline_start(anim_timeline);
        // 等待
        break;
    case LISAUI_LLM_STATUS_PROCESSING:
        // 处理中
        break;
    case LISAUI_LLM_STATUS_SUCCESS:
        ls_lv_llm_dialog_set_emoji_state(LISAUI_LLM_DIALOG_EMOJI_STATE_THINKING);
        if (text_mode == LISAUI_TEXT_MODE_APPEND) {
            lv_textarea_add_text(llm_asr_result_label, text);
        } else {
            // lv_label_set_text(llm_asr_result_, text);
            lv_textarea_set_text(llm_asr_result_label, text);
        }
        break;
    case LISAUI_LLM_STATUS_FAIL:
    case LISAUI_LLM_STATUS_CLOSE:
        if (last != LISAUI_LLM_STATUS_CLOSE) {
            lv_anim_timeline_set_reverse(anim_timeline, true);
            lv_anim_timeline_start(anim_timeline);
        }
        // lv_obj_set_size(llm_dialog, LV_PCT(0), lv_pct(0));
        break;
    case LISAUI_LLM_STATUS_FORCE_CLOSE:
        lv_anim_timeline_set_reverse(anim_timeline, true);
        lv_anim_timeline_start(anim_timeline);
        // lv_obj_set_size(llm_dialog, LV_PCT(0), lv_pct(0));
        break;

    default:
        break;
    }

    last = llm_ret_status;

    return 0;
}

static void btn_start_event_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    LV_UNUSED(btn);

    // if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    // bool reverse = lv_obj_has_state(btn, LV_STATE_CHECKED);
    // lv_anim_timeline_set_reverse(anim_timeline, reverse);
    // lv_anim_timeline_start(anim_timeline);
    ls_llm_dialog_popup("您好，我是小美，有什么可以帮助您的吗？");
    // }

    // ls_lv_llm_dialog_set_emoji_state(LLM_DIALOG_EMOJI_STATE_THINKING);
    ls_lv_llm_dialog_set_emoji_state(LISAUI_LLM_DIALOG_EMOJI_STATE_LISTENING);
}

void ls_llm_dialog_example(void)
{
    // lv_obj_t *par = lv_scr_act();
    // lv_obj_set_flex_flow(par, LV_FLEX_FLOW_ROW);
    // lv_obj_set_flex_align(par, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // /* create btn_start */
    // lv_obj_t *btn_start = lv_btn_create(par);
    // lv_obj_add_event_cb(btn_start, btn_start_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    // // lv_obj_add_flag(btn_start, LV_OBJ_FLAG_IGNORE_LAYOUT);
    // lv_obj_add_flag(btn_start, LV_OBJ_FLAG_CHECKABLE);
    // // lv_obj_align(btn_start, LV_ALIGN_TOP_MID, -100, 20);

    // lv_obj_t *label_start = lv_label_create(btn_start);
    // lv_label_set_text(label_start, "Show LLM Dialog");
    // lv_obj_center(label_start);

    lv_obj_t *float_btn = lv_btn_create(lv_scr_act()); // 创建按键对象
    lv_obj_set_size(float_btn, 50, 50);                // 设置大小
    lv_obj_add_flag(float_btn, LV_OBJ_FLAG_FLOATING);  // 设置floating特性
    lv_obj_add_flag(float_btn, LV_OBJ_FLAG_CHECKABLE);
    // lv_obj_align(float_btn, LV_ALIGN_BOTTOM_RIGHT, 0, -lv_obj_get_style_pad_right(list, LV_PART_MAIN)); // 设置对齐
    lv_obj_add_event_cb(float_btn, btn_start_event_handler, LV_EVENT_VALUE_CHANGED, NULL); // 添加响应事件
    lv_obj_set_style_radius(float_btn, LV_RADIUS_CIRCLE, 0);                               // 设置圆形风格
    lv_obj_set_style_bg_img_src(float_btn, LV_SYMBOL_PLUS, 0); // 背影图片为内部图片LV_SYMBOL_PLUS
    lv_obj_set_style_text_font(float_btn, lv_theme_get_font_large(float_btn), 0); // 设置字体
}
