#ifndef __LISA_UI_SD_MUSIC_SYNC_VIEW_H__
#define __LISA_UI_SD_MUSIC_SYNC_VIEW_H__

#include "lvgl.h"
#include "voice_msg_structure.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *lisa_ui_sd_music_sync_view_create(lv_obj_t *parent);
void lisa_ui_sd_music_sync_view_update(lv_obj_t *obj,
                                        const voice_msg_sd_music_sync_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
