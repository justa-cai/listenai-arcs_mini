#include "lisa_ui.h"
#include "lisa_ui_img_label.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_ai_human_commu.h"
#include "stdio.h"
#include "stdlib.h"

struct lisa_ui_scr_ai_human {
    lisa_ui_scr_base_t obj;
    lv_obj_t *img;
    lv_obj_t *btn_switch;
    lv_obj_t *lbl_switch;
    lv_obj_t *lbl_comm;
    lv_obj_t *btn_mute;
    lv_obj_t *btn_cam;
    lv_obj_t *btn_mute_img;
    lv_obj_t *btn_cam_img;
    lv_obj_t *talk_cont;
    lv_obj_t *talk_txt;
    lv_obj_t *talk_anim;
    lv_obj_t *talk_tips;
    lv_obj_t *commu_cont;
    lv_obj_t *label_commu_me;
    lv_obj_t *label_commu_ai;
    lv_obj_t *emoji_anim;
    lv_timer_t *emoji_timer;
    uint8_t emoji_index;
    uint8_t emoji_play_pic_count;
    lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_t btn_mute_click_event_cb;
    lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_t btn_cam_click_event_cb;
    void *user_data_btn_mute;
    void *user_data_btn_cam;
    uint8_t is_show_human;

    bool gif;
};

typedef struct lisa_ui_scr_ai_human lisa_ui_scr_ai_human_t;

static void lisa_ui_scr_ai_human_commu_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_scr_ai_human_commu_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_ai_human_class = {
    .constructor_cb = lisa_ui_scr_ai_human_commu_constructor,
    .destructor_cb = lisa_ui_scr_ai_human_commu_destructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_ai_human_t),
};

#define MY_CLASS &lv_lisa_ui_scr_ai_human_class

lv_obj_t *lisa_ui_scr_ai_human_commu_btn_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_btn_create(parent);
    lv_obj_set_size(obj, 42, 36);
    lv_obj_set_style_radius(obj, 36, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFABE00), LV_PART_MAIN);

    return obj;
}

static void lisa_ui_scr_ai_human_commu_emoji_timer_cb(lv_timer_t *timer)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)timer->user_data;
    if (scr_ai_human->emoji_anim == NULL) {
        return;
    }

    lv_animimg_t *animimg = (lv_animimg_t *)scr_ai_human->emoji_anim;

    if (animimg->pic_count <= 0 || animimg->dsc == NULL) {
        return;
    }

    if (scr_ai_human->emoji_play_pic_count >= animimg->pic_count) {
        scr_ai_human->emoji_play_pic_count = 0;
        scr_ai_human->emoji_index = rand() % animimg->pic_count;
        /* 停在第一帧 */
        lv_img_set_src(scr_ai_human->emoji_anim, animimg->dsc[0]);
        /* 3~5s随机时间后重新开始 */
        uint32_t period = rand() % 2001 + 3000;
        lv_timer_set_period(scr_ai_human->emoji_timer, period);
        lv_timer_set_repeat_count(scr_ai_human->emoji_timer, 1);
        LOGI("emoji replay %d ms later, next idx: %d", period, scr_ai_human->emoji_index);
    } else {
        scr_ai_human->emoji_index++;
        scr_ai_human->emoji_index %= animimg->pic_count;
        scr_ai_human->emoji_play_pic_count++;
        lv_img_set_src(scr_ai_human->emoji_anim, animimg->dsc[scr_ai_human->emoji_index]);
        lv_timer_set_period(scr_ai_human->emoji_timer, 1000 / animimg->pic_count);
        lv_timer_set_repeat_count(scr_ai_human->emoji_timer, 1);
    }
}

static void lisa_ui_scr_ai_human_commu_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_timer_del(scr_ai_human->emoji_timer);
}

