/**
 ****************************************************************************************
 *
 * @file ipc_types.h
 *
 * @brief Public IPC wire-format types and IDs shared by AP/CP.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */
#ifndef _IPC_TYPES_H_
#define _IPC_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_MSG_SEGMENT
#define IPC_MSG_SEGMENT_MAX                       3

#if defined(IPC_MSG_SEGMENT) && (IPC_MSG_SEGMENT_MAX < 2)
#error "IPC_MSG_SEGMENT_MAX should >= 2"
#endif

#define CO_BIT(pos)                               (1UL << (pos))

#define IPC_ERR_OK                                (0)
#define IPC_ERR_INVALID                           (-1)
#define IPC_ERR_NO_MEM                            (-2)
#define IPC_ERR_BUFF_SIZE                         (-3)
#define IPC_ERR_PARAM                             (-4)
#define IPC_ERR_TIMEOUT                           (-5)
#define IPC_ERR_NO_BUFF                           (-6)
#define IPC_ERR_NOT_READY                         (-7)

#define IPC_CHAN_FLAGS_NO_LOCK                    0x00000001
#define IPC_CHAN_FLAGS_USER_MODE                  0x00000002
#define IPC_CHAN_FLAGS_SIGNAL                     0x00000004
#define IPC_CHAN_FLAGS_REMOTE                     0x80000000

#define CHAN_LOCAL                                0
#define CHAN_REMOTE                               1

#define IPC_IRQ_ID_OFFSET                         0
#define IPC_IRQ_ID_MASK                           0x3F
#define IPC_LINK_ID_OFFSET                        6
#define IPC_LINK_ID_MASK                          0x3

#define IPC_TIMEOUT                               200

#define IPC_DBG_STATUS_ACTIVE                     1
#define IPC_DBG_STATUS_SLEEP                      0

enum
{
    CORE_ID_MASTER,
    CORE_ID_SLAVE,
};

#define IPC_GET_LINK_ID_FROM_CHAN(chan)           ((chan >> IPC_LINK_ID_OFFSET) & IPC_LINK_ID_MASK)
#define IPC_GET_IRQ_ID_FROM_CHAN(chan)            ((chan >> IPC_IRQ_ID_OFFSET) & IPC_IRQ_ID_MASK)

#define IPC_SIG_LINKUP                            0x00000001
#define IPC_SIG_HALT                              0x00000002
#define IPC_SIG_PRINT                             0x00000004
#define IPC_SIG_VRTC_SET                          0x00000008
#define IPC_SIG_VRTC_ALERT                        0x00000010
#define IPC_SIG_ENTER_IDLE                        0x00000020
#define IPC_SIG_WAKEUP                            0x00000040

#define IPC_CHAN_ANY                              (-1)
#define IPC_EP_ANY                                (-1)

/*
 * IPC message buffers are smaller than 256 bytes, so the high byte of the
 * per-segment length can carry a request sequence without changing the shared
 * memory layout.
 */
#define IPC_EPMSG_LEN_MASK                        0x00FFU
#define IPC_EPMSG_SEQ_MASK                        0x00FFU
#define IPC_EPMSG_SEQ_SHIFT                       8U
#define IPC_EPMSG_PACK_LEN_SEQ(len, seq)          \
    ((uint16_t)((((uint16_t)(seq) & IPC_EPMSG_SEQ_MASK) << IPC_EPMSG_SEQ_SHIFT) | \
                ((uint16_t)(len) & IPC_EPMSG_LEN_MASK)))
#define IPC_EPMSG_GET_LEN(epmsg)                  ((uint16_t)((epmsg)->hdr.len & IPC_EPMSG_LEN_MASK))
#define IPC_EPMSG_GET_SEQ(epmsg)                  ((uint16_t)(((epmsg)->hdr.len >> IPC_EPMSG_SEQ_SHIFT) & IPC_EPMSG_SEQ_MASK))

/* The IPC channel ID consists of the link ID and IPC IRQ number. */
enum
{
    IPC_CHAN_MASTER_WIFI_TXCFM = (CORE_ID_MASTER << IPC_LINK_ID_OFFSET) | (0 << IPC_IRQ_ID_OFFSET),
    IPC_CHAN_MASTER_WIFI_RXDESC,
    IPC_CHAN_MASTER_SIGNAL,
    IPC_CHAN_MASTER_MSG,
    IPC_CHAN_MASTER_BT_H2C,
    IPC_CHAN_MASTER_MAX,
};

enum
{
    IPC_CHAN_SLAVE_WIFI_TXDESC = (CORE_ID_SLAVE << IPC_LINK_ID_OFFSET) | (0 << IPC_IRQ_ID_OFFSET),
    IPC_CHAN_SLAVE_WIFI_RXCFM,
    IPC_CHAN_SLAVE_SIGNAL,
    IPC_CHAN_SLAVE_MSG,
    IPC_CHAN_SLAVE_BT_C2H,
    IPC_CHAN_SLAVE_MAX,
};

enum
{
    IPC_EP_IND,
    IPC_EP_MRPC_SRV,
    IPC_EP_MRPC_CLT,
    IPC_EP_MRPC_SRV_TEST,
    IPC_EP_MRPC_CLT_TEST,
    IPC_EP_MRPC_CMN_SRV,
    IPC_EP_MRPC_CMN_CLT,
    IPC_EP_MRPC_BUS_CLT,
    IPC_EP_MRPC_BUS_SRV,
    IPC_EP_MAX,
};

enum
{
    IPC_MSG_RELEASE,
    IPC_MSG_HOLD,
};

struct vring_desc
{
    uint32_t addr;
    uint16_t len;
    uint16_t flags;
};

struct vring_ready
{
    uint16_t desc_idx;
};

struct vring_avail
{
    uint16_t desc_idx;
};

struct vring_hdr
{
    uint32_t status;
    uint16_t ready_wr_idx;
    uint16_t avail_wr_idx;
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_ready *ready;
};

union ipc_eid
{
    uint16_t val;
    struct
    {
        uint8_t chan;
        uint8_t ep;
    } id;
};

struct ipc_epmsg_hdr
{
    union ipc_eid dst_id;
    union ipc_eid src_id;
#ifdef IPC_MSG_SEGMENT
    void *next;
#endif
    uint16_t desc_idx;
    uint16_t len;
};

struct ipc_epmsg
{
    struct ipc_epmsg_hdr hdr;
    uint8_t data[];
};

struct ipc_msg_hdr
{
    uint16_t id;
    uint16_t len;
    uint32_t data[];
};

struct ipc_msg_desc_hdr
{
    union ipc_eid dst_id;
    union ipc_eid src_id;
    uint16_t flags;
    uint16_t seq;
    uint16_t data_len;
#ifdef IPC_MSG_SEGMENT
    uint32_t total_len;
#endif
};

struct ipc_msg_desc
{
    struct ipc_msg_desc_hdr hdr;
    void *data;
#ifdef IPC_MSG_SEGMENT
    void *seg[IPC_MSG_SEGMENT_MAX - 1];
#endif
};

struct ipc_signal
{
    uint32_t state;
};

struct ipc_ep;
typedef int32_t (*ipc_ep_handler_t)(struct ipc_msg_desc *msg, void *arg);

#define EPMSG_FROM_BUF(buf)                      ((struct ipc_epmsg *)((char *)(buf) - offsetof(struct ipc_epmsg, data)))
#define IPC_GET_EPMSG_LEN(buf)                   (IPC_EPMSG_GET_LEN(EPMSG_FROM_BUF(buf)))

#endif
