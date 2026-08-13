/**
 ****************************************************************************************
 *
 * @file net_al.c
 *
 * @brief Implementation of the networking stack abstraction layer using LwIP.
 *
 * Copyright (C) listenAI 2023-2024
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include "net_al.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/dns.h"
#include "netif/ethernet.h"
#include "llc.h"
#include "utils_endian.h"
#include "rtos_al.h"
#include "sim_pbuf.h"
#include "wlif.h"
#include "ipc.h"
#include "ipc_slave_wifi.h"
#include "ipc_utils.h"
#include "log_print.h"

net_if_call_fun net_if_fun;
static struct netif_handle net_if_handle;

#ifdef TX_BUF_COPY
rtos_queue net_simsoc_tx_queue_buf;
static struct net_simsoc_tx_buf_tag net_simsoc_tx_buf_mem[NET_SIMSOC_TX_BUF_CNT] __SHAREDRAM;
#endif

#if PBUF_LINK_ENCAPSULATION_HLEN < NET_AL_TX_HEADROOM
#error "PBUF_LINK_ENCAPSULATION_HLEN must be at least NET_AL_TX_HEADROOM"
#endif

#ifdef WIFI_HOST_STANDALONE
#define CO_ALIGN4_HI(val) (((val) + 3) & ~3)
#endif

/*
 * FUNCTIONS
 ****************************************************************************************
 */
/// Fake function used to detected too small link encapsulation header length
void p_buf_link_encapsulation_hlen_too_small(void);

/// Declaration of the LwIP checksum computation function
u16_t lwip_standard_chksum(const void *dataptr, int len);
static uint8_t netif_num = 0;

#ifdef TX_BUF_COPY
void net_tx_release_mac_buf(void *tx_buf, bool acknowledged)
{
    struct net_tx_buf_head *head;
    struct net_tx_buf_head *head_tmp;
    int ret;

    head = (struct net_tx_buf_head *)tx_buf;
    if (head->is_master_core)
    {
        net_tx_cfm((uint32_t)tx_buf, acknowledged, NULL);
    }
    else
    {
        while (head)
        {
            head_tmp = head->next;
            head->next = NULL;

            ret = rtos_queue_write(net_simsoc_tx_queue_buf, &head, 0, 0);
            ASSERT_ERR(ret == 0);

            head = head_tmp;
        }
    }
}

void *net_tx_alloc_mac_buf(net_buf_tx_t *buf, uint16_t rsv_head_len)
{
    uint8_t idx;
    uint8_t ava_tx_buf_cnt;
    uint16_t seg_cnt_max;
    uint16_t length;
    struct net_tx_buf_tag *tx_buf_first = NULL;
    struct net_tx_buf_tag *tx_buf;
    struct net_tx_buf_tag *pre_tx_buf;
    uint8_t *src_buf_loca;
    uint16_t src_left_len;
    uint8_t *dst_buf_loca;
    uint16_t dst_left_len;
    uint16_t tx_buf_size = NET_SIMSOC_TX_BUF_UNIT_SIZE;
    rtos_queue *queue = &net_simsoc_tx_queue_buf;
    uint16_t tot_len;

    length = buf->tot_len;
    tot_len = length + rsv_head_len;
    seg_cnt_max = (tot_len + tx_buf_size - 1) / tx_buf_size;

    ava_tx_buf_cnt = rtos_queue_cnt(*queue);

    if (seg_cnt_max > ava_tx_buf_cnt)
    {
        //CLOGD("tx alloc fail %d %d %d\n", ava_tx_buf_cnt, seg_cnt_max, length);
        goto end;
    }

    src_buf_loca = (uint8_t *)buf->payload;
    src_left_len = buf->len;
    dst_left_len = 0;
    idx = 0;
    pre_tx_buf = NULL;
    while (length)
    {
        if (dst_left_len == 0)
        {
            tx_buf = NULL;
            rtos_queue_read(*queue, &tx_buf, 0, 0);
            ASSERT_ERR(tx_buf != NULL);

            tx_buf->head.next = NULL;
            if (pre_tx_buf)
            {
                pre_tx_buf->head.next = (struct net_tx_buf_head *)tx_buf;
            }
            pre_tx_buf = tx_buf;

            if (idx == 0)
            {
                tx_buf_first = tx_buf;
                dst_buf_loca = tx_buf->buf + rsv_head_len;
                dst_left_len = tx_buf_size - rsv_head_len;
            }
            else
            {
                dst_buf_loca = tx_buf->buf;
                dst_left_len = tx_buf_size;
            }
            idx++;
        }

        if (dst_left_len >= src_left_len)
        {
            memcpy(dst_buf_loca, src_buf_loca, src_left_len);
            dst_left_len -= src_left_len;
            dst_buf_loca += src_left_len;
            length -= src_left_len;
            src_left_len = 0;
        }
        else
        {
            memcpy(dst_buf_loca, src_buf_loca, dst_left_len);
            src_left_len -= dst_left_len;
            src_buf_loca += dst_left_len;
            length -= dst_left_len;
            dst_left_len = 0;
        }

        if (src_left_len == 0)
        {
            if (length == 0)
            {
                tx_buf->head.buf_len = tx_buf_size - dst_left_len;
            }
            else
            {
                if (dst_left_len == 0)
                {
                    tx_buf->head.buf_len = tx_buf_size;
                }

                buf = buf->next;
                src_buf_loca = (uint8_t *)buf->payload;
                src_left_len = buf->len;
            }
        }
        else if (dst_left_len == 0)
        {
            tx_buf->head.buf_len = tx_buf_size;
        }
    }

end:
    return tx_buf_first;
}
#else
void net_tx_release_mac_buf(void *tx_buf, bool acknowledged)
{
    return;
}
void *net_tx_alloc_mac_buf(net_buf_tx_t *buf, uint16_t rsv_head_len)
{
    return NULL;
}
#endif

