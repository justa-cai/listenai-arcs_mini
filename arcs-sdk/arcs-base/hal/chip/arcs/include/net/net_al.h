/**
 ****************************************************************************************
 *
 * @file net_al.h
 *
 * @brief Declaration of the networking stack abstraction layer.
 * The functions declared here shall be implemented in the networking stack and call the
 * corresponding functions.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef NET_AL_H_
#define NET_AL_H_

/**
 ****************************************************************************************
 * @defgroup FHOST_NET_AL FHOST_NET_AL
 * @ingroup FHOST_AL
 * @brief Network stack interface
 *
 * The Fully Hosted firmware requires an implementation of the functions described here.
 *
 * A file net_def.h must also be provided and include definition of the following types:
 * - @b net_if_t:
 * Network stack internal representation of a interface. FHOST module simply keep a
 * reference to such structure it is never dereferenced and as such doesn't need to be
 * fully defined.
 *
 * - @b net_buf_rx_t:
 * Network stack header for a RX buffer (i.e. doesn't contains data, just a pointer to
 * it). It is allocated (but not initialized) by the FHOST module and must then be fully
 * defined.
 *
 * - @b net_buf_tx_t:
 * Network stack header for a TX buffer (i.e. doesn't contains data, just a pointer to
 * it). It is never dereferenced (@ref net_buf_tx_info is used to read it) and as such
 * doesn't need to be fully defined in net_def.h
 *
 * <b>TX path: </b>\n
 * The network stack should push buffer to the FHOST firmware using the function
 * @ref fhost_tx_start. The buffers pushed should contains a IEEE 802.3 (aka ethernet)
 * MAC header. The FHOST module will take care to replace it by a IEEE 802.11 MAC
 * header.\n
 * Each buffer allocated for TX shall be allocated with at least @ref NET_AL_TX_HEADROOM
 * bytes available in the buffer headroom. \n
 * The payload of a TX buffer may be split on several segments (up to 3) but the first
 * segment must at least contains the full IEEE 802.3 header and the headroom.\n
 * Each data segment allocated for TX shall also be allocated in SHARED memory. Indeed
 * The FHOST module uses @ref net_buf_tx_info to retrieve data segment addresses and
 * length and passes this information 'directly' to the MACHW for transmission.
 *
 * <b>RX path: </b>\n
 * When a buffer is received by the FHOST module, the IEEE 802.11 MAC header is replaced
 * by a IEEE 802.3 MAC header and then the buffer is pushed to the Network stack using
 * @ref net_if_input.
 *
 * <b>IP related functions: </b>\n
 * There are a few functions that are only needed if @ref fhost_set_vif_ip or
 * @ref fhost_get_vif_ip function of fhost_api will be used by the integration layer.
 * If this is not the case then there is no need to implemment them and the macro
 * @p NET_AL_NO_IP should be defined in net_def.h.
 *
 * The optional functions are:
 * - net_if_set_default
 * - net_if_set_ip
 * - net_if_get_ip
 * - net_dhcp_start
 * - net_dhcp_stop
 * - net_dhcp_release
 * - net_dhcp_address_obtained
 * - net_set_dns
 * - net_get_dns
 *
 * @{
 ****************************************************************************************
 */
#include "net_def.h"
#include "ls_wifi_type.h"

/// Minimum headroom to include in all TX buffer
#ifdef TX_BUF_COPY
#if NX_WAPI
#define NET_AL_TX_HEADROOM 72
#else
#define NET_AL_TX_HEADROOM 64
#endif
#else
#if NX_WAPI
#define NET_AL_TX_HEADROOM 400
#else
#define NET_AL_TX_HEADROOM 392
#endif
#endif

/// netif handle
typedef struct netif_handle
{
    net_if_t *netif[WLIF_IDX_MAX];
} netif_handle_t;
/// Prototype for a function to free a network buffer */
typedef void (*net_buf_free_fn)(void *net_buf);

typedef void (*cb_fhost_tx)(uint32_t frame_id, bool acknowledged, void *arg);

