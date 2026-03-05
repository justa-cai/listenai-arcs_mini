#define TAG "LISA_UI_ANIM"

#include "lisa_ui.h"
#include "lisa_ui_anim.h"
#include <string.h>

#define MY_CLASS              &lisa_ui_anim_class
#define ANIM_DEFAULT_DELAY_MS 100

static void lisa_ui_anim_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_anim_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void timer_cb(lv_timer_t *timer);
static uint32_t get_frame_delay(lisa_ui_anim_t *anim, uint16_t frame_idx);
static void update_frame(lisa_ui_anim_t *anim);

const lv_obj_class_t lisa_ui_anim_class = {.constructor_cb = lisa_ui_anim_constructor,
                                           .destructor_cb = lisa_ui_anim_destructor,
                                           .event_cb = NULL,
                                           .width_def = LV_SIZE_CONTENT,
                                           .height_def = LV_SIZE_CONTENT,
                                           .instance_size = sizeof(lisa_ui_anim_t),
                                           .base_class = &lv_img_class};

static void lisa_ui_anim_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;

    anim->frames = NULL;
    anim->delays = NULL;
    anim->frame_count = 0;
    anim->current_frame = 0;
    anim->default_delay = ANIM_DEFAULT_DELAY_MS;
    anim->timer = NULL;
    anim->loop = -1;
    anim->playing = 0;

    anim->timer = lv_timer_create(timer_cb, anim->default_delay, obj);
    if (anim->timer == NULL) {
        LISA_UI_LOGE("Failed to create timer");
        return;
    }
    lv_timer_pause(anim->timer);

    LISA_UI_LOGI("Animation widget created");
    LV_TRACE_OBJ_CREATE("finished");
}

static void lisa_ui_anim_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;

    if (anim->timer != NULL) {
        lv_timer_del(anim->timer);
        anim->timer = NULL;
    }

    if (anim->delays != NULL) {
        lv_mem_free(anim->delays);
        anim->delays = NULL;
    }

    LISA_UI_LOGI("Animation widget destroyed");
    LV_TRACE_OBJ_CREATE("finished");
}

static void timer_cb(lv_timer_t *timer)
{
    lv_obj_t *obj = (lv_obj_t *)timer->user_data;

    if (obj == NULL) {
        return;
    }

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    if (anim->frames == NULL || anim->frame_count == 0) {
        return;
    }

    anim->current_frame++;

    if (anim->current_frame >= anim->frame_count) {
        if (anim->loop < 0) {
            anim->current_frame = 0;
            if (anim->loop_cb) {
                anim->loop_cb(obj);
            }
        } else if (anim->loop > 0) {
            anim->current_frame = 0;
            anim->loop--;
            if (anim->loop_cb) {
                anim->loop_cb(obj);
            }
        } else if (anim->loop == 0) {
            anim->current_frame = anim->frame_count - 1;
            anim->playing = 0;
            lv_timer_pause(timer);
            if (anim->stop_cb) {
                anim->stop_cb(obj);
            }
            LISA_UI_LOGD("Animation finished");
            return;
        }
    }

    update_frame(anim);

    uint32_t next_delay = get_frame_delay(anim, anim->current_frame);
    lv_timer_set_period(timer, next_delay);
}

static uint32_t get_frame_delay(lisa_ui_anim_t *anim, uint16_t frame_idx)
{
    uint32_t delay = anim->default_delay;

    if (anim->delays != NULL && frame_idx < anim->frame_count) {
        delay = anim->delays[frame_idx];
    }

    if (delay > 10 * 1000) {
        LISA_UI_LOGW("Delay too long: %d, idx=%d", delay, frame_idx);
    } else if (delay == 0) {
        LISA_UI_LOGW("Delay too short: %d, idx=%d", delay, frame_idx);
    }

    return delay;
}

static void update_frame(lisa_ui_anim_t *anim)
{
    if (anim->current_frame < anim->frame_count && anim->frames != NULL) {
        lv_img_set_src(&anim->img.obj, anim->frames[anim->current_frame]);
        LISA_UI_LOGD("Frame %u/%u displayed", anim->current_frame + 1, anim->frame_count);
    }
}

