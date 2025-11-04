/**
 ****************************************************************************************
 *
 * Implementation of the fully hosted entry point on RTOS host.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include "net_al.h"
#include "wlif.h"
#include "ipc.h"

#define  WLIF_TX_LOCK()
#define  WLIF_TX_UNLOCK()

/*
 * DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

struct wlif_env netif_env;
typedef void (*netif_tx_cb)(uint32_t frame_id, bool acknowledged, void *arg);

int wlif_name(int vif_idx, char *name, int len)
{
    if (vif_idx > WLIF_IDX_MAX)
        return -1;

    return net_if_get_name(netif_env.vif[vif_idx].netif, name, len);
}

int wlif_idx_from_name(const char *name)
{
    net_if_t *net_if;
    int i;

    if (name == NULL)
        return -1;

    net_if = net_if_find_from_name(name);
    if (!net_if)
        return -1;

    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        if (netif_env.vif[i].netif == net_if)
            return i;
    }

    return -1;
}

LWIP_FUNC_ATTR int wlif_idx_from_netif(net_if_t *net_if)
{
    int i;

    if (!net_if)
        return -1;

    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        if (netif_env.vif[i].netif == net_if)
            return i;
    }

    return -1;
}

void wlif_get_status(struct wlif_status *status)
{
    int i;

    status->vif_max_cnt = WLIF_IDX_MAX;
    status->vif_active_cnt = 0;
    status->vif_first_active = -1;

    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        if (netif_env.vif[i].mac_vif.type != VIF_UNKNOWN)
        {
            status->vif_active_cnt++;
            if (status->vif_first_active < 0)
                status->vif_first_active = i;
        }
    }
}

static void wlif_rx_buf_free(void *net_buf)
{
    struct ipc_rxcfm entry;

    entry.data = net_buf;
    ipc_master_wifi_rxcfm_push((void*)(&entry), sizeof(struct ipc_rxcfm));
}

/**
 ****************************************************************************************
 * @brief Forward/Resend a RX buffer to the networking stack/Tx path.
 *
 * @param[in] buf                Pointer to the RX buffer to forward
 * @param[in] rx_buf_action      RX send action status
 * @param[in] net_if             Network interface to which the buffer is intended
 * @param[in] length             Length of Rx buffer to send
 * @param[in] offset             Offset to point on RX buffer
 * @param[in] skip_after_eth_hdr Offset to that should be skipped for the forward
 ****************************************************************************************
 */
static void wlif_rx_buf_input(struct wlif_rx_buf_tag *buf,
                              uint8_t rx_buf_action,
                              net_if_t *net_if,
                              uint16_t length,
                              uint8_t offset,
                              uint8_t skip_after_eth_hdr)
{
    if (rx_buf_action & RX_BUF_FORWARD)
    {
        if (skip_after_eth_hdr != 0)
            memcpy((char *)buf->payload + skip_after_eth_hdr, buf->payload,
                   sizeof(struct mac_eth_hdr));
        // Forward to the networking stack
        net_if_input(&buf->net_buf, net_if,
                     (uint8_t *)(buf->payload) + offset + skip_after_eth_hdr,
                     length - offset - skip_after_eth_hdr, wlif_rx_buf_free);
    }
}

/**
 ****************************************************************************************
 * @brief Forward a RX buffer to the networking stack.
 *
 * @param[in] buf Pointer to the RX buffer to forward
 ****************************************************************************************
 */
void wlif_rx_buf_forward(void *data)
{
    struct ipc_rxbuf_hdr *hdr;
    struct wlif_rx_buf_tag *buf;

    buf = (struct wlif_rx_buf_tag*)data;
    hdr = (struct ipc_rxbuf_hdr*)((uint8_t*)buf->payload - sizeof(struct ipc_rxbuf_hdr));

    if (hdr->fvif_idx < WLIF_IDX_MAX)
        wlif_rx_buf_input(buf, RX_BUF_FORWARD, netif_env.vif[hdr->fvif_idx].netif, hdr->len, 0, 0);
    else
    	wlif_rx_buf_free(buf);
}

