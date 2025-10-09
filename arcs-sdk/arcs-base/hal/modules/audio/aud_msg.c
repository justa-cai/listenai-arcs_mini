/**
 ****************************************************************************************
 * @file app_aud.c
 *
 * @brief  audio process source
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
 
#include <string.h>            // For memset
#include "aud_mgr.h"
#include "aud_pro.h"
#include "aud_msg.h"

/*
 * MACROS
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * FUNCTIONS DECLARATION
 ****************************************************************************************
 */
uint8_t aud_if_msg_send_to_mgr(uint16_t msg_id, uint16_t len, void *event);
uint8_t aud_if_msg_isr_send_to_mgr(uint16_t msg_id, uint16_t len, void *event);
uint8_t aud_if_msg_send_to_pro(uint16_t msg_id, uint16_t len, void *event);
/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
uint16_t aud_send_msg_to_mgr(uint16_t msg_id, uint16_t len, void *event)
{
	return (uint16_t)aud_if_msg_send_to_mgr(msg_id, len, event);
}

uint16_t aud_send_isr_msg_to_mgr(uint16_t msg_id, uint16_t len, void *event)
{
	return (uint16_t)aud_if_msg_isr_send_to_mgr(msg_id, len, event);
}
uint16_t aud_send_msg_to_pro(uint16_t msg_id,  uint16_t len, void *event)
{
	return (uint16_t)aud_if_msg_send_to_pro(msg_id, len, event);
}

/// @} AUDIO



