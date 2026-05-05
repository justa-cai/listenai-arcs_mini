/**
 ****************************************************************************************
 *
 * @file atcmd_ble.h
 *
 * @brief BLE AT Command Header
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef _ATCMD_BLE_H_
#define _ATCMD_BLE_H_

#include <stdint.h>
#include <stdbool.h>

/*
AD type |    AD data    |    	define
----------------------------------------------------------------
0x01 	|    0x05		|		LE 有限发现模式 &	不支持 BR/EDR
----------------------------------------------------------------
0x09    |	0x43，0x4d  |
		|	0x43，0x43  |
		|	0x5f, 0x56	|
		|	0x6f, 0x69	|
		|	0x63, 0x65	|		"CMCC_Voice_Remote"
		|	0x5f, 0x52	|
		|	0x65, 0x6d	|
		|	0x6f, 0x74	|
		|	0x65		|
----------------------------------------------------------------
0x19	|	0xc1, 0x03	|		键盘
----------------------------------------------------------------
0xff	|	0x49, 0x46,	|		
		|   0x4c, 0x59, |		"IFLY RC"
		|	0x20, 0x52, |
		|	0x43		|
----------------------------------------------------------------

*/
#include "ls_bt_type.h"

#include "btos_al.h"
#include "os_task_init.h"
#include "event.h"
#define BLE_ADV_DATA_LEN        31
#define ATCMD_DEFAULT_BLE_ADV_DATA {0x02, 0x01, 0x05, 0x05, 0xFF, 0x66, 0x79, 0x30, 0x02, 0x12, 0x09, 0x43, 0x4D, 0x43, 0x43, 0x5F, 0x56, 0x6F, 0x69, 0x63, 0x65, 0x5F, 0x52, 0x65, 0x6D, 0x6F, 0x74, 0x65}
#define ATCMD_DEFAULT_BLE_SCAN_RSP_DATA {0x03, 0x19, 0xC1, 0x03, 0x08, 0xFF, 0x49, 0x46, 0x4C, 0x59, 0x20, 0x52, 0x43}
uint8_t g_default_dis_reason = 0x13; // Reason code for disconnection, default to "Remote User Terminated Connection"


typedef struct atcmd_msg_type
{
    uint16_t        event_id;
    uint16_t        data_len;
    uint8_t         data[__ARRAY_EMPTY];
}atcmd_msg_t;

/**
 * @brief Register BLE Host AT commands
 */
void atcmd_ble_host_register(void);

/**
 * @brief Display BLE Host AT command help information
 */
void atcmd_ble_host_help(void);

#endif /* _ATCMD_BLE_H_ */

