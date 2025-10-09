
/*
 * Copyright (c) 2023, LISINTNAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * @brief Touch hardware configuration structure
 */
typedef struct {
    // I2C configuration
    void *i2c_dev;              /*!< I2C device handle */

    // I2C pin configuration
    struct {
        struct {
            uint8_t sda_pad;        /*!< SDA pin pad */
            uint8_t sda_pin;        /*!< SDA pin number */
            uint8_t sda_func;       /*!< SDA pin function */
        } sda;
        struct {
            uint8_t scl_pad;        /*!< SCL pin pad */
            uint8_t scl_pin;        /*!< SCL pin number */
            uint8_t scl_func;       /*!< SCL pin function */
        } scl;
    } i2c_pins;
    
    // GPIO configuration
    struct {
        struct {
            uint8_t reset_pad;      /*!< Reset pin pad */
            uint8_t reset_pin;      /*!< Reset pin number */
            uint8_t reset_func;     /*!< Reset pin function */
        } reset;
        struct {
            uint8_t int_pad;        /*!< Interrupt pin pad */
            uint8_t int_pin;        /*!< Interrupt pin number */
            uint8_t int_func;       /*!< Interrupt pin function */
        } intr;
    } gpio_pins;
} touch_hw_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Touch interrupt callback function type
 *
 * @warning Do not call any touch driver functions from within this callback
 *
 * @note This callback is called from interrupt context
 * @note The callback should be kept as short as possible
 * @note The callback should not call any blocking functions
 */
typedef void (*lisa_touch_callback_t)(void);

/**
 * @brief Touch screen driver API interface
 *
 * API which a touch screen driver must implement. All functions should be
 * thread-safe and non-blocking unless explicitly stated otherwise.
 */
struct touch_driver_api {
    int (*read_coordinates)(uint16_t *x, uint16_t *y, bool *pressed);
    void (*set_int_callback)(lisa_touch_callback_t cb);
    int (*set_enable)(bool enable);
    int (*set_inverted_x)(bool inverted);
    int (*set_inverted_y)(bool inverted);
    int (*set_swap_xy)(bool swap);
};

/**
 * @brief Touch device structure
 *
 * Contains device identification and operation interfaces for a touch device
 */
struct touch_device {
    /** Device name for identification */
    const char *name;

    /**
     * @brief Device initialization function
     * @retval 0 on success, negative errno on failure
     */
    int (*device_init)(const touch_hw_config_t *config);

    /** Pointer to device-specific API implementation */
    const struct touch_driver_api *api;
};

/**
 * @brief Create and initialize a touch device instance
 *
 * This function creates a new touch device instance and performs
 * necessary hardware initialization.
 *
 * @note This function must be called before any other touch functions
 * @note The returned pointer should be passed to other touch functions

 * @retval Pointer to touch device structure on success
 * @retval NULL on initialization failure
 */
void *lisa_touch_create(touch_hw_config_t *config);

/**
 * @brief Set touch interrupt callback function
 *
 * @param dev Pointer to touch device structure
 * @param cb Callback function to be called when touch interrupt occurs
 *
 * @note The callback function will be called from interrupt context
 * @note Previous callback will be replaced if already set
 * @note Set NULL to disable interrupt callback
 * @warning The callback function should be kept as short as possible
 */
#ifdef CONFIG_LISA_TOUCH_INTERRUPT
void lisa_touch_set_int_callback(const struct touch_device *dev, lisa_touch_callback_t cb);
#endif

/**
 * @brief Read current touch coordinates
 *
 * @param dev Pointer to touch device structure
 * @param[out] x Pointer to store X coordinate
 * @param[out] y Pointer to store Y coordinate
 *
 * @retval 0 Touch detected and coordinates read successfully
 *
 * @note This function may block until valid coordinates are available
 * @note If multiple touches are supported, only the first touch is returned
 */
int lisa_touch_read_coordinates(const struct touch_device *dev, uint16_t *x, uint16_t *y, bool *pressed);

/**
 * @brief Enable or disable touch screen
 *
 * @param[in] dev Pointer to touch device structure
 * @param[in] enable 0 disable, 1 enable
 * @return int
 */
int lisa_touch_set_enable(const struct touch_device *dev, bool enable);

/**
 * @brief Set whether X-axis coordinates should be inverted
 *
 * @param dev Pointer to touch device structure
 * @param inverted[in] true to invert X coordinates, false for normal orientation
 * @return int 0 on success, negative errno on failure
 */
int lisa_touch_set_inverted_x(const struct touch_device *dev, bool inverted);

/**
 * @brief Set whether Y-axis coordinates should be inverted
 *
 * @param dev Pointer to touch device structure
 * @param inverted[in] true to invert Y coordinates, false for normal orientation
 * @return int 0 on success, negative errno on failure
 */
int lisa_touch_set_inverted_y(const struct touch_device *dev, bool inverted);

/**
 * @brief Set whether X and Y coordinates should be swapped
 *
 * @param dev Pointer to touch device structure
 * @param swap[in] true to swap X and Y coordinates, false for normal orientation
 * @return int 0 on success, negative errno on failure
 */
int lisa_touch_set_swap_xy(const struct touch_device *dev, bool swap);

#ifdef __cplusplus
}
#endif