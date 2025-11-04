/*
 * remote_ble.c
 *
 *  remote ble functions
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

#include "app_if.h"

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
static const os_task_cb_t app_os_if_cb = {
    .cb_os_init          = app_if_init,
    .cb_os_msg_handle    = app_if_msg_handle,
    .cb_os_user_schedule = app_if_user_schedule,
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
os_task_cb_t *app_if_get_cb(void)
{
    return (os_task_cb_t *)&app_os_if_cb;
}
int app_nvs_init(uint8_t *base, uint32_t len)
{
    nvds_init(base);

    return 0;
}

void app_if_init(uint8_t type)
{
    //app_nvs_init(NULL, 256);
}

uint8_t app_if_msg_handle(btos_event_t* msg)
{
    btos_event_t *event = msg;
    uint8_t msg_free = 1;
    
    if(event->msg_body)
    {
        switch(event->msg_body->msg_id)
        {
            case APP_START_EVT:
            {

            }break;
            default:
            {

            }break;
        }
    }
    return msg_free;
}

uint8_t app_if_user_schedule(void)
{
    /// if need
    return 1;
}