static void lisa_ui_scr_ai_human_commu_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    if (!scr_ai_human->gif) {
        scr_ai_human->img = lv_img_create(container);
    } else {
        scr_ai_human->img = lv_gif_create(container);
    }

    scr_ai_human->lbl_comm = lv_label_create(container);
    scr_ai_human->btn_mute = lisa_ui_scr_ai_human_commu_btn_create(container);
    scr_ai_human->btn_cam = lisa_ui_scr_ai_human_commu_btn_create(container);
    scr_ai_human->btn_mute_img = lv_img_create(scr_ai_human->btn_mute);
    scr_ai_human->btn_cam_img = lv_img_create(scr_ai_human->btn_cam);
    scr_ai_human->talk_cont = lv_img_create(container);
    scr_ai_human->talk_txt = lv_label_create(scr_ai_human->talk_cont);
    scr_ai_human->talk_anim = lv_animimg_create(container);
    scr_ai_human->talk_tips = lv_label_create(container);
    scr_ai_human->commu_cont = lv_obj_create(container);
    scr_ai_human->emoji_anim = lv_animimg_create(container);
    scr_ai_human->emoji_timer = lv_timer_create(lisa_ui_scr_ai_human_commu_emoji_timer_cb, 1000, scr_ai_human);
    lisa_ui_scr_ai_human_commu_label_new_context(obj);

    scr_ai_human->btn_switch = lv_obj_create(container);
}

static void lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb(lv_event_t *event)
{
    printf("lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb\n");
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)lv_event_get_user_data(event);
    if (scr_ai_human->btn_mute_click_event_cb) {
        scr_ai_human->btn_mute_click_event_cb(scr_ai_human->user_data_btn_mute);
    }
}

static void lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb(lv_event_t *event)
{
    printf("lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb\n");
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)lv_event_get_user_data(event);
    if (scr_ai_human->btn_cam_click_event_cb) {
        scr_ai_human->btn_cam_click_event_cb(scr_ai_human->user_data_btn_cam);
    }
}