typedef struct net_if_call_fun_t
{
    void (*net_init_done_cb)(void);
    int (*tx_start_fn)(net_if_t *net_if, net_buf_tx_t *net_buf,
                          cb_fhost_tx cfm_cb, void *cfm_cb_arg);
    int32_t (*tx_start_from_ipc)(void *param);
    void (*rx_push_from_ipc)(void *net_buf);
} net_if_call_fun;

/// NET TX buf
#define NET_TX_BUF_DESC_LEN 388
struct net_tx_buf_head
{
    void *next;
    uint8_t is_short;
    uint8_t is_master_core;
    uint16_t buf_len;
    uint8_t tx_desc_rsv[NET_TX_BUF_DESC_LEN];
};

#define NET_TX_BUF_UNIT_SIZE 1536
#define NET_TX_BUF_CNT 10
struct net_tx_buf_tag
{
    struct net_tx_buf_head head;
    uint8_t buf[NET_TX_BUF_UNIT_SIZE];
};

#define NET_SHORT_TX_BUF_UNIT_SIZE 128
#define NET_SHORT_TX_BUF_CNT 6
struct net_short_tx_buf_tag
{
    struct net_tx_buf_head head;
    uint8_t buf[NET_SHORT_TX_BUF_UNIT_SIZE];
};

#define NET_SIMSOC_TX_BUF_UNIT_SIZE 256
#define NET_SIMSOC_TX_BUF_CNT 6
struct net_simsoc_tx_buf_tag
{
    struct net_tx_buf_head head;
    uint8_t buf[NET_SIMSOC_TX_BUF_UNIT_SIZE];
};

#define TX_BUF_COPY_RETRY_TIMES 3
#define TX_BUF_COPY_RETRY_DELAY_MS 2
/*
 * FUNCTIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Call the checksum computation function of the TCP/IP stack
 *
 * @param[in] dataptr Pointer to the data buffer on which the checksum is computed
 * @param[in] len Length of the data buffer
 *
 * @return The computed checksum
 ****************************************************************************************
 */
uint16_t net_ip_chksum(const void *dataptr, int len);

/**
 ****************************************************************************************
 * @brief Add a network interface
 *
 * This function must initialize the provided net_if_t structure.
 * The private VIF structure must be the one returned by @ref net_if_vif_info
 *
 * @param[in] net_if    Pointer to the net_if structure to add
 * @param[in] mac_addr  MAC address of the interface
 * @param[in] ipaddr    IPv4 address of the interface (NULL if not available)
 * @param[in] netmask   Net mask of the interface (NULL if not available)
 * @param[in] gw        Gateway address of the interface (NULL if not available)
 * @param[in] vif_priv  Pointer to the VIF private structure
 *
 * @return 0 on success and != 0 if error occurred
 ****************************************************************************************
 */
int net_if_add(net_if_t *net_if, const uint8_t *mac_addr, const uint32_t *ipaddr,
               const uint32_t *netmask, const uint32_t *gw, void *vif_priv);

/**
 ****************************************************************************************
 * @brief Get network interface MAC address
 *
 * @param[in] net_if Pointer to the net_if structure
 *
 * @return Pointer to interface MAC address
 ****************************************************************************************
 */
