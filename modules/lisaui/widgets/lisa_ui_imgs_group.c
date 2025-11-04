#include "lvgl.h"

struct _list_item {
    const void *src;
    const char *name;
};

struct lisa_ui_imgs_group {
    lv_obj_t obj;
    lv_obj_t *imgs;
    lv_obj_t *img_name;
    lv_ll_t imgs_list;
    uint32_t id;
    struct _list_item *curr;
    bool loop;
};

typedef struct lisa_ui_imgs_group lisa_ui_imgs_group_t;

static void lisa_ui_imgs_group_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_imgs_group_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
void lisa_ui_imgs_group_clear(lv_obj_t *obj);

const lv_obj_class_t lv_lisa_ui_imgs_group_class = {
    .constructor_cb = lisa_ui_imgs_group_constructor,
    .destructor_cb = lisa_ui_imgs_group_destructor,
    .base_class = &lv_obj_class,
    .instance_size = sizeof(lisa_ui_imgs_group_t),
};

#define MY_CLASS &lv_lisa_ui_imgs_group_class

static void lisa_ui_imgs_group_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    imgs_group->imgs = lv_img_create(obj);
    imgs_group->img_name = lv_label_create(obj);
    _lv_ll_init(&imgs_group->imgs_list, sizeof(struct _list_item));

    /* -1 means no img */
    imgs_group->id = -1;
    imgs_group->curr = NULL;
}

static void lisa_ui_imgs_group_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;
    lisa_ui_imgs_group_clear(obj);
}

lv_obj_t *lisa_ui_imgs_group_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    lv_obj_set_size(imgs_group->imgs, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(imgs_group->imgs, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_size(imgs_group->img_name, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(imgs_group->img_name, imgs_group->imgs, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_add_flag(imgs_group->imgs, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(imgs_group->imgs, LV_OBJ_FLAG_EVENT_BUBBLE);

    return obj;
}

static void lisa_ui_imgs_group_show_item(lv_obj_t *obj, struct _list_item *item)
{
    if (item == NULL || item->src == NULL) {
        return;
    }
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;
    lv_img_set_src(imgs_group->imgs, item->src);
    lv_label_set_text(imgs_group->img_name, item->name);

    lv_obj_set_size(imgs_group->img_name, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(imgs_group->img_name, imgs_group->imgs, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    imgs_group->curr = item;
}

void lisa_ui_imgs_group_show(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    if (imgs_group->curr == NULL) {
        imgs_group->curr = _lv_ll_get_head(&imgs_group->imgs_list);

        if (imgs_group->curr) {
            lisa_ui_imgs_group_show_item(obj, imgs_group->curr);
        }
    }
}

void lisa_ui_imgs_group_show_next(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    struct _list_item *item;

    if (imgs_group->curr == NULL) {
        if (_lv_ll_get_len(&imgs_group->imgs_list) == 0) {
            return;
        }

        item = _lv_ll_get_head(&imgs_group->imgs_list);
    } else {
        item = _lv_ll_get_next(&imgs_group->imgs_list, imgs_group->curr);
        if (item == NULL) {
            if (imgs_group->loop) {
                item = _lv_ll_get_head(&imgs_group->imgs_list);
            } else {
                item = imgs_group->curr;
            }
        }
    }

    lisa_ui_imgs_group_show_item(obj, item);
}

void lisa_ui_imgs_group_show_prev(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    struct _list_item *item;

    if (imgs_group->curr == NULL) {
        if (_lv_ll_get_len(&imgs_group->imgs_list) == 0) {
            return;
        }

        item = _lv_ll_get_head(&imgs_group->imgs_list);
    } else {
        item = _lv_ll_get_prev(&imgs_group->imgs_list, imgs_group->curr);
        if (item == NULL) {
            if (imgs_group->loop) {
                item = _lv_ll_get_tail(&imgs_group->imgs_list);
            } else {
                item = imgs_group->curr;
            }
        }
    }

    lisa_ui_imgs_group_show_item(obj, item);
}

void lisa_ui_imgs_group_clear(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    _lv_ll_clear(&imgs_group->imgs_list);
    imgs_group->curr = NULL;

    lv_obj_clean(imgs_group->imgs);
    lv_obj_clean(imgs_group->img_name);
}

void lisa_ui_imgs_group_add_img(lv_obj_t *obj, const void *img_path, const char *name)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    struct _list_item *new = _lv_ll_ins_tail(&imgs_group->imgs_list);
    new->src = (void *)img_path;
    new->name = name;

    lisa_ui_imgs_group_show(obj);
}

void lisa_ui_imgs_group_add_and_show(lv_obj_t *obj, const void *img_path, const char *name)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;
    lisa_ui_imgs_group_add_img(obj, img_path, name);
    void *tail = _lv_ll_get_tail(&imgs_group->imgs_list);
    lisa_ui_imgs_group_show_item(obj, tail);
}

const void *lisa_ui_imgs_group_img_get_curr_src(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    if (imgs_group->curr == NULL) {
        return NULL;
    }

    return imgs_group->curr->src;
}

const char *lisa_ui_imgs_group_img_get_curr_name(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    if (imgs_group->curr == NULL) {
        return NULL;
    }

    return imgs_group->curr->name;
}

void lisa_ui_imgs_group_set_loop(lv_obj_t *obj, bool loop)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;
    imgs_group->loop = loop;
}

void lisa_ui_imgs_group_name_show(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    lv_obj_clear_flag(imgs_group->img_name, LV_OBJ_FLAG_HIDDEN);
}

void lisa_ui_imgs_group_name_hide(lv_obj_t *obj)
{
    lisa_ui_imgs_group_t *imgs_group = (lisa_ui_imgs_group_t *)obj;

    lv_obj_add_flag(imgs_group->img_name, LV_OBJ_FLAG_HIDDEN);
}
