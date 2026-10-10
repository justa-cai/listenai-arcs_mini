#ifndef SERVICE_CAMERA_H
#define SERVICE_CAMERA_H

#include <stdint.h>
#include <stdbool.h>

int service_camera_init(void);
int service_camera_capture(uint8_t *buffer, uint32_t buffer_len);

/* 同 capture，但可控丢弃帧数：连续视频（如 SIP 通话）传 0 —— 流式运行时
 * ping-pong 缓冲最多滞后一两帧（百毫秒级），对通话无感；拍照保留默认丢
 * 2 帧取新鲜画面。 */
int service_camera_capture_ex(uint8_t *buffer, uint32_t buffer_len, uint8_t drop_frames);
int service_camera_stop(void);
int service_camera_suspend(void);
int service_camera_resume(void);
int service_camera_get_framesize(uint16_t *width, uint16_t *height);
bool service_camera_is_inited(void);

#endif
