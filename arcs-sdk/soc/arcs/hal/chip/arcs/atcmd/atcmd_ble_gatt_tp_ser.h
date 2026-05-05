/**
****************************************************************************************
*
* @file tps.h
*
* @brief BLE TP Service header
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

#ifndef SRC_BT_PROFILES_TP_TPS_API_TPS_H_
#define SRC_BT_PROFILES_TP_TPS_API_TPS_H_

///must be small than tp client mtu 247.
#define TP_DATA_MAX_LEN            (247)
enum tp_cmd_type
{
    TP_CMD_TYPE_START = 0x01,
    TP_CMD_TYPE_STOP  = 0x02,
    TP_CMD_TYPE_READ_STATS = 0x03,
};
enum tp_test_mode
{
    TP_TEST_MODE_WC      = 0x01,
    TP_TEST_MODE_WR      = 0x02,
    TP_TEST_MODE_NOTIFY  = 0x03,
    TP_TEST_MODE_INDICATE= 0x04,
    TP_TEST_MODE_READ    = 0x05,
};

uint16_t tps_init(uint8_t sec_lvl, uint8_t user_prio);

#endif /* SRC_BT_PROFILES_TP_TPS_API_TPS_H_ */
