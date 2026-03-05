/*
 * bt_ipc_api.h
 *
 *  bt ipc function
 */

/*
 * INCLUDES
 ****************************************************************************************
 */

#include "string.h"
#include "ls_event.h"
#include "ls_bt_type.h"
#include "btos_def.h"


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


ls_err_t btos_send_at_evt_api(btos_task_id task_id, uint16_t msg_id, void *msg_body, uint32_t msg_body_len, TickType_t time_out);
ls_err_t btos_send_app_evt_api(btos_task_id task_id, uint16_t msg_id, void * msg_body, uint32_t msg_body_len, TickType_t time_out);
ls_err_t btos_malloc_api(void **buffer_ptr, uint32_t size);
ls_err_t btos_free_api(void **ptr);
ls_err_t ble_gap_set_loc_pub_addr_api(struct out_bd_addr *bd_addr);
ls_err_t llm_get_local_pub_addr_api(struct out_bd_addr *bd_addr);
ls_err_t lsip_reset_api(void);

