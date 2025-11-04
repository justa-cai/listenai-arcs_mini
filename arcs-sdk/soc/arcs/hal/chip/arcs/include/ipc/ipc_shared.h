/**
 ****************************************************************************************
 *
 * @file ipc_shared.h
 *
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _IPC_SHARED_H_
#define _IPC_SHARED_H_

#include <stdbool.h>
#include "ipc_core.h"


#define IPC_TXDESC_CNT                  16
#define IPC_TXCFM_CNT                   16
#define IPC_RXDESC_CNT                  16
#define IPC_RXCFM_CNT                   16
#define IPC_MSGC2A_BUF_CNT              16
#define IPC_MSGA2C_BUF_CNT              16


/// Size, in bytes, of IPC buffers for command
#define IPC_A2C_MSG_BUF_SIZE            96

/// Size, in bytes, of IPC buffers for response/print
#define IPC_C2A_MSG_BUF_SIZE            64


#define CO_BIT(pos)                     (1UL << (pos))

/*"Please do not modify it; its value is determined by the layout of IPC RAM in the .ld file.
 * Otherwise, you need to recompile the firmware."*/
#define IPC_SHARED_HDR_ADDR             0x20050000
#define IPC_PATTERN1                    0x4950435F
#define IPC_PATTERN2                    0x69736F6B
#define IPC_BUSY                        0x00000000    /*BUSY*/
#define IPC_READY                       0x52454459    /*REDY*/
#define IPC_CFG_SIZE                    128
#define IPC_WIFI_SHARE_SIZE             72

/*
 * Halt the other core
 */
#define IPC_APP_STATUS_HALT_PEER_BITS_ACK        0x00000001
#define IPC_APP_STATUS_HALT_PEER_BITS_RESUME     0x00000002
#define IPC_APP_STATUS_HALT_PEER_BITS_ALL        0x00000003

struct ipc_rxdesc
{
    void *data;
};

struct ipc_epmsg_rxdesc
{
    struct ipc_epmsg ephdr;
    struct ipc_rxdesc buf;
};

struct ipc_rxcfm
{
    void *data;
};

struct ipc_epmsg_rxcfm
{
    struct ipc_epmsg ephdr;
    struct ipc_rxcfm buf;
};

struct ipc_txdesc
{
    void *data;
};

struct ipc_epmsg_txdesc
{
    struct ipc_epmsg ephdr;
    struct ipc_txdesc buf;
};

struct ipc_txcfm
{
    void *data;
    uint32_t status;
};

struct ipc_epmsg_txcfm
{
    struct ipc_epmsg ephdr;
    struct ipc_txcfm buf;
};

struct ipc_a2c_buf
{
    uint32_t data[IPC_A2C_MSG_BUF_SIZE / 4]; ///< Message data
};

struct ipc_epmsg_a2c_msg
{
    struct ipc_epmsg ephdr;
    struct ipc_a2c_buf buf;
};

struct ipc_c2a_buf
{
    uint32_t data[IPC_C2A_MSG_BUF_SIZE / 4]; ///< Message data
};

struct ipc_epmsg_c2a_msg
{
    struct ipc_epmsg ephdr;
    struct ipc_c2a_buf buf;
};

struct ipc_txdesc_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_TXDESC_CNT];
    struct vring_avail avail[IPC_TXDESC_CNT];
    struct vring_ready ready[IPC_TXDESC_CNT];
    struct ipc_epmsg_txdesc items[IPC_TXDESC_CNT];
};

struct ipc_rxdesc_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_RXDESC_CNT];
    struct vring_avail avail[IPC_RXDESC_CNT];
    struct vring_ready ready[IPC_RXDESC_CNT];
    struct ipc_epmsg_rxdesc items[IPC_RXDESC_CNT];
};

struct ipc_rxcfm_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_RXCFM_CNT];
    struct vring_avail avail[IPC_RXCFM_CNT];
    struct vring_ready ready[IPC_RXCFM_CNT];
    struct ipc_epmsg_rxcfm  items[IPC_RXCFM_CNT];
};

struct ipc_txcfm_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_TXCFM_CNT];
    struct vring_avail avail[IPC_TXCFM_CNT];
    struct vring_ready ready[IPC_TXCFM_CNT];
    struct ipc_epmsg_txcfm  items[IPC_TXCFM_CNT];
};

struct ipc_a2c_msg_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_MSGA2C_BUF_CNT];
    struct vring_avail avail[IPC_MSGA2C_BUF_CNT];
    struct vring_ready ready[IPC_MSGA2C_BUF_CNT];
    struct ipc_epmsg_a2c_msg items[IPC_MSGA2C_BUF_CNT];
};

struct ipc_c2a_msg_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_MSGC2A_BUF_CNT];
    struct vring_avail avail[IPC_MSGC2A_BUF_CNT];
    struct vring_ready ready[IPC_MSGC2A_BUF_CNT];
    struct ipc_epmsg_c2a_msg items[IPC_MSGC2A_BUF_CNT];
};

struct __attribute__((aligned(4))) ipc_shared_hdr
{
    /// Pattern for host to check if shared memory is initialized
    volatile uint32_t pattern1;
    volatile uint32_t pattern2;
    volatile uint32_t config_addr; /*The address of 'config'*/
    volatile uint32_t config_len;  /*The length of 'config'*/
};

struct ipc_dbg_tag
{
    volatile uint32_t pattern;
    volatile uint32_t status;
    volatile uint32_t buffer_start;
    volatile uint32_t buffer_size;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
};

/// Structure describing the IPC data shared with the host CPU
struct __attribute__((aligned(4))) ipc_shared_env_tag
{
    volatile struct ipc_shared_hdr hdr;
    volatile uint32_t state;
    volatile struct ipc_status master_status;
    volatile struct ipc_status slave_status;
    volatile struct ipc_a2c_msg_tag msg_a2c_buf;
    volatile struct ipc_c2a_msg_tag msg_c2a_buf;
    volatile struct ipc_txdesc_tag txdesc;
    volatile struct ipc_txcfm_tag  txcfm;
    volatile struct ipc_rxdesc_tag rxdesc;
    volatile struct ipc_rxcfm_tag  rxcfm;
    volatile uint32_t config[IPC_CFG_SIZE / 4];
    volatile struct ipc_dbg_tag dbg_buffer;
    volatile uint32_t ipc_app_status;
};

struct ipc_rxbuf_hdr
{
    /// Interface index
    uint16_t fvif_idx;
    uint16_t len;
};

struct ipc_txbuf_hdr
{
    /// Interface index
    uint16_t fvif_idx;
    uint16_t type;
    void (*cfm_cb)(uint32_t frame_id, bool acknowledged, void *arg);
    void *cfm_cb_arg;
};

#define __SHAREDRAM_AMP_IPC_ENV __attribute__ ((section("SHAREDRAM_AMP_IPC_ENV")));
#define __IPC_WIFI_SHARE    __attribute__ ((section("IPC_WIFI_SHARE")));
#endif
