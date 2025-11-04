#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_setting_net.h"
#include "lisa_ui_scr_kb_input.h"
#include "stdio.h"
#include "lisa_ui_toast.h"
#include "stdio.h"

#define TAG "ui.setting.net"

static void lisa_ui_wifi_item_connecting(lv_obj_t *obj);

struct lisa_ui_scr_setting_net {
    lisa_ui_scr_base_t base;
    lv_obj_t *list_mine;
    lv_obj_t *list_other;
    lv_obj_t *curr_item;
    lv_obj_t *cont_switch;
    lv_obj_t *label_mine;
    lv_obj_t *label_other;
    bool connecting;
};

typedef struct lisa_ui_scr_setting_net lisa_ui_scr_setting_net_t;

static void lisa_ui_scr_setting_net_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_setting_net_class = {
    .constructor_cb = lisa_ui_scr_setting_net_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_setting_net_t),
};

#define MY_CLASS &lv_lisa_ui_scr_setting_net_class

enum {
    LISA_UI_WIFI_LIST_ITEM_STATE_NONE = 0,
    LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTING,
    LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTED,
};

typedef struct {
    lv_obj_t obj;
    lv_obj_t *status_anim;
    lv_obj_t *lbl;
    lv_obj_t *img;
    bool encrypted;
    lisa_ui_scr_setting_net_wifi_connecting_cb_t cb;
    void *user_data;
    uint8_t state;
    lv_obj_t *scr;
} lisa_ui_wifi_list_item_t;

const lv_obj_class_t lv_lisa_ui_wifi_list_item_class = {
    .base_class = &lv_btn_class,
    .instance_size = sizeof(lisa_ui_wifi_list_item_t),
};

const void *ic_wifi_connecting[] = {
    &loading_00,
    &loading_01,
    &loading_02,
    &loading_03,
    &loading_04,
    &loading_05,
    &loading_06,
    &loading_07,
    &loading_08,
    &loading_10,
    &loading_11,
    &loading_12,
    &loading_13,
    &loading_14,
};

const void *ic_wifi_connected[] = {
    &ic_ok,
};

const void *ic_wifi_encrypted_list[] = {
    &ic_wifi_list_0,
    &ic_wifi_list_1,
    &ic_wifi_list_2,
    &ic_wifi_list_3,
};

static void lisa_ui_scr_setting_net_switch_event_cb(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target(event);
    lv_event_code_t code = lv_event_get_code(event);
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)lv_event_get_user_data(event);
    uint32_t state = lv_obj_get_state(obj);
    
    if (code == LV_EVENT_VALUE_CHANGED) { 
        
        lv_event_send((lv_obj_t*)scr_setting_net, LV_EVENT_VALUE_CHANGED, &state);
    }
}

static void lisa_ui_scr_setting_net_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(container, 1);

    lv_obj_t *cont = lv_obj_create(container);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);

    lv_obj_t *cont_label = lv_label_create(cont);
    lv_obj_align(cont_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(cont_label, "无线局域网");
    lv_obj_set_style_text_font(cont_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_t *cont_switch = lv_switch_create(cont);
    lv_obj_align(cont_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    // lv_obj_add_state(cont_switch, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(cont_switch, lv_color_hex(0xFABE00), LV_PART_INDICATOR);
    lv_obj_add_event_cb(cont_switch, lisa_ui_scr_setting_net_switch_event_cb, LV_EVENT_VALUE_CHANGED, obj);
    scr_setting_net->cont_switch = cont_switch;

    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, "我的");
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN);
    scr_setting_net->label_mine = label;

    scr_setting_net->list_mine = lv_list_create(container);
    lv_obj_set_size(scr_setting_net->list_mine, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(scr_setting_net->list_mine, LV_OPA_60, LV_PART_MAIN);
    // lv_obj_set_style_border_width(scr_setting_net->list_mine, 0, LV_PART_MAIN);

    label = lv_label_create(container);
    lv_label_set_text(label, "其他");
    scr_setting_net->label_other = label;

    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN);
    scr_setting_net->list_other = lv_list_create(container);
    lv_obj_set_size(scr_setting_net->list_other, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(scr_setting_net->list_other, LV_OPA_60, LV_PART_MAIN);
    // lv_obj_set_style_border_width(scr_setting_net->list_other, 0, LV_PART_MAIN);

    scr_setting_net->connecting = false;
}

static void lisa_ui_scr_kb_input_ready_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        lv_obj_t *obj = lv_event_get_target(event);
        const char *password = lisa_ui_scr_kb_input_get_text(obj);
        if (password == NULL || strlen(password) < 8) {
            lisa_ui_toast_show("密码长度必须大于8");
            return;
        }
        lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)lv_event_get_user_data(event);
        item->cb(lv_label_get_text(item->lbl), password, item->encrypted, false, item->user_data);
        lisa_ui_wifi_item_connecting((lv_obj_t *)item);
        lv_obj_del(obj);
        lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)item->scr;
        scr_setting_net->connecting = false;
        LOGI("lisa_ui_scr_kb_input_ready_event_cb, connecting:%d\n",scr_setting_net->connecting);
    }
}

