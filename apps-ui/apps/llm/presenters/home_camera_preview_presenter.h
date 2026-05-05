#ifndef __HOME_CAMERA_PREVIEW_PRESENTER_H__
#define __HOME_CAMERA_PREVIEW_PRESENTER_H__

#include <stdbool.h>
#include <stdint.h>

#include "lisa_ui.h"
#include "model_camera_preview.h"

struct home_nav_scr_data;

bool camera_preview_is_camera_work_type(uint8_t work_type);

void camera_preview_hide(struct home_nav_scr_data *scr_data);
void camera_preview_capture_timer_cb(lv_timer_t *timer);

extern const struct model_camera_preview_cb home_camera_preview_presenter_cbs;

#endif // __HOME_CAMERA_PREVIEW_PRESENTER_H__
