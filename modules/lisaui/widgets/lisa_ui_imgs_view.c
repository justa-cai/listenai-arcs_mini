#include "lvgl.h"

struct lisa_ui_imgs_view {
    lv_obj_t obj;
    lv_obj_t *img;
    lv_obj_t *lbl;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    lv_obj_t *lbl_status;
    void **srcs;
    char **names;
    uint32_t srcs_cnt;
    int32_t idx;
};

typedef struct lisa_ui_imgs_view lisa_ui_imgs_view_t;
static void lisa_ui_imgs_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lv_lisa_ui_imgs_view_class = {
    .constructor_cb = lisa_ui_imgs_view_constructor,
    .base_class = &lv_obj_class,
    .instance_size = sizeof(lisa_ui_imgs_view_t),
};

#define MY_CLASS &lv_lisa_ui_imgs_view_class

static void lisa_ui_imgs_view_status_update(lv_obj_t *obj)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    if (imgs_view->srcs_cnt == 0) {
        lv_label_set_text(imgs_view->lbl_status, "0/0");
        return;
    }

    lv_label_set_text_fmt(imgs_view->lbl_status, "%d/%d", imgs_view->idx + 1, imgs_view->srcs_cnt);
}

static void lisa_ui_imgs_view_set_idx(lv_obj_t *obj, int32_t idx)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    if (imgs_view->srcs_cnt == 0 || imgs_view->srcs == NULL || imgs_view->names == NULL) {
        return;
    }

    imgs_view->idx = idx;

    if (imgs_view->idx >= imgs_view->srcs_cnt) {
        imgs_view->idx = 0;
    }

    if (imgs_view->idx < 0) {
        imgs_view->idx = imgs_view->srcs_cnt - 1;
    }

    // lv_img_set_src(imgs_view->img, imgs_view->srcs[imgs_view->idx]);
    lv_label_set_text(imgs_view->lbl, imgs_view->names[imgs_view->idx]);

    lisa_ui_imgs_view_status_update(obj);
}

static void lisa_ui_imgs_view_prev(lv_obj_t *obj)
{
    lisa_ui_imgs_view_set_idx(obj, ((lisa_ui_imgs_view_t *)obj)->idx - 1);
}

static void lisa_ui_imgs_view_next(lv_obj_t *obj)
{
    lisa_ui_imgs_view_set_idx(obj, ((lisa_ui_imgs_view_t *)obj)->idx + 1);
}

static void lisa_ui_imgs_view_clear(lv_obj_t *obj)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    imgs_view->srcs = NULL;
    imgs_view->names = NULL;
    imgs_view->srcs_cnt = 0;
    imgs_view->idx = 0;
}

static void lisa_ui_imgs_view_delete_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_DELETE) {
        lisa_ui_imgs_view_clear(obj);
    }
}

static void lisa_ui_imgs_view_btn_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_CLICKED) {
        lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj->user_data;

        if (imgs_view->btn_next == obj) {
            lisa_ui_imgs_view_next(obj);
        } else if (imgs_view->btn_prev == obj) {
            lisa_ui_imgs_view_prev(obj);
        }
    }
}

static void lisa_ui_imgs_view_gesture_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_GESTURE) {
        lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj->user_data;

        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
            lisa_ui_imgs_view_prev(obj);
        } else if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
            lisa_ui_imgs_view_next(obj);
        }
    }
}

static void lisa_ui_imgs_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    imgs_view->img = lv_img_create(obj);
    imgs_view->lbl = lv_label_create(obj);
    imgs_view->btn_prev = lv_btn_create(obj);
    imgs_view->btn_next = lv_btn_create(obj);
    imgs_view->lbl_status = lv_label_create(obj);

    imgs_view->idx = 0;
    imgs_view->srcs = NULL;
    imgs_view->names = NULL;
    imgs_view->srcs_cnt = 0;
}

lv_obj_t *lisa_ui_imgs_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    lv_obj_set_size(imgs_view->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(imgs_view->img, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_size(imgs_view->btn_prev, 20, 20);
    lv_obj_align(imgs_view->btn_prev, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(imgs_view->btn_prev, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(imgs_view->btn_prev, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(imgs_view->btn_prev, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_set_size(imgs_view->btn_prev, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(imgs_view->btn_prev, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_size(imgs_view->btn_next, 20, 20);
    lv_obj_align(imgs_view->btn_next, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(imgs_view->btn_next, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(imgs_view->btn_next, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(imgs_view->btn_next, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_label_set_text(imgs_view->lbl, "name");
    lv_obj_align(imgs_view->lbl, LV_ALIGN_TOP_MID, 0, 0);

    lv_label_set_text(imgs_view->lbl_status, "0/0");
    lv_obj_align(imgs_view->lbl_status, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(imgs_view->lbl_status, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(obj, lisa_ui_imgs_view_delete_event_cb, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(obj, lisa_ui_imgs_view_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(imgs_view->btn_prev, lisa_ui_imgs_view_btn_event_cb, LV_EVENT_CLICKED, obj);
    lv_obj_add_event_cb(imgs_view->btn_next, lisa_ui_imgs_view_btn_event_cb, LV_EVENT_CLICKED, obj);

    return obj;
}

void lisa_ui_imgs_view_status_show(lv_obj_t *obj)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;
    lv_obj_clear_flag(imgs_view->lbl_status, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_imgs_view_status_hide(lv_obj_t *obj)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;
    lv_obj_add_flag(imgs_view->lbl_status, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_imgs_view_set_imgs(lv_obj_t *obj, void **srcs, char **names, uint32_t cnt)
{
    lisa_ui_imgs_view_t *imgs_view = (lisa_ui_imgs_view_t *)obj;

    if (srcs == NULL || names == NULL || cnt == 0) {
        return;
    }

    lisa_ui_imgs_view_clear(obj);

    imgs_view->srcs_cnt = cnt;

    imgs_view->srcs = srcs;
    imgs_view->names = names;
    imgs_view->idx = 0;

    lisa_ui_imgs_view_set_idx(obj, 0);
}
