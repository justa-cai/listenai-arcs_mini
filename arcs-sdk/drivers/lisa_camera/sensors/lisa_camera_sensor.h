#ifndef __LISA_CAMERA_SENSOR_H__
#define __LISA_CAMERA_SENSOR_H__

#include "../lisa_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int (*init)(lisa_device_t *dev);
    int (*deinit)(lisa_device_t *dev);
    int (*read_id)(lisa_device_t *dev, uint16_t *id);
    int (*set_pixformat)(lisa_device_t *dev, lisa_camera_pixel_format_t format);
    int (*set_framesize)(lisa_device_t *dev, lisa_camera_framesize_t framesize);
    int (*set_hmirror)(lisa_device_t *dev, bool enable);
    int (*set_vflip)(lisa_device_t *dev, bool enable);
    int (*set_exposure)(lisa_device_t *dev, int exposure);
    int (*set_gain)(lisa_device_t *dev, int gain);
} lisa_camera_sensor_if_t;

#ifdef __cplusplus
}
#endif

#endif // __LISA_CAMERA_SENSOR_H__