const uint8_t *net_if_get_mac_addr(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Get pointer to network interface from its name
 *
 * @param[in] name Name of the interface
 *
 * @return pointer to the net_if structure and NULL if such interface doesn't exist.
 ****************************************************************************************
 */
net_if_t *net_if_find_from_name(const char *name);

/**
 ****************************************************************************************
 * @brief Get name of network interface
 *
 * Copy the name on the interface (including a terminating a null byte) in the given
 * buffer. If buffer is not big enough then the interface name is truncated and no
 * null byte is written in the buffer.
 *
 * @param[in] net_if  Pointer to the net_if structure
 * @param[in] buf     Buffer to write the interface name
 * @param[in] len     Length of the buffer.
 *
 * @return  < 0 if error occurred, otherwise the number of characters (excluding the
 * terminating null byte) needed to write the interface name. If return value is greater
 * or equal to @p len, it means that the interface name has been truncated
 ****************************************************************************************
 */
int net_if_get_name(net_if_t *net_if, char *buf, int len);

/**
 ****************************************************************************************
 * @brief Indicate that the network interface is now up (i.e. able to do traffic)
 *
 * @param[in] net_if Pointer to the net_if structure
 ****************************************************************************************
 */
void net_if_up(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Indicate that the network interface is now down
 *
 * @param[in] net_if Pointer to the net_if structure
 ****************************************************************************************
 */
void net_if_down(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Call the networking stack input function.
 * This function is supposed to link the payload data and length to the RX buffer
 * structure passed as parameter. The free_fn function shall be called when the networking
 * stack is not using the buffer anymore.
 *
 * @param[in] buf Pointer to the RX buffer structure
 * @param[in] net_if Pointer to the net_if structure that receives the packet
 * @param[in] addr Pointer to the payload data
 * @param[in] len Length of the data available at payload address
 * @param[in] free_fn Pointer to buffer freeing function to be called after use
 *
 * @return 0 on success and != 0 if packet is not accepted
 ****************************************************************************************
 */
int net_if_input(net_buf_rx_t *buf, net_if_t *net_if, void *addr, uint16_t len,
                 net_buf_free_fn free_fn);

/**
 ****************************************************************************************
 * @brief Get the pointer to the VIF private structure attached to a net interface.
 *
 * @param[in] net_if Pointer to the net_if structure
 *
 * @return The pointer to the VIF private structure attached to the net interface when it
 * has been initialized (@ref net_if_add)
 ****************************************************************************************
 */
void *net_if_vif_info(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Allocate a buffer for TX.
 *
 * This function is used to transmit buffer that do not originate from the Network Stack.
 * (e.g. a management frame sent by wpa_supplicant)
 * This function allocates a buffer for transmission. The buffer must still reserve
 * @ref NET_AL_TX_HEADROOM headroom space like for regular TX buffers.
 *
 * @param[in] length   Size, in bytes, of the payload
 *
 * @return The pointer to the allocated TX buffer and NULL if allocation failed
 ****************************************************************************************
 */
net_buf_tx_t *net_buf_tx_alloc(uint32_t length);
/**
 ****************************************************************************************
 * @brief Allocate a buffer for TX with a reference to a payload.
 *
 * This function allocates a buffer for transmission. It only allocates memory for the
 * TX buffer structure. The pointer to the payload is set to NULL and should be updated
 *
 * @param[in] length   Size, in bytes, of the payload
 *
 * @return The pointer to the allocated TX buffer and NULL if allocation failed
 ****************************************************************************************
 */
net_buf_tx_t *net_buf_tx_alloc_ref(uint32_t length);

/**
 ****************************************************************************************
 * @brief Provides information on a TX buffer.
 *
 * This function is used by the FHOST module before queuing the buffer for transmission.
 * This function must returns information (pointer and length) on all data segments that
 * compose the buffer. Each buffer must at least have one data segment.
 * It must also return a pointer to the headroom (of size @ref NET_AL_TX_HEADROOM)
 * which must be reserved for each TX buffer on their first data segment.
 *
 * @param[in]     buf       Pointer to the TX buffer structure
 * @param[in]     tot_len   Total size in bytes on the buffer (includes size of all data
 *                          segment)
 * @param[in,out] seg_cnt   Contains the maximum number of data segment supported (i.e.
 *                          the size of @p seg_addr and @p seg_len parameter) and must be
 *                          updated with the actual number of segment in this buffer.
 * @param[out]    seg_addr  Table to retrieve the address of each segment.
 * @param[out]    seg_len   Table to retrieve the length, in bytes, of each segment.
 *
 * @return The pointer to the headroom reserved at the beginning of the first data segment
 ****************************************************************************************
 */
void *net_buf_tx_info(net_buf_tx_t *buf, uint16_t *tot_len, int *seg_cnt,
                      uint32_t seg_addr[], uint16_t seg_len[]);

/**
 ****************************************************************************************
 * @brief Free a TX buffer that was involved in a transmission.
 *
 * @param[in] buf Pointer to the TX buffer structure
 ****************************************************************************************
 */
void net_buf_tx_free(net_buf_tx_t *buf);

/**
 ****************************************************************************************
 * @brief Initialize the networking stack
 *
 * Once the initialization is complete (i.e. the associated RTOS has started) the
 * function @ref fhost_task_ready must be called with @p IP_TASK as parameter.
 *
 * @return 0 on success and != 0 if packet is not accepted
 ****************************************************************************************
 */
int net_init(net_if_call_fun *net_cb);

/**
 ****************************************************************************************
 * @brief Send a L2 (aka ethernet) packet
 *
 * Send data on the link layer (L2). If destination address is not NULL, Ethernet header
 * will be added (using ethertype parameter) and MAC address of the sending interface is
 * used as source address. If destination address is NULL, it means that ethernet header
 * is already present and frame should be send as is.
 * The data buffer will be copied by this function, and must then be freed by the caller.
 *
 * The primary purpose of this function is to allow the supplicant sending EAPOL frames.
 * As these frames are often followed by addition/deletion of crypto keys, that
 * can cause encryption to be enabled/disabled in the MAC, it is required to ensure that
 * the packet transmission is completed before proceeding to the key setting.
 * This function shall therefore be blocking until the frame has been transmitted by the
 * MAC.
 *
 * @param[in] net_if    Pointer to the net_if structure.
 * @param[in] data      Data buffer to send.
 * @param[in] data_len  Buffer size, in bytes.
 * @param[in] ethertype Ethernet type to set in the ethernet header. (in host endianess)
 * @param[in] dst_addr  Ethernet address of the destination. If NULL then it means that
 *                      ethernet header is already present in the frame (and in this case
 *                      ethertype should be ignored)
 * @param[out] ack      Optional to get transmission status. If not NULL, the value
 *                      pointed is set to true if peer acknowledged the transmission and
 *                      false in all other cases.
 *
 * @return 0 on success and != 0 if packet hasn't been sent
 ****************************************************************************************
 */
int net_l2_send(net_if_t *net_if, const uint8_t *data, int data_len, uint16_t ethertype,
                const uint8_t *dst_addr, bool *ack);

/**
 ****************************************************************************************
 * @brief Create a L2 (aka ethernet) socket for specific packet
 *
 * Create a L2 socket that will receive specified frames: a given ethertype on a given
 * interface.
 * It is expected to fail if a L2 socket for the same ethertype/interface couple already
 * exists.
 *
 * @note As L2 sockets are not specified in POSIX standard, the implementation of such
 * function may be impossible in some network stack.
 *
 * @param[in] net_if    Pointer to the net_if structure.
 * @param[in] ethertype Ethernet type to filter. (in host endianess)
 *
 * @return <0 if error occurred and the socket descriptor otherwise.
 ****************************************************************************************
 */
int net_l2_socket_create(net_if_t *net_if, uint16_t ethertype);

/**
 ****************************************************************************************
 * @brief Delete a L2 (aka ethernet) socket
 *
 * @param[in] sock Socket descriptor returned by @ref net_l2_socket_create
 *
 * @return 0 on success and != 0 if error occurred.
 ****************************************************************************************
 **/
int net_l2_socket_delete(int sock);

/**
 ****************************************************************************************
 * @brief Set a network interface as the default output interface
 *
 * If IP routing failed to select an output interface solely based on interface and
 * destination addresses then this interface will be selected.
 * Use a NULL parameter to reset the default interface.
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the net_if structure
 ****************************************************************************************
 */
void net_if_set_default(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Set IPv4 address of an interface
 *
 * It is assumed that only one address can be configured on the interface and then
 * setting a new address can be used to replace/delete the current address.
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the net_if structure
 * @param[in] ip     IPv4 address
 * @param[in] mask   IPv4 network mask
 * @param[in] gw     IPv4 gateway address
 ****************************************************************************************
 */
void net_if_set_ip(net_if_t *net_if, uint32_t ip, uint32_t mask, uint32_t gw);

/**
 ****************************************************************************************
 * @brief Get IPv4 address of an interface
 *
 * Set to NULL parameter you're not interested in.
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in]  net_if Pointer to the net_if structure
 * @param[out] ip     IPv4 address
 * @param[out] mask   IPv4 network mask
 * @param[out] gw     IPv4 gateway address
 * @return 0 if requested parameters have been updated successfully and !=0 otherwise.
 ****************************************************************************************
 */
int net_if_get_ip(net_if_t *net_if, uint32_t *ip, uint32_t *mask, uint32_t *gw);

/**
 ****************************************************************************************
 * @brief Start DHCP procedure on a given interface
 *
 * @note Not need if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the interface on which DHCP must be started
 *
 * @return 0 if DHCP procedure successfully started and != 0 if an error occurred
 ****************************************************************************************
 */
int net_dhcp_start(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Stop DHCP procedure on a given interface
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the interface on which DHCP must be stopped
 ****************************************************************************************
 */
void net_dhcp_stop(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Release DHCP lease on a given interface
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the interface on which DHCP must be released
 * @return 0 if DHCP lease has been released and != 0 if an error occurred
 ****************************************************************************************
 */
int net_dhcp_release(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Check if an IP has been assigned with DHCP
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] net_if Pointer to the interface to test
 *
 * @return 0 if ip address assigned to the interface has been obtained via DHCP and != 0
 * otherwise
 ****************************************************************************************
 */
int net_dhcp_address_obtained(net_if_t *net_if);

/**
 ****************************************************************************************
 * @brief Configure DNS server IP address
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[in] dns_server  DNS server IPv4 address
 * @return 0 on success and != 0 if error occurred.
 ****************************************************************************************
 */
int net_set_dns(uint32_t dns_server);

/**
 ****************************************************************************************
 * @brief Get DNS server IP address
 *
 * @note Not needed if NET_AL_NO_IP is defined
 *
 * @param[out] dns_server  DNS server IPv4 address
 * @return 0 on success and != 0 if error occurred.
 ****************************************************************************************
 */
int net_get_dns(uint32_t *dns_server);

/**
 ****************************************************************************************
 * @brief Concatenate 2 Tx buffers
 *
 * @param[in] buf1 Pointer to the TX buffer structure 1
 * @param[in] buf2 Pointer to the TX buffer structure 2
 ****************************************************************************************
 */
void net_buf_tx_cat(net_buf_tx_t *buf1, net_buf_tx_t *buf2);
int32_t net_ipc_rx_cfm(void *ecb, void *param);
int32_t net_ipc_send(void *ecb, void *param);
void net_ipc_send_cfm(uint32_t frame_id, bool acknowledged, void *arg);
net_if_t *netif_alloc(void);
net_if_t *net_if_get(int wifi_idx);
int16_t net_if_to_idx(net_if_t *net_if);
bool net_ip_task_avail(void);
void net_tx_cfm(uint32_t frame_id, bool acknowledged, void *arg);
void net_dhcps_start(struct netif * netif);
void net_dhcps_stop(void);
void net_wifi_init_done(void);
ls_err_t lwip_pbuf_alloc(const struct pbuf ** pbuf, pbuf_layer layer, u16_t length, pbuf_type type);
ls_err_t lwip_pbuf_free(struct pbuf *p, uint8_t *count);

void net_tx_release_mac_buf(void *tx_buf, bool acknowledged);
void *net_tx_alloc_mac_buf(net_buf_tx_t *buf, uint16_t rsv_head_len);
bool net_is_tx_buf_copy(void);

char* net_get_monitor_name(void);
#endif // NET_AL_H_
/**
 * @}
 */
