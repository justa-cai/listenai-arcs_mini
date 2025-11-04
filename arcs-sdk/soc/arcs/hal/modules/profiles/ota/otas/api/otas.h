/**
****************************************************************************************
*
* @file otas.h
*
* @brief BLE OTA Service header
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

#ifndef SRC_BT_PROFILES_OTA_OTAS_API_OTAS_H_
#define SRC_BT_PROFILES_OTA_OTAS_API_OTAS_H_

#define OTA_DATA_MAX_LEN            (255)


uint16_t otas_init(uint8_t sec_lvl, uint8_t user_prio);

#endif /* SRC_BT_PROFILES_OTA_OTAS_API_OTAS_H_ */
