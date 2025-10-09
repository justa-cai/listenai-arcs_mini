/**
 ****************************************************************************************
 *
 * @file llc.h
 *
 * @brief LLC module interface include file
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef _LLC_H_
#define _LLC_H_

/** @defgroup LLC LLC
 * @ingroup UMAC
 * @brief LLC translation definitions
 * @{
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

//#include "co_int.h"
//#include "co_bool.h"

/*
 * DEFINES
 ****************************************************************************************
 */
/// @name LLC field definitions
/// LLC = DSAP + SSAP + CTRL
/// @note: The 0xAA values indicate the presence of the SNAP
/// @{

/// LLC definition
#define LLC_LLC_LEN                  3
#define LLC_DSAP                     0xAA
#define LLC_SSAP                     0xAA
#define LLC_CTRL                     0x03
/// @}

/// @name SNAP field definitions
/// SNAP = OUI + ETHER_TYPE
/// @{

/// SNAP definition
#define LLC_SNAP_LEN                 5
#define LLC_OUI_LEN                  3
/// @}

/// 802.2 Header (LLC + SNAP) Length
#define LLC_802_2_HDR_LEN            (LLC_LLC_LEN + LLC_SNAP_LEN)

/// @name 802.2 Ethernet definitions
/// @{

/// Ethernet MTU
#define LLC_ETHER_MTU                1500
/// Size of the Ethernet header
#define LLC_ETHER_HDR_LEN            14
/// Size of the Ethernet type
#define LLC_ETHERTYPE_LEN            2
/// Offset of the Ethernet type within the header
#define LLC_ETHERTYPE_LEN_OFT        12
/// Offset of the EtherType in the 802.2 header
#define LLC_802_2_ETHERTYPE_OFT      6
/// Maximum value of the frame length
#define LLC_FRAMELENGTH_MAXVALUE     0x600
/// @}

/// @name Useful EtherType values
/// @{

/// EtherType values
#define LLC_ETHERTYPE_APPLETALK      0x809B
#define LLC_ETHERTYPE_IPX            0x8137
#define LLC_ETHERTYPE_AARP           0x80F3
#define LLC_ETHERTYPE_EAP_T          0x888E
#define LLC_ETHERTYPE_802_1Q         0x8100
#define LLC_ETHERTYPE_IP             0x0800
#define LLC_ETHERTYPE_ARP            0x0806
/// @}

/// @name 802_1Q VLAN header definitions
/// @{

/// VLAN definition
#define LLC_802_1Q_TCI_VID_MASK       0x0FFF
#define LLC_802_1Q_TCI_PRI_MASK       0xE000
#define LLC_802_1Q_TCI_CFI_MASK       0x1000
#define LLC_802_1Q_HDR_LEN            4
#define LLC_802_1Q_VID_NULL           0
#define LLC_OFFSET_ETHERTYPE          12
#define LLC_OFFSET_IPV4_PRIO          15
#define LLC_OFFSET_802_1Q_TCI         14
/// @}


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// LLC/SNAP structure
struct llc_snap_short
{
    /// DSAP + SSAP fieldsr
    uint16_t dsap_ssap;
    /// LLC control + OUI byte 0
    uint16_t control_oui0;
    /// OUI bytes 1 and 2
    uint16_t oui1_2;
};

/// LLC/SNAP structure
struct llc_snap
{
    /// DSAP + SSAP fieldsr
    uint16_t dsap_ssap;
    /// LLC control + OUI byte 0
    uint16_t control_oui0;
    /// OUI bytes 1 and 2
    uint16_t oui1_2;
    /// Protocol
    uint16_t proto_id;
};

/*
 * CONSTANTS
 ****************************************************************************************
 */
/// RFC1042 LLC/SNAP Header
extern const struct llc_snap_short llc_rfc1042_hdr;

/// Bridge-Tunnel LLC/SNAP Header
extern const struct llc_snap_short llc_bridge_tunnel_hdr;

/**
 ****************************************************************************************
 * @brief Check whether buffer contains a known LLC/SNAP
 *
 * @param[in] buf Buffer to check. Shall be at least sizeof(struct llc_snap) long
 * @return True if buffers starts with an  RFC1042 or Bridge tunnel LLC/SNAP header
 ****************************************************************************************
 */
bool llc_snap_present(void *buf);

/// @}

#endif // _LLC_H_
