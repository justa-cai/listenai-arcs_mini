/**
 * @file app_common.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-26
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lisaui_app_common.h"
#include "lisaui_app_manager.h"
#include "lisaui_log.h"

static const char *TAG = "app_common";

lisaui_err_t _lisaui_set_style_container(lv_obj_t *obj, lv_color_t bg_color, lv_opa_t bg_opa, lv_color_t border_color,
                                         lv_coord_t border_width, lv_coord_t radius)
{
    if (obj == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
#if 0
    lv_obj_set_style_local_bg_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, bg_color);
    lv_obj_set_style_local_bg_opa(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, bg_opa);
    lv_obj_set_style_local_radius(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, radius);
    lv_obj_set_style_local_border_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, border_color);
    lv_obj_set_style_local_border_width(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, border_width);
    lv_obj_set_style_local_pad_all(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
#else
    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, bg_opa);
    // lv_obj_set_style_radius(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, radius);
    // lv_obj_set_style_border_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, border_color);
    // lv_obj_set_style_border_width(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, border_width);
    // lv_obj_set_style_pad_all(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    lv_obj_set_style_bg_opa(obj, bg_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_side(ui_StatusBar, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, border_width, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
    return LISAUI_ERR_OK;
}

static lv_style_t default_style;
static bool default_style_inited = false;

lisaui_err_t _lisaui_set_default_style(lv_style_t *style)
{
#if 0
    if (style != NULL) {
        lv_style_copy(&default_style, style);
    } else {
        lv_style_init(&default_style);
        lv_style_set_text_font(&default_style, LV_STATE_DEFAULT, &lv_font_chinese_18);
    }
#endif
    return LISAUI_ERR_OK;
}

lisaui_err_t _lisaui_lv_obj_set_default_style(lv_obj_t *obj)
{
    if (obj == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
#if 0
    // if (lv_debug_check_style(&default_style) != LV_RES_OK)
    // {
    //     _lisaui_set_default_style(NULL);
    // }
    if (!default_style_inited) {
        _lisaui_set_default_style(NULL);
        default_style_inited = true;
    }
    // lv_obj_set_style_local_text_font(obj, LV_OBJ_PART_MAIN, &default_style);
    lv_obj_add_style(obj, LV_OBJ_PART_MAIN, &default_style);
#endif
    return LISAUI_ERR_OK;
}

lisaui_err_t _lisaui_lv_obj_set_btn_custom_style(lv_obj_t *obj)
{
    if (obj == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
#if 0
    static lv_style_t style_btn;

    /*Create a simple button style*/
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, LV_STATE_DEFAULT, 6);

    /*Add a border*/
    // lv_style_set_border_opa(&style_btn, LV_STATE_DEFAULT, LV_OPA_70);
    lv_style_set_border_width(&style_btn, LV_STATE_DEFAULT, 0);

    /*Different border color in focused state*/
    // lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED, LV_COLOR_BLUE);
    lv_style_set_bg_color(&style_btn, LV_STATE_DEFAULT, lv_color_hex(0x1886EE));
    lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED, lv_color_hex(0x39BAFF));
    lv_style_set_border_color(&style_btn, LV_STATE_FOCUSED | LV_STATE_PRESSED, LV_COLOR_NAVY);

    lv_obj_add_style(obj, LV_BTN_PART_MAIN, &style_btn);
#endif
    return LISAUI_ERR_OK;
}
static lv_obj_t *sys_toast = NULL;
static lv_anim_timeline_t *anim_timeline = NULL;
static lv_obj_t *llm_asr_result_label;
static lv_coord_t llm_asr_result_label_width = 0;
static lv_coord_t llm_asr_result_label_height = 0;
lv_anim_t a1;
lv_anim_t a2;
#define TOAST_DURATION 1200
static void set_width(void *var, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)var, v);
}

static void set_height(void *var, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)var, v);
}

