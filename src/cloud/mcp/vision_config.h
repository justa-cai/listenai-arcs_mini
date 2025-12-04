#ifndef VISION_CONFIG_H
#define VISION_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Vision capability configuration
 */
typedef struct {
    char url[256];      /**< Vision API URL */
    char token[512];    /**< Authorization token */
    bool valid;         /**< Whether config is valid */
} vision_config_t;

/**
 * @brief Initialize vision config module
 */
void vision_config_init(void);

/**
 * @brief Set vision configuration from cloud
 * @param url Vision API URL
 * @param token Authorization token
 * @return 0 on success, -1 on failure
 */
int vision_config_set(const char *url, const char *token);

/**
 * @brief Get current vision configuration
 * @param config Pointer to store config
 * @return 0 on success, -1 on failure
 */
int vision_config_get(vision_config_t *config);

/**
 * @brief Check if vision config is valid
 * @return true if valid, false otherwise
 */
bool vision_config_is_valid(void);

/**
 * @brief Clear vision configuration
 */
void vision_config_clear(void);

#endif /* VISION_CONFIG_H */
