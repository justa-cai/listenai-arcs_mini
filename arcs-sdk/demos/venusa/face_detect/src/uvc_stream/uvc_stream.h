#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "video_camera.h"

#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
#include "acomp_fd.h"
#endif

int uvc_stream_init(void);
bool uvc_stream_is_open(void);
int uvc_stream_submit_frame(const camera_frame_t *frame);
#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
int uvc_stream_update_face_result(const acomp_fd_result_info_t *info, uint32_t info_len);
#endif
