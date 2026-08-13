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
#include "ipc_types.h"
#include "amp_shared.h"

/*
 * IPC log forwarding direction. CFG_IPC_PRINT_WRITER / CFG_IPC_PRINT_READER
 * are normally provided by the build system and are independent of the IPC
 * master/slave role. Legacy builds that only define CFG_IPC_PRINT fall back
 * to the historical role-based direction: slave = writer, master = reader.
 */
#if defined(CFG_IPC_PRINT) && !defined(CFG_IPC_PRINT_WRITER) && !defined(CFG_IPC_PRINT_READER)
#if defined(CFG_AMP_IPC_SLAVE)
#define CFG_IPC_PRINT_WRITER 1
#else
#define CFG_IPC_PRINT_READER 1
#endif
#endif

#define IPC_SLAVE_MSG_BUF_CNT              16
#define IPC_MASTER_MSG_BUF_CNT             16

#ifdef CFG_AMP_IPC_WIFI_CHAN
#include "ipc_wifi_shared.h"
#endif
#ifdef CFG_AMP_IPC_BT_CHAN
#include "ipc_bt_shared.h"
#endif

/// Size, in bytes, of IPC buffers for command
#define IPC_SLAVE_MSG_BUF_SIZE            96

/// Size, in bytes, of IPC buffers for response/print
#define IPC_MASTER_MSG_BUF_SIZE            64


/*"Please do not modify it; its value is determined by the layout of IPC RAM in the .ld file.
 * Otherwise, you need to recompile the firmware."*/
#define IPC_SHARED_HDR_ADDR             0x20050000
#define IPC_PATTERN1                    0x4950435F
#define IPC_PATTERN2                    0x69736F6B
#define IPC_BUSY                        0x00000000    /*BUSY*/
#define IPC_READY                       0x52454459    /*REDY*/
#define IPC_CFG_SIZE                    128

struct ipc_slave_buf
{
    uint32_t data[IPC_SLAVE_MSG_BUF_SIZE / 4]; ///< Message data
};

struct ipc_epmsg_slave_msg
{
    struct ipc_epmsg ephdr;
    struct ipc_slave_buf buf;
};

struct ipc_master_buf
{
    uint32_t data[IPC_MASTER_MSG_BUF_SIZE / 4]; ///< Message data
};

struct ipc_epmsg_master_msg
{
    struct ipc_epmsg ephdr;
    struct ipc_master_buf buf;
};

struct ipc_slave_msg_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_SLAVE_MSG_BUF_CNT];
    struct vring_avail avail[IPC_SLAVE_MSG_BUF_CNT];
    struct vring_ready ready[IPC_SLAVE_MSG_BUF_CNT];
    struct ipc_epmsg_slave_msg items[IPC_SLAVE_MSG_BUF_CNT];
};

struct ipc_master_msg_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_MASTER_MSG_BUF_CNT];
    struct vring_avail avail[IPC_MASTER_MSG_BUF_CNT];
    struct vring_ready ready[IPC_MASTER_MSG_BUF_CNT];
    struct ipc_epmsg_master_msg items[IPC_MASTER_MSG_BUF_CNT];
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
    volatile struct ipc_signal master_signal;
    volatile struct ipc_signal slave_signal;
    volatile struct ipc_slave_msg_tag slave_msg_buf;
    volatile struct ipc_master_msg_tag master_msg_buf;
#ifdef CFG_AMP_IPC_WIFI_CHAN
    volatile struct ipc_wifi_shared_env wifi;
#endif
#ifdef CFG_AMP_IPC_BT_CHAN
    volatile struct ipc_bt_shared_env bt;
#endif
    volatile uint32_t config[IPC_CFG_SIZE / 4];
    volatile struct ipc_dbg_tag dbg_buffer;
    volatile struct amp_shared_info amp_shared;
};

#define __SHAREDRAM_AMP_IPC_ENV __attribute__ ((section("SHAREDRAM_AMP_IPC_ENV")));

void ipc_mem_init(void);
int32_t ipc_mem_validate(void);

#endif
