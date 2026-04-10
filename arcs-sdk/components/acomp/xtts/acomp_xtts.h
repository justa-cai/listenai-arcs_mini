/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "../utils/acomp_err.h"
#include "acomp_stream_ipc.h"
#include "ipc/acomp_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

/* XTTS callback event definitions */
#define XTTS_CB_EVENT_STATUS        BIT(0) /* Synthesis status notification */
#define XTTS_CB_EVENT_STREAM_UPDATE BIT(1) /* Stream data update (PCM RX) */

typedef void (*xtts_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief Initialize XTTS component
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_NO_MEM if out of memory
 * @retval ACOMP_ERR_INVALID_STATE if already initialized
 * @retval ACOMP_ERR_NOT_FOUND if device not found
 */
extern int acomp_xtts_init(void);

/**
 * @brief Prepare XTTS component with resource configuration
 *
 * @param prepare[in] Prepare data structure pointer
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_ARG if prepare is NULL
 */
extern int acomp_xtts_prepare(acomp_ipc_prepare_t *prepare);

/**
 * @brief Start XTTS component
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_start(void);

/**
 * @brief Stop XTTS component
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_stop(void);

/**
 * @brief Cleanup XTTS component resources
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_cleanup(void);

/**
 * @brief Synthesize text to speech
 *
 * @param text[in] Text string to synthesize
 * @param len[in] Length of text in bytes
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_ARG if text is NULL or len is 0
 * @retval ACOMP_ERR_NO_MEM if out of memory
 */
extern int acomp_xtts_synth_text(const char *text, uint32_t len);

/**
 * @brief Set speech speed
 *
 * @param speed[in] Speech speed value
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_set_speed(int speed);

/**
 * @brief Set speech volume
 *
 * @param volume[in] Speech volume value
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_set_volume(int volume);

/**
 * @brief Set speech role/voice
 *
 * @param role[in] Role/voice identifier
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_xtts_set_role(int role);

/**
 * @brief Add event callback for XTTS component
 *
 * @param events[in] Event bitmask to subscribe to
 * @param cb[in] Callback function pointer
 * @param priv[in] Private data passed to callback
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 */
extern int acomp_xtts_add_callback(uint32_t events, xtts_event_cb_t cb, void *priv);

/**
 * @brief Remove event callback from XTTS component
 *
 * @param cb[in] Callback function to remove
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 */
extern int acomp_xtts_remove_callback(xtts_event_cb_t cb);

/**
 * @brief Enable stream channel for receiving PCM audio
 *
 * @param chn[in] Channel index
 * @param desc[in] Channel create descriptor
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 * @retval ACOMP_ERR_INVALID_ARG if channel index out of range
 * @retval ACOMP_ERR_CREATE_STREAM_FAILED if channel creation failed
 */
extern int acomp_xtts_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

/**
 * @brief Disable stream channel
 *
 * @param chn[in] Channel index
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 * @retval ACOMP_ERR_INVALID_ARG if channel index out of range
 */
extern int acomp_xtts_stream_ch_disable(int chn);

/**
 * @brief Get RX buffer from stream channel (receive PCM audio from AP)
 *
 * @param chn[in] Channel index
 * @param len[out] Available buffer length pointer
 * @param desc_idx[out] Descriptor index pointer
 *
 * @return Buffer pointer, NULL on failure
 */
extern void *acomp_xtts_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);

/**
 * @brief Release RX buffer back to stream channel
 *
 * @param chn[in] Channel index
 * @param desc_idx[in] Descriptor index
 * @param len[in] Data length
 * @param buffer[in] Buffer pointer
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 * @retval ACOMP_ERR_INVALID_ARG if channel index out of range
 */
extern int acomp_xtts_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);

/**
 * @brief Prepare XTTS resources and start the component
 *
 * Handles init, callback registration, resource loading (flash + eMMC),
 * stream channel setup, and start. Resource addresses come from Kconfig.
 * Does NOT clean up other algorithms — caller must coordinate.
 *
 * @param event_cb  Event callback (registered on first call, can be NULL)
 * @param cb_priv   Private data passed to callback
 * @return 0 on success, negative on error
 */
extern int acomp_xtts_do_prepare(xtts_event_cb_t event_cb, void *cb_priv);

/**
 * @brief Stop and cleanup XTTS component
 *
 * @return 0 on success
 */
extern int acomp_xtts_do_cleanup(void);

#ifdef __cplusplus
}
#endif
