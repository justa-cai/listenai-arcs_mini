/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file lisa_bt_classic_api.h
 * @brief Unified BT Classic application API.
 *        Works transparently on AP core (local), CP core (IPC), or single-core.
 *        Covers A2DP, HFP, AVRCP, GAP, and Audio HAL profiles.
 *        Also includes BLE whitelist/resolve list helpers that depend on
 *        BT Classic pairing information.
 */

#ifndef __LISA_BT_CLASSIC_API_H__
#define __LISA_BT_CLASSIC_API_H__

#include <stdint.h>
#include <stdbool.h>
#include "bt_app_hal.h"

/*
 * A2DP API
 ****************************************************************************************
 */

/**
 * @brief Enable A2DP profile
 *
 * @param cfg  Pointer to bt_a2dp_enable_info_t (a2dp_role, aac_support)
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_enable(const void *cfg);

/**
 * @brief Initiate A2DP connection
 *
 * @param conidx  Connection index
 * @param role    A2DP role (source/sink)
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_connect(uint8_t conidx, uint8_t role);

/**
 * @brief Start A2DP streaming
 *
 * @param conidx  Connection index
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_start(uint8_t conidx);

/**
 * @brief Send A2DP media data to peer
 *
 * @param conidx    Connection index
 * @param frame_num Number of frames
 * @param len       Data length
 * @param data      Data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_send_media(uint8_t conidx, uint8_t frame_num, uint16_t len, const uint8_t *data);

/*
 * HFP API
 ****************************************************************************************
 */

/**
 * @brief Enable HFP profile
 *
 * @param cfg  Pointer to bt_hfp_enable_info_t (hfp_role, hfp_feats)
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_enable(const void *cfg);

/**
 * @brief Initiate HFP connection
 *
 * @param conidx    Connection index
 * @param peer_role Peer HFP role
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_connect(uint8_t conidx, uint8_t peer_role);

/**
 * @brief Set HFP codec type
 *
 * @param conidx     Connection index
 * @param codec_type Codec type
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_set_codec_type(uint8_t conidx, uint8_t codec_type);

/**
 * @brief Start HFP call
 *
 * @param conidx   Connection index
 * @param call_idx Call index
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_call_start(uint8_t conidx, uint8_t call_idx);

/**
 * @brief Add audio to HFP call (SCO link)
 *
 * @param conidx     Connection index
 * @param codec_type Codec type
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_call_add_audio(uint8_t conidx, uint8_t codec_type);

/**
 * @brief Remove audio from HFP call
 *
 * @param conidx Connection index
 * @param reason Reason code
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_call_remove_audio(uint8_t conidx, uint8_t reason);

/**
 * @brief Handle incoming HFP call
 *
 * @param conidx Connection index
 * @param idx    Incoming call index
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_call_incoming(uint8_t conidx, uint8_t idx);

/**
 * @brief Send HFP audio data to peer
 *
 * @param conidx Connection index
 * @param len    Data length
 * @param data   Audio data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_send_audio(uint8_t conidx, uint16_t len, const uint8_t *data);

/*
 * AVRCP API
 ****************************************************************************************
 */

/**
 * @brief Set AVRCP play status
 *
 * @param conidx      Connection index
 * @param play_status Play status value
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_avrcp_play_status_set(uint8_t conidx, uint8_t play_status);

/*
 * GAP API
 ****************************************************************************************
 */

/**
 * @brief Send authentication request
 *
 * @param conidx  Connection index
 * @param sec_req Security requirement
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_gap_auth_req(uint8_t conidx, uint8_t sec_req);

/**
 * @brief Save link key from memory to NVS
 *
 * @param conidx Connection index
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_gap_save_link_key(uint8_t conidx);

/**
 * @brief Set ASIC CVSD enable
 *
 * @param enable Enable flag (0 or 1)
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_set_asic_cvsd(uint8_t enable);

/**
 * @brief Initiate BT Classic connection
 *
 * @param addr               Pointer to peer BD address
 * @param type               Connection type
 * @param clk_off            Clock offset
 * @param page_scan_rep_mode Page scan repetition mode
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_connect(const gap_bdaddr_t *addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);

/**
 * @brief Start BT Classic inquiry
 *
 * @param disc_mode Discovery mode
 * @param max_count Maximum number of responses
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_inquiry_start(uint8_t disc_mode, uint8_t max_count);

/**
 * @brief Stop BT Classic inquiry
 *
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_inquiry_stop(void);

/**
 * @brief Enable/disable BT Classic scan (inquiry + page scan)
 *
 * @param enable Enable flag (0 or 1)
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_scan(uint8_t enable);

/*
 * Audio HAL API
 ****************************************************************************************
 */

/**
 * @brief Notify A2DP audio send start
 *
 * @param conidx      Connection index
 * @param codec       Codec type
 * @param ch          Channel count
 * @param sample_rate Sample rate
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_audio_start(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);

/**
 * @brief Notify A2DP audio send stop
 *
 * @param conidx Connection index
 * @param status Stop status
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_audio_stop(uint8_t conidx, uint8_t status);

/**
 * @brief Send A2DP audio data
 *
 * @param conidx    Connection index
 * @param frame_num Frame count
 * @param seq       Sequence number
 * @param len       Data length
 * @param data      Audio data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_a2dp_audio_send(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, const uint8_t *data);

/**
 * @brief Notify HFP audio send start
 *
 * @param conidx     Connection index
 * @param codec_type Codec type
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_audio_start(uint8_t conidx, uint8_t codec_type);

/**
 * @brief Notify HFP audio send stop
 *
 * @param conidx Connection index
 * @param reason Stop reason
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_audio_stop(uint8_t conidx, uint8_t reason);

/**
 * @brief Send HFP audio data
 *
 * @param conidx  Connection index
 * @param pkt_sta Packet status
 * @param len     Data length
 * @param data    Audio data buffer
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_hfp_audio_send(uint8_t conidx, uint8_t pkt_sta, uint16_t len, const uint8_t *data);

/*
 * BLE whitelist / resolve list API
 ****************************************************************************************
 */

/**
 * @brief Add paired devices to BLE white list
 *
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_ble_add_paired_to_wlist(void);

/**
 * @brief Add paired RPA devices to BLE resolve list
 *
 * @return 0 on success, non-zero on failure
 */
uint8_t lisa_bt_ble_add_paired_to_rlist(void);

#endif /* __LISA_BT_CLASSIC_API_H__ */
