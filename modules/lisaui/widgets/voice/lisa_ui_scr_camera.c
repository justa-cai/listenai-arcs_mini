#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_camera.h"

struct lisa_ui_scr_camera {
    struct lisa_ui_scr_base base;
    lv_obj_t *btn_shut;
    lv_obj_t *label_back;
    lv_obj_t *img_rec;
    lv_obj_t *img;
    lisa_ui_scr_camera_btn_shut_click_event_cb_t btn_shut_click_event_cb;
    void *btn_shut_user_data;
    lisa_ui_scr_camera_btn_back_click_event_cb_t btn_back_click_event_cb;
    void *btn_back_user_data;
};
typedef struct lisa_ui_scr_camera lisa_ui_scr_camera_t;

static void lisa_ui_scr_camera_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_camera_class = {
    .constructor_cb = lisa_ui_scr_camera_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_camera_t),
};
#define MY_CLASS &lv_lisa_ui_scr_camera_class

static void lisa_ui_scr_camera_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;

    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    scr_camera->img = lv_img_create(container);
    scr_camera->btn_shut = lv_btn_create(container);
    scr_camera->img_rec = lv_img_create(container);
    scr_camera->label_back = lv_label_create(container);
}

lv_obj_t *lisa_ui_scr_camera_draw_line(lv_obj_t *parent, const lv_point_t points[], uint16_t point_num)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, point_num);
    lv_obj_set_style_line_color(line, lv_color_hex(0xFED300), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);

    return line;
}

static void lisa_ui_scr_camera_btn_shut_click_event_cb(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target(event);
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)lv_event_get_user_data(event);
    if(scr_camera->btn_shut_click_event_cb != NULL) {
        scr_camera->btn_shut_click_event_cb(scr_camera->btn_shut_user_data);
    }
}

static void lisa_ui_scr_camera_btn_back_click_event_cb(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target(event);
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)lv_event_get_user_data(event);
    if(scr_camera->btn_back_click_event_cb != NULL) {
        scr_camera->btn_back_click_event_cb(scr_camera->btn_back_user_data);
    }
}

lv_obj_t *lisa_ui_scr_camera_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;

    lv_obj_align(scr_camera->img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(scr_camera->img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_camera->img, 0, LV_PART_MAIN);

    lv_obj_align(scr_camera->img_rec, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_opa(scr_camera->img_rec, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_align(scr_camera->btn_shut, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(scr_camera->btn_shut, LV_DPX(42), LV_DPX(42));
    lv_obj_set_style_radius(scr_camera->btn_shut, 36, LV_PART_MAIN);
    lv_obj_set_style_bg_color(scr_camera->btn_shut, lv_color_hex(0xFED300), LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_camera->btn_shut, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(scr_camera->btn_shut, lv_color_hex(0xCE861A), LV_PART_MAIN);
    lv_obj_add_flag(scr_camera->btn_shut, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_camera->btn_shut, lisa_ui_scr_camera_btn_shut_click_event_cb, LV_EVENT_CLICKED, scr_camera);

    lv_label_set_text(scr_camera->label_back, LV_SYMBOL_LEFT);
    lv_obj_align(scr_camera->label_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(scr_camera->label_back, LV_DPX(10), LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_camera->label_back, lv_color_hex(0xFED300), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_camera->label_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_camera->label_back, 0, LV_PART_MAIN);
    lv_obj_add_flag(scr_camera->label_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_camera->label_back, lisa_ui_scr_camera_btn_back_click_event_cb, LV_EVENT_CLICKED, scr_camera);
    lv_obj_set_style_text_color(scr_camera->label_back, lv_color_darken(lv_color_hex(0xFED300), 50), LV_STATE_PRESSED);

    // lisa_ui_scr_base_bar_hide(obj);

    return obj;
}

void lisa_ui_scr_camera_img_set(lv_obj_t *obj, const void *img_data)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;

    lv_img_set_src(scr_camera->img, img_data);
    lv_obj_set_style_bg_img_opa(scr_camera->img, LV_OPA_COVER, LV_PART_MAIN);
}

void lisa_ui_scr_camera_img_rotate(lv_obj_t *obj, int angle)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;
    lv_img_set_angle(scr_camera->img, angle);
}

const void *lisa_ui_scr_camera_img_get(lv_obj_t *obj)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;
    return lv_obj_get_style_bg_img_src(scr_camera->img, LV_PART_MAIN);
}

void lisa_ui_scr_camera_rec_img_set(lv_obj_t *obj, const void *img_data)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;
    lv_img_set_src(scr_camera->img_rec, img_data);
}

void lisa_ui_scr_camera_btn_shut_click_event_cb_set(lv_obj_t *obj, lisa_ui_scr_camera_btn_shut_click_event_cb_t cb,
                                                    void *user_data)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;
    scr_camera->btn_shut_click_event_cb = cb;
    scr_camera->btn_shut_user_data = user_data;
}

void lisa_ui_scr_camera_btn_back_click_event_cb_set(lv_obj_t *obj, lisa_ui_scr_camera_btn_back_click_event_cb_t cb,
                                                    void *user_data)
{
    lisa_ui_scr_camera_t *scr_camera = (lisa_ui_scr_camera_t *)obj;
    scr_camera->btn_back_click_event_cb = cb;
    scr_camera->btn_back_user_data = user_data;
}
