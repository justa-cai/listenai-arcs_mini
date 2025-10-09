/**
 ****************************************************************************************
 *
 * @file aud_os_task.c
 *
 * @brief AUD OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUD OS TASK
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "aud_os_task.h"
//#include "aud_mgr.h"

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
static os_task_cb_t *aud_os_cb = NULL;

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 void aud_os_init(os_task_cb_t *cb)
{
    if(cb)
    {
        aud_os_cb = cb;
    }
}
void aud_os_task(void *args)
{
    btos_event_t event;
    os_task_cb_t *aud_cb = aud_os_cb;
    uint8_t msg_free = 0;
    
    if(aud_cb == NULL)
    {
        CLOGD("AUD os task init failed!\n"); 
        return;
    }
    CLOGD("AUD os task start!\n");
    
    aud_cb->cb_os_init(OS_TASK_INIT);

    while(1)
    {
        btos_wait_event(OS_TASK_ID_AUD, &event, BTOS_TASK_MAX_DELAY);
        /// process bt msg handle.
        msg_free = aud_cb->cb_os_msg_handle(&event);
        /// process user schedule.
        if(aud_cb->cb_os_user_schedule)
        {
            aud_cb->cb_os_user_schedule();
        }
        //1:need to free, 0:donot free, maybe msg send from isr,msg_body is not malloc.
        //CLOGD("aud:%d,0x%x",msg_free, event.msg_body->msg_id);
        if(msg_free)
        {
            btos_free(event.msg_body);
        }
        event.msg_body = NULL;
    }
}



/// @} AUD OS TASK
