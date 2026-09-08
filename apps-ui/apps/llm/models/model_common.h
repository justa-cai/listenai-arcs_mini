#ifndef __MODEL_COMMON_H__
#define __MODEL_COMMON_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODEL_COMMON_POWER_STATE_NORMAL = 0,
    MODEL_COMMON_POWER_STATE_IDLE,
    MODEL_COMMON_POWER_STATE_HIBERNATE,
    MODEL_COMMON_POWER_STATE_SHUTDOWN_PENDING,
} model_common_power_state_t;

typedef void (*model_common_power_state_cb_t)(model_common_power_state_t state,
                                              uint8_t reason,
                                              void *arg);

/**
 * @brief Get current volume level
 * @return Volume level (0-100)
 */
uint8_t model_common_volume_get(void);

/**
 * @brief Set volume level
 * @param volume Volume level (0-100)
 * @return 0 on success, -1 on error
 */
int model_common_volume_set(uint8_t volume);

/**
 * @brief Get current brightness level
 * @return Brightness level (0-100)
 */
uint8_t model_common_brightness_get(void);

/**
 * @brief Set brightness level
 * @param brightness Brightness level (0-100)
 * @return 0 on success, -1 on error
 */
int model_common_brightness_set(uint8_t brightness);

/**
 * @brief Temporarily apply brightness level without persisting user settings
 * @param brightness Brightness level (0-100)
 * @return 0 on success, -1 on error
 */
int model_common_brightness_set_temp(uint8_t brightness);

/**
 * @brief Enable or disable panel blanking without changing stored brightness
 * @param blanked true to turn the panel display off, false to turn it on
 * @return 0 when the request is queued
 */
int model_common_display_set_blanked(bool blanked);

/**
 * @brief Suspend or resume the USB device controller without affecting audio capture
 * @param suspended true to disconnect and gate USB clocks, false to resume and reconnect
 * @return 0 when the request is queued
 */
int model_common_usb_set_suspended(bool suspended);

/**
 * @brief Suspend or resume non-UI standby power consumers.
 *
 * Recording and network connectivity remain active.
 */
int model_common_power_experiment_set_suspended(bool suspended);

int model_common_power_state_cb_register(model_common_power_state_cb_t cb, void *arg);
int model_common_power_state_cb_unregister(model_common_power_state_cb_t cb);

/**
 * @brief Initialize common model
 * @return 0 on success, -1 on error
 */
int model_common_init(void);

/**
 * @brief Synchronize current state from system services
 * @return 0 on success, -1 if unavailable
 */
int model_common_sync_from_system(void);

#endif
