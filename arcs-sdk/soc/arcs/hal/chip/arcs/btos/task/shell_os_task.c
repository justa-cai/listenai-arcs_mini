/**
 ****************************************************************************************
 *
 * @file bt_os_task.c
 *
 * @brief BT OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BT OS TASK
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "shell_os_task.h"
#include "btos_al.h"

/*
 * EXPORTED FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
extern int32_t app_shell_init(void);
extern uint32_t app_shell_input_data(uint8_t* buffer, int32_t len);
extern int32_t app_shell_parse(uint8_t key);
/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 
void shell_os_task(void *args)
{
    btos_event_t event;
    int32_t len, i;
    uint8_t data;
        
    CLOGD("Shell os task start!\n");
    
    app_shell_init();
    app_shell_input_data(&data, 1);
    while(1)
    {
        /// start read uart data.
        btos_wait_event(OS_TASK_ID_SHELL, &event, BTOS_TASK_MAX_DELAY);
        if(event.msg_body == NULL)
        {
            CLOGD("SHELL event msg body is NULL!\n");
            continue;
        }
        switch(event.msg_body->msg_id)
        {
            case SHELL_READ_DATA_CMP_EVT : 
            {
                //CLOGD("Shell os task rcv uart data:%d\n", data);
                app_shell_parse(data);
                /// read next data.
                app_shell_input_data(&data, 1);
            }break;
            default :
            {
                CLOGD("Shell os task rcv unknown msg : %d!\n", event.msg_body->msg_id);
            }break;
        }
        btos_free(event.msg_body);
        event.msg_body = NULL;

    }
}


/// @} BT OS TASK