LWIP_FUNC_ATTR static void wlif_tx_req(net_if_t *net_if, void *buf,
                         enum net_tx_buf_type type, netif_tx_cb cfm_cb,
                         void *cfm_cb_arg)
{
    struct ipc_txbuf_hdr *head;
    struct ipc_txdesc entry;
#ifdef TX_BUF_COPY
    struct net_tx_buf_tag *tx_buf = buf;
    head = (struct ipc_txbuf_hdr*)((uint8_t*)tx_buf->buf - sizeof(struct ipc_txbuf_hdr));
#else
    net_buf_tx_t *tx_buf = buf;
    head = (struct ipc_txbuf_hdr*)((uint8_t*)tx_buf->payload - sizeof(struct ipc_txbuf_hdr));
#endif
    head->type     = type;
    head->fvif_idx = wlif_idx_from_netif(net_if);
    head->cfm_cb     = cfm_cb;
    head->cfm_cb_arg = cfm_cb_arg;
    entry.data       = (void*)tx_buf;
    if (ipc_master_wifi_tx_push((void*)&entry, sizeof(struct ipc_txdesc)) < 0)
        wlif_tx_cfm(tx_buf, -1);
}

LWIP_FUNC_ATTR static int wlif_tx_start(net_if_t *net_if, net_buf_tx_t *buf,
		netif_tx_cb cfm_cb, void *cfm_cb_arg)
{
	WLIF_TX_LOCK();
    wlif_tx_req(net_if, (void*)buf, IEEE802_3, cfm_cb, cfm_cb_arg);
    WLIF_TX_UNLOCK();

    return 0;
}

LWIP_FUNC_ATTR void wlif_tx_cfm(void *data, uint32_t status)
{
    if (status != -1)
    {
        struct wlif_tx_desc_tag_partial *desc;
        netif_tx_cb func;
#ifdef TX_BUF_COPY
        desc = (struct wlif_tx_desc_tag_partial*)(((struct net_tx_buf_head *)data)->tx_desc_rsv);
#else
        desc = (struct wlif_tx_desc_tag_partial*)CO_ALIGN4_HI((uint32_t)((net_buf_tx_t*)data)->payload);
#endif
        func = (netif_tx_cb)desc->ctrl.cfm_cb;
        if (func)
            func((uint32_t)data, status & TX_STATUS_ACKNOWLEDGED, desc->ctrl.cfm_cb_arg);
    }
#ifdef TX_BUF_COPY
    net_tx_release_mac_buf(data, status);
#else
    net_buf_tx_free((net_buf_tx_t*)data);
#endif
}

void wlif_vif_init(int vif_idx, uint8_t *base_mac_addr)
{
    struct wlif *vif =  &netif_env.vif[vif_idx];

    memcpy(vif->mac_addr, base_mac_addr, 6);
    vif->mac_addr[5] ^= vif_idx;
    vif->mac_vif.type = VIF_UNKNOWN;
    vif->netif = net_if_get(vif_idx);
    net_if_add(vif->netif, (uint8_t *)vif->mac_addr,
               NULL, NULL, NULL, vif);
}

net_if_t* wlif_get_default_if(void)
{
    return (netif_env.vif[0].netif);
}

int32_t wlif_init(void)
{
    net_if_call_fun net_cb = {.tx_start_fn = wlif_tx_start};

    // TCP/IP stack
    if (net_init(&net_cb))
    {
        ASSERT_ERR(0);
        return 1;
    }

    return 0;
}

void wlif_netif_up(uint8_t vif_idx, uint8_t vif_type)
{
    struct vif_info_tag *mac_vif;
    net_if_t *net_if;

    if (vif_idx < WLIF_IDX_MAX)
    {
        CLOGD("WiFi if: up");
        net_if = net_if_get(vif_idx);
        net_if_up(net_if);
        mac_vif = wlif_get_mac_vif(vif_idx);
        mac_vif->type = vif_type;
    }
}

void wlif_netif_down(uint8_t vif_idx)
{
    struct vif_info_tag *mac_vif;
    net_if_t *net_if;

    if (vif_idx < WLIF_IDX_MAX)
    {
        CLOGD("WiFi if: down");
        net_if = net_if_get(vif_idx);
        net_if_down(net_if);
        mac_vif = wlif_get_mac_vif(vif_idx);
        mac_vif->type = VIF_UNKNOWN;
    }
}

/// @}
