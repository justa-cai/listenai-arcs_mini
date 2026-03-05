/**
 * @file model_battery.h
 * @brief Battery data model interface
 */

#ifndef __MODEL_BATTERY_H__
#define __MODEL_BATTERY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODEL_BATTERY_STATUS_NO_BATTERY = 0,
    MODEL_BATTERY_STATUS_NOT_CONNECT,
    MODEL_BATTERY_STATUS_CHARGING,
    MODEL_BATTERY_STATUS_CHARGE_DONE,
    MODEL_BATTERY_STATUS_UNKNOWN,
} model_battery_status_t;

typedef struct {
    uint8_t level; /* 0-100 */
    model_battery_status_t status;
} model_battery_info_t;

typedef void (*model_battery_update_cb_t)(const model_battery_info_t *info, void *arg);

int model_battery_init(void);
int model_battery_deinit(void);
int model_battery_get_info(model_battery_info_t *info);
int model_battery_cb_register(model_battery_update_cb_t cb, void *arg);
int model_battery_cb_unregister(model_battery_update_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __MODEL_BATTERY_H__ */