static void anim_timeline_create(lv_obj_t *target, int32_t width, int32_t height)
{
    /* obj1 */
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, target);
    lv_anim_set_values(&a1, 0, width);
    lv_anim_set_early_apply(&a1, false);
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)set_width);
    lv_anim_set_path_cb(&a1, lv_anim_path_overshoot);
    lv_anim_set_time(&a1, 300);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, target);
    lv_anim_set_values(&a2, 0, height);
    lv_anim_set_early_apply(&a2, false);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)set_height);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_set_time(&a2, 300);

    if (anim_timeline) {
        lv_anim_timeline_del(anim_timeline);
        anim_timeline = NULL;
    }
    anim_timeline = lv_anim_timeline_create();
    lv_anim_timeline_add(anim_timeline, 0, &a1);
    lv_anim_timeline_add(anim_timeline, 0, &a2);
}
lv_obj_t *ls_lv_llm_toast_create(lv_obj_t *parent)
{
    lv_obj_t *toast = lv_obj_create(parent);
    lv_obj_set_style_pad_all(toast, 0, 0);
    lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(toast, 0, 0);

    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -LV_DPX(10));
    lv_obj_set_style_border_width(toast, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(toast, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(toast, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(toast, lv_color_hex(0xFFF4DE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(toast, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);

    static lv_style_t label_style;
    lv_style_init(&label_style);

    llm_asr_result_label = lv_textarea_create(toast);
    // lv_obj_set_style_pad_all(llm_asr_result_label, 0, 0);
    lv_obj_set_style_border_width(llm_asr_result_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_state(llm_asr_result_label, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(llm_asr_result_label, lv_color_hex(0x9d6531), 0);
    lv_obj_set_style_text_font(llm_asr_result_label, &lv_font_chinese_18, 0);
    lv_obj_set_style_bg_opa(llm_asr_result_label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(llm_asr_result_label, lv_pct(100));
    return toast;
}

lv_timer_t *pop_toast_timer;
static void lisaui_app_sys_toast_hidden_cb(lv_timer_t *timer)
{
    // lv_obj_set_hidden(sys_toast, true);
    // lv_obj_set_hidden(llm_asr_result_label, true);
    lv_obj_del(sys_toast);

    sys_toast = NULL;
    if (anim_timeline) {
        lv_anim_timeline_stop(anim_timeline);
        lv_anim_timeline_del(anim_timeline);
        anim_timeline = NULL;
    }
    if (pop_toast_timer) {
        lv_timer_del(pop_toast_timer);
        pop_toast_timer = NULL;
    }
}

lisaui_err_t lisaui_popup_toast(const char *message)
{
    LVGL_OBJ_SAFE_DEL(sys_toast);
    sys_toast = ls_lv_llm_toast_create(lv_layer_sys());
    LISAUI_LOGV(TAG, "lisaui_popup_toast message: %s", message);
    lv_textarea_set_text(llm_asr_result_label, message);
    if (anim_timeline) {
        lv_anim_timeline_stop(anim_timeline);
        lv_anim_timeline_del(anim_timeline);
        anim_timeline = NULL;
    }
    anim_timeline_create(sys_toast, 300, 48); // width: 320 * 0.8 = 256, height: 240 * 0.2 = 48;
    lv_anim_timeline_start(anim_timeline);
    if (pop_toast_timer) {
        lv_timer_del(pop_toast_timer);
        pop_toast_timer = NULL;
    }
    pop_toast_timer = lv_timer_create(lisaui_app_sys_toast_hidden_cb, TOAST_DURATION, NULL);
    return LISAUI_ERR_OK;
}

#define CONFIG_LISAUI_VIEW_MANAGER_DEBUG_ENABLE 0

#if CONFIG_LISAUI_EXEC_HOOK_ENABLE

static lisaui_app_manager_app_hook_t m_app_common_enter_hook = NULL;
lisaui_err_t lisaui_app_common_set_enter_app_hook(lisaui_app_manager_app_hook_t hook)
{
    if (hook == NULL) {
        LISAUI_LOGE(TAG, "[ui] hook is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }
    m_app_common_enter_hook = hook;
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_enter_lvgl_hook(lisaui_app_t *app)
{
    lisaui_view_stack_t *view_stack;
    lisaui_app_manager_get_view_stack(&view_stack);

    if (view_stack == NULL) {
        LISAUI_LOGE(TAG, "[ui] view stack is NULL");
        return LISAUI_ERR_NO_MEMORY;
    }

    lisaui_view_page_t *page = (lisaui_view_page_t *)lisaui_malloc(sizeof(lisaui_view_page_t));
    if (page == NULL) {
        LISAUI_LOGE(TAG, "TAG, [ui] malloc failed");
        return LISAUI_ERR_NO_MEMORY;
    }
    memset(page, 0, sizeof(lisaui_view_page_t));
    page->root = (lisaui_view_t *)app->get_app_view();
    page->app_id = app->info.id;
    // LISAUI_LOGI(TAG, "[ui] lv_disp_load_scr page:%p page->root: %p", page, page->root);
    if (page->root == NULL) {
        LISAUI_LOGE(TAG, "TAG, [ui] view page root is NULL");
        return LISAUI_ERR_NO_MEMORY;
    }

#if CONFIG_LISAUI_VIEW_MANAGER_DEBUG_ENABLE
    lisaui_view_manager_print_usage(view_stack);
#endif
    lisaui_view_manager_push(view_stack, page);
#if CONFIG_LISAUI_VIEW_MANAGER_DEBUG_ENABLE
    lisaui_view_manager_print_usage(view_stack);
#endif
    lv_disp_load_scr(page->root);
    // lisaui_memory_monitor(NULL);
    if (m_app_common_enter_hook != NULL) {
        if (m_app_common_enter_hook(app) != LISAUI_ERR_OK) {
            LISAUI_LOGE(TAG, "[ui] app enter hook failed");
            return LISAUI_ERR_APP_ENTER_FAILED;
        }
    }
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_exit_lvgl_hook(lisaui_app_t *app)
{
    lisaui_view_stack_t *view_stack;
    lisaui_app_manager_get_view_stack(&view_stack);
#if CONFIG_LISAUI_VIEW_MANAGER_DEBUG_ENABLE
    lisaui_view_manager_print_usage(view_stack);
#endif
    lisaui_view_page_t *page = NULL;
    if (lisaui_view_manager_pop(view_stack, &page) != LISAUI_ERR_OK) {
        LISAUI_LOGE(TAG, "[ui] view page is NULL");
        return LISAUI_ERR_NO_MEMORY;
    }
    lisaui_free(page);
    page = NULL;
#if CONFIG_LISAUI_VIEW_MANAGER_DEBUG_ENABLE
    lisaui_view_manager_print_usage(view_stack);
#endif
    if (lisaui_view_manager_get_current(view_stack, &page) != LISAUI_ERR_OK) {
        // LISAUI_LOGE(TAG, "[ui] view page is NULL");
        return LISAUI_ERR_NO_MEMORY;
    }
    if (page == NULL) {
        LISAUI_LOGE(TAG, "[ui] view page is NULL");
        return LISAUI_ERR_NO_MEMORY;
    }
    if (page->root != NULL) {
        lv_disp_load_scr(page->root);
    } else {
        LISAUI_LOGI(TAG, "[ui] view page prev is NULL");
    }
    // LISAUI_LOGI(TAG, "app(%s) exit success", app->icon->title);
    // lisaui_memory_monitor(NULL);
    return LISAUI_ERR_OK;
}

#endif

// 直接载入对应 View 界面
lisaui_err_t lisaui_app_common_switch_app_scr_with_lvgl(const int app_id)
{
    lisaui_app_t *app = NULL;
    if (lisaui_app_manager_get_app_by_id(app_id, &app) != LISAUI_ERR_APP_OK) {
        LISAUI_LOGE(TAG, "[ui] app(id:%d) not registered", app_id);
        return LISAUI_ERR_APP_NOT_REGISTERED;
    }
    if (app->get_app_view() == NULL) {
        if (app->create(NULL) != LISAUI_ERR_APP_OK) {
            return LISAUI_ERR_APP_CREATE_FAILED;
        }
    }
    lisaui_app_enter_lvgl_hook(app);
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_common_pop_app_scr_with_lvgl(const int app_id)
{
    lisaui_app_t *app = NULL;
    if (lisaui_app_manager_get_app_by_id(app_id, &app) != LISAUI_ERR_APP_OK) {
        LISAUI_LOGE(TAG, "[ui] app(id:%d) not registered", app_id);
        return LISAUI_ERR_APP_NOT_REGISTERED;
    }
    if (app->get_app_view() == NULL) {
        if (app->create(NULL) != LISAUI_ERR_APP_OK) {
            return LISAUI_ERR_APP_CREATE_FAILED;
        }
    }
    lv_obj_t *view = lv_disp_get_scr_act(NULL);
    if (view != NULL) {
        LISAUI_LOGI(TAG, "[ui] prev_view: %p", view);
        if (view != app->get_app_view()) {
            lisaui_app_exit_lvgl_hook(app);
        } else {
            int current_app_id = lisaui_app_manager_get_current_appid();
            LISAUI_LOGI(TAG, "[ui] current_app_id: %d", current_app_id);
            // lisaui_app_exit(app_id);
            if (lisaui_app_manager_get_app_by_id(current_app_id, &app) != LISAUI_ERR_APP_OK) {
                LISAUI_LOGE(TAG, "[ui] app(id:%d) not registered", current_app_id);
                return LISAUI_ERR_APP_NOT_REGISTERED;
            }
        }
    }

    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_app_common_get_current_scr(lv_obj_t **view)
{
    if (view == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }
    // lisaui_view_stack_t *view_stack;
    // lisaui_app_manager_get_view_stack(&view_stack);
    // lisaui_view_page_t *page = NULL;
    // if (lisaui_view_manager_get_current(view_stack, &page) != LISAUI_ERR_OK) {
    //     LISAUI_LOGE(TAG, "[ui] view page is NULL");
    //     return LISAUI_ERR_NO_MEMORY;
    // }
    // if (page == NULL) {
    //     LISAUI_LOGE(TAG, "[ui] view page is NULL");
    //     return LISAUI_ERR_NO_MEMORY;
    // }
    // *view = page->root;
    return LISAUI_ERR_OK;
}

void lisaui_memory_monitor(void *param)
{
    (void)param; /*Unused*/

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    // printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n", (int)mon.total_size - mon.free_size,
    // mon.used_pct,
    //        mon.frag_pct, (int)mon.free_biggest_size);
    LISAUI_PRINTK("------------Memory usage------------\n");
    LISAUI_PRINTK("\tTotal size              : %0.3lfKB(%d Byte)\n", mon.total_size / 1024.0, mon.total_size);
    LISAUI_PRINTK("\tFree count              : %d\n", mon.free_cnt);
    LISAUI_PRINTK("\tFree size               : %d\n", mon.free_size);
    LISAUI_PRINTK("\tFree biggest size       : %d\n", mon.free_biggest_size);
    LISAUI_PRINTK("\tUsed count              : %d\n", mon.used_cnt);
    LISAUI_PRINTK("\tMax used                : %0.3lfKB(%d Byte)\n", mon.max_used / 1024.0, mon.max_used);
    LISAUI_PRINTK("\tUsed percentage         : %d\n", mon.used_pct);
    LISAUI_PRINTK("\tFragmentation percentage: %d\n", mon.frag_pct);
    LISAUI_PRINTK("\n");

    lisaui_view_stack_t *view_stack;
    lisaui_app_manager_get_view_stack(&view_stack);
    lisaui_view_manager_print_usage(view_stack);
}