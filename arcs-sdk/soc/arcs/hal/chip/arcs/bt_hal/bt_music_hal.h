/**
 ****************************************************************************************
 *
 * @file bt_music_hal.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_MUSIC_HAL_H_
#define BT_MUSIC_HAL_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "bt_a2dp.h"
/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_stack_a2dp_enable(bt_a2dp_cfg_t *a2dp_cfg);
uint8_t bt_stack_a2dp_send_start(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);
void bt_stack_a2dp_send_stop(uint8_t conidx, uint8_t status);
void bt_stack_a2dp_send_data(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);
void bt_stack_a2dp_caps_set(uint8_t role, bt_a2dp_meida_caps_cfg_t *caps_cfg);
void bt_stack_a2dp_connection_update(uint8_t conidx, bool connected);
void bt_stack_a2dp_send_media_rsp(uint8_t conidx);
/// @} BT STACK
#endif // BT_MUSIC_IF_H_
