/**
 ****************************************************************************************
 *
 * @file ipc_bt_hci.h
 *
 * @brief BT HCI IPC transport - Common definitions
 *
 * Defines HCI packet types, shared memory ring structures, and callback types
 * for BT HCI transport over IPC between dual cores.
 *
 * Architecture:
 *   AP (Master) = BT Controller
 *   CP (Slave)  = BT Host
 *   H2C: Host -> Controller (CMD, ACL TX, SCO TX, ISO TX)
 *   C2H: Controller -> Host (EVT, ACL RX, SCO RX, ISO RX)
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */
#ifndef _IPC_BT_HCI_H_
#define _IPC_BT_HCI_H_

#include <stdint.h>
#include <stdbool.h>
#include "rtos_al.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/** HCI packet type indicators (H:4 transport, BT Core Spec Vol4 Part A) */
#define HCI_PKT_TYPE_CMD    0x01
#define HCI_PKT_TYPE_ACL    0x02
#define HCI_PKT_TYPE_SCO    0x03
#define HCI_PKT_TYPE_EVT    0x04
#define HCI_PKT_TYPE_ISO    0x05

/** HCI event codes used internally */
#define HCI_EVT_CMD_COMPLETE        0x0E
#define HCI_EVT_CMD_STATUS          0x0F
#define HCI_EVT_NUM_COMP_PKTS      0x13
#define HCI_EVT_LE_META             0x3E
#define HCI_EVT_VENDOR_SPECIFIC     0xFF

/** LE Meta Event sub-event codes */
#define HCI_LE_EVT_ADV_REPORT      0x02
#define HCI_LE_EVT_EXT_ADV_REPORT  0x0D

/** HCI packet header sizes */
#define HCI_CMD_HDR_SIZE        3       /**< [opcode:2][param_len:1] */
#define HCI_EVT_HDR_SIZE        2       /**< [evt_code:1][param_len:1] */
#define HCI_ACL_HDR_SIZE        4       /**< [handle_flags:2][data_len:2] */
#define HCI_SCO_HDR_SIZE        3       /**< [handle_flags:2][data_len:1] */
#define HCI_ISO_HDR_SIZE        4       /**< [handle_flags:2][data_len:2] */

/** ISO data_len field is 14 bits per BT Core Spec 5.2 Vol4 Part E §5.4.5 */
#define HCI_ISO_DATA_LEN_MASK   0x3FFF

/** Timeout (ms) for queue write in handler, avoids silent packet drop */
#define BT_IPC_QUEUE_WRITE_TIMEOUT  50
#define BT_IPC_HCI_BUF_SIZE      1060

/**
 * IPC send retry configuration (inspired by Zephyr CONFIG_BT_HCI_IPC_SEND_RETRY).
 *
 * When the IPC ring buffer is full (IPC_ERR_NO_BUFF), the send will retry
 * up to BT_IPC_SEND_RETRY_COUNT times, sleeping BT_IPC_SEND_RETRY_DELAY_MS
 * between each attempt.
 */
#define BT_IPC_SEND_RETRY_COUNT     3   /**< Max retries on ring-full */
#define BT_IPC_SEND_RETRY_DELAY_MS  1   /**< Delay between retries (ms) */

/**
 * BT IPC channel IDs
 * Placed after existing IPC channels (TXCFM=0, RXDESC=1, FAST=2, MSG=3)
 * IRQ ID = 4 for both directions (each in its own core domain)
 */

/** BT IPC task configuration */
#define BT_IPC_TASK_STACK_SIZE  1024
#define BT_IPC_TASK_PRIORITY    RTOS_TASK_PRIORITY(8)

/*
 * STRUCTURES
 ****************************************************************************************
 */

/**
 * HCI packet wrapper for IPC ring buffer.
 * Each ring entry carries one complete HCI packet.
 *
 * Note: packed attribute ensures consistent layout across AP/CP cores
 * even if compiled with different toolchain flags.
 */

/*
 * STATISTICS
 ****************************************************************************************
 */

/**
 * BT IPC transport statistics.
 *
 * Tracks packet counts and errors for monitoring and debugging.
 * Inspired by Zephyr's HCI IPC driver logging/counters.
 */
struct bt_ipc_stats
{
    volatile uint32_t tx_count;        /**< Total packets sent */
    volatile uint32_t rx_count;        /**< Total packets received */
    volatile uint32_t tx_err_count;    /**< Send failures (after all retries) */
    volatile uint32_t tx_retry_count;  /**< Total retry attempts */
    volatile uint32_t rx_err_count;    /**< Receive errors (corrupt/invalid) */
    volatile uint32_t rx_drop_count;   /**< Received but dropped (queue full, discardable, etc.) */
};

/*
 * DISCARDABLE EVENT HELPER
 ****************************************************************************************
 */

/**
 * @brief Check if an HCI event is discardable (inspired by Zephyr is_hci_event_discardable).
 *
 * Discardable events (e.g., LE Advertising Reports) can be silently dropped
 * when the receive queue is full, rather than blocking the IPC receive path.
 * This prevents advertising floods from stalling other critical HCI traffic.
 *
 * @param evt_data  Pointer to HCI event data starting at evt_code
 * @param len       Length of event data
 * @return true if the event can be safely dropped
 */
static inline bool bt_ipc_is_evt_discardable(const uint8_t *evt_data, uint16_t len)
{
    if (len < HCI_EVT_HDR_SIZE)
        return false;

    uint8_t evt_code = evt_data[0];

    switch (evt_code)
    {
        case HCI_EVT_LE_META:
        {
            /* LE Meta Event - check sub-event code */
            if (len < (HCI_EVT_HDR_SIZE + 1))
                return false;

            uint8_t subevent = evt_data[HCI_EVT_HDR_SIZE];

            switch (subevent)
            {
                case HCI_LE_EVT_ADV_REPORT:
                case HCI_LE_EVT_EXT_ADV_REPORT:
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

/*
 * CALLBACK TYPES
 ****************************************************************************************
 */

/**
 * Callback for receiving HCI packets from IPC.
 *
 * @param type  HCI packet type (HCI_PKT_TYPE_xxx)
 * @param data  HCI packet data (content depends on type)
 * @param len   Data length in bytes
 */
typedef void (*bt_ipc_hci_recv_cb_t)(const uint8_t *data, uint16_t len);

#endif /* _IPC_BT_HCI_H_ */
