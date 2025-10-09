#include "ls_wifi_connect.h"

#include "stdio.h"

LV_FONT_DECLARE(lv_font_chinese_18);

void ls_wifi_connect_set_tips(ls_ui_wifi_connect_t *obj, const char *tips)
{
    lv_label_set_text(obj->tips_lbl, tips);
}

void ls_wifi_connect_set_cancel_cb(ls_ui_wifi_connect_t *obj, ls_wifi_connect_cancel_evt_cb_t cb)
{
    obj->cancel_cb = cb;
}

void ls_wifi_connect_set_connect_cb(ls_ui_wifi_connect_t *obj, ls_wifi_connect_connect_evt_cb_t cb)
{
    obj->connect_cb = cb;
}

static void ls_wifi_connect_ta_evt_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    ls_ui_wifi_connect_t *parent = (ls_ui_wifi_connect_t *)lv_obj_get_parent(ta);

    if (code == LV_EVENT_FOCUSED) {
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
            lv_obj_align(parent->pwd_ta, LV_ALIGN_TOP_MID, LV_PCT(0), LV_DPX(0));
            lv_obj_align(parent->kb, LV_ALIGN_BOTTOM_MID, LV_PCT(0), LV_DPX(0));

            lv_obj_set_size(parent->kb, LV_HOR_RES, LV_VER_RES - 80);
            lv_keyboard_set_textarea(parent->kb, parent->pwd_ta);

            lv_obj_clear_flag(parent->kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_view_recursive(parent->pwd_ta, LV_ANIM_OFF);

            lv_obj_add_flag(parent->tips_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(parent->apply_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(parent->cancel_btn, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        /* 防止输入时误触, 不处理失去焦点事件, 只能通过点击回车键退出 */
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(parent->kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(parent->pwd_ta, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, parent->pwd_ta); /*To forget the last clicked object to make it focusable again*/

        lv_obj_clear_flag(parent->tips_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(parent->apply_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(parent->cancel_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(parent->pwd_ta, LV_ALIGN_CENTER, LV_PCT(0), LV_DPX(0));
    }
}

static void ls_wifi_connect_cancel_btn_evt_handler(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    ls_ui_wifi_connect_t *parent = (ls_ui_wifi_connect_t *)lv_obj_get_parent(obj);

    if (parent->cancel_cb) {
        parent->cancel_cb((lv_obj_t *)parent);
    }
}

static void ls_wifi_connect_apply_btn_evt_handler(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    ls_ui_wifi_connect_t *parent = (ls_ui_wifi_connect_t *)lv_obj_get_parent(obj);

    if (parent && parent->pwd_ta && parent->tips_lbl) {
        if (parent->connect_cb) {
            const char *ssid = lv_label_get_text(parent->tips_lbl);
            const char *pwd = lv_textarea_get_text(parent->pwd_ta);
            parent->connect_cb((lv_obj_t *)parent, ssid, pwd);
        }
    }
}

lv_obj_t *ls_wifi_connect_create(lv_obj_t *parent)
{
    lv_obj_t *menu = (lv_obj_t *)lv_mem_realloc(lv_obj_create(parent), sizeof(ls_ui_wifi_connect_t));
    ls_ui_wifi_connect_t *ui_conn = (ls_ui_wifi_connect_t *)menu;

    lv_obj_set_size(menu, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(menu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(menu, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scroll_dir(menu, LV_DIR_NONE);

    ui_conn->tips_lbl = lv_label_create(menu);
    lv_label_set_long_mode(ui_conn->tips_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(ui_conn->tips_lbl, LV_ALIGN_CENTER, 0, -LV_DPX(80));
    lv_obj_set_style_text_color(ui_conn->tips_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_conn->tips_lbl, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_conn->tips_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_conn->tips_lbl, "ssid");

    /*Create a keyboard*/
    ui_conn->kb = lv_keyboard_create(menu);
    lv_obj_add_flag(ui_conn->kb, LV_OBJ_FLAG_HIDDEN);

    ui_conn->pwd_ta = lv_textarea_create(menu);
    lv_obj_align(ui_conn->pwd_ta, LV_ALIGN_CENTER, LV_PCT(0), LV_DPX(0));
    // lv_obj_align(password, LV_ALIGN_CENTER, -LV_PCT(20), LV_DPX(0));
    // lv_obj_set_width(password, LV_PCT(60));
    lv_textarea_set_one_line(ui_conn->pwd_ta, true);
    lv_textarea_set_password_mode(ui_conn->pwd_ta, false);
    lv_textarea_set_placeholder_text(ui_conn->pwd_ta, "input password");
    lv_obj_add_event_cb(ui_conn->pwd_ta, ls_wifi_connect_ta_evt_handler, LV_EVENT_ALL, ui_conn->kb);

    lv_obj_t *password_show_btn = lv_btn_create(menu);
    lv_obj_set_size(password_show_btn, LV_DPX(40), LV_DPX(40));
    lv_obj_align_to(password_show_btn, ui_conn->pwd_ta, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(10), 0);
    lv_obj_set_style_bg_color(password_show_btn, lv_color_hex(0x24242d), 0);
    lv_obj_set_style_radius(password_show_btn, 8, 0);
    lv_obj_set_style_shadow_width(password_show_btn, 0, 0);
    lv_obj_set_style_border_width(password_show_btn, 0, 0);
    lv_obj_set_style_outline_width(password_show_btn, 0, 0);
    lv_obj_set_style_bg_opa(password_show_btn, 0, 0);
    lv_obj_set_style_border_opa(password_show_btn, 0, 0);
    lv_obj_set_style_outline_opa(password_show_btn, 0, 0);
    lv_obj_set_style_pad_all(password_show_btn, 0, 0);

    ui_conn->apply_btn = lv_btn_create(menu);
    // lv_obj_align(ui_conn->apply_btn, LV_ALIGN_BOTTOM_MID, LV_DPX(80), -LV_DPX(30));
    lv_obj_align_to(ui_conn->apply_btn, ui_conn->pwd_ta, LV_ALIGN_OUT_BOTTOM_MID, LV_DPX(80), LV_DPX(30));
    lv_obj_add_event_cb(ui_conn->apply_btn, ls_wifi_connect_apply_btn_evt_handler, LV_EVENT_CLICKED, (void *)NULL);
    lv_obj_set_size(ui_conn->apply_btn, LV_DPX(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(ui_conn->apply_btn, lv_color_hex(0x5078D0), 0);
    lv_obj_set_style_radius(ui_conn->apply_btn, 8, 0);
    lv_obj_set_style_shadow_width(ui_conn->apply_btn, 0, 0);
    lv_obj_set_style_border_width(ui_conn->apply_btn, 0, 0);
    lv_obj_set_style_outline_width(ui_conn->apply_btn, 0, 0);

    lv_obj_t *label;
    label = lv_label_create(ui_conn->apply_btn);
    lv_label_set_text(label, "连接");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);

    ui_conn->cancel_btn = lv_btn_create(menu);
    // lv_obj_align(ui_conn->cancel_btn, LV_ALIGN_BOTTOM_MID, -LV_DPX(80), -LV_DPX(30));
    lv_obj_align_to(ui_conn->cancel_btn, ui_conn->pwd_ta, LV_ALIGN_OUT_BOTTOM_MID, -LV_DPX(80), LV_DPX(30));
    lv_obj_set_size(ui_conn->cancel_btn, LV_DPX(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(ui_conn->cancel_btn, lv_color_hex(0x24242d), 0);
    lv_obj_set_style_radius(ui_conn->cancel_btn, 8, 0);
    lv_obj_set_style_shadow_width(ui_conn->cancel_btn, 0, 0);
    lv_obj_set_style_border_width(ui_conn->cancel_btn, 0, 0);
    lv_obj_set_style_outline_width(ui_conn->cancel_btn, 0, 0);
    lv_obj_add_event_cb(ui_conn->cancel_btn, ls_wifi_connect_cancel_btn_evt_handler, LV_EVENT_CLICKED, (void *)NULL);

    label = lv_label_create(ui_conn->cancel_btn);
    lv_label_set_text(label, "取消");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);

    return menu;
}