int net_if_add(net_if_t *net_if,
               const uint8_t *mac_addr,
               const uint32_t *ipaddr,
               const uint32_t *netmask,
               const uint32_t *gw,
               void *vif)
{
    err_t status;

    status = ERR_OK;
    net_if->state = vif;
    net_if->name[ 0 ] = 'w';
    net_if->name[ 1 ] = 'l';
    net_if->num = netif_num++;
    net_if->hwaddr_len = ETHARP_HWADDR_LEN;
    net_if->mtu = LLC_ETHER_MTU;

    // Init MAC addr here as we can't do it in net_if_init (without dereferencing vif)
    memcpy(net_if->hwaddr, mac_addr, ETHARP_HWADDR_LEN);

    return (status == ERR_OK ? 0 : -1);
}

const uint8_t *net_if_get_mac_addr(net_if_t *net_if)
{
    return net_if->hwaddr;
}

net_if_t *net_if_find_from_name(const char *name)
{
    int32_t i;

    if (name == NULL)
        return NULL;
    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        if ( (net_if_handle.netif[i]->name[0] == name[0])
                && (net_if_handle.netif[i]->name[1] == name[1])
                && (net_if_handle.netif[i]->num == name[2] - '0') )
            return net_if_handle.netif[i];
    }

    return NULL;
}

int net_if_get_name(net_if_t *net_if, char *buf, int len)
{
    if (len > 0)
        buf[0] = net_if->name[0];
    if (len > 1)
        buf[1] = net_if->name[1];
    if (len > 2)
        buf[2] = net_if->num + '0';
    if ( len > 3)
        buf[3] = '\0';

    return 3;
}

int net_ipc_event(net_if_t *net_if, int32_t event_type)
{
    int ret = -1;
    struct cfg_ind_netif event;

    event.fvif_idx = net_if_to_idx(net_if);
    if (event.fvif_idx < WLIF_IDX_MAX)
    {
        event.hdr.id  = IPC_IND_NETIF;
        event.hdr.len = sizeof(struct cfg_ind_netif) - sizeof(struct ipc_msg_hdr);
        event.evt = event_type;
        ret = ipc_slave_msg_push(IPC_CHAN_MASTER_MSG, IPC_EP_IND, sizeof(struct cfg_ind_netif), &event);
    }

    return ret;
}

void net_if_up(net_if_t *net_if)
{
    net_ipc_event(net_if, NET_EVENT_UP);
    netif_set_flags(net_if, NETIF_FLAG_UP);
}

void net_if_down(net_if_t *net_if)
{
    net_ipc_event(net_if, NET_EVENT_DOWN);
    netif_clear_flags(net_if, NETIF_FLAG_UP);
}