static void lisa_ui_scr_kb_input_cancel_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CANCEL) {
        lv_obj_t *obj = lv_event_get_target(event);
        lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)lv_event_get_user_data(event);
        lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)item->scr;
        scr_setting_net->connecting = false;
        lv_obj_del(obj);
    }
}

static void lisa_ui_wifi_list_item_click_event_cb(lv_event_t *event)
{
    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)lv_event_get_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code != LV_EVENT_CLICKED) {
        return;
    }

    if (item->state == LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTING ||
        item->state == LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTED) {
        return;
    }

    lv_obj_t *list = lv_obj_get_parent((lv_obj_t *)item);
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)item->scr;

    scr_setting_net->connecting = true;
    LOGI("lisa_ui_wifi_list_item_click_event_cb, connecting:%d\n",scr_setting_net->connecting);

    if (list == scr_setting_net->list_mine) {
        item->cb(lv_label_get_text(item->lbl), NULL, item->encrypted, true, item->user_data);
        scr_setting_net->connecting = false;
        LOGI("lisa_ui_wifi_list_item_click_event_cb, list_mine, connecting:%d\n",scr_setting_net->connecting);
    } else {
        if (!item->encrypted) {
            item->cb(lv_label_get_text(item->lbl), NULL, false, false, item->user_data);
            scr_setting_net->connecting = false;
            LOGI("lisa_ui_wifi_list_item_click_event_cb, list_other, connecting:%d\n",scr_setting_net->connecting);
        } else {
            lv_obj_t *scr_kb_input = lisa_ui_scr_kb_input_create();
            const char *title = lv_label_get_text(item->lbl);
            lisa_ui_scr_kb_input_set_title(scr_kb_input, title);
            lisa_ui_scr_kb_input_set_tips(scr_kb_input, "请输入WiFi密码");
            lv_obj_add_event_cb(scr_kb_input, lisa_ui_scr_kb_input_ready_event_cb, LV_EVENT_READY, item);
            lv_obj_add_event_cb(scr_kb_input, lisa_ui_scr_kb_input_cancel_event_cb, LV_EVENT_CANCEL, item);
        }
    }
}

