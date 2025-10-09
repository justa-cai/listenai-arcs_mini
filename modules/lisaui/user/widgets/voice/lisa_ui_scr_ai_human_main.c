#include "lisa_ui.h"
#include "lisa_ui_imgs_group.h"
#include "lisa_ui_scr_ai_human_main.h"
#include "stdio.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"

struct lisa_ui_scr_ai_human {
    lisa_ui_scr_base_t base;
    lv_obj_t *loading_anim;
    lv_obj_t *tips;
    lv_obj_t *btn;
    lv_obj_t *imgs_cont;
    lisa_ui_scr_ai_human_main_imgs_click_event_cb_t imgs_click_event_cb;
    lisa_ui_scr_ai_human_main_btn_click_event_cb_t btn_click_event_cb;
    lv_style_t imags_style_pressed;
    void *user_data;
};

typedef struct lisa_ui_scr_ai_human lisa_ui_scr_ai_human_t;

static void lisa_ui_scr_ai_human_main_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lv_lisa_ui_scr_ai_human_main_class = {
    .constructor_cb = lisa_ui_scr_ai_human_main_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_ai_human_t),
};

#define MY_CLASS &lv_lisa_ui_scr_ai_human_main_class

static void lisa_ui_scr_ai_human_main_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    scr_ai_human->loading_anim = lv_animimg_create(container);
    scr_ai_human->tips = lv_label_create(container);
    scr_ai_human->btn = lv_btn_create(container);
    scr_ai_human->imgs_cont = lv_obj_create(container);

    lv_style_init(&scr_ai_human->imags_style_pressed);
    lv_style_set_img_recolor(&scr_ai_human->imags_style_pressed, lv_color_black());  // 设置重新着色为黑色
    lv_style_set_img_recolor_opa(&scr_ai_human->imags_style_pressed, 40);  // 设置重新着色的不透明度
}

static void lisa_ui_imgs_group_click_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)lv_event_get_user_data(event);

    if (code == LV_EVENT_CLICKED) {
        printf("lisa_ui_imgs_group_click_event_cb, code :%d, obj:%p\r\n", code, obj);
        // 获取点击的图片的名称
        const char *name = lv_obj_get_user_data(obj);
        if (scr_ai_human->imgs_click_event_cb && name) {
            scr_ai_human->imgs_click_event_cb(scr_ai_human->user_data, name);
        }
    }
}

static const void *loading_anim[] = {
    &ic_comm_loading_synth0,
    &ic_comm_loading_synth1,
    &ic_comm_loading_synth2,
    &ic_comm_loading_synth3,
    &ic_comm_loading_synth4,
    &ic_comm_loading_synth5,
    &ic_comm_loading_synth6,
    &ic_comm_loading_synth7,
    &ic_comm_loading_synth8,
    &ic_comm_loading_synth9,
    &ic_comm_loading_synth10,
    &ic_comm_loading_synth11,
    &ic_comm_loading_synth12,
    &ic_comm_loading_synth13,
    &ic_comm_loading_synth14,
    &ic_comm_loading_synth15,
    &ic_comm_loading_synth16,
    &ic_comm_loading_synth17,
    &ic_comm_loading_synth18,
    &ic_comm_loading_synth19,
    &ic_comm_loading_synth20,
    &ic_comm_loading_synth21,
    &ic_comm_loading_synth22,
    &ic_comm_loading_synth23,
    &ic_comm_loading_synth24,
    &ic_comm_loading_synth25,
    &ic_comm_loading_synth26,
    &ic_comm_loading_synth27,
    &ic_comm_loading_synth28,
    &ic_comm_loading_synth29,
};