void net_if_set_default(net_if_t *net_if)
{

}

void net_if_set_ip(net_if_t *net_if, uint32_t ip, uint32_t mask, uint32_t gw)
{
    if (!net_if)
        return;
}

int net_if_get_ip(net_if_t *net_if, uint32_t *ip, uint32_t *mask, uint32_t *gw)
{
    if (!net_if)
        return -1;

    if (ip)
        *ip = netif_ip4_addr(net_if)->addr;
    if (mask)
        *mask = netif_ip4_netmask(net_if)->addr;
    if (gw)
        *gw = netif_ip4_gw(net_if)->addr;

    return 0;
}


void *net_if_vif_info(net_if_t *net_if)
{
    return net_if->state;
}

net_buf_tx_t *net_buf_tx_alloc(uint32_t length)
{
    struct pbuf *pbuf;

    pbuf = pbuf_alloc(PBUF_RAW_TX, length, PBUF_RAM);
    if (pbuf == NULL)
        return NULL;

    return pbuf;
}

net_buf_tx_t *net_buf_tx_alloc_ref(uint32_t length)
{
    struct pbuf *pbuf;

    pbuf = pbuf_alloc(PBUF_RAW_TX, length, PBUF_REF);
    if (pbuf == NULL)
        return NULL;

    return pbuf;
}

void *net_buf_tx_info(net_buf_tx_t *buf, uint16_t *tot_len, int *seg_cnt,
                      uint32_t seg_addr[], uint16_t seg_len[])
{
    int idx, seg_cnt_max = *seg_cnt;
    uint16_t length = buf->tot_len;
    void *headroom;

    *tot_len = length;

    seg_addr[0] = (uint32_t)buf->payload;
    seg_len[0] = buf->len;
    length -= buf->len;

    // Get pointer to reserved headroom
    if (pbuf_header(buf, PBUF_LINK_ENCAPSULATION_HLEN))
    {
        // Sanity check - we shall have enough space in the buffer
        ASSERT_ERR(0);
        return NULL;
    }
    headroom = (void *)CO_ALIGN4_HI((uint32_t)buf->payload);

    // Get info of extra segments if any
    buf = buf->next;
    idx = 1;
    while (length && buf && (idx < seg_cnt_max))
    {
        seg_addr[idx] = (uint32_t)buf->payload;
        seg_len[idx] = buf->len;
        length -= buf->len;
        idx++;
        buf = buf->next;
    }

    *seg_cnt = idx;
    if (length != 0)
    {
        // The complete buffer must be included in all the segments
        ASSERT_ERR(0);
        return NULL;
    }

    return headroom;
}

void net_buf_tx_free(net_buf_tx_t *buf)
{
#ifndef TX_BUF_COPY
    // Remove the link encapsulation header
    pbuf_header(buf, -PBUF_LINK_ENCAPSULATION_HLEN);
#endif
    // Free the buffer
    pbuf_free(buf);
}

void net_buf_tx_cat(net_buf_tx_t *net_buf_tx_1, net_buf_tx_t *net_buf_tx_2)
{
    sim_pbuf_cat(net_buf_tx_1, net_buf_tx_2);
}

void net_buf_rx_free(net_buf_rx_t *buf)
{
    // Free the buffer
    pbuf_free(&buf->pbuf);
}

/**
 ****************************************************************************************
 * @brief Callback when lwip init is done
 *
 * @param[in] arg Not used
 ****************************************************************************************
 */
void net_init_done(void *arg)
{

}

volatile bool net_tx_buf_copy_flag = false;
bool net_is_tx_buf_copy(void)
{
    return net_tx_buf_copy_flag;
}

int net_init(net_if_call_fun *net_cb)
{
    int i;

    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        net_if_handle.netif[i] = rtos_calloc(sizeof(net_if_t), sizeof(uint8_t));
    }

#ifdef TX_BUF_COPY
    net_tx_buf_copy_flag = true;

    if (rtos_queue_create(sizeof(void *), NET_SIMSOC_TX_BUF_CNT, &net_simsoc_tx_queue_buf))
    {
        ASSERT_ERR(0);
    }
#endif

    net_if_fun = *net_cb;

    return 0;
}

