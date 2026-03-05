#ifndef __MODEL_COMMON_H__
#define __MODEL_COMMON_H__

#include <stdint.h>

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
