/**
 ****************************************************************************************
 *
 * @file bt_call_if.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_CALL_IF_H_
#define BT_CALL_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "bt_hfp.h"
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
void bt_stack_hfp_enable(bt_hfp_cfg_t *cfg);
void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status);
void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);

/// @} BT STACK
#endif // BT_CALL_IF_H_
