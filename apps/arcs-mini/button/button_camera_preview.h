#ifndef BUTTON_CAMERA_PREVIEW_H
#define BUTTON_CAMERA_PREVIEW_H

#include <stdbool.h>

void button_camera_preview_init(void);
bool button_camera_preview_handle_click(void);
void button_camera_preview_handle_double_click(void);
bool button_camera_preview_handle_long_hold(void);
bool button_camera_preview_is_busy(void);
bool button_camera_preview_request_exit(void);
bool button_camera_preview_wait_exit(void);

#endif /* BUTTON_CAMERA_PREVIEW_H */
