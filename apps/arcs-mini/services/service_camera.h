#ifndef SERVICE_CAMERA_H
#define SERVICE_CAMERA_H

#include <stdint.h>
#include <stdbool.h>

int service_camera_init(void);
int service_camera_capture(uint8_t *buffer, uint32_t buffer_len);
int service_camera_get_framesize(uint16_t *width, uint16_t *height);
bool service_camera_is_inited(void);

#endif
