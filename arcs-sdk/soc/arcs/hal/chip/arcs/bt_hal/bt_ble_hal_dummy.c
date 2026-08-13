/*
 * bt_ble_hal_dummy.c
 *
 *  bt stack ble dummy functions
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

#include "bt_stack_cfg.h"
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
extern uint8_t app_ble_hogpd_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);

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
        btos_timer_cancel(dumy_time_handle_id);
        dumy_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_voice_send_timer_cb);
    }
}

void bt_voice_send_by_timer_stop(void)
{
    if(dumy_time_handle_id != NULL)
    {
        btos_timer_cancel(dumy_time_handle_id);
        dumy_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_voice_send_timer_cb);
    }
}

#endif

#if BLE_HID_SEND_DUMMY_MOUSE_DATA
enum
{
  REPORT_ID_MOUSE = 1,
  REPORT_ID_KEYBOARD,
  REPORT_ID_VOICE,
  REPORT_ID_COUNT
};

#define HID_RPT_FLAG_OPSS    (0x1 << 0) // optical sensor (X, Y) changed
#define HID_RPT_FLAG_WHEEL   (0x1 << 1) // wheel changed
#define HID_RPT_FLAG_MKEY    (0x1 << 2) // mouse keypad state changed
#define HID_RPT_FLAG_MOUSE   (HID_RPT_FLAG_OPSS | HID_RPT_FLAG_WHEEL | HID_RPT_FLAG_MKEY)

typedef struct app_hid_mouse_report
{
  uint8_t buttons; /**< buttons mask for currently pressed buttons in the mouse. */
  int8_t  x;       /**< Current delta x movement of the mouse. */
  int8_t  y;       /**< Current delta y movement on the mouse. */
  int8_t  wheel;   /**< Current delta wheel movement on the mouse. */
  int8_t  pan;     // using AC Pan
} app_hid_mouse_report;

TimerHandle_t mouse_dumy_time_handle_id = NULL;

app_hid_mouse_report mouse_dumy_array;

void bt_mouse_send_timer_cb(TimerHandle_t time_id)
{
    //static uint32_t time1 = 0, time2 = 0, time3 = 0;

    static uint32_t send_num = 0;
    static uint32_t send_value = 1;

    //time1 = co_time_us_get();
    
    send_value = 20-send_num;

    //time2 = co_time_us_get();
    mouse_dumy_array.buttons = 0;

    mouse_dumy_array.x = send_value;
    mouse_dumy_array.y = send_value;
    mouse_dumy_array.wheel = 0;
    mouse_dumy_array.pan = 0;
    app_ble_hogpd_hid_send(0, HIDS_MOUSE_INDEX, sizeof(app_hid_mouse_report), (uint8_t *)&mouse_dumy_array);
    
    if(++send_num > 40)
    {
        send_num = 0;
    }
    CLOGD("x:%d,y:%d\n",mouse_dumy_array.x, mouse_dumy_array.y);
}

void bt_mouse_send_by_timer_start(uint32_t milli_seconds)
{
    if(mouse_dumy_time_handle_id == NULL)
    {
       mouse_dumy_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_mouse_send_timer_cb);
    }
    else
    {
        btos_timer_cancel(mouse_dumy_time_handle_id);
        mouse_dumy_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_mouse_send_timer_cb);
    }

}

void bt_mouse_send_by_timer_stop(void)
{
    if(mouse_dumy_time_handle_id != NULL)
    {
        btos_timer_cancel(mouse_dumy_time_handle_id);
        mouse_dumy_time_handle_id = NULL;
    }

}

#endif