lv_obj_t *lisa_ui_anim_create(lv_obj_t *parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

int lisa_ui_anim_set_config(lv_obj_t *obj, const lisa_ui_anim_config_t *config)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    if (config == NULL || config->frames == NULL || config->frame_count == 0) {
        LISA_UI_LOGE("Invalid parameters");
        return -1;
    }

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;

    if (anim->playing) {
        lisa_ui_anim_pause(obj);
    }

    if (anim->delays != NULL) {
        lv_mem_free(anim->delays);
        anim->delays = NULL;
    }

    anim->frames = config->frames;
    anim->frame_count = config->frame_count;
    anim->default_delay = config->default_delay > 0 ? config->default_delay : ANIM_DEFAULT_DELAY_MS;
    anim->loop = config->loop;
    anim->current_frame = 0;

    if (config->delays != NULL) {
        anim->delays = (uint32_t *)lv_mem_alloc(sizeof(uint32_t) * config->frame_count);
        if (anim->delays != NULL) {
            memcpy(anim->delays, config->delays, sizeof(uint32_t) * config->frame_count);
        } else {
            LISA_UI_LOGW("Failed to allocate delays array, using default delay");
        }
    }

    update_frame(anim);

    int delay = get_frame_delay(anim, 0);

    if (anim->timer != NULL) {
        lv_timer_set_period(anim->timer, delay);
    }

    LISA_UI_LOGI("Animation configured: %u frames, default_delay=%ums, next_delay=%ums, loop=%d", config->frame_count,
         anim->default_delay, delay, config->loop);

    return 0;
}

int lisa_ui_anim_set_frames(lv_obj_t *obj, const void **frames, uint16_t frame_count, uint32_t delay_ms, int loop)
{
    lisa_ui_anim_config_t config = {
        .frames = frames, .delays = NULL, .frame_count = frame_count, .default_delay = delay_ms, .loop = loop};

    return lisa_ui_anim_set_config(obj, &config);
}

void lisa_ui_anim_start(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;

    if (anim->timer == NULL || anim->frames == NULL || anim->frame_count == 0) {
        LISA_UI_LOGE("Cannot start: animation not configured");
        return;
    }

    if (!anim->playing) {
        anim->playing = 1;
        lv_timer_resume(anim->timer);
        LISA_UI_LOGI("Animation started");
    }

    if (anim->loop > 0) {
        anim->loop--;
    }
}

void lisa_ui_anim_pause(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    if (anim->timer == NULL) {
        return;
    }

    if (anim->playing) {
        anim->playing = 0;
        lv_timer_pause(anim->timer);
        LISA_UI_LOGI("Animation paused at frame %u", anim->current_frame);
    }
}

void lisa_ui_anim_stop(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    if (anim->timer == NULL) {
        return;
    }

    anim->playing = 0;
    anim->current_frame = 0;
    lv_timer_pause(anim->timer);
    lv_timer_set_period(anim->timer, get_frame_delay(anim, 0));

    if (anim->frames != NULL && anim->frame_count > 0) {
        update_frame(anim);
    }

    LISA_UI_LOGI("Animation stopped and reset");
}

void lisa_ui_anim_set_frame(lv_obj_t *obj, uint16_t frame_idx)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    if (anim->frames == NULL) {
        return;
    }

    if (frame_idx < anim->frame_count) {
        anim->current_frame = frame_idx;
        update_frame(anim);
        if (anim->timer != NULL) {
            lv_timer_set_period(anim->timer, get_frame_delay(anim, frame_idx));
        }
        LISA_UI_LOGD("Frame set to %u", frame_idx);
    } else {
        LISA_UI_LOGW("Invalid frame index: %u (max: %u)", frame_idx, anim->frame_count - 1);
    }
}

uint16_t lisa_ui_anim_get_frame(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    return anim->current_frame;
}

bool lisa_ui_anim_is_playing(lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    return anim->playing != 0;
}

void lisa_ui_anim_stop_cb_set(lv_obj_t *obj, lisa_ui_anim_stop_cb_t cb)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    anim->stop_cb = cb;
}

void lisa_ui_anim_loop_cb_set(lv_obj_t *obj, lisa_ui_anim_loop_cb_t cb)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lisa_ui_anim_t *anim = (lisa_ui_anim_t *)obj;
    anim->loop_cb = cb;
}
