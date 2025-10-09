/**
 ****************************************************************************************
 *
 * @file net_al.c
 *
 * @brief Implementation of the networking stack abstraction layer using LwIP.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <string.h>
#include <stdbool.h>
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/dns.h"
#include "netif/ethernet.h"
#include "net_al.h"
#include "utils_math.h"
#include "llc.h"
#include "rtos_al.h"
#include "dma.h"
#include "PSRAMManager.h"

#define NX_NB_L2_FILTER 2
#define RX_BUF_COPY 1
struct l2_filter_tag
{
    struct netif *net_if;
    int sock;
    struct netconn *conn;
    uint16_t ethertype;
};

static struct l2_filter_tag l2_filter[NX_NB_L2_FILTER];
static rtos_semaphore l2_semaphore;
static volatile bool l2_send_ack;
static rtos_mutex     l2_mutex;
static net_if_call_fun net_if_fun;
static struct netif_handle net_if_handle;
#if PBUF_LINK_ENCAPSULATION_HLEN < NET_AL_TX_HEADROOM
#error "PBUF_LINK_ENCAPSULATION_HLEN must be at least NET_AL_TX_HEADROOM"
#endif

#ifdef TX_BUF_COPY
rtos_queue net_tx_queue_buf;
rtos_queue net_tx_queue_short_buf;
struct net_tx_buf_tag net_tx_buf_mem[NET_TX_BUF_CNT] __SHAREDRAM;
struct net_short_tx_buf_tag net_short_tx_buf_mem[NET_SHORT_TX_BUF_CNT] __SHAREDRAM;

#define TX_BUF_DMA_MEMCPY 0
#if TX_BUF_DMA_MEMCPY
#define TX_BUF_DMA_MEMCPY_CH 0
#define TX_BUF_DMA_COPY_THRESHOLD 512

volatile uint32_t net_tx_buf_memcpy_dam_event = 0;
rtos_mutex net_tx_buf_cp_mutex;

static void net_tx_buf_copy_dma_event_handler(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    net_tx_buf_memcpy_dam_event = event_info & 0xFF;
}

static void net_tx_buf_copy_dma_done_waiting(void)
{
    while(1)
    {
        if(net_tx_buf_memcpy_dam_event & DMA_EVENT_TRANSFER_COMPLETE)
        {
            net_tx_buf_memcpy_dam_event &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
    }
}
#endif
#endif

/*
 * FUNCTIONS
 ****************************************************************************************
 */
/// Fake function used to detected too small link encapsulation header length
void p_buf_link_encapsulation_hlen_too_small(void);

/// Declaration of the LwIP checksum computation function
u16_t lwip_standard_chksum(const void *dataptr, int len);