lv_obj_t *lisa_ui_scr_ai_human_main_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    lv_animimg_set_src(scr_ai_human->loading_anim, loading_anim, sizeof(loading_anim) / sizeof(loading_anim[0]));
    lv_obj_align(scr_ai_human->loading_anim, LV_ALIGN_CENTER, 0, 0);
    lv_animimg_set_duration(scr_ai_human->loading_anim, 1000);
    lv_animimg_set_repeat_count(scr_ai_human->loading_anim, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(scr_ai_human->loading_anim);

    // lv_obj_add_flag(scr_ai_human->loading_anim, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(scr_ai_human->tips, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(scr_ai_human->tips, &lv_font_chinese_18, 0);
    lv_label_set_text(scr_ai_human->tips, "智能体获取中");
    lv_obj_set_style_text_align(scr_ai_human->tips, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(scr_ai_human->btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // lv_obj_add_flag(scr_ai_human->tips, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(scr_ai_human->btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(scr_ai_human->btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_size(scr_ai_human->imgs_cont, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_align(scr_ai_human->imgs_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(scr_ai_human->imgs_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(scr_ai_human->imgs_cont, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_ai_human->imgs_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(scr_ai_human->imgs_cont, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_flex_flow(scr_ai_human->imgs_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scr_ai_human->imgs_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(scr_ai_human->imgs_cont, LV_OBJ_FLAG_HIDDEN);

    return obj;
}

void lisa_ui_scr_ai_human_main_show_tips(lv_obj_t *obj, const char *tips)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->tips, tips);
    lv_obj_clear_flag(scr_ai_human->tips, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_main_hide_tips(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_add_flag(scr_ai_human->tips, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_main_show_btn(lv_obj_t *obj, const char *btn_text)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_clear_flag(scr_ai_human->btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->tips, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(scr_ai_human->btn, btn_text);
}

static void lisa_ui_scr_ai_human_main_btn_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_CLICKED) {
        lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)lv_event_get_user_data(event);
        if (scr_ai_human->btn_click_event_cb) {
            scr_ai_human->btn_click_event_cb(scr_ai_human->user_data);
        }
    }
}

void lisa_ui_scr_ai_human_main_btn_click_event_cb_set(lv_obj_t *obj, lisa_ui_scr_ai_human_main_btn_click_event_cb_t cb,
                                                      void *user_data)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    scr_ai_human->btn_click_event_cb = cb;
    scr_ai_human->user_data = user_data;
    lv_obj_add_event_cb(scr_ai_human->btn, lisa_ui_scr_ai_human_main_btn_event_cb, LV_EVENT_CLICKED, scr_ai_human);
}

void lisa_ui_scr_ai_human_main_imgs_clear(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    lisa_ui_imgs_group_clear(scr_ai_human->imgs_cont);
}

void lisa_ui_scr_ai_human_main_imgs_add(lv_obj_t *obj, const void *img_path, const char *name)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    lv_obj_t *img = lv_img_create(scr_ai_human->imgs_cont);
    lv_img_set_src(img, img_path);
    lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(img, 15, LV_PART_MAIN);
    lv_obj_set_user_data(img, (void *)name);
    lv_obj_add_event_cb(img, lisa_ui_imgs_group_click_event_cb, LV_EVENT_CLICKED, scr_ai_human);

    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(img, &scr_ai_human->imags_style_pressed, LV_STATE_PRESSED);

    lv_obj_add_flag(scr_ai_human->loading_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->tips, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(scr_ai_human->imgs_cont, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_main_imgs_click_event_cb_set(lv_obj_t *obj,
                                                       lisa_ui_scr_ai_human_main_imgs_click_event_cb_t cb,
                                                       void *user_data)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    if (cb == NULL) {
        return;
    }
    scr_ai_human->imgs_click_event_cb = cb;
    scr_ai_human->user_data = user_data;
}

void lisa_ui_scr_ai_human_main_imgs_click_event_cb_remove(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_remove_event_cb(scr_ai_human->imgs_cont, lisa_ui_imgs_group_click_event_cb);
}

// void lisa_ui_scr_ai_human_main_imgs_next_show(lv_obj_t *obj)
// {
//     lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
//     lisa_ui_imgs_group_show_next(scr_ai_human->imgs);
// }

// void lisa_ui_scr_ai_human_main_imgs_prev_show(lv_obj_t *obj)
// {
//     lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
//     lisa_ui_imgs_group_show_prev(scr_ai_human->imgs);
// }
