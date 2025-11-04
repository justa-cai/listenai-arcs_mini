/**
 ****************************************************************************************
 *
 * @file app_task.h
 *
 * @brief Header file - APPTASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef OS_TASK_INIT_H_
#define OS_TASK_INIT_H_

/**
 ****************************************************************************************
 * @addtogroup APPTASK Task
 * @ingroup APP
 * @brief Routes ALL messages to/from APP block.
 *
 * The APPTASK is the block responsible for bridging the final application with the
 * BLE software host stack. It communicates with the different modules of the BLE host,
 * i.e. @ref SMP, @ref GAP and @ref GATT.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "btos_al.h"

/*
 * DEFINES
 ****************************************************************************************
 */

typedef struct os_task_cb
{
    /**
     ****************************************************************************************
     * @brief Call back when the os init.
     *
     * @param type      Initialize type @see enum os_init_type
     ****************************************************************************************
     */
    void (*cb_os_init)(uint8_t type);

    /**
     ****************************************************************************************
     * @brief Process message of os task.
     * @param return     0:free msg,1:resume msg
     ****************************************************************************************
     */
    uint8_t (*cb_os_msg_handle)(btos_event_t* msg);

    /**
     ****************************************************************************************
     * @brief execute user schedule one step
     ****************************************************************************************
     */
    uint8_t (*cb_os_user_schedule)(void);

} os_task_cb_t;

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
/// Types of initialization of the IP
enum os_init_type
{
    OS_TASK_INIT    = 0,
    OS_TASK_RST,
};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
uint8_t os_task_init(uint8_t *args);

/// @} APPTASK


#endif // APP_TASK_H_
