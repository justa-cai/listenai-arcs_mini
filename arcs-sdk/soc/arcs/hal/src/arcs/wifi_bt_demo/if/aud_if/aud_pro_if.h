/**
 ****************************************************************************************
 *
 * @file aud_if.h
 *
 * @brief Header file - AUDIO INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef AUDIO_IF_H_
#define AUDIO_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "aud_os_task.h"
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
void aud_pro_if_init(uint8_t type);
uint8_t aud_pro_if_msg_handle(btos_event_t* msg);
uint8_t aud_pro_if_user_schedule(void);
os_task_cb_t *aud_pro_if_get_cb(void);

/// @} AUDIO INTERFACE
#endif // AUDIO_IF_H_
