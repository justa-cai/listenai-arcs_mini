/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "../utils/acomp_err.h"
#include "ipc/acomp_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

/* Translation callback event definitions */
#define TRANS_CB_EVENT_RESULT BIT(0) /* Translation result notification */
#define TRANS_CB_EVENT_STATUS BIT(1) /* Translation status notification */

typedef void (*trans_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief Initialize Translation component
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_NO_MEM if out of memory
 * @retval ACOMP_ERR_INVALID_STATE if already initialized
 * @retval ACOMP_ERR_NOT_FOUND if device not found
 */
extern int acomp_translation_init(void);

/**
 * @brief Prepare Translation component with resource configuration
 *
 * @param prepare[in] Prepare data structure pointer
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_ARG if prepare is NULL
 */
extern int acomp_translation_prepare(acomp_ipc_prepare_t *prepare);

/**
 * @brief Start Translation component
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_translation_start(void);

/**
 * @brief Stop Translation component
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_translation_stop(void);

/**
 * @brief Cleanup Translation component resources
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_translation_cleanup(void);

/**
 * @brief Send text for translation
 *
 * @param text[in] Text string to translate
 * @param len[in] Length of text in bytes
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_ARG if text is NULL or len is 0
 * @retval ACOMP_ERR_NO_MEM if out of memory
 */
extern int acomp_translation_translate(const char *text, uint32_t len);

/**
 * @brief Set translation result type
 *
 * @param type[in] Result type value
 *
 * @return ACOMP_ERR_OK on success
 */
extern int acomp_translation_set_res_type(int type);

/**
 * @brief Add event callback for Translation component
 *
 * @param events[in] Event bitmask to subscribe to
 * @param cb[in] Callback function pointer
 * @param priv[in] Private data passed to callback
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 */
extern int acomp_translation_add_callback(uint32_t events, trans_event_cb_t cb, void *priv);

/**
 * @brief Remove event callback from Translation component
 *
 * @param cb[in] Callback function to remove
 *
 * @return ACOMP_ERR_OK on success
 * @retval ACOMP_ERR_INVALID_STATE if not initialized
 */
extern int acomp_translation_remove_callback(trans_event_cb_t cb);

/**
 * @brief Prepare Translation resources and start the component
 *
 * Handles init, callback registration, resource loading (all from eMMC),
 * and start. Resource addresses come from Kconfig.
 * Does NOT clean up other algorithms — caller must coordinate.
 *
 * @param event_cb  Event callback (registered on first call, can be NULL)
 * @param cb_priv   Private data passed to callback
 * @return 0 on success, negative on error
 */
extern int acomp_translation_do_prepare(trans_event_cb_t event_cb, void *cb_priv);

/**
 * @brief Stop and cleanup Translation component
 *
 * @return 0 on success
 */
extern int acomp_translation_do_cleanup(void);

#ifdef __cplusplus
}
#endif
