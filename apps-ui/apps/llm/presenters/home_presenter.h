#ifndef __HOME_PRESENTER_H__
#define __HOME_PRESENTER_H__

#include <stdint.h>

#include "lisa_ui.h"
#include "model_battery.h"
#include "model_camera_preview.h"

#define WORK_TYPE_VOICE 0
#define WORK_TYPE_IMG_REC 1
#define WORK_TYPE_CAMERA 2
#define HOME_EMOJI_NAME_MAX 64

struct home_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *anim_timer;
    lv_timer_t *oneshot_emoji_timer;
    lv_timer_t *network_status_timer;
    lv_timer_t *camera_capture_timer;
    lv_timer_t *camera_preview_countdown_timer;
    lv_timer_t *img_hide_timer;
    lv_timer_t *battery_query_timer;
    lv_timer_t *standby_text_timer;
    lv_timer_t *standby_sleep_timer;
    uint32_t standby_text_index;
    lv_img_dsc_t img;
    lv_img_dsc_t *net_img;
    uint8_t *cap_buf;
    uint32_t cap_buf_size;
    uint8_t img_rec_running;
    uint8_t img_rec_in_progress;
    uint8_t img_rec_triggered;
    uint8_t img_rec_is_mcp;
    uint8_t img_rec_is_button;
    uint8_t camera_preview_result_pending;
    uint8_t mcp_emoji_running;
    uint8_t mcp_emoji_pending;
    uint32_t mcp_emoji_start_tick;
    char mcp_emoji_pending_name[HOME_EMOJI_NAME_MAX];
    uint8_t mcp_loading;
    uint8_t finished;
    uint8_t speaking;
    uint8_t work_type;
    uint8_t oneshot_emoji_running;
    uint8_t standby_sleep_active;
    uint8_t standby_after_tts_pending;
    uint8_t standby_sleep_restore_brightness;
    uint8_t music_text_active;
    model_battery_status_t last_battery_status;
    model_camera_preview_t camera_preview;
    char current_emoji_name[HOME_EMOJI_NAME_MAX];
    char oneshot_restore_emoji_name[HOME_EMOJI_NAME_MAX];
    char tts_text_buf[2048];
    uint16_t tts_text_len;
    uint16_t tts_text_displayed;
    uint8_t tts_text_stream_done;
    lv_timer_t *tts_text_timer;
};

void standby_text_timer_update(struct home_nav_scr_data *scr_data);
void home_handle_activity(struct home_nav_scr_data *scr_data);
void home_reset(struct home_nav_scr_data *scr_data);

#endif // __HOME_PRESENTER_H__