static lv_obj_t *lisa_ui_wifi_list_item_add(lv_obj_t *list, const char *txt, bool encrypted,uint8_t rssi_level,uint8_t connect_state,
                                            lisa_ui_scr_setting_net_wifi_connecting_cb_t cb, void *user_data)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_lisa_ui_wifi_list_item_class, list);
    lv_obj_class_init_obj(obj);

    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xCCCCCC), LV_PART_MAIN);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_border_side(&style, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&style, 1);
    lv_style_set_border_color(&style, lv_color_hex(0xFABE00));
    lv_obj_add_style(obj, &style, LV_STATE_PRESSED);

    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)obj;
    item->cb = cb;
    item->encrypted = encrypted;
    item->user_data = user_data;
    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_START, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_END, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    item->lbl = lv_label_create(obj);
    // lv_obj_set_size(item->lbl, LV_PCT(80), LV_SIZE_CONTENT);
    lv_label_set_text(item->lbl, txt);
    lv_label_set_long_mode(item->lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(item->lbl, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_flex_grow(item->lbl, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_right(item->lbl, 5, LV_PART_MAIN);

    uint8_t level = rssi_level >= sizeof(ic_wifi_encrypted_list) / sizeof(ic_wifi_encrypted_list[0])
                        ? sizeof(ic_wifi_encrypted_list) / sizeof(ic_wifi_encrypted_list[0]) - 1
                        : rssi_level;
    item->status_anim = lv_animimg_create(obj);
    lv_img_set_src(item->status_anim, ic_wifi_encrypted_list[level]);
    lv_obj_set_style_pad_ver(item->status_anim, 5, LV_PART_MAIN);


    item->img = lv_img_create(obj);
    const void *icon = encrypted ? &ic_wifi_lock_1 : &ic_wifi_list_1;
    lv_img_set_src(item->img, icon);
    lv_obj_set_size(item->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(item->img, 5, LV_PART_MAIN);
    lv_obj_add_flag(item->img, LV_OBJ_FLAG_HIDDEN);

    // lv_obj_add_flag(item->status_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb((lv_obj_t *)item, lisa_ui_wifi_list_item_click_event_cb, LV_EVENT_CLICKED, obj);
    lv_obj_add_flag((lv_obj_t *)item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data((lv_obj_t *)item, lv_obj_get_parent(list));
    item->state = LISA_UI_WIFI_LIST_ITEM_STATE_NONE;

    return obj;
}

static lv_obj_t *lisa_ui_wifi_list_item_get_by_txt(lv_obj_t *list, const char *txt)
{
    int cnt = lv_obj_get_child_cnt(list);
    for (int i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(list, i);
        lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)child;
        if (strcmp(lv_label_get_text(item->lbl), txt) == 0) {
            return child;
        }
    }

    return NULL;
}

static const char *lisa_ui_wifi_list_item_get_txt(lv_obj_t *obj)
{
    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)obj;
    return lv_label_get_text(item->lbl);
}

static void lisa_ui_wifi_list_item_cb(const char *ssid, const char *pwd, bool is_encrypted, bool is_mine,
                                      void *user_data)
{
    LOGI("ssid: %s, pwd: %s, is_encrypted: %d, is_mine: %d\n", ssid, pwd, is_encrypted, is_mine);
}

lv_obj_t *lisa_ui_scr_setting_net_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    // lisa_ui_wifi_item_add_to_list_mine(obj, "WiFi1", false, lisa_ui_wifi_list_item_cb, NULL);
    // lisa_ui_wifi_item_add_to_list_mine(obj, "WiFi2", true, lisa_ui_wifi_list_item_cb, NULL);
    // lisa_ui_wifi_item_add_to_list_mine(obj, "WiFi3", false, lisa_ui_wifi_list_item_cb, NULL);

    // lisa_ui_scr_setting_net_wifi_item_connecting(obj, "WiFi1");
    // lisa_ui_scr_setting_net_wifi_item_connected(obj, "WiFi2");

    // for (int i = 0; i < 10; i++) {
    //     lisa_ui_wifi_item_add_to_list_other(obj, "WiFi", true, lisa_ui_wifi_list_item_cb, NULL);
    // }

    return obj;
}

lv_obj_t *lisa_ui_wifi_item_add_to_list_other(lv_obj_t *obj, const char *txt, bool encrypted,uint8_t rssi_level,uint8_t connect_state,
                                              lisa_ui_scr_setting_net_wifi_connecting_cb_t cb, void *user_data)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    if (scr_setting_net->connecting) {
        return NULL;
    }

    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_add(scr_setting_net->list_other, txt, encrypted,rssi_level,connect_state, cb, user_data);
    item->scr = obj;

    return (lv_obj_t *)item;
}

lv_obj_t *lisa_ui_wifi_item_add_to_list_mine(lv_obj_t *obj, const char *txt, bool encrypted,uint8_t rssi_level,uint8_t connect_state,
                                             lisa_ui_scr_setting_net_wifi_connecting_cb_t cb, void *user_data)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    if (scr_setting_net->connecting) {
        return NULL;
    }

    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_add(scr_setting_net->list_mine, txt, encrypted, rssi_level,
                                                                connect_state, cb, user_data);

    lv_obj_clear_flag((lv_obj_t *)item, LV_OBJ_FLAG_CLICKABLE);

    item->scr = obj;

    return (lv_obj_t *)item;
}

