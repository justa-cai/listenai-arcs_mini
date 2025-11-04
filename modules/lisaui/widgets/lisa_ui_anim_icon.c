#include "lvgl.h"

struct lisa_ui_anim_icon {
    lv_obj_t obj;
    uint32_t interval;
    lv_timer_t *timer;
    char **icons_path;
    uint8_t cnt;
    uint8_t idx;
};

const lv_obj_class_t lv_lisa_ui_anim_icon_class = {
    .base_class = &lv_obj_class,
    .instance_size = sizeof(struct lisa_ui_anim_icon)
};

static void lisa_ui_anim_icon_timer_cb(lv_timer_t *timer)
{
    struct lisa_ui_anim_icon *anim_icon = (struct lisa_ui_anim_icon *)timer->user_data;
    anim_icon->idx = (anim_icon->idx + 1) % anim_icon->cnt;
    lv_img_set_src(&anim_icon->obj, anim_icon->icons_path[anim_icon->idx]);
}

static void lisa_ui_anim_icon_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_DELETE) {
        struct lisa_ui_anim_icon *anim_icon = (struct lisa_ui_anim_icon *)obj;
        lv_timer_del(anim_icon->timer);
        for (int i = 0; i < anim_icon->cnt; i++) {
            if (anim_icon->icons_path[i]) {
                lv_mem_free(anim_icon->icons_path[i]);
            }
        }
        lv_mem_free(anim_icon->icons_path);
    }
}

lv_obj_t *lisa_ui_anim_icon_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_lisa_ui_anim_icon_class, parent);
    lv_obj_class_init_obj(obj);

    lv_obj_add_event_cb(obj, lisa_ui_anim_icon_event_cb, LV_EVENT_DELETE, obj);

    return obj;
}

int  lisa_ui_anim_icon_config(lv_obj_t *obj, const char **icons_path, uint8_t icon_count, uint32_t interval)
{
    struct lisa_ui_anim_icon *anim_icon = (struct lisa_ui_anim_icon *)obj;

    if (anim_icon->icons_path == NULL || icon_count == 0 || interval == 0) {
        return -1;
    }

    anim_icon->icons_path = lv_mem_alloc(icon_count * sizeof(char *));
    if (!anim_icon->icons_path) {
        return -1;
    }

    lv_memset(anim_icon->icons_path, 0, icon_count * sizeof(char *));

    for (int i = 0; i < icon_count; i++) {
        if (icons_path[i] == NULL) {
            return -1;
        }

        anim_icon->icons_path[i] = lv_mem_alloc(strlen(icons_path[i]) + 1);
        if (!anim_icon->icons_path[i]) {
            return -1;
        }
        lv_memcpy(anim_icon->icons_path[i], icons_path[i], strlen(icons_path[i]) + 1);
    }

    anim_icon->interval = interval;
    anim_icon->timer = lv_timer_create(lisa_ui_anim_icon_timer_cb, interval, obj);
    anim_icon->cnt = icon_count;
    anim_icon->idx = 0;
    lv_img_set_src(obj, icons_path[0]);

    lv_timer_set_repeat_count(anim_icon->timer, 0);

    return 0;
}

int lisa_ui_anim_icon_start(lv_obj_t *obj)
{
    struct lisa_ui_anim_icon *anim_icon = (struct lisa_ui_anim_icon *)obj;
    if (anim_icon->timer == NULL) {
        return -1;
    }

    if (anim_icon->timer->repeat_count == -1) {
        return 0;
    }

    if (anim_icon->interval == 0) {
        return -1;
    }

    if (anim_icon->cnt == 0 || anim_icon->icons_path == NULL) {
        return -1;
    }

    lv_timer_set_repeat_count(anim_icon->timer, -1);

    return 0;
}

int lisa_ui_anim_icon_stop(lv_obj_t *obj)
{
    struct lisa_ui_anim_icon *anim_icon = (struct lisa_ui_anim_icon *)obj;

    if (anim_icon == NULL || anim_icon->timer == NULL) {
        return -1;
    }

    lv_timer_set_repeat_count(anim_icon->timer, 0);

    return 0;
}