#ifdef TX_BUF_COPY
void net_tx_release_mac_buf(void *tx_buf, bool acknowledged)
{
    struct net_tx_buf_head *head;
    struct net_tx_buf_head *head_tmp;
    int ret;

    head = (struct net_tx_buf_head *)tx_buf;
    while (head)
    {
        head_tmp = head->next;
        head->next = NULL;

        if (head->is_short)
        {
            ret = rtos_queue_write(net_tx_queue_short_buf, &head, 0, 0);
        }
        else
        {
            ret = rtos_queue_write(net_tx_queue_buf, &head, 0, 0);
        }
        ASSERT_ERR(ret == 0);

        head = head_tmp;
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
    uint16_t tx_buf_size;
    rtos_queue *queue;
    uint16_t tot_len;

    length = buf->tot_len;
    tot_len = length + rsv_head_len;
    if (tot_len > NET_SHORT_TX_BUF_UNIT_SIZE)
    {
        tx_buf_size = NET_TX_BUF_UNIT_SIZE;
    }
    else
    {
        tx_buf_size = NET_SHORT_TX_BUF_UNIT_SIZE;
    }
    seg_cnt_max = (tot_len + tx_buf_size - 1) / tx_buf_size;

    if (tx_buf_size == NET_TX_BUF_UNIT_SIZE)
    {
        queue = &net_tx_queue_buf;
    }
    else
    {
        queue = &net_tx_queue_short_buf;
    }
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
#if TX_BUF_DMA_MEMCPY
            if (src_left_len > TX_BUF_DMA_COPY_THRESHOLD)
            {
                int ret;
                rtos_mutex_lock(net_tx_buf_cp_mutex);

                #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
                if ((src_buf_loca >= PSRAM_BASE_ADDRESS) && DCachePresent())
                {
                    vPortEnterCritical();
                    HAL_FlushDCache_by_Addr(src_buf_loca, src_left_len);
                    vPortExitCritical();
                }
                #endif

                ret = dma_memcpy(TX_BUF_DMA_MEMCPY_CH, src_buf_loca, dst_buf_loca, src_left_len);
                ASSERT_ERR(ret == 0);

                net_tx_buf_copy_dma_done_waiting();
                rtos_mutex_unlock(net_tx_buf_cp_mutex);
            }
            else
#endif
                memcpy(dst_buf_loca, src_buf_loca, src_left_len);
            dst_left_len -= src_left_len;
            dst_buf_loca += src_left_len;
            length -= src_left_len;
            src_left_len = 0;
        }
        else
        {
#if TX_BUF_DMA_MEMCPY
            if (dst_left_len > TX_BUF_DMA_COPY_THRESHOLD)
            {
                int ret;
                rtos_mutex_lock(net_tx_buf_cp_mutex);

                #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
                if ((src_buf_loca >= PSRAM_BASE_ADDRESS) && DCachePresent())
                {
                    vPortEnterCritical();
                    HAL_FlushDCache_by_Addr(src_buf_loca, dst_left_len);
                    vPortExitCritical();
                }
                #endif

                ret = dma_memcpy(TX_BUF_DMA_MEMCPY_CH, src_buf_loca, dst_buf_loca, dst_left_len);
                ASSERT_ERR(ret == 0);

                net_tx_buf_copy_dma_done_waiting();
                rtos_mutex_unlock(net_tx_buf_cp_mutex);
            }
            else
#endif
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

/**
 ****************************************************************************************
 * @brief Callback used by the networking stack to push a buffer for transmission by the
 * WiFi interface.
 *
 * @param[in] net_if Pointer to the network interface on which the TX is done
 * @param[in] p_buf  Pointer to the buffer to transmit
 *
 * @return ERR_OK upon successful pushing of the buffer, ERR_BUF otherwise
 ****************************************************************************************
 */
LWIP_FUNC_ATTR static err_t net_if_output(struct netif *net_if, struct pbuf *p_buf)
{
    err_t status = ERR_BUF;
#ifdef WIFI_HOST_STANDALONE
    toggle_debug_gpio_2(1);
#endif
    // Increase the ref count so that the buffer is not freed by the networking stack
    // until it is actually sent over the WiFi interface
    pbuf_ref(p_buf);

#ifdef TX_BUF_COPY
    if (netif_is_up(net_if) && net_if_fun.tx_start_fn)
    {
        uint8_t retry_times = 0;
        void *tx_buf;
retry:
        tx_buf = net_tx_alloc_mac_buf(p_buf, 0);
        if (tx_buf)
        {
            status = ERR_OK;
            net_if_fun.tx_start_fn(net_if, tx_buf, NULL, NULL);
        }
        else
        {
            if (retry_times++ < TX_BUF_COPY_RETRY_TIMES)
            {
                rtos_delay(TX_BUF_COPY_RETRY_DELAY_MS);
                goto retry;
            }
            //CLOG("drop\n");
            status = ERR_WOULDBLOCK;
        }
    }
    else
    {
        status = ERR_WOULDBLOCK;
    }
    net_buf_tx_free(p_buf);
#else
    // Push the buffer and verify the status
    if (netif_is_up(net_if) && net_if_fun.tx_start_fn)
    {
        if (net_if_fun.tx_start_fn(net_if, p_buf, NULL, NULL) == 0)
        {
            status = ERR_OK;
        }
    }
    else
    {
        pbuf_free(p_buf);
        status = ERR_IF;
    }
#endif

#ifdef WIFI_HOST_STANDALONE
    toggle_debug_gpio_2(0);
#endif
    return (status);
}

/**
 ****************************************************************************************
 * @brief Callback used by the networking stack to setup the network interface.
 * This function should be passed as a parameter to netifapi_netif_add().
 *
 * @param[in] net_if Pointer to the network interface to setup
 * @param[in] p_buf  Pointer to the buffer to transmit
 *
 * @return ERR_OK upon successful setup of the interface, other status otherwise
 ****************************************************************************************
 */
static err_t net_if_init(struct netif *net_if)
{
    err_t status = ERR_OK;

    #if LWIP_NETIF_HOSTNAME
    {
        /* Initialize interface hostname */
        net_if->hostname = "ListenAi";
    }
    #endif /* LWIP_NETIF_HOSTNAME */

    net_if->name[ 0 ] = 'w';
    net_if->name[ 1 ] = 'l';

    net_if->output = etharp_output;
    net_if->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    #if LWIP_IGMP
    net_if->flags |= NETIF_FLAG_IGMP;
    #endif
    net_if->hwaddr_len = ETHARP_HWADDR_LEN;
    // hwaddr is updated in net_if_add
    net_if->mtu = LLC_ETHER_MTU;
    net_if->linkoutput = net_if_output;

    return status;
}

uint16_t net_ip_chksum(const void *dataptr, int len)
{
    // Simply call the LwIP function
    return lwip_standard_chksum(dataptr, len);
}

int net_if_add(net_if_t *net_if,
               const uint8_t *mac_addr,
               const uint32_t *ipaddr,
               const uint32_t *netmask,
               const uint32_t *gw,
               void *vif)
{
    err_t status;

    status = netifapi_netif_add(net_if,
                               (const ip4_addr_t *)ipaddr,
                               (const ip4_addr_t *)netmask,
                               (const ip4_addr_t *)gw,
                               vif,
                               net_if_init,
                               tcpip_input);
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
    return netif_find(name);
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

void net_if_up(net_if_t *net_if)
{
    netifapi_netif_set_up(net_if);
}

void net_if_down(net_if_t *net_if)
{
    netifapi_netif_set_down(net_if);
}

void net_if_set_default(net_if_t *net_if)
{
    netifapi_netif_set_default(net_if);
}

void net_if_set_ip(net_if_t *net_if, uint32_t ip, uint32_t mask, uint32_t gw)
{
    if (!net_if)
        return;

    netif_set_addr(net_if, (const ip4_addr_t *)&ip, (const ip4_addr_t *)&mask,
                   (const ip4_addr_t *)&gw);
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

LWIP_FUNC_ALIGN int net_if_input(net_buf_rx_t *buf, net_if_t *net_if, void *addr, uint16_t len, net_buf_free_fn free_fn)
{
    struct pbuf *p = NULL;
#ifdef WIFI_HOST_STANDALONE
    toggle_debug_gpio_2(1);
#endif
#if RX_BUF_COPY
    net_buf_rx_t *copy_buf = NULL;
    int retry = 0;
    while (!copy_buf && retry < 5) {
        copy_buf = rtos_malloc(sizeof(net_buf_rx_t) + len);
        if (!copy_buf) {
            rtos_delay(10);
            retry++;
        }
    }
    if (!copy_buf) {
        free_fn(buf);
	CLOGD("NO buf allocate, free rx packet \r\n");
#ifdef WIFI_HOST_STANDALONE
        toggle_debug_gpio_2(0);
#endif
        return -1;
    }

    uint8_t *copy_addr = ((uint8_t *)copy_buf) + sizeof(net_buf_rx_t);
    memcpy(copy_addr, addr, len);
    copy_buf->custom_free_function = (pbuf_free_custom_fn)rtos_free;
    free_fn(buf);

    p = pbuf_alloced_custom(PBUF_RAW, len, PBUF_REF, copy_buf, copy_addr, len);
    if (p == NULL) {
        rtos_free(copy_buf);
#ifdef WIFI_HOST_STANDALONE
        toggle_debug_gpio_2(0);
#endif
        return -1;
    }
    if (net_if->input(p, net_if))
    {
        pbuf_free(p);
#else
    buf->custom_free_function = (pbuf_free_custom_fn)free_fn;
    p = pbuf_alloced_custom(PBUF_RAW, len, PBUF_REF, buf, addr, len);
    ASSERT_ERR(p != NULL);

    if (net_if->input(p, net_if))
    {
        free_fn(buf);
#endif
#ifdef WIFI_HOST_STANDALONE
        toggle_debug_gpio_2(0);
#endif
        return -1;
    }
#ifdef WIFI_HOST_STANDALONE
    toggle_debug_gpio_2(0);
#endif
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

    // Bypass the sanity check for hosted mode
#ifndef WIFI_HOST_STANDALONE
//    ASSERT_ERR(!TST_SHRAM_PTR(buf->payload));
#endif
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
        // Sanity check - the payload shall be in shared RAM
#ifndef WIFI_HOST_STANDALONE
       // ASSERT_ERR(!TST_SHRAM_PTR(buf->payload));
#endif

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

LWIP_FUNC_ATTR void net_buf_tx_free(net_buf_tx_t *buf)
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
    pbuf_cat(net_buf_tx_1, net_buf_tx_2);
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
#ifdef TX_BUF_COPY
    int ret;
    int i;
    struct net_tx_buf_head *head;

    for (i = 0; i < NET_TX_BUF_CNT; i++)
    {
        head = (struct net_tx_buf_head *)&net_tx_buf_mem[i];
        head->is_short = 0;
#if defined(CFG_AMP_IPC_TCPIP) && defined(CFG_AMP_IPC_MASTER)
        head->is_master_core = 1;
#else
        head->is_master_core = 0;
#endif
        ret = rtos_queue_write(net_tx_queue_buf, &head, 0, 0);
        ASSERT_ERR(ret == 0);
    }
    for (i = 0; i < NET_SHORT_TX_BUF_CNT; i++)
    {
        head = (struct net_tx_buf_head *)&net_short_tx_buf_mem[i];
        head->is_short = 1;
#if defined(CFG_AMP_IPC_TCPIP) && defined(CFG_AMP_IPC_MASTER)
        head->is_master_core = 1;
#else
        head->is_master_core = 0;
#endif
        ret = rtos_queue_write(net_tx_queue_short_buf, &head, 0, 0);
        ASSERT_ERR(ret == 0);
    }

#if TX_BUF_DMA_MEMCPY
    {
        uint8_t ch;

        dma_initialize();

        ASSERT_ERR(!dma_channel_is_reserved(TX_BUF_DMA_MEMCPY_CH));
        ch = dma_channel_reserve(TX_BUF_DMA_MEMCPY_CH, net_tx_buf_copy_dma_event_handler, 0, DMA_CACHE_SYNC_NOP);
        ASSERT_ERR(ch != DMA_CHANNEL_ANY);

        if (rtos_mutex_create(&net_tx_buf_cp_mutex))
        {
            ASSERT_ERR(0);
        }

        CLOG("%s: use DMA channel %d", __func__, ch);
    }
#endif

#endif
    if (net_if_fun.net_init_done_cb)
        net_if_fun.net_init_done_cb();
}

volatile bool net_tx_buf_copy_flag = false;
bool net_is_tx_buf_copy(void)
{
    return net_tx_buf_copy_flag;
}

int net_init(net_if_call_fun *net_cb)
{
    int i;

    for (i = 0; i < NX_NB_L2_FILTER; i++)
    {
        l2_filter[i].net_if = NULL;
    }
    for (i = 0; i < WLIF_IDX_MAX; i++)
    {
        net_if_handle.netif[i] = rtos_calloc(sizeof(net_if_t), sizeof(uint8_t));
    }

    if (rtos_semaphore_create(&l2_semaphore, 1, 0))
    {
        ASSERT_ERR(0);
    }

    if (rtos_mutex_create(&l2_mutex))
    {
        ASSERT_ERR(0);
    }

    net_if_fun = *net_cb;

#ifdef TX_BUF_COPY
    net_tx_buf_copy_flag = true;

    if (rtos_queue_create(sizeof(void *), NET_TX_BUF_CNT, &net_tx_queue_buf))
    {
        ASSERT_ERR(0);
    }

    if (rtos_queue_create(sizeof(void *), NET_SHORT_TX_BUF_CNT, &net_tx_queue_short_buf))
    {
        ASSERT_ERR(0);
    }
#endif

    // Initialize the TCP/IP stack
    tcpip_init(net_init_done, NULL);

    return 0;
}

static void net_l2_send_cfm(uint32_t frame_id, bool acknowledged, void *arg)
{
    if (arg)
        *((bool *)arg) = acknowledged;
    l2_send_ack = acknowledged;
    CLOG("%s:%d\n", __func__, l2_send_ack);
    rtos_semaphore_signal(l2_semaphore, false);
}

int net_l2_send(net_if_t *net_if, const uint8_t *data, int data_len, uint16_t ethertype,
                const uint8_t *dst_addr, bool *ack)
{
    int res;
    uint8_t fail_retry_thres = 3;
    uint8_t fail_retry_times = 0;

    if (net_if == NULL || data == NULL || data_len >= net_if->mtu)
        return -1;

    l2_send_ack = false;

fail_retry:
    if (net_if_fun.tx_start_fn) {
#ifdef TX_BUF_COPY
        uint8_t retry_times = 0;
        void *tx_buf;
        net_buf_tx_t net_buf;
        uint8_t rsv_head_len = 0;

        if (dst_addr)
        {
            rsv_head_len = SIZEOF_ETH_HDR;
        }

        net_buf.payload = data;
        net_buf.tot_len = data_len;
        net_buf.len = data_len;
        net_buf.next = NULL;

retry:
        tx_buf = net_tx_alloc_mac_buf(&net_buf, rsv_head_len);
        if (tx_buf == NULL)
        {
            if (retry_times++ < TX_BUF_COPY_RETRY_TIMES)
            {
                rtos_delay(TX_BUF_COPY_RETRY_DELAY_MS);
                goto retry;
            }

            CLOG("net_l2_send fail\n");
            return -1;
        }
        else
        {
            if (rsv_head_len)
            {
                struct eth_hdr* ethhdr;

                ethhdr = (struct eth_hdr*)(((struct net_tx_buf_tag *)tx_buf)->buf);
                ethhdr->type = htons(ethertype);
                memcpy(&ethhdr->dest, dst_addr, sizeof(struct eth_addr));
                memcpy(&ethhdr->src, net_if->hwaddr, sizeof(struct eth_addr));
            }
        }

        rtos_mutex_lock(l2_mutex);

        res = net_if_fun.tx_start_fn(net_if, tx_buf, net_l2_send_cfm, ack);
#else
        struct pbuf *pbuf;

        /* In most of the cases, it'll cause some problems if pbuf alloc failed when running
         * throughput test, so calling pbuf_alloc_wait instead of pbuf_alloc.*/
        pbuf = pbuf_alloc_wait(PBUF_LINK, data_len, PBUF_RAM);
        if (pbuf == NULL)
        {
            CLOG("l2 netbuf null\n");
            return -1;
        }

        memcpy(pbuf->payload, data, data_len);

        if (dst_addr)
        {
            // Need to add ethernet header as tx_start_fn is called directly
            struct eth_hdr* ethhdr;
            if (pbuf_header(pbuf, SIZEOF_ETH_HDR))
            {
                pbuf_free(pbuf);
                return -1;
            }
            ethhdr = (struct eth_hdr*)pbuf->payload;
            ethhdr->type = htons(ethertype);
            memcpy(&ethhdr->dest, dst_addr, sizeof(struct eth_addr));
            memcpy(&ethhdr->src, net_if->hwaddr, sizeof(struct eth_addr));
        }

        rtos_mutex_lock(l2_mutex);

        res = net_if_fun.tx_start_fn(net_if, pbuf, net_l2_send_cfm, ack);
#endif
    } else {
        return -1;
    }

    // Wait for the transmission completion
    rtos_semaphore_wait(l2_semaphore, -1);

    // Now new L2 transmissions are possible
    rtos_mutex_unlock(l2_mutex);

    if (!l2_send_ack)
    {
        if (fail_retry_times++ < fail_retry_thres)
        {
            CLOG("l2 send retry %d", fail_retry_times);
            goto fail_retry;
        }
    }

    return res;
}


int net_l2_socket_create(net_if_t *net_if, uint16_t ethertype)
{
    struct l2_filter_tag *filter = NULL;
    socklen_t len = sizeof(filter->conn);
    int i;

    /* First find free filter and check that socket for this ethertype/net_if couple
       doesn't already exists */
    for (i = 0; i < NX_NB_L2_FILTER; i++)
    {
        if ((l2_filter[i].net_if == net_if) &&
            (l2_filter[i].ethertype == ethertype))
        {
            return -1;
        }
        else if ((filter == NULL) && (l2_filter[i].net_if == NULL))
        {
            filter = &l2_filter[i];
        }
    }

    if (!filter)
        return -1;

    /* Note: we create DGRAM socket here but in practice we don't care, net_eth_receive
       will use the socket as a L2 raw socket */
    filter->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (filter->sock < 0)
        return -1;

    if (getsockopt(filter->sock, SOL_SOCKET, SO_CONNINFO, &(filter->conn), &len))
    {
        close(filter->sock);
        return -1;
    }
    filter->net_if = net_if;
    filter->ethertype = ethertype;

    return filter->sock;
}

int net_l2_socket_delete(int sock)
{
    int i;
    for (i = 0; i < NX_NB_L2_FILTER; i++)
    {
        if ((l2_filter[i].net_if != NULL) &&
            (l2_filter[i].sock == sock))
        {
            l2_filter[i].net_if = NULL;
            close(l2_filter[i].sock);
            l2_filter[i].sock = -1;
            return 0;
        }
    }

    return -1;
}
err_t net_eth_receive(struct pbuf *pbuf, struct netif *netif)
{
    struct l2_filter_tag *filter = NULL;
    struct eth_hdr* ethhdr = pbuf->payload;
    uint16_t ethertype = ntohs(ethhdr->type);
    struct netconn *conn;
    struct netbuf *buf;
    int i;

    for (i = 0; i < NX_NB_L2_FILTER; i++)
    {
        if ((l2_filter[i].net_if == netif) &&
            (l2_filter[i].ethertype == ethertype))
        {
            filter = &l2_filter[i];
            break;
        }
    }

    if (!filter)
        return ERR_VAL;

    buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
    if (buf == NULL)
    {
        return ERR_MEM;
    }

    buf->p = pbuf;
    buf->ptr = pbuf;
    conn = filter->conn;

    if (sys_mbox_trypost(&conn->recvmbox, buf) != ERR_OK)
    {
        netbuf_delete(buf);
        return ERR_OK;
    }
    else
    {
        #if LWIP_SO_RCVBUF
        SYS_ARCH_INC(conn->recv_avail, pbuf->tot_len);
        #endif /* LWIP_SO_RCVBUF */
        /* Register event with callback */
        API_EVENT(conn, NETCONN_EVT_RCVPLUS, pbuf->tot_len);
    }

    return ERR_OK;
}

int net_dhcp_start(net_if_t *net_if)
{
    #if LWIP_IPV4 && LWIP_DHCP
    if (netifapi_dhcp_start(net_if) ==  ERR_OK)
        return 0;
    #endif //LWIP_IPV4 && LWIP_DHCP

    return -1;
}

void net_dhcp_stop(net_if_t *net_if)
{
    #if LWIP_IPV4 && LWIP_DHCP
    netifapi_dhcp_stop(net_if);
    #endif //LWIP_IPV4 && LWIP_DHCP
}

int net_dhcp_release(net_if_t *net_if)
{
    #if LWIP_IPV4 && LWIP_DHCP
    if (netifapi_dhcp_release(net_if) ==  ERR_OK)
        return 0;
    #endif //LWIP_IPV4 && LWIP_DHCP

    return -1;
}

int net_dhcp_address_obtained(net_if_t *net_if)
{
    #if LWIP_IPV4 && LWIP_DHCP
    if (dhcp_supplied_address(net_if))
        return 0;
    #endif //LWIP_IPV4 && LWIP_DHCP

    return -1;
}

int net_set_dns(uint32_t dns_server)
{
    #if LWIP_DNS
    ip_addr_t ip;
    ip_addr_set_ip4_u32(&ip, dns_server);
    dns_setserver(0, &ip);
    return 0;
    #else
    return -1;
    #endif
}

int net_get_dns(uint32_t *dns_server)
{
    #if LWIP_DNS
    const ip_addr_t *ip;

    if (dns_server == NULL)
        return -1;

    ip = dns_getserver(0);
    *dns_server = ip_addr_get_ip4_u32(ip);
    return 0;
    #else
    return -1;
    #endif
}

int net_compat_check(size_t netif_size)
{
    return (netif_size != sizeof(net_if_t));
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

void net_tx_cfm(uint32_t frame_id, bool acknowledged, void *arg)
{

}

bool net_ip_task_avail(void)
{
    return true;
}

void net_dhcps_start(struct netif * netif)
{
    dhcps_start(netif);
}

void net_dhcps_stop(void)
{
    dhcps_stop();
}

void net_wifi_init_done(void)
{
}

ls_err_t lwip_pbuf_alloc(const struct pbuf ** pbuf, pbuf_layer layer, u16_t length, pbuf_type type)
{
    *pbuf = pbuf_alloc(layer, length, type);

    return LS_OK;
}

ls_err_t lwip_pbuf_free(struct pbuf *p, uint8_t *count)
{
    *count = pbuf_free(p);

    return LS_OK;
}

char* net_get_monitor_name(void)
{
    return "wl1";
}
