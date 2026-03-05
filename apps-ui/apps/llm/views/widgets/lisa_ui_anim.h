#ifndef LISA_UI_ANIM_H
#define LISA_UI_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef void (*lisa_ui_anim_stop_cb_t)(lv_obj_t *obj);
typedef void (*lisa_ui_anim_loop_cb_t)(lv_obj_t *obj);

typedef struct {
    lv_img_t img;
    lisa_ui_anim_stop_cb_t stop_cb;
    lisa_ui_anim_loop_cb_t loop_cb;
    const void **frames;
    uint32_t *delays;
    uint16_t frame_count;
    uint16_t current_frame;
    uint32_t default_delay;
    lv_timer_t *timer;
    int32_t loop;
    uint8_t playing : 1;
} lisa_ui_anim_t;

typedef struct {
    const void **frames;
    const uint32_t *delays;
    uint16_t frame_count;
    uint32_t default_delay;
    int loop;
    int pause_frame_idx;
} lisa_ui_anim_config_t;

extern const lv_obj_class_t lisa_ui_anim_class;

lv_obj_t *lisa_ui_anim_create(lv_obj_t *parent);
int lisa_ui_anim_set_config(lv_obj_t *obj, const lisa_ui_anim_config_t *config);
int lisa_ui_anim_set_frames(lv_obj_t *obj, const void **frames, uint16_t frame_count,
                            uint32_t delay_ms, int loop);
void lisa_ui_anim_start(lv_obj_t *obj);
void lisa_ui_anim_pause(lv_obj_t *obj);
void lisa_ui_anim_stop(lv_obj_t *obj);
void lisa_ui_anim_set_frame(lv_obj_t *obj, uint16_t frame_idx);
uint16_t lisa_ui_anim_get_frame(lv_obj_t *obj);
bool lisa_ui_anim_is_playing(lv_obj_t *obj);

void lisa_ui_anim_stop_cb_set(lv_obj_t *obj, lisa_ui_anim_stop_cb_t cb);
void lisa_ui_anim_loop_cb_set(lv_obj_t *obj, lisa_ui_anim_loop_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif
