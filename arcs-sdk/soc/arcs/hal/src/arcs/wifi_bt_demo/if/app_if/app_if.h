/**
 ****************************************************************************************
 *
 * @file app_if.h
 *
 * @brief Header file - APP INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef APP_IF_H_
#define APP_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "app_os_task.h"
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

void app_if_init(uint8_t type);
uint8_t app_if_msg_handle(btos_event_t* msg);
uint8_t app_if_user_schedule(void);

/// @} APP INTERFACE
#endif // APP_IF_H_
