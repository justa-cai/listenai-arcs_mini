
/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t pad;
    uint8_t pin;
    uint8_t func;
} pin_info_t;

struct spi_4line_config {
	void *spi_dev;              /*!< SPI device handle */
	uint8_t spi_tx_dma_ch;      /*!< SPI TX DMA channel */
	uint32_t spi_sck_freq;     /*!< SPI clock frequency, if not set, use default(50MHz) */

	/* SPI pin configuration */
	struct {
		pin_info_t cs;
		pin_info_t clk;
		pin_info_t sda;
		pin_info_t dc;
	} spi_pins;
};

struct qspi_config {
	void *qspi_dev;             /*!< QSPI device handle */
	uint8_t qspi_tx_dma_ch;     /*!< QSPI TX DMA channel */
	uint32_t qspi_sck_freq;     /*!< QSPI clock frequency, if not set, use default(50MHz) */

	/* QSPI pin configuration */
	struct {
		pin_info_t cs;
		pin_info_t clk;
		pin_info_t dio[4];
	} qspi_pins;
};

struct blacklight_config {
	pin_info_t pin;
	void *dev;
	uint32_t freq;
	uint8_t channel;
};

 
/**
 * @brief Display hardware configuration structure
 */
typedef struct {
    union {
        struct spi_4line_config spi_4line;
        struct qspi_config qspi;
    } trans_config;
	struct blacklight_config blacklight;
	pin_info_t reset;
	pin_info_t te;

#if CONFIG_LISA_DISPLAY_BUSY_SYNC
	pin_info_t busy;
#endif

} display_hw_config_t;


/**
 * @brief Display pixel formats
 *
 * Display pixel format enumeration.
 *
 * In case a pixel format consists out of multiple bytes the byte order is
 * big endian.
 */
enum display_pixel_format {
	PIXEL_FORMAT_RGB_888 = 0 << 1,
	PIXEL_FORMAT_ARGB_8888 = 1 << 1,
	PIXEL_FORMAT_RGB_565 = 2 << 1,
	PIXEL_FORMAT_BGR_565 = 3 << 1,
	PIXEL_FORMAT_MONO_1 = 4 << 1,
};

/**
 * @enum display_orientation
 * @brief Enumeration with possible display orientation
 *
 */
enum display_orientation {
	DISPLAY_ORIENTATION_NORMAL,
	DISPLAY_ORIENTATION_ROTATED_90,
	DISPLAY_ORIENTATION_ROTATED_180,
	DISPLAY_ORIENTATION_ROTATED_270,
};

/** @brief Structure holding display capabilities. */
struct display_capabilities {
	/** Display resolution in the X direction */
	uint16_t x_resolution;
	/** Display resolution in the Y direction */
	uint16_t y_resolution;
	/** Bitwise or of pixel formats supported by the display */
	uint32_t supported_pixel_formats;
	/** Currently active pixel format for the display */
	enum display_pixel_format current_pixel_format;
	/** Current display orientation */
	enum display_orientation current_orientation;
};

/** @brief Structure to describe display data buffer layout */
struct display_buffer_descriptor {
	/** Data buffer size in bytes */
	uint32_t buf_size;
	/** Data buffer row width in pixels */
	uint16_t width;
	/** Data buffer column height in pixels */
	uint16_t height;
	/** Number of pixels between consecutive rows in the data buffer */
	uint16_t pitch;
};

/**
 * @brief Display driver API
 * API which a display driver should expose
 */
struct display_driver_api {
	int (*display_blanking_on)(void);
	int (*display_blanking_off)(void);
	int (*display_set_brightness)(const uint8_t brightness);
	void (*display_get_capabilities)(struct display_capabilities *capabilities);
	int (*display_write)(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
			     const void *buf);
	int (*display_set_orientation)(const enum display_orientation orientation);
	int (*display_sleep)(const uint8_t onoff);
	int (*display_color_invert)(const uint8_t onoff);
};

/** @brief Structure display device. */
struct display_device {
	const char *name;
	int (*device_init)(display_hw_config_t *config);
	const struct display_driver_api *api;
};

/**
 * @brief get display device
 *
 * @retval Pointer to device structure on success else NULL
 */
void *lisa_display_create(display_hw_config_t *config);

/**
 * @brief Turn display blanking on
 *
 * This function blanks the complete display.
 * The content of the frame buffer will be retained while blanking is enabled
 * and the frame buffer will be accessible for read and write operations.
 *
 * In case backlight control is supported by the driver the backlight is
 * turned off. The backlight configuration is retained and accessible for
 * configuration.
 *
 * In case the driver supports display blanking the initial state of the driver
 * would be the same as if this function was called.
 *
 * @param dev Pointer to device structure
 * @retval 0 on success else negative errno code.
 */
int lisa_display_blanking_on(const struct display_device *dev);

/**
 * @brief Turn display blanking off
 *
 * Restore the frame buffer content to the display.
 * In case backlight control is supported by the driver the backlight
 * configuration is restored.
 *
 * @param dev Pointer to device structure
 * @retval 0 on success else negative errno code.
 */
int lisa_display_blanking_off(const struct display_device *dev);

/**
 * @brief Get display capabilities
 *
 * @param dev Pointer to device structure
 * @param capabilities Pointer to capabilities structure to populate
 */
void lisa_display_get_capabilities(const struct display_device *dev, struct display_capabilities *capabilities);

/**
 * @brief Set the brightness of the display
 *
 * Set the brightness of the display in steps of 1/100, where 100 is full
 * brightness and 0 is minimal.
 *
 * @param dev Pointer to device structure
 * @param brightness Brightness in steps of 1/100
 *
 * @retval 0 on success else negative errno code.
 */
int lisa_display_set_brightness(const struct display_device *dev, const uint8_t brightness);

/**
 * @brief Write data to display
 *
 * @param dev Pointer to device structure
 * @param x x Coordinate of the upper left corner where to write the buffer
 * @param y y Coordinate of the upper left corner where to write the buffer
 * @param desc Pointer to a structure describing the buffer layout
 * @param buf Pointer to buffer array
 *
 * @retval 0 on success else negative errno code.
 */
int lisa_display_write(const struct display_device *dev, const uint16_t x, const uint16_t y,
		       const struct display_buffer_descriptor *desc, const void *buf);

/**
 * @brief Set display orientation
 *
 * @param dev Pointer to device structure
 * @param orientation Display orientation
 * @retval 0 on success else negative errno code.
 */
int lisa_display_set_orientation(const struct display_device *dev, const enum display_orientation orientation);

/**
 * @brief Set display sleep
 *
 * @param dev Pointer to device structure
 * @param onoff  1: go to seleep,0:out from sleep
 * @retval 0 on success else negative errno code.
 */
int lisa_display_sleep(const struct display_device *dev, const uint8_t onoff);

/**
 * @brief Get display device
 *
 * @retval Pointer to device structure on success else NULL
 */
const struct display_device *lisa_display_get(void);

#ifdef __cplusplus
}
#endif