int net_if_input(net_buf_rx_t *buf, net_if_t *net_if, void *addr, uint16_t len, net_buf_free_fn free_fn)
{
    struct ipc_rxbuf_hdr *hdr;
    struct ipc_wifi_rxdesc entry;
    int ret = 0;

    sim_pbuf_alloced_custom(PBUF_RAW, len, PBUF_REF, buf, addr, len);
    buf->custom_free_function = NULL;

    hdr = (struct ipc_rxbuf_hdr*)((uint8_t*)(((struct wlif_rx_buf_tag*)buf)->payload) - sizeof(struct ipc_rxbuf_hdr));
    hdr->len	  = len;
    hdr->fvif_idx = net_if_to_idx(net_if);
    ASSERT_ERR(hdr->fvif_idx >= 0);
    entry.data = (void*)buf;
    if ((ret = ipc_slave_wifi_rxdesc_push((void*)(&entry), sizeof(struct ipc_wifi_rxdesc))))
        free_fn(buf);

    return ret;
}

int net_dhcp_start(net_if_t *net_if)
{
    return net_ipc_event(net_if, NET_EVENT_DHCP_START);
}

void net_dhcp_stop(net_if_t *net_if)
{
    net_ipc_event(net_if, NET_EVENT_DHCP_STOP);
}

int net_dhcp_release(net_if_t *net_if)
{
    return net_ipc_event(net_if, NET_EVENT_DHCP_RELEASE);
}

int net_dhcp_address_obtained(net_if_t *net_if)
{
    return 0;
}

int net_set_dns(uint32_t dns_server)
{
    return -1;
}

int net_get_dns(uint32_t *dns_server)
{
    return -1;
}

int net_compat_check(size_t netif_size)
{
    return (netif_size != sizeof(net_if_t));
}

int32_t net_ipc_rx_cfm(void *ccb, void *param)
{
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    if (net_if_fun.rx_push_from_ipc)
        net_if_fun.rx_push_from_ipc(param);
#else
    struct ipc_msg_desc *desc = param;

    if (net_if_fun.rx_push_from_ipc)
        net_if_fun.rx_push_from_ipc(((struct ipc_wifi_rxcfm*)desc->data)->data);
#endif
    return IPC_MSG_RELEASE;
}

int32_t net_ipc_send(void *ccb, void *param)
{
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    if (net_if_fun.tx_start_from_ipc)
        net_if_fun.tx_start_from_ipc(param);
#else
    struct ipc_msg_desc *desc = param;

    if (net_if_fun.tx_start_from_ipc)
        net_if_fun.tx_start_from_ipc(((struct ipc_wifi_txdesc*)desc->data)->data);
#endif
    return IPC_MSG_RELEASE;
}

void net_tx_cfm(uint32_t frame_id, bool acknowledged, void *arg)
{
    struct ipc_wifi_txcfm entry;

    entry.data   = (void*)frame_id;
    entry.status = (uint32_t)acknowledged;
    ipc_slave_wifi_txcfm_push((void*)&entry, sizeof(struct ipc_wifi_txcfm));
}

net_if_t *netif_alloc(void)
{
    return rtos_calloc(sizeof(net_if_t), sizeof(uint8_t));
}

net_if_t *net_if_get(int wifi_idx)
{
    if (wifi_idx >= WLIF_IDX_MAX)
    {
        return NULL;
    }

    return net_if_handle.netif[wifi_idx];
}

int16_t net_if_to_idx(net_if_t *net_if)
{
    int32_t i;

    for (i = 0; i < WLIF_IDX_MAX; i++)
        if (net_if_handle.netif[i] == net_if)
            return (int16_t)i;

    return (int16_t)(-1);
}

bool net_ip_task_avail(void)
{
    return false;
}


void net_wifi_init_done(void)
{
#ifdef TX_BUF_COPY
    int ret;
    int i;
    struct net_tx_buf_head *head;

    for (i = 0; i < NET_SIMSOC_TX_BUF_CNT; i++)
    {
        head = (struct net_tx_buf_head *)&net_simsoc_tx_buf_mem[i];
        head->is_master_core = 0;
        ret = rtos_queue_write(net_simsoc_tx_queue_buf, &head, 0, 0);
        ASSERT_ERR(ret == 0);
    }
#endif
    ipc_send_signal(IPC_SIG_LINKUP);
}

char* net_get_monitor_name(void)
{
    return "wl0";
}
