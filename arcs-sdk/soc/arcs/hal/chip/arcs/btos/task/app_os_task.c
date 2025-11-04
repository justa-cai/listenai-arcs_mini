/**
 ****************************************************************************************
 *
 * @file app_os_task.c
 *
 * @brief APP OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP OS TASK
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "app_os_task.h"
//#include "app_if.h"

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
static os_task_cb_t *app_os_cb = NULL;
/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void app_task_timer_to_bt_test( TimerHandle_t xTimer )
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t));
    ev.msg_body->msg_id = 0;
    ev.msg_body->param_len = 0;
    btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}
void app_task_timer_to_aud_test( TimerHandle_t xTimer )
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t));
    ev.msg_body->msg_id = 0;
    ev.msg_body->param_len = 0;
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

void app_os_init(os_task_cb_t *cb)
{
    if(cb)
    {
        app_os_cb = cb;
    }
}

void app_os_task(void *args)
{
    btos_event_t event;
    os_task_cb_t *app_cb = app_os_cb;
    uint8_t msg_free = 0;

    if(app_cb == NULL)
    {
        CLOGD("APP os task init failed!\n");
        return;
    }
    
    CLOGD("App os task start!\n");
    
    app_cb->cb_os_init(OS_TASK_INIT);

    while(1)
    {
        btos_wait_event(OS_TASK_ID_APP, &event, BTOS_TASK_MAX_DELAY);
        /// process app msg handle.
        msg_free = app_cb->cb_os_msg_handle(&event);
        /// process user schedule.
        if(app_cb->cb_os_user_schedule)
        {
            app_cb->cb_os_user_schedule();
        }
        //1:need to free, 0:donot free, maybe msg send from isr,msg_body is not malloc.
        if(msg_free)
        {
            btos_free(event.msg_body);
            event.msg_body = NULL;
        }

        //btos_timer_creat(TIMER_TYPE_PERIODIC, 1000, app_task_timer_to_bt_test);
        //btos_timer_creat(TIMER_TYPE_PERIODIC, 2000, app_task_timer_to_aud_test);
    }
}



/// @} APP OS TASK
