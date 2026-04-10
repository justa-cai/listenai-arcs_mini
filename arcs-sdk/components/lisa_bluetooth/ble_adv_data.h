/**
 * @file ble_adv_data.h
 * @brief BLE Advertising Data Builder Macros
 *
 * Provides helper macros for constructing BLE AD structures in a readable,
 * type-safe manner. Inspired by Zephyr's BT_DATA_BYTES pattern.
 *
 * Usage example:
 * @code
 * static const uint8_t adv_data[] = {
 *     BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
 *     BLE_AD_COMPLETE_NAME("MyDevice"),
 *     BLE_AD_MFG_DATA(0xAB, 0x0A, 0x01, 0x02),
 * };
 * @endcode
 *
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BLE_ADV_DATA_H__
#define __BLE_ADV_DATA_H__

#include "lisa_bluetooth_gap.h"

/**
 * @brief Generic AD structure with explicit data count
 *
 * @param _type  AD type (e.g. GAP_AD_TYPE_FLAGS)
 * @param _n     Number of data bytes that follow
 * @param ...    Data bytes
 */
#define BLE_AD_BYTES(_type, _n, ...) \
    ((_n) + 1), (_type), __VA_ARGS__

/**
 * @brief AD Flags structure (always 3 bytes total)
 *
 * @param _flags  Combination of GAP_AD_TYPE_FLAGS_* values
 */
#define BLE_AD_FLAGS(_flags) \
    0x02, GAP_AD_TYPE_FLAGS, (_flags)

/**
 * @brief Complete Local Name
 *
 * @param _n    Name length in bytes
 * @param ...   Individual character bytes (e.g. 'M','y','D','e','v')
 *
 * Example: BLE_AD_COMPLETE_NAME(4, 'T','E','S','T')
 */
#define BLE_AD_COMPLETE_NAME(_n, ...) \
    ((_n) + 1), GAP_AD_TYPE_LOCAL_NAME_COMPLETE, __VA_ARGS__

/**
 * @brief Shortened Local Name
 *
 * @param _n    Name length in bytes
 * @param ...   Individual character bytes
 */
#define BLE_AD_SHORT_NAME(_n, ...) \
    ((_n) + 1), GAP_AD_TYPE_LOCAL_NAME_SHORT, __VA_ARGS__

/**
 * @brief Appearance (always 4 bytes total)
 *
 * @param _appearance  16-bit appearance value
 */
#define BLE_AD_APPEARANCE(_appearance) \
    0x03, GAP_AD_TYPE_APPEARANCE, \
    ((_appearance) & 0xFF), (((_appearance) >> 8) & 0xFF)

/**
 * @brief Manufacturer Specific Data with explicit byte count
 *
 * @param _n   Number of data bytes (company ID + payload)
 * @param ...  Raw bytes
 */
#define BLE_AD_MFG_DATA(_n, ...) \
    ((_n) + 1), GAP_AD_TYPE_MANUFACTURER_SPECIFIC, __VA_ARGS__

/**
 * @brief Complete List of 16-bit Service UUIDs with explicit byte count
 *
 * @param _n   Number of UUID bytes (2 per UUID)
 * @param ...  UUID bytes (little-endian)
 */
#define BLE_AD_UUID16_COMPLETE(_n, ...) \
    ((_n) + 1), GAP_AD_TYPE_16BIT_COMPLETE, __VA_ARGS__

/**
 * @brief TX Power Level (always 3 bytes total)
 *
 * @param _level  TX power level in dBm (-127 to +127)
 */
#define BLE_AD_TX_POWER(_level) \
    0x02, GAP_AD_TYPE_POWER_LEVEL, (_level)

/**
 * @brief Service Data - 16-bit UUID with explicit byte count
 *
 * @param _n        Total number of data bytes (2 for UUID + payload)
 * @param _uuid_lo  Low byte of 16-bit UUID
 * @param _uuid_hi  High byte of 16-bit UUID
 * @param ...       Service data bytes
 */
#define BLE_AD_SERVICE_DATA_16(_n, _uuid_lo, _uuid_hi, ...) \
    ((_n) + 1), GAP_AD_TYPE_SERVICE_DATA, (_uuid_lo), (_uuid_hi), __VA_ARGS__

#endif /* __BLE_ADV_DATA_H__ */
