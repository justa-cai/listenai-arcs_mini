/**
 * @file lisa_bluetooth_gap.h
 * @brief 
 * @version 0.1
 * @date 2025-12-05
 * 
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISA_BLUETOOTH_GAP_H__
#define __LISA_BLUETOOTH_GAP_H__


// GAP_AD_TYPE_DEFINES GAP Advertisement Data Types
#define GAP_AD_TYPE_FLAGS                        0x01 //!< Discovery Mode: @ref GAP_ADTYPE_FLAGS_MODES
#define GAP_AD_TYPE_16BIT_MORE                   0x02 //!< Service: More 16-bit UUIDs available
#define GAP_AD_TYPE_16BIT_COMPLETE               0x03 //!< Service: Complete list of 16-bit UUIDs
#define GAP_AD_TYPE_32BIT_MORE                   0x04 //!< Service: More 32-bit UUIDs available
#define GAP_AD_TYPE_32BIT_COMPLETE               0x05 //!< Service: Complete list of 32-bit UUIDs
#define GAP_AD_TYPE_128BIT_MORE                  0x06 //!< Service: More 128-bit UUIDs available
#define GAP_AD_TYPE_128BIT_COMPLETE              0x07 //!< Service: Complete list of 128-bit UUIDs
#define GAP_AD_TYPE_LOCAL_NAME_SHORT             0x08 //!< Shortened local name
#define GAP_AD_TYPE_LOCAL_NAME_COMPLETE          0x09 //!< Complete local name
#define GAP_AD_TYPE_POWER_LEVEL                  0x0A //!< TX Power Level: -127 to +127 dBm
#define GAP_AD_TYPE_OOB_CLASS_OF_DEVICE          0x0D //!< Simple Pairing OOB Tag: Class of device (3 octets)
#define GAP_AD_TYPE_OOB_SIMPLE_PAIRING_HASHC     0x0E //!< Simple Pairing OOB Tag: Simple Pairing Hash C (16 octets)
#define GAP_AD_TYPE_OOB_SIMPLE_PAIRING_RANDR     0x0F //!< Simple Pairing OOB Tag: Simple Pairing Randomizer R (16 octets)
#define GAP_AD_TYPE_SM_TK                        0x10 //!< Security Manager TK Value
#define GAP_AD_TYPE_SM_OOB_FLAG                  0x11 //!< Security Manager OOB Flags
#define GAP_AD_TYPE_SLAVE_CONN_INTERVAL_RANGE    0x12 //!< Min and Max values of the connection interval (2 octets Min, 2 octets Max) (0xFFFF indicates no conn interval min or max)
#define GAP_AD_TYPE_SIGNED_DATA                  0x13 //!< Signed Data field
#define GAP_AD_TYPE_SERVICES_LIST_16BIT          0x14 //!< Service Solicitation: list of 16-bit Service UUIDs
#define GAP_AD_TYPE_SERVICES_LIST_128BIT         0x15 //!< Service Solicitation: list of 128-bit Service UUIDs
#define GAP_AD_TYPE_SERVICE_DATA                 0x16 //!< Service Data - 16-bit UUID
#define GAP_AD_TYPE_PUBLIC_TARGET_ADDR           0x17 //!< Public Target Address
#define GAP_AD_TYPE_RANDOM_TARGET_ADDR           0x18 //!< Random Target Address
#define GAP_AD_TYPE_APPEARANCE                   0x19 //!< Appearance
#define GAP_AD_TYPE_ADV_INTERVAL                 0x1A //!< Advertising Interval
#define GAP_AD_TYPE_LE_BD_ADDR                   0x1B //!< LE Bluetooth Device Address
#define GAP_AD_TYPE_LE_ROLE                      0x1C //!< LE Role
#define GAP_AD_TYPE_SIMPLE_PAIRING_HASHC_256     0x1D //!< Simple Pairing Hash C-256
#define GAP_AD_TYPE_SIMPLE_PAIRING_RANDR_256     0x1E //!< Simple Pairing Randomizer R-256
#define GAP_AD_TYPE_SERVICE_DATA_32BIT           0x20 //!< Service Data - 32-bit UUID
#define GAP_AD_TYPE_SERVICE_DATA_128BIT          0x21 //!< Service Data - 128-bit UUID
#define GAP_AD_TYPE_LE_SC_CONFIRMATION_VALUE     0x22 //!< LE Secure Connections Confirmation Value
#define GAP_AD_TYPE_LE_SC_RANDOM_VALUE           0x23 //!< LE Secure Connections Random Value
#define GAP_AD_TYPE_URI                          0x24 //!< URI
#define GAP_AD_TYPE_INDOOR_POSITION              0x25 //!< Indoor Positioning Service v1.0 or later
#define GAP_AD_TYPE_TRAN_DISCOVERY_DATA          0x26 //!< Transport Discovery Service v1.0 or later
#define GAP_AD_TYPE_SUPPORTED_FEATURES           0x27 //!< LE Supported Features
#define GAP_AD_TYPE_CHANNEL_MAP_UPDATE           0x28 //!< Channel Map Update Indication
#define GAP_AD_TYPE_PB_ADV                       0x29 //!< PB-ADV. Mesh Profile Specification Section 5.2.1
#define GAP_AD_TYPE_MESH_MESSAGE                 0x2A //!< Mesh Message. Mesh Profile Specification Section 3.3.1
#define GAP_AD_TYPE_MESH_BEACON                  0x2B //!< Mesh Beacon. Mesh Profile Specification Section 3.9
#define GAP_AD_TYPE_BIG_INFO                     0x2C //!< BIGInfo
#define GAP_AD_TYPE_BROADCAST_CODE               0x2D //!< Broadcast_Code
#define GAP_AD_TYPE_RSL_SET_IDENT                0x2E //!< Resolvable Set Identifier.Coordinated Set Identification Profile 1.0
#define GAP_AD_TYPE_ADV_INTERVAL_LONG            0x2F //!< Advertising Interval - long
#define GAP_AD_TYPE_BROADCAST_NAME               0x30 //!< Public Broadcast Profile v1.0 or later
#define GAP_AD_TYPE_ENCRYPTED_ADV_DATA           0x31 //!< Core Specification Supplement, Part A, Section 1.23
#define GAP_AD_TYPE_PERI_ADV_RSP_TIMING_INFO     0x32 //!< Periodic Advertising Response Timing Information
#define GAP_AD_TYPE_ELECTRONIC_SHELF_LABEL       0x34 //!< ESL Profile
#define GAP_AD_TYPE_3D_INFO_DATA                 0x3D //!< 3D Information Data
#define GAP_AD_TYPE_MANUFACTURER_SPECIFIC        0xFF //!< Manufacturer Specific Data: first 2 octets contain the Company Identifier Code followed by the additional manufacturer specific data

// GAP_AD_TYPE_FLAGS_MODES GAP ADTYPE Flags Discovery Modes
#define GAP_AD_TYPE_FLAGS_LIMITED                (0x01<<0) //!< Discovery Mode: LE Limited Discoverable Mode
#define GAP_AD_TYPE_FLAGS_GENERAL                (0x01<<1) //!< Discovery Mode: LE General Discoverable Mode
#define GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED    (0x01<<2) //!< Discovery Mode: BR/EDR Not Supported
#define GAP_AD_TYPE_FLAGS_SIMUL_LE_BREDR         (0x01<<3) //!< Discovery Mode: Simultaneous LE and BR/EDR to Same Device Capable (Controller)
#define GAP_AD_TYPE_FLAGS_SIMUL_LE_BREDR_HOST    (0x01<<4) //!< Discovery Mode: Simultaneous LE and BR/EDR to Same Device Capable (Host)


#endif /* __LISA_BLUETOOTH_GAP_H__ */