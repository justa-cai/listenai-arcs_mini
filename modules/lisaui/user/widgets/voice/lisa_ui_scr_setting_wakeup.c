#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_scr_setting_wakeup.h"
#include "lisa_ui_src_base.h"

struct lisa_ui_scr_setting_wakeup {
    lisa_ui_scr_base_t base;
    lv_obj_t *list;
};
typedef struct lisa_ui_scr_setting_wakeup lisa_ui_scr_setting_wakeup_t;

static void lisa_ui_scr_setting_wakeup_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
const lv_obj_class_t lv_lisa_ui_scr_setting_wakeup_class = {
    .constructor_cb = lisa_ui_scr_setting_wakeup_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_scr_setting_wakeup_t)
};

#define MY_CLASS &lv_lisa_ui_scr_setting_wakeup_class

static void lisa_ui_scr_setting_wakeup_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_scr_setting_wakeup_t *scr_setting_wakeup = (lisa_ui_scr_setting_wakeup_t *)obj;
    scr_setting_wakeup->list = lv_list_create(container);
}

lv_obj_t *lisa_ui_scr_setting_wakeup_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    lisa_ui_scr_setting_wakeup_t *scr_setting_wakeup = (lisa_ui_scr_setting_wakeup_t *)obj;

    lv_obj_align(scr_setting_wakeup->list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(scr_setting_wakeup->list, LV_PCT(100), LV_SIZE_CONTENT);

    return obj;
}