static lv_obj_t *lisa_ui_scr_ai_human_commu_label_item_create(lv_obj_t *obj, bool is_main)
{
    int cnt = lv_obj_get_child_cnt(obj);
    if (cnt >= 10) {
        lv_obj_t *child = lv_obj_get_child(obj, 0);
        lv_obj_del(child);
    }

    lv_color_t c = is_main ? lv_color_hex(0x4F2814) : lv_color_hex(0x646464);
    lv_obj_t *l = lv_label_create(obj);
    lv_obj_set_style_text_font(l, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_width(l, LV_PCT(85));
    lv_obj_set_height(l, LV_SIZE_CONTENT);
    lv_label_set_text(l, "");

    return l;
}

static void lisa_ui_scr_ai_human_commu_btn_switch_click_event_cb(lv_event_t *event)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)lv_event_get_user_data(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (scr_ai_human->is_show_human) {
        lv_label_set_text(scr_ai_human->lbl_switch, "人物");

        lv_obj_add_flag(scr_ai_human->talk_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr_ai_human->img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(scr_ai_human->commu_cont, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(scr_ai_human->lbl_switch, "字幕");

        lv_obj_clear_flag(scr_ai_human->talk_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr_ai_human->img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(scr_ai_human->commu_cont, LV_OBJ_FLAG_HIDDEN);
    }
    scr_ai_human->is_show_human = !scr_ai_human->is_show_human;
}

lv_obj_t *lisa_ui_scr_ai_human_commu_create(lv_obj_t *parent, bool gif)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    scr_ai_human->gif = gif;
    lv_obj_class_init_obj(obj);

    lv_obj_align(scr_ai_human->btn_switch, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(scr_ai_human->btn_switch, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_ai_human->btn_switch, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_ai_human->btn_switch, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(scr_ai_human->btn_switch, lv_color_hex(0xFABE00), LV_PART_MAIN);
    scr_ai_human->lbl_switch = lv_label_create(scr_ai_human->btn_switch);
    lv_obj_set_style_text_font(scr_ai_human->lbl_switch, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_ai_human->lbl_switch, lv_color_hex(0x4F2814), LV_PART_MAIN);
    lv_obj_set_width(scr_ai_human->lbl_switch, LV_SIZE_CONTENT);
    lv_obj_set_height(scr_ai_human->lbl_switch, LV_SIZE_CONTENT);
    lv_obj_align(scr_ai_human->lbl_switch, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(scr_ai_human->lbl_switch, "字幕");
    lv_obj_set_style_pad_hor(scr_ai_human->btn_switch, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(scr_ai_human->btn_switch, 3, LV_PART_MAIN);
    lv_obj_add_event_cb(scr_ai_human->btn_switch, lisa_ui_scr_ai_human_commu_btn_switch_click_event_cb, LV_EVENT_CLICKED,
                        scr_ai_human);
    lv_obj_add_flag(scr_ai_human->btn_switch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(scr_ai_human->btn_switch, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    
    lv_obj_add_flag(scr_ai_human->btn_switch, LV_OBJ_FLAG_HIDDEN);


    scr_ai_human->is_show_human = true;

    /* 图片 */
    lv_obj_align(scr_ai_human->img, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_border_width(scr_ai_human->img, 0, LV_PART_MAIN);
    lv_obj_set_size(scr_ai_human->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_set_width(scr_ai_human->lbl_comm, LV_PCT(70));
    lv_obj_set_style_border_width(scr_ai_human->lbl_comm, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scr_ai_human->lbl_comm, 24, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_ai_human->lbl_comm, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(scr_ai_human->lbl_comm, LV_DPX(8), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(scr_ai_human->lbl_comm, LV_DPX(3), LV_PART_MAIN);

    lisa_ui_scr_ai_human_commu_label_set_tips(obj, "请说话");
    lv_obj_set_style_text_font(scr_ai_human->lbl_comm, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(scr_ai_human->lbl_comm, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_label_set_long_mode(scr_ai_human->lbl_comm, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // lv_obj_set_style_text_align(scr_ai_human->lbl_comm, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // lv_obj_add_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);

    /* 禁音按钮 */
    lv_obj_align(scr_ai_human->btn_mute, LV_ALIGN_LEFT_MID, 0, -30);
    lv_obj_align(scr_ai_human->btn_mute_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(scr_ai_human->btn_mute, lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb, LV_EVENT_CLICKED,
                        scr_ai_human);
    lv_obj_add_flag(scr_ai_human->btn_mute, LV_OBJ_FLAG_CLICKABLE);

    /* 拍照按钮 */
    lv_obj_align(scr_ai_human->btn_cam, LV_ALIGN_LEFT_MID, 0, 30);
    lv_obj_align(scr_ai_human->btn_cam_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(scr_ai_human->btn_cam, lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb, LV_EVENT_CLICKED,
                        scr_ai_human);
    lv_obj_add_flag(scr_ai_human->btn_cam, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_align(scr_ai_human->talk_txt, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(scr_ai_human->talk_txt, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_ai_human->talk_txt, lv_color_hex(0x4F2814), LV_PART_MAIN);

    lv_label_set_text(scr_ai_human->talk_tips, "请说话");
    lv_obj_set_size(scr_ai_human->talk_tips, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(scr_ai_human->talk_tips, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_ai_human->talk_tips, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(scr_ai_human->talk_tips, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(scr_ai_human->talk_tips, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_ai_human->talk_tips, lv_color_hex(0x4F2814), LV_PART_MAIN);
    lv_obj_align(scr_ai_human->talk_tips, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(scr_ai_human->talk_tips, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(scr_ai_human->talk_anim, LV_ALIGN_BOTTOM_LEFT, 5, -3);
    lv_animimg_set_duration(scr_ai_human->talk_anim, 2000);
    lv_animimg_set_repeat_count(scr_ai_human->talk_anim, 1);

    lv_obj_set_size(scr_ai_human->commu_cont, LV_PCT(100), LV_PCT(80));
    lv_obj_set_style_border_width(scr_ai_human->commu_cont, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(scr_ai_human->commu_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(scr_ai_human->commu_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_row(scr_ai_human->commu_cont, LV_DPX(15), LV_PART_MAIN);
    lv_obj_add_flag(scr_ai_human->commu_cont, LV_OBJ_FLAG_HIDDEN);

    return obj;
}


int lisa_ui_scr_ai_human_commu_img_set(lv_obj_t *obj, const void *img_path, const char *name)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    if (!scr_ai_human->gif) {
        lv_img_set_src(scr_ai_human->img, img_path);
    } else {
        lv_gif_set_src(scr_ai_human->img, img_path);
    }

    lv_obj_set_user_data(scr_ai_human->img, (void *)name);

    lv_obj_set_style_bg_color((lv_obj_t *)scr_ai_human, lv_color_hex(0xFFF6CC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa((lv_obj_t *)scr_ai_human, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(scr_ai_human->obj.bar, lv_color_hex(0xFFF6CC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_ai_human->obj.bar, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_clear_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_clear_flag(scr_ai_human->talk_tips, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scr_ai_human->talk_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scr_ai_human->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scr_ai_human->btn_mute, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scr_ai_human->btn_cam, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

int lisa_ui_scr_ai_human_commu_talk_bg_img_set(lv_obj_t *obj, const void *img)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_set_style_bg_img_src(scr_ai_human->talk_cont, img, LV_PART_MAIN);
    lv_obj_set_style_bg_img_opa(scr_ai_human->talk_cont, LV_OPA_COVER, LV_PART_MAIN);

    lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)img;
    lv_obj_set_size(scr_ai_human->talk_cont, img_dsc->header.w, img_dsc->header.h);
    lv_obj_align(scr_ai_human->talk_cont, LV_ALIGN_TOP_RIGHT, 0, 0);
}

int lisa_ui_scr_ai_human_commu_talk_txt_img_set(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->talk_txt, txt);
    lv_obj_set_style_text_font(scr_ai_human->talk_txt, &lv_font_chinese_18, LV_PART_MAIN);

    return 0;
}

int lisa_ui_scr_ai_human_commu_label_set(lv_obj_t *obj, const char *label)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->lbl_comm, label);

    return 0;
}

void lisa_ui_scr_ai_human_commu_label_me_set_txt(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->label_commu_me, txt);
}

void lisa_ui_scr_ai_human_commu_label_ai_set_txt(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->label_commu_ai, txt);
}

void lisa_ui_scr_ai_human_commu_label_new_context(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    scr_ai_human->label_commu_me = lisa_ui_scr_ai_human_commu_label_item_create(scr_ai_human->commu_cont, true);
    scr_ai_human->label_commu_ai = lisa_ui_scr_ai_human_commu_label_item_create(scr_ai_human->commu_cont, false);
}

void lisa_ui_scr_ai_human_commu_label_set_tips(lv_obj_t *obj, const char *tips)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->lbl_comm, tips);
    lv_obj_set_style_text_color(scr_ai_human->lbl_comm, lv_color_hex(0x955A42), LV_PART_MAIN);
    lv_obj_set_height(scr_ai_human->lbl_comm, LV_SIZE_CONTENT);
}

void lisa_ui_scr_ai_human_commu_label_set_normal(lv_obj_t *obj, const char *txt)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_label_set_text(scr_ai_human->lbl_comm, txt);
    lv_obj_set_style_text_color(scr_ai_human->lbl_comm, lv_color_hex(0x4F2814), LV_PART_MAIN);
}

void lisa_ui_scr_ai_human_commu_label_append_normal(lv_obj_t *obj, const char *label)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    char *txt = lv_label_get_text(scr_ai_human->lbl_comm);
    lv_label_set_text_fmt(scr_ai_human->lbl_comm, "%s%s", txt, label);
    lv_obj_set_style_bg_color(scr_ai_human->lbl_comm, lv_color_hex(0x4F2814), LV_PART_MAIN);
}

void lisa_ui_scr_ai_human_commu_label_show(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_clear_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_commu_label_hide(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_add_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_commu_btn_mute_img_set(lv_obj_t *obj, const void *img_path)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_img_set_src(scr_ai_human->btn_mute_img, img_path);
}

void lisa_ui_scr_ai_human_commu_btn_cam_img_set(lv_obj_t *obj, const void *img_path)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_img_set_src(scr_ai_human->btn_cam_img, img_path);
}

void lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set(lv_obj_t *obj, lv_color_t color)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_set_style_bg_color(scr_ai_human->btn_mute, color, LV_PART_MAIN);
}

void lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_set(lv_obj_t *obj,
                                                            lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_t cb,
                                                            void *user_data)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    scr_ai_human->btn_mute_click_event_cb = cb;
    scr_ai_human->user_data_btn_mute = user_data;
}

void lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_set(lv_obj_t *obj,
                                                           lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_t cb,
                                                           void *user_data)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    scr_ai_human->btn_cam_click_event_cb = cb;
    scr_ai_human->user_data_btn_cam = user_data;
}

void lisa_ui_scr_ai_human_commu_talk_anim_set(lv_obj_t *obj, const void *dsc[], uint8_t num)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_animimg_set_src(scr_ai_human->talk_anim, dsc, num);
    lv_img_set_src(scr_ai_human->talk_anim, dsc[0]);
    lv_obj_align(scr_ai_human->talk_anim, LV_ALIGN_BOTTOM_LEFT, 5, -3);
    lv_animimg_set_duration(scr_ai_human->talk_anim, 1000);
    lv_animimg_set_repeat_count(scr_ai_human->talk_anim, 1);
}

void lisa_ui_scr_ai_human_commu_talk_anim_start(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_animimg_start(scr_ai_human->talk_anim);
    lv_obj_clear_flag(scr_ai_human->talk_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->emoji_anim, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_commu_talk_anim_stop(lv_obj_t *obj)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    lv_obj_add_flag(scr_ai_human->talk_anim, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_scr_ai_human_commu_talk_emoji_anim_set(lv_obj_t *obj, const void *dsc[], uint8_t num)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;

    // lv_animimg_t * animimg = (lv_animimg_t *)scr_ai_human->talk_anim;
    // if (animimg->dsc == dsc) {
    //     LOGI("dsc is same, dsc: %p", dsc);
    //     return;
    // }
    // LOGI("dsc is not same, dsc: %p", dsc);
    lv_animimg_set_src(scr_ai_human->emoji_anim, dsc, num);
    lv_img_set_src(scr_ai_human->emoji_anim, dsc[0]);
    lv_obj_align(scr_ai_human->emoji_anim, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(scr_ai_human->lbl_comm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->talk_tips, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->talk_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->btn_mute, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->btn_cam, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr_ai_human->talk_anim, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_bg_color((lv_obj_t *)scr_ai_human, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa((lv_obj_t *)scr_ai_human, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(scr_ai_human->obj.bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_ai_human->obj.bar, LV_OPA_COVER, LV_PART_MAIN);
}

void lisa_ui_scr_ai_human_commu_talk_emoji_anim_start(lv_obj_t *obj, uint32_t time)
{
    lisa_ui_scr_ai_human_t *scr_ai_human = (lisa_ui_scr_ai_human_t *)obj;
    // lv_animimg_set_duration(scr_ai_human->emoji_anim, time);
    // lv_animimg_set_repeat_count(scr_ai_human->emoji_anim, LV_ANIM_REPEAT_INFINITE);
    // lv_animimg_start(scr_ai_human->emoji_anim);
    lv_animimg_t * animimg = (lv_animimg_t *)scr_ai_human->emoji_anim;
    scr_ai_human->emoji_play_pic_count = 0;
    scr_ai_human->emoji_index = rand() % animimg->pic_count;
    LOGI("emoji play start, next idx: %d", scr_ai_human->emoji_index);
    lv_img_set_src(scr_ai_human->emoji_anim, animimg->dsc[scr_ai_human->emoji_index]);
    lv_timer_set_period(scr_ai_human->emoji_timer, 1000 / animimg->pic_count);
    lv_timer_set_repeat_count(scr_ai_human->emoji_timer, 1);
    lv_obj_clear_flag(scr_ai_human->emoji_anim, LV_OBJ_FLAG_HIDDEN);
}

