/**
 * @file model_qrcode.h
 * @brief QR code data model interface
 * 
 * Provides data access for QR code display and management.
 */

#ifndef __MODEL_QRCODE_H__
#define __MODEL_QRCODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief QR code status enumeration
 */
typedef enum {
    QR_STATUS_NOT_CONNECTED = 0,  /**< Network not connected, show BLE QR */
    QR_STATUS_CONNECTED,           /**< Network connected, show cloud QR */
    QR_STATUS_OUT_OF_LIMIT,        /**< Usage limit exceeded */
} qrcode_status_t;

/**
 * @brief QR code data structure
 */
typedef struct {
    qrcode_status_t status;        /**< Current QR code status */
    const char *top_text;          /**< Top instruction text */
    const char *bottom_text;       /**< Bottom information text */
    const void *qr_image;          /**< QR code image source (C array) */
} qrcode_data_t;

/**
 * @brief Initialize QR code model
 * 
 * @return 0 on success, negative on error
 */
int model_qrcode_init(void);

/**
 * @brief Get current QR code status
 * 
 * @return Current QR code status
 */
qrcode_status_t model_qrcode_get_status(void);

/**
 * @brief Get QR code data for display
 * 
 * @param data Pointer to receive QR code data
 * @return 0 on success, negative on error
 */
int model_qrcode_get_data(qrcode_data_t *data);

int model_qrcode_get_config_data(qrcode_data_t *data);

int model_qrcode_get_quota_data(qrcode_data_t *data);

/**
 * @brief Update QR code from cloud (RGB565 format)
 * 
 * @param rgb565_data RGB565 image data
 * @param width Image width
 * @param height Image height
 * @param data_size Data size in bytes
 * @return 0 on success, negative on error
 */
int model_qrcode_update_cloud_qr(const uint16_t *rgb565_data, uint16_t width, uint16_t height, uint32_t data_size);

/**
 * @brief Set QR code status
 * 
 * @param status New status
 * @return 0 on success, negative on error
 */
int model_qrcode_set_status(qrcode_status_t status);

/**
 * @brief Set custom text for QR code page
 * 
 * @param top_text Top text (NULL to use default)
 * @param bottom_text Bottom text (NULL to use default)
 * @return 0 on success, negative on error
 */
int model_qrcode_set_text(const char *top_text, const char *bottom_text);

#ifdef __cplusplus
}
#endif

#endif /* __MODEL_QRCODE_H__ */
