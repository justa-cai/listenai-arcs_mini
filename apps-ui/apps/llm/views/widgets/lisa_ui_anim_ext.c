#include "lisa_ui_anim_ext.h"

static void lisa_ui_anim_ext_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_anim_ext_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_anim_ext_class = {
    .constructor_cb = lisa_ui_anim_ext_class_constructor,
    .destructor_cb = lisa_ui_anim_ext_class_destructor,
    .event_cb = NULL,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .instance_size = sizeof(lisa_ui_anim_ext_t),
    .base_class = &lisa_ui_anim_class,
};

#define MY_CLASS &lisa_ui_anim_ext_class

static void lisa_ui_anim_stop_cb_handle(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if (anim_ext->state == LISA_UI_ANIM_STATE_ENTER) {
        anim_ext->state = LISA_UI_ANIM_STATE_LOOP;
        lisa_ui_anim_set_config(obj, &anim_ext->curr.loop);
        lisa_ui_anim_start(obj);
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_LOOP) {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPING;
        if (anim_ext->curr.exit.frame_count != 0) {
            lisa_ui_anim_set_config(obj, &anim_ext->curr.exit);
            lisa_ui_anim_start(obj);
        }
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_NEXT_REQ) {
        lisa_ui_anim_ext_set_config(obj, &anim_ext->next);
        lisa_ui_anim_ext_start(obj);
    } else {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
    }
}

static void lisa_ui_anim_loop_cb_handle(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    if (anim_ext->state == LISA_UI_ANIM_STATE_STOP_REQ) {
        if (anim_ext->curr.exit.frame_count != 0) {
            anim_ext->state = LISA_UI_ANIM_STATE_STOPING;
            lisa_ui_anim_set_config(obj, &anim_ext->curr.exit);
            lisa_ui_anim_start(obj);
        } else {
            anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
            lisa_ui_anim_stop(obj);
        }
    } else if (anim_ext->state == LISA_UI_ANIM_STATE_NEXT_REQ) {
        lisa_ui_anim_ext_set_config(obj, &anim_ext->next);
        lisa_ui_anim_ext_start(obj);
    }
}

static void lisa_ui_anim_ext_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    memset(&anim_ext->curr, 0, sizeof(lisa_ui_anim_config_t));
    memset(&anim_ext->next, 0, sizeof(lisa_ui_anim_config_t));
    anim_ext->state = LISA_UI_ANIM_STATE_NONE;

    lisa_ui_anim_stop_cb_set(obj, lisa_ui_anim_stop_cb_handle);
    lisa_ui_anim_loop_cb_set(obj, lisa_ui_anim_loop_cb_handle);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lisa_ui_anim_ext_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    LV_TRACE_OBJ_CREATE("finished");
}

lv_obj_t *lisa_ui_anim_ext_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

void lisa_ui_anim_ext_set_config(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *config)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT(config != NULL);

    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    memcpy(&anim_ext->curr.enter, &config->enter, sizeof(lisa_ui_anim_config_t));
    memcpy(&anim_ext->curr.loop, &config->loop, sizeof(lisa_ui_anim_config_t));
    memcpy(&anim_ext->curr.exit, &config->exit, sizeof(lisa_ui_anim_config_t));
}

void lisa_ui_anim_ext_start(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    lisa_ui_anim_config_t *cfg = &anim_ext->curr.enter;

    if (cfg->frame_count == 0) {
        cfg = &anim_ext->curr.loop;
        anim_ext->state = LISA_UI_ANIM_STATE_LOOP;
    } else {
        anim_ext->state = LISA_UI_ANIM_STATE_ENTER;
    }

    lisa_ui_anim_set_config(obj, cfg);
    lisa_ui_anim_start(obj);
}

void lisa_ui_anim_ext_stop(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if (anim_ext->state == LISA_UI_ANIM_STATE_NONE || anim_ext->state == LISA_UI_ANIM_STATE_STOPED) {
        return;
    }

    if (anim_ext->curr.exit.frame_count == 0) {
        anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
        lisa_ui_anim_stop(obj);
        return;
    }

    anim_ext->state = LISA_UI_ANIM_STATE_STOP_REQ;
}

void lisa_ui_anim_ext_next(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;

    if ((anim_ext->state != LISA_UI_ANIM_STATE_STOPED) && (anim_ext->state != LISA_UI_ANIM_STATE_NONE)) {
        memcpy(&anim_ext->next, next, sizeof(lisa_ui_anim_ext_config_t));
        anim_ext->state = LISA_UI_ANIM_STATE_NEXT_REQ;
    } else {
        lisa_ui_anim_ext_set_config(obj, next);
        lisa_ui_anim_ext_start(obj);
    }
}

void lisa_ui_anim_ext_next_imm(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    lisa_ui_anim_ext_exit(obj);
    lisa_ui_anim_ext_next(obj, next);
}

void lisa_ui_anim_ext_exit(lv_obj_t *obj)
{
    lisa_ui_anim_ext_t *anim_ext = (lisa_ui_anim_ext_t *)obj;
    anim_ext->state = LISA_UI_ANIM_STATE_STOPED;
    lisa_ui_anim_stop(obj);
}
