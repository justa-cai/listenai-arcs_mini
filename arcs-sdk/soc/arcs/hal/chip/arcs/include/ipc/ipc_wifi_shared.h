/**
 ****************************************************************************************
 *
 * @file ipc_wifi_shared.h
 *
 * @brief WiFi IPC shared-memory data plane layout.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#ifndef _IPC_WIFI_SHARED_H_
#define _IPC_WIFI_SHARED_H_

#include <stdint.h>
#include <stdbool.h>
#include "ipc_types.h"

#define IPC_WIFI_TXDESC_CNT             16
#define IPC_WIFI_TXCFM_CNT              16
#define IPC_WIFI_RXDESC_CNT             16
#define IPC_WIFI_RXCFM_CNT              16
#define IPC_WIFI_SHARE_SIZE             72

struct ipc_wifi_rxdesc
{
    void *data;
};

struct ipc_wifi_epmsg_rxdesc
{
    struct ipc_epmsg ephdr;
    struct ipc_wifi_rxdesc buf;
};

struct ipc_wifi_rxcfm
{
    void *data;
};

struct ipc_wifi_epmsg_rxcfm
{
    struct ipc_epmsg ephdr;
    struct ipc_wifi_rxcfm buf;
};

struct ipc_wifi_txdesc
{
    void *data;
};

struct ipc_wifi_epmsg_txdesc
{
    struct ipc_epmsg ephdr;
    struct ipc_wifi_txdesc buf;
};

struct ipc_wifi_txcfm
{
    void *data;
    uint32_t status;
};

struct ipc_wifi_epmsg_txcfm
{
    struct ipc_epmsg ephdr;
    struct ipc_wifi_txcfm buf;
};

struct ipc_wifi_txdesc_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_WIFI_TXDESC_CNT];
    struct vring_avail avail[IPC_WIFI_TXDESC_CNT];
    struct vring_ready ready[IPC_WIFI_TXDESC_CNT];
    struct ipc_wifi_epmsg_txdesc items[IPC_WIFI_TXDESC_CNT];
};

struct ipc_wifi_rxdesc_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_WIFI_RXDESC_CNT];
    struct vring_avail avail[IPC_WIFI_RXDESC_CNT];
    struct vring_ready ready[IPC_WIFI_RXDESC_CNT];
    struct ipc_wifi_epmsg_rxdesc items[IPC_WIFI_RXDESC_CNT];
};

struct ipc_wifi_rxcfm_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_WIFI_RXCFM_CNT];
    struct vring_avail avail[IPC_WIFI_RXCFM_CNT];
    struct vring_ready ready[IPC_WIFI_RXCFM_CNT];
    struct ipc_wifi_epmsg_rxcfm items[IPC_WIFI_RXCFM_CNT];
};

struct ipc_wifi_txcfm_tag
{
    struct vring_hdr   ring;
    struct vring_desc  desc[IPC_WIFI_TXCFM_CNT];
    struct vring_avail avail[IPC_WIFI_TXCFM_CNT];
    struct vring_ready ready[IPC_WIFI_TXCFM_CNT];
    struct ipc_wifi_epmsg_txcfm items[IPC_WIFI_TXCFM_CNT];
};

struct ipc_wifi_shared_env
{
    struct ipc_wifi_txdesc_tag txdesc;
    struct ipc_wifi_txcfm_tag  txcfm;
    struct ipc_wifi_rxdesc_tag rxdesc;
    struct ipc_wifi_rxcfm_tag  rxcfm;
};

struct ipc_rxbuf_hdr
{
    uint16_t fvif_idx;
    uint16_t len;
};

struct ipc_txbuf_hdr
{
    uint16_t fvif_idx;
    uint16_t type;
    void (*cfm_cb)(uint32_t frame_id, bool acknowledged, void *arg);
    void *cfm_cb_arg;
};

#define __IPC_WIFI_SHARE    __attribute__ ((section("IPC_WIFI_SHARE")));


#endif
