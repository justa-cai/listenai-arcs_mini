/*
 * bt_stack_if.c
 *
 *  bt stack interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"

//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"

#include "bt_stack_if.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern uint16_t dis_profile_get_cb(uint8_t conidx, uint8_t att_idx, uint8_t *p_value, uint16_t max_len, uint16_t *ret_len);
extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
extern uint8_t app_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern uint8_t app_nvs_del(uint8_t param_id);
extern void gapc_con_param_clear_peer_feat(uint8_t conidx);
extern uint8_t ble_gap_get_ltk_nocon(gap_addr_t * addr, uint8_t *p_ltk);
extern uint8_t app_hid_rcv_data(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);

void bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
static void bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id);
static void  bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id);
uint8_t *bt_stack_vbat_percent_get(void);
void bt_stack_ble_parameter_update_by_timer(uint32_t milli_seconds);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

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
#if BLE_VOICE_SIMULATOR
TimerHandle_t dumy_time_handle_id = NULL;

uint8_t audio_dumy_array[124] = 
{
    0,0,0,0,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
    0, 1,2,3,4,5,6,7,8,9,
};

void bt_voice_send_timer_cb(TimerHandle_t time_id)
{
    //static uint32_t time1 = 0, time2 = 0, time3 = 0;

    static uint32_t send_num = 0;

    //time1 = co_time_us_get();
    

    //time2 = co_time_us_get();
    audio_dumy_array[0] = send_num & 0xff;
    audio_dumy_array[1] = (send_num & 0xff00) >> 8;
    audio_dumy_array[2] = (send_num & 0xff0000) >> 16;
    audio_dumy_array[3] = (send_num & 0xff000000) >> 24;
    app_ble_user_data_send(0, sizeof(audio_dumy_array), audio_dumy_array);
    send_num++;

}

void bt_voice_send_by_timer_start(uint32_t milli_seconds)
{
    if(dumy_time_handle_id == NULL)
    {
       dumy_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_voice_send_timer_cb);
    }
    else
    {
        btos_timer_stop(dumy_time_handle_id);
        dumy_time_handle_id = NULL;
    }
}

#endif


