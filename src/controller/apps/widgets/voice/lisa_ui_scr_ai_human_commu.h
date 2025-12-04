#ifndef __LISA_UI_SCR_AI_HUMAN_COMMU_H__
#define __LISA_UI_SCR_AI_HUMAN_COMMU_H__

#include "lvgl.h"

typedef void (*lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_t)(void *user_data);
typedef void (*lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_t)(void *user_data);

lv_obj_t *lisa_ui_scr_ai_human_commu_create(lv_obj_t *parent, bool gif);
int lisa_ui_scr_ai_human_commu_img_set(lv_obj_t *obj, const void *img_path, const char *name);
void lisa_ui_scr_ai_human_commu_btn_mute_img_set(lv_obj_t *obj, const void *img_path);
void lisa_ui_scr_ai_human_commu_btn_cam_img_set(lv_obj_t *obj, const void *img_path);
void lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_set(lv_obj_t *obj,
                                                            lisa_ui_scr_ai_human_commu_btn_mute_click_event_cb_t cb,
                                                            void *user_data);
void lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_set(lv_obj_t *obj,
                                                           lisa_ui_scr_ai_human_commu_btn_cam_click_event_cb_t cb,
                                                           void *user_data);
void lisa_ui_scr_ai_human_commu_label_set_tips(lv_obj_t *obj, const char *tips);
int lisa_ui_scr_ai_human_commu_talk_bg_img_set(lv_obj_t *obj, const void *img);
int lisa_ui_scr_ai_human_commu_talk_txt_img_set(lv_obj_t *obj, const char *txt);
void lisa_ui_scr_ai_human_commu_btn_mute_bg_color_set(lv_obj_t *obj, lv_color_t color);
void lisa_ui_scr_ai_human_commu_talk_anim_set(lv_obj_t *obj, const void *dsc[], uint8_t num);
void lisa_ui_scr_ai_human_commu_talk_anim_start(lv_obj_t *obj);
void lisa_ui_scr_ai_human_commu_talk_anim_stop(lv_obj_t *obj);
void lisa_ui_scr_ai_human_commu_label_new_context(lv_obj_t *obj);
void lisa_ui_scr_ai_human_commu_label_me_set_txt(lv_obj_t *obj, const char *txt);
void lisa_ui_scr_ai_human_commu_label_ai_set_txt(lv_obj_t *obj, const char *txt);


void lisa_ui_scr_ai_human_commu_talk_emoji_anim_set(lv_obj_t *obj, const void *dsc[], uint8_t num);
void lisa_ui_scr_ai_human_commu_talk_emoji_anim_start(lv_obj_t *obj, uint32_t time);

#endif
