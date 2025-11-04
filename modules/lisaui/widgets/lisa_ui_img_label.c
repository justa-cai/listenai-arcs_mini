#include "lisa_ui.h"

struct lisa_ui_img_label {
    lv_obj_t obj;
    lv_obj_t *img;
    lv_obj_t *lbl;
};

typedef struct lisa_ui_img_label lisa_ui_img_label_t;

const lv_obj_class_t lv_lisa_ui_img_label_class = {
    .base_class = &lv_obj_class,
    .instance_size = sizeof(lisa_ui_img_label_t),
};

#define MY_CLASS &lv_lisa_ui_img_label_class

lv_obj_t *lisa_ui_img_label_create(lv_obj_t *parent, const void *img_path, const char *label)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_hor(obj, 10, LV_PART_MAIN);
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_width(obj, LV_SIZE_CONTENT);

    lisa_ui_img_label_t *img_label = (lisa_ui_img_label_t *)obj;

    img_label->img = lv_img_create(obj);
    lv_obj_set_size(img_label->img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    if (img_path) {
        lv_img_set_src(img_label->img, img_path);
    }

    img_label->lbl = lv_label_create(obj);
    lv_label_set_text(img_label->lbl, label);
    lv_obj_set_flex_grow(img_label->lbl, 1);

    return obj;
}

int lisa_ui_img_label_img_set(lv_obj_t *obj, const void *img_path)
{
    lisa_ui_img_label_t *img_label = (lisa_ui_img_label_t *)obj;
    lv_img_set_src(img_label->img, img_path);

    return 0;
}

int lisa_ui_img_label_label_set(lv_obj_t *obj, const char *label)
{
    lisa_ui_img_label_t *img_label = (lisa_ui_img_label_t *)obj;
    lv_label_set_text(img_label->lbl, label);

    return 0;
}
