/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_USB_CDC_H
#define APP_USB_CDC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB CDC audio streaming with protocol support
 * @return 0 on success, negative on error
 */
int app_usb_cdc_init(void);

/**
 * @brief Write audio data to CDC interface using protocol
 * @param data Audio data buffer (PCM data)
 * @param len Audio data length
 * @return 0 on success, negative on error
 */
int app_usb_cdc_audio_write(uint8_t *data, uint32_t len);

/**
 * @brief Send MD5 hash of recorded audio
 * @param md5 MD5 hash buffer (16 bytes)
 * @return 0 on success, negative on error
 */
int app_usb_cdc_send_md5(const uint8_t *md5);

/**
 * @brief Get current recording state
 * @return true if recording, false otherwise
 */
bool app_usb_cdc_is_recording(void);

/**
 * @brief Get total bytes sent
 * @return Total bytes of audio data sent
 */
uint32_t app_usb_cdc_get_total_bytes(void);

/**
 * @brief Get current sequence number
 * @return Current sequence number
 */
uint32_t app_usb_cdc_get_seq_num(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_USB_CDC_H */
