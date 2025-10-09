/*
 * aud_pro_if.c
 *
 *  audio interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"
#include "aud_pro.h"
#include "aud_pro_if.h"
#include "aud_pro_os_task.h"

#include "codec_lc3.h"

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
static const os_task_cb_t aud_pro_os_if_cb = {
    .cb_os_init          = aud_pro_if_init,
    .cb_os_msg_handle    = aud_pro_if_msg_handle,
    .cb_os_user_schedule = aud_pro_if_user_schedule,
};

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
os_task_cb_t *aud_pro_if_get_cb(void)
{
    return (os_task_cb_t *)&aud_pro_os_if_cb;
}
void aud_pro_if_init(uint8_t type)
{
}
uint8_t aud_pro_if_msg_handle(btos_event_t* msg)
{
    uint8_t msg_free = 1;
    btos_event_t *event = msg;
    if(event->msg_body)
    {
        //CLOGD("aud pro rcv msg,id:%d", event->msg_body->msg_id);
    	msg_free  = aud_pro_msg_handle(event->msg_body->msg_id, event->msg_body->param_len, event->msg_body->param);
    }
    return msg_free;
}

uint8_t aud_pro_if_user_schedule(void)
{
    /// if need
	return 1;
}
 
