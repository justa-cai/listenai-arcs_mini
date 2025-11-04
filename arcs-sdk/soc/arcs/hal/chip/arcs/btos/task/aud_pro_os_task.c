/**
 ****************************************************************************************
 *
 * @file aud_pro_os_task.c
 *
 * @brief AUD PRO OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUD PRO OS TASK
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "aud_pro_os_task.h"

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
static os_task_cb_t *aud_pro_os_cb = NULL;

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void aud_pro_os_init(os_task_cb_t *cb)
{
    if(cb)
    {
        aud_pro_os_cb = cb;
    }
}
void aud_pro_os_task(void *args)
{
    btos_event_t event;
    os_task_cb_t *aud_pro_cb = aud_pro_os_cb;
    uint8_t msg_free = 0;

    if(aud_pro_cb == NULL)
    {
        CLOGD("AUD pro os task init failed!\n"); 
        return;
    }
    CLOGD("AUD pro os task start!\n");
    
    aud_pro_cb->cb_os_init(OS_TASK_INIT);

    while(1)
    {
        btos_wait_event(OS_TASK_ID_AUD_PRO, &event, BTOS_TASK_MAX_DELAY);
        /// process bt msg handle.
        msg_free = aud_pro_cb->cb_os_msg_handle(&event);
        /// process user schedule.
        if(aud_pro_cb->cb_os_user_schedule)
        {
            aud_pro_cb->cb_os_user_schedule();
        }
        //1:need to free, 0:donot free, maybe msg send from isr,msg_body is not malloc.
        //if(event.msg_body->msg_id !=AUD_MSG_PLAY_DATA_IND)
        //CLOGD("pro:%d,0x%x",msg_free, event.msg_body->msg_id);
        if(msg_free)
        {
            btos_free(event.msg_body);
        }
        event.msg_body = NULL;
    }
}



/// @} AUD OS TASK
