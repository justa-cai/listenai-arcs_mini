#ifndef __LISA_UI_ANIM_EXT_H__
#define __LISA_UI_ANIM_EXT_H__

#include "lisa_ui_anim.h"

enum {
    LISA_UI_ANIM_STATE_NONE,
    LISA_UI_ANIM_STATE_ENTER,
    LISA_UI_ANIM_STATE_LOOP,
    LISA_UI_ANIM_STATE_STOP_REQ,
    LISA_UI_ANIM_STATE_NEXT_REQ,
    LISA_UI_ANIM_STATE_STOPING,
    LISA_UI_ANIM_STATE_STOPED,
    LISA_UI_ANIM_STATE_MAX,
};

typedef struct {
    lisa_ui_anim_config_t enter;
    lisa_ui_anim_config_t loop;
    lisa_ui_anim_config_t exit;
} lisa_ui_anim_ext_config_t;

typedef struct {
    lisa_ui_anim_t anim;
    lisa_ui_anim_ext_config_t curr;
    lisa_ui_anim_ext_config_t next;
    uint8_t state;
} lisa_ui_anim_ext_t;

extern const lv_obj_class_t lisa_ui_anim_ext_class;

lv_obj_t *lisa_ui_anim_ext_create(lv_obj_t *parent);
void lisa_ui_anim_ext_exit(lv_obj_t *obj);
void lisa_ui_anim_ext_stop(lv_obj_t *obj);
void lisa_ui_anim_ext_start(lv_obj_t *obj);
void lisa_ui_anim_ext_next(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next);
void lisa_ui_anim_ext_next_imm(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *next);
void lisa_ui_anim_ext_set_config(lv_obj_t *obj, const lisa_ui_anim_ext_config_t *config);

#endif /* __LISA_UI_ANIM_EXT_H__ */
