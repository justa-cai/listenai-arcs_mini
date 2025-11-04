/**
 ****************************************************************************************
 *
 * @file nvds_tag_def.h
 *
 * @brief  nvds tag define
 *
 * Copyright (C) ListenAI 2024-2099
 *
 ****************************************************************************************
 */
#ifndef _NVDS_TAG_DEF_H_
#define _NVDS_TAG_DEF_H_

/// List of  NVDS TAG identifiers
enum ls_nvds_tag
{
// tag 0x00 ~ 0xff reserved for BT/BLE
// Please don't define any TAG between 0x00 ~ 0xff,
// as the tag may be defined in other place used for BT/BLE 


// Tag 0x100 ~ 0x120 reserved for wifi
NVDS_TAG_WIFI_STA_SSID        = 0x100,
NVDS_LEN_WIFI_STA_SSID        = 32,

NVDS_TAG_WIFI_STA_PWD         = 0x101,
NVDS_LEN_WIFI_STA_PWD         = 64,

NVDS_TAG_WIFI_STA_AUTOCONN    = 0x102,
NVDS_LEN_WIFI_STA_AUTOCONN    = 1,

NVDS_TAG_WIFI_MAC_ADDR        = 0x103,
NVDS_LEN_WIFI_MAC_ADDR        = 6,

NVDS_TAG_WIFI_CHANNEL        = 0x104,
NVDS_LEN_WIFI_CHANNEL        = 1,

NVDS_TAG_DHCP_IP_ADDR        = 0x105,
NVDS_LEN_IP_ADDR_ADDR        = 4,

// Other for customer define


};


/// @}

#endif
