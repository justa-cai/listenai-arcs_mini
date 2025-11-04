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
#include "bt_os_task.h"

/*
 * EXPORTED FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

static os_task_cb_t *bt_os_cb = NULL;

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void bt_os_init(os_task_cb_t *cb)
{
    if(cb)
    {
        bt_os_cb = cb;
    }
}
void bt_os_task(void *args)
{
    btos_event_t event;
    os_task_cb_t *bt_cb = bt_os_cb;
    uint8_t msg_free = 0;
    
    if(bt_cb == NULL)
    {
        CLOGD("BT os task init failed!\n"); 
        return;
    }
    
    CLOGD("BT os task start!\n"); 
    
    bt_cb->cb_os_init(OS_TASK_INIT);

    while(1)
    {
        btos_wait_event(OS_TASK_ID_BT, &event, BTOS_TASK_MAX_DELAY);
        /// process bt msg handle.
        msg_free = bt_cb->cb_os_msg_handle(&event);
        /// process user schedule.
        if(bt_cb->cb_os_user_schedule)
        {
            bt_cb->cb_os_user_schedule();
        }
        //0:need to free, 1:donot free, maybe msg send from isr,msg_body is not malloc.
        //CLOGD("bt:%d,0x%x",msg_free, event.msg_body->msg_id);
        if(msg_free)//if(event.msg_body->msg_id != BT_OS_NOTIFY_EVT)
        {
            btos_free(event.msg_body);
        }
        event.msg_body = NULL;
    }
}


/// @} BT OS TASK