static void lisa_ui_wifi_item_connecting(lv_obj_t *obj)
{
    lisa_ui_wifi_list_item_t *item = (lisa_ui_wifi_list_item_t *)obj;
    lv_animimg_set_src(item->status_anim, ic_wifi_connecting,
                       sizeof(ic_wifi_connecting) / sizeof(ic_wifi_connecting[0]));
    lv_animimg_set_duration(item->status_anim, 1000);
    lv_animimg_set_repeat_count(item->status_anim, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(item->status_anim);
    item->state = LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTING;
}

void lisa_ui_scr_setting_net_wifi_item_connecting(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;
    lisa_ui_wifi_list_item_t *item =
        (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_mine, txt);
    if (item == NULL) {
        item = (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_other, txt);
    }

    if (item == NULL) {
        return;
    }

    lisa_ui_wifi_item_connecting((lv_obj_t *)item);
    lv_obj_set_parent((lv_obj_t *)item, scr_setting_net->list_mine);
    lv_obj_move_to_index((lv_obj_t *)item, 0);
}

void lisa_ui_scr_setting_net_wifi_item_connected(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;
    lisa_ui_wifi_list_item_t *item =
        (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_mine, txt);

    if (item == NULL) {
        item = (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_other, txt);
    }

    if (item == NULL) {
        return;
    }

    lisa_ui_wifi_list_item_t *prev = (lisa_ui_wifi_list_item_t *)lv_obj_get_child(scr_setting_net->list_mine, 0);

    if (prev != NULL) {
        lv_obj_add_flag(prev->status_anim, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_parent((lv_obj_t *)item, scr_setting_net->list_mine);
    lv_obj_move_to_index((lv_obj_t *)item, 0);

    lv_animimg_set_repeat_count(item->status_anim, 1);
    lv_animimg_set_duration(item->status_anim, 100);
    lv_animimg_set_src(item->status_anim, ic_wifi_connected, sizeof(ic_wifi_connected) / sizeof(ic_wifi_connected[0]));
    lv_animimg_start(item->status_anim);
    lv_obj_clear_flag(item->status_anim, LV_OBJ_FLAG_HIDDEN);
    item->state = LISA_UI_WIFI_LIST_ITEM_STATE_CONNECTED;
}

void lisa_ui_scr_setting_net_wifi_item_connecting_failed(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;
    lisa_ui_wifi_list_item_t *item =
        (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_mine, txt);
    if (item == NULL) {
        item = (lisa_ui_wifi_list_item_t *)lisa_ui_wifi_list_item_get_by_txt(scr_setting_net->list_other, txt);
    }

    if (item == NULL) {
        return;
    }

    lv_animimg_set_repeat_count(item->status_anim, 1);
    lv_animimg_set_duration(item->status_anim, 100);
    lv_animimg_set_src(item->status_anim, ic_wifi_connected, sizeof(ic_wifi_connected) / sizeof(ic_wifi_connected[0]));
    lv_animimg_start(item->status_anim);
    scr_setting_net->curr_item = NULL;
}

void lisa_ui_scr_setting_net_wifi_list_clear(lv_obj_t *obj)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    if (scr_setting_net->connecting) {
        return;
    }

    lv_obj_clean(scr_setting_net->list_mine);
    lv_obj_clean(scr_setting_net->list_other);
}

int lisa_ui_scr_setting_net_switch_add_state(const lv_obj_t *obj, lv_state_t state)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    if (state & LV_STATE_CHECKED) {
        lv_obj_clear_flag(scr_setting_net->label_mine, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr_setting_net->list_mine, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr_setting_net->label_other, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr_setting_net->list_other, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_state(scr_setting_net->cont_switch, state);
}

int lisa_ui_scr_setting_net_switch_clear_state(const lv_obj_t *obj, lv_state_t state)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    if (state & LV_STATE_CHECKED) {
        lv_obj_add_flag(scr_setting_net->label_mine, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr_setting_net->list_mine, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr_setting_net->label_other, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr_setting_net->list_other, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_state(scr_setting_net->cont_switch, state);
}

bool lisa_ui_scr_setting_net_is_connecting(lv_obj_t *obj)
{
    lisa_ui_scr_setting_net_t *scr_setting_net = (lisa_ui_scr_setting_net_t *)obj;

    LOGI("lisa_ui_scr_setting_net_is_connecting, connecting:%d\n",scr_setting_net->connecting);

    return scr_setting_net->connecting;
}