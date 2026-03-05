/*
 * bt_ipc_api.c
 *
 *  bt ipc interface
 */

/*
 * INCLUDES
 ****************************************************************************************
 */

#include "string.h"
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include "bt_config.h"


#include "ls_event.h"
#include "ls_bt_type.h"

#include "btos_def.h"
#include "btos_al.h"
#include "bt_ipc_api.h"
#include "atcmd_bt_if.h"

#include "log_print.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

extern void *btos_malloc(uint32_t size);
ls_err_t btos_malloc_api(void **buffer_ptr, uint32_t size)
{
	if(buffer_ptr == NULL)
	{
		return LS_FAIL;
	}
    *buffer_ptr = btos_malloc(size);

    return LS_OK;
}

extern void btos_free(void *ptr);
ls_err_t btos_free_api(void **ptr)
{
    btos_free(*ptr);

    return LS_OK;
}

/*************
send event throuth IPC, 
the event id is event.msg_body->msg_id

uint32_t event_len: is used for ipc memcpy event->msg_body->param

*************/
extern uint8_t btos_send_event(btos_task_id task_id, btos_event_t *event, TickType_t time_out);
ls_err_t btos_send_event_api(btos_task_id task_id, void **event, btos_msg_t *msg_body, uint32_t msg_body_len, TickType_t time_out)
{
	//btos_event_t *btos_event_ptr = (btos_event_t*)*event;
    memcpy(*event, msg_body, msg_body_len);
    return btos_send_event(task_id, (btos_event_t*)event, time_out);
}

/*
send at command event throuth IPC, 
the event id is BT_OS_AT_SEND_EVT
*/
ls_err_t btos_send_at_evt_api(btos_task_id task_id, uint16_t msg_id, void *msg_body, uint32_t msg_body_len, TickType_t time_out)
{
    btos_event_t event;
    bt_at_cmd_t *at_cmd;

    CLOGD("btos_send_api:0x%x", msg_id);
    at_cmd = atcmd_msg_alloc(&event, msg_body_len+sizeof(bt_at_cmd_t));

    at_cmd->at_id = msg_id;
    at_cmd->data_len = msg_body_len;

    if(0 !=msg_body_len)
    {
        memcpy(at_cmd->data, msg_body, msg_body_len);
    }

    return btos_send_event(task_id, &event, time_out);
}

/*
send app event throuth IPC, 
the event id is input msg_id
*/
ls_err_t btos_send_app_evt_api(btos_task_id task_id, uint16_t msg_id, void *msg_body, uint32_t msg_body_len, TickType_t time_out)
{
    btos_event_t app_event;

    CLOGD("btos_send_app_evt_api:0x%x", msg_id);

#ifdef CFG_AMP_IPC
    btos_malloc_api(&(app_event.msg_body), sizeof(btos_msg_t)+msg_body_len);
#else
    app_event.msg_body = btos_malloc(sizeof(btos_msg_t) + msg_body_len);
#endif
    
    app_event.msg_body->msg_id = msg_id;
    app_event.msg_body->param_len = msg_body_len;
    
    if(0 !=msg_body_len)
    {
        memcpy(app_event.msg_body->param, msg_body, msg_body_len);
    }

    return btos_send_event(task_id, &app_event, time_out);
}




extern void ble_gap_set_loc_pub_addr(uint8_t *addr);
ls_err_t ble_gap_set_loc_pub_addr_api(struct out_bd_addr *bd_addr)
{
    ble_gap_set_loc_pub_addr(bd_addr->addr);

    return LS_OK;
}

extern void llm_get_local_pub_addr(uint8_t *addr);
ls_err_t llm_get_local_pub_addr_api(struct out_bd_addr *bd_addr)
{
    llm_get_local_pub_addr(bd_addr->addr);

    return LS_OK;
}

ls_err_t lsip_reset_api(void)
{
	lsip_reset();
	return LS_OK;
}
