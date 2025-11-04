#include <string.h>
#include "dhcps.h"
#include "lwip/netifapi.h"
#include "lwip/sys.h"

/* use this to check whether the message is dhcp related or not */
static const uint8_t dhcp_magic_cookie[4] = {99, 130, 83, 99};
static const uint8_t dhcp_option_lease_time[] = {0x00, 0x00, 0x70,0x80}; //8hours.
//static const uint8_t dhcp_option_lease_time[] = {0x00, 0x00, 0x0e, 0x10}; // one hour
//static const uint8_t dhcp_option_interface_mtu_576[] = {0x02, 0x40};
static const uint8_t dhcp_option_interface_mtu[] = {0x05, 0xDC};

//static struct dhcp_server_state dhcp_server_state_machine;
static uint8_t dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;
/* recorded the client MAC addr(default sudo mac) */
//static uint8_t dhcps_record_first_client_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
/* recorded transaction ID (default sudo id)*/
static uint8_t dhcp_recorded_xid[4] = {0xff, 0xff, 0xff, 0xff};

/* UDP Protocol Control Block(PCB) */
static struct udp_pcb *dhcps_pcb;

static ip_addr_t dhcps_send_broadcast_address;
static ip_addr_t dhcps_local_address;
static ip_addr_t dhcps_dns_address;
static ip_addr_t dhcps_pool_start;
static ip_addr_t dhcps_pool_end;
static ip_addr_t dhcps_local_mask;
static ip_addr_t dhcps_local_gateway;
static ip_addr_t dhcps_network_id;
static ip_addr_t dhcps_subnet_broadcast;
ip_addr_t dhcps_allocated_client_address;
static int dhcps_addr_pool_set = 0;
static ip_addr_t dhcps_addr_pool_start;
static ip_addr_t dhcps_addr_pool_end;
static ip_addr_t dhcps_owned_first_ip;
static ip_addr_t dhcps_owned_last_ip;
static uint8_t dhcps_num_of_available_ips;
static struct dhcp_msg *dhcp_message_repository;
static int dhcp_message_total_options_lenth;

/* allocated IP range */
static struct table  ip_table;
static ip_addr_t client_request_ip;
uint8_t client_addr[6];
uint8_t dhcps_reply_use_broadcast;

static sys_sem_t dhcps_ip_table_semaphore;

static struct netif * dhcps_netif;

#ifdef WIFI_API
//extern int  wifi_sap_save_sta_ip(uint8_t *mac, uint32_t ip);
#endif

/**
  * @brief  latch the specific ip in the ip table.
  * @param  d the specific index
  * @retval None.
  */
#if (!IS_USE_FIXED_IP)
static void mark_ip_in_table(uint8_t d)
{
#if (debug_dhcps)
    lwip_printf("mark ip %d\n",d);
#endif
    sys_arch_sem_wait(&dhcps_ip_table_semaphore, 0);
    if (0 < d && d <= 32) {
        ip_table.ip_range[0] = MARK_RANGE1_IP_BIT(ip_table, d);
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[0] = 0x%x\n",ip_table.ip_range[0]);
#endif
    } else if (32 < d && d <= 64) {
        ip_table.ip_range[1] = MARK_RANGE2_IP_BIT(ip_table, (d - 32));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[1] = 0x%x\n",ip_table.ip_range[1]);
#endif
    } else if (64 < d && d <= 96) {
        ip_table.ip_range[2] = MARK_RANGE3_IP_BIT(ip_table, (d - 64));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[2] = 0x%x\n",ip_table.ip_range[2]);
#endif
    } else if (96 < d && d <= 128) {
        ip_table.ip_range[3] = MARK_RANGE4_IP_BIT(ip_table, (d - 96));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[3] = 0x%x\n",ip_table.ip_range[3]);
#endif
    } else if(128 < d && d <= 160) {
        ip_table.ip_range[4] = MARK_RANGE5_IP_BIT(ip_table, d - 128);
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[4] = 0x%x\n",ip_table.ip_range[4]);
#endif
    } else if (160 < d && d <= 192) {
        ip_table.ip_range[5] = MARK_RANGE6_IP_BIT(ip_table, (d - 160));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[5] = 0x%x\n",ip_table.ip_range[5]);
#endif
    } else if (192 < d && d <= 224) {
        ip_table.ip_range[6] = MARK_RANGE7_IP_BIT(ip_table, (d - 192));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[6] = 0x%x\n",ip_table.ip_range[6]);
#endif
    } else if (224 < d) {
        ip_table.ip_range[7] = MARK_RANGE8_IP_BIT(ip_table, (d - 224));
#if (debug_dhcps)
        lwip_printf("ip_table.ip_range[7] = 0x%x\n",ip_table.ip_range[7]);
#endif
    } else {
        lwip_printf("Request ip over the range(1-128)\n");
    }
    sys_sem_signal(&dhcps_ip_table_semaphore);
}
#ifdef CONFIG_DHCPS_KEPT_CLIENT_INFO
static void save_client_addr(ip_addr_t *client_ip, uint8_t *hwaddr)
{
    uint8_t d = (uint8_t)ip4_addr4(client_ip);
    uint32_t start_ip = 0;

    start_ip = DHCP_POOL_START;

    sys_arch_sem_wait(&dhcps_ip_table_semaphore, 0);
    if(((d-start_ip) < DHCPS_MAX_CLIENT_NUM) && ((d-start_ip) >= 0))
        memcpy(ip_table.client_mac[d-start_ip], hwaddr, 6);
    else
        lwip_printf("save ip over the range. start_ip:%u,ip:%u.%u.%u.%u\n",
                    (unsigned int)start_ip, (unsigned int)ip4_addr1(client_ip), (unsigned int)ip4_addr2(client_ip),
                    (unsigned int)ip4_addr3(client_ip), (unsigned int)ip4_addr4(client_ip));

#if (debug_dhcps)
        lwip_printf("start_ip:%d,ip %d.%d.%d.%d, hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
                    start_ip,ip4_addr1(client_ip), ip4_addr2(client_ip), ip4_addr3(client_ip), ip4_addr4(client_ip),
                    ip_table.client_mac[d-start_ip][0], ip_table.client_mac[d-start_ip][1],
                    ip_table.client_mac[d-start_ip][2], ip_table.client_mac[d-start_ip][3],
                    ip_table.client_mac[d-start_ip][4], ip_table.client_mac[d-start_ip][5]);
#endif
    sys_sem_signal(&dhcps_ip_table_semaphore);
}

static uint8_t check_client_request_ip(ip_addr_t *client_req_ip, uint8_t *hwaddr)
{
    uint32_t ip_addr4 = 0, i;
    uint32_t start_ip = 0;
    uint32_t end_ip = 0;

    start_ip = DHCP_POOL_START;
    end_ip = DHCP_POOL_END;

#if (debug_dhcps)
    lwip_printf("request ip %d.%d.%d.%d, hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
                ip4_addr1(client_req_ip), ip4_addr2(client_req_ip), ip4_addr3(client_req_ip), ip4_addr4(client_req_ip),
                hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
#endif

    sys_arch_sem_wait(&dhcps_ip_table_semaphore, 0);
    for(i=start_ip;i<=end_ip;i++)
    {
#if (debug_dhcps)
        lwip_printf("client[%d] = %02x:%02x:%02x:%02x:%02x:%02x\n",i-start_ip,
                    ip_table.client_mac[i-start_ip][0],ip_table.client_mac[i-start_ip][1],
                    ip_table.client_mac[i-start_ip][2],ip_table.client_mac[i-start_ip][3],
                    ip_table.client_mac[i-start_ip][4],ip_table.client_mac[i-start_ip][5]);
#endif
        if(memcmp(ip_table.client_mac[i-start_ip], hwaddr, 6) == 0){
            if((ip_table.ip_range[i/32]>>(i%32-1)) & 1){
                ip_addr4 = i;
                break;
            }
        }
    }
    sys_sem_signal(&dhcps_ip_table_semaphore);

    if(i == end_ip +1)
        ip_addr4 = 0;

//Exit:
    return ip_addr4;
}

#if 0
static void dump_client_table()
{
    int i;
    uint8_t *p = NULL;
    lwip_printf("ip_range: %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                ip_table.ip_range[0], ip_table.ip_range[1], ip_table.ip_range[2], ip_table.ip_range[3],
                ip_table.ip_range[4], ip_table.ip_range[5], ip_table.ip_range[6], ip_table.ip_range[7]);
    for(i=1; i<=DHCPS_MAX_CLIENT_NUM; i++)
    {
        p = ip_table.client_mac[i];
        lwip_printf("Client[%d]: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                    i, p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    lwip_printf("\r\n");
}
#endif

#endif //CONFIG_DHCPS_KEPT_CLIENT_INFO
#endif

/**
  * @brief  get one usable ip from the ip table of dhcp server.
  * @param: None
  * @retval the usable index which represent the ip4_addr(ip) of allocated ip addr.
  */
#if (!IS_USE_FIXED_IP)
static uint8_t search_next_ip(void)
{
    uint8_t range_count, offset_count;
    uint8_t start, end;
    uint8_t max_count;
    if(dhcps_addr_pool_set){
        start = (uint8_t)ip4_addr4(&dhcps_addr_pool_start);
        end = (uint8_t)ip4_addr4(&dhcps_addr_pool_end);
    }else{
        start = 0;
        end = 255;
    }
    sys_arch_sem_wait(&dhcps_ip_table_semaphore, 0);
    for (range_count = 0; range_count < (max_count = 8); range_count++) {
        for (offset_count = 0;offset_count < 32; offset_count++) {
            if ((((ip_table.ip_range[range_count] >> offset_count) & 0x01) == 0)
                &&(((range_count * 32) + (offset_count + 1)) >= start)
                &&(((range_count * 32) + (offset_count + 1)) <= end)) {
                sys_sem_signal(&dhcps_ip_table_semaphore);
                return ((range_count * 32) + (offset_count + 1));
            }
        }
    }
    sys_sem_signal(&dhcps_ip_table_semaphore);
    return 0;
}

/**
  * @brief  check if assigned ip is used the ip table of dhcp server.
  * @param: assigned ip
  * @retval 0 means used, 1 means not used and is usable.
  */
static uint8_t search_assigned_ip(uint8_t ip)
{
    uint8_t range_count, offset_count;
    uint8_t start, end;
    uint8_t max_count;
    if(dhcps_addr_pool_set){
        start = (uint8_t)ip4_addr4(&dhcps_addr_pool_start);
        end = (uint8_t)ip4_addr4(&dhcps_addr_pool_end);
    }else{
        start = 0;
        end = 255;
    }

    if (ip < start || ip > end)
    {
        return 0;
    }

    sys_arch_sem_wait(&dhcps_ip_table_semaphore, 0);
    for (range_count = 0; range_count < (max_count = 8); range_count++) {
        for (offset_count = 0;offset_count < 32; offset_count++) {
            if ((((ip_table.ip_range[range_count] >> offset_count) & 0x01) == 1)
                &&(((range_count * 32) + (offset_count + 1)) == ip)) {
                sys_sem_signal(&dhcps_ip_table_semaphore);
                return 0;
            }
        }
    }
    sys_sem_signal(&dhcps_ip_table_semaphore);
    return 1;
}

#endif

/**
  * @brief  fill in the option field with message type of a dhcp message.
  * @param  msg_option_base_addr: the addr be filled start.
  *        message_type: the type code you want to fill in
  * @retval the start addr of the next dhcp option.
  */
static uint8_t *add_msg_type(uint8_t *msg_option_base_addr, uint8_t message_type)
{
    uint8_t *option_start;
    msg_option_base_addr[0] = DHCP_OPTION_CODE_MSG_TYPE;
    msg_option_base_addr[1] = DHCP_OPTION_LENGTH_ONE;
    msg_option_base_addr[2] = message_type;
    option_start = msg_option_base_addr + 3;
    if (DHCP_MESSAGE_TYPE_NAK == message_type)
        *option_start++ = DHCP_OPTION_CODE_END;
    return option_start;
}


static uint8_t *fill_one_option_content(uint8_t *option_base_addr,
    uint8_t option_code, uint8_t option_length, void *copy_info)
{
    uint8_t *option_data_base_address;
    uint8_t *next_option_start_address = NULL;
    option_base_addr[0] = option_code;
    option_base_addr[1] = option_length;
    option_data_base_address = option_base_addr + 2;
#if (debug_dhcps)
    lwip_printf("[%s] dst=0x%x src=0x%x option_length=%d\n", __func__, option_length);
#endif
    switch (option_length) {
    case DHCP_OPTION_LENGTH_FOUR:
        if(copy_info)
            memcpy(option_data_base_address, copy_info, DHCP_OPTION_LENGTH_FOUR);
        next_option_start_address = option_data_base_address + 4;
        break;
    case DHCP_OPTION_LENGTH_TWO:
        if(copy_info)
            memcpy(option_data_base_address, copy_info, DHCP_OPTION_LENGTH_TWO);
        next_option_start_address = option_data_base_address + 2;
        break;
    case DHCP_OPTION_LENGTH_ONE:
        if(copy_info)
            memcpy(option_data_base_address, copy_info, DHCP_OPTION_LENGTH_ONE);
        next_option_start_address = option_data_base_address + 1;
        break;
    default:
        break;
    }
#if (debug_dhcps)
    lwip_printf("[%s] next_option_start_address 0x%x\n", __func__, next_option_start_address);
#endif
    return next_option_start_address;
}

/**
  * @brief  fill in the needed content of the dhcp offer message.
  * @param  optptr  the addr which the tail of dhcp magic field.
  * @retval    0, add ok
  *            -1, add fail
  */
static int8_t add_offer_options(uint8_t *option_start_address)
{
    // Total minimum len = 6+6+6+6+6+6+4+3+1 = 44
    uint8_t *temp_option_addr = option_start_address;
    int max_addable_option_len = dhcp_message_total_options_lenth - 4 - 3;    // -magic-type

    if(option_start_address == NULL)
    {
#if (debug_dhcps)
        lwip_printf("[%s] invalid arguements\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 1.
    The subnet mask option specifies the client's subnet mask */
    if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(option_start_address, DHCP_OPTION_CODE_SUBNET_MASK,
                        DHCP_OPTION_LENGTH_FOUR,(void *)&dhcps_local_mask);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 1\n", __func__);
#endif
        goto ERROR;
    }

        /* add DHCP options 3 (i.e router(gateway)). The time server option
        specifies a list of RFC 868 [6] time servers available to the client. */
        if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_ROUTER,
                        DHCP_OPTION_LENGTH_FOUR, (void *)&dhcps_local_address);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 3\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 6 (i.e DNS).
        The option specifies a list of DNS servers available to the client. */
     if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
         temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_DNS_SERVER,
                        DHCP_OPTION_LENGTH_FOUR, (void *)&dhcps_dns_address);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 6\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 51.
    This option is used to request a lease time for the IP address. */
     if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_LEASE_TIME,
                        DHCP_OPTION_LENGTH_FOUR, (void *)&dhcp_option_lease_time);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 51\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 54.
    The identifier is the IP address of the selected server. */
     if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_SERVER_ID,
                        DHCP_OPTION_LENGTH_FOUR, (void *)&dhcps_local_address);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 54\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 28.
    This option specifies the broadcast address in use on client's subnet.*/
    if(temp_option_addr + 6 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_BROADCAST_ADDRESS,
                        DHCP_OPTION_LENGTH_FOUR, (void *)&dhcps_subnet_broadcast);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 28\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 26.
    This option specifies the Maximum transmission unit to use */
    if(temp_option_addr + 4 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_INTERFACE_MTU,
                        DHCP_OPTION_LENGTH_TWO, (void *) &dhcp_option_interface_mtu);//dhcp_option_interface_mtu_576);
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp options 26\n", __func__);
#endif
        goto ERROR;
    }

    /* add DHCP options 31.
    This option specifies whether or not the client should solicit routers */
    if(temp_option_addr + 3 -option_start_address <= max_addable_option_len) {
        temp_option_addr = fill_one_option_content(temp_option_addr, DHCP_OPTION_CODE_PERFORM_ROUTER_DISCOVERY,
                        DHCP_OPTION_LENGTH_ONE,    NULL);
    }else{
        goto ERROR;
    }

    // END
    if(temp_option_addr + 1 -option_start_address <= max_addable_option_len) {
    *temp_option_addr++ = DHCP_OPTION_CODE_END;
    }else{
#if (debug_dhcps)
        lwip_printf("[%s] invalid dhcp end error\n", __func__);
#endif
        goto ERROR;
    }
    return 0;

ERROR:
    lwip_printf("[%s] error: add options fail\n", __func__);
    return -1;
}


/**
  * @brief  fill in common content of a dhcp message.
  * @param  m the pointer which point to the dhcp message store in.
  * @retval None.
  */
static void dhcps_initialize_message(struct dhcp_msg *dhcp_message_repository)
{
    dhcp_message_repository->op = DHCP_MESSAGE_OP_REPLY;
    dhcp_message_repository->htype = DHCP_MESSAGE_HTYPE;
    dhcp_message_repository->hlen = DHCP_MESSAGE_HLEN;
    dhcp_message_repository->hops = 0;
    memcpy((char *)dhcp_recorded_xid, (char *) dhcp_message_repository->xid,
                sizeof(dhcp_message_repository->xid));
    dhcp_message_repository->secs = 0;
    dhcp_message_repository->flags = htons(BOOTP_BROADCAST);

    memcpy((char *)dhcp_message_repository->yiaddr,
        (char *)&dhcps_allocated_client_address,
            sizeof(dhcp_message_repository->yiaddr));

    memset((char *)dhcp_message_repository->ciaddr, 0,
                sizeof(dhcp_message_repository->ciaddr));
    memset((char *)dhcp_message_repository->siaddr, 0,
                sizeof(dhcp_message_repository->siaddr));
    memset((char *)dhcp_message_repository->giaddr, 0,
                sizeof(dhcp_message_repository->giaddr));
    memset((char *)dhcp_message_repository->sname,  0,
                sizeof(dhcp_message_repository->sname));
    memset((char *)dhcp_message_repository->file,   0,
                sizeof(dhcp_message_repository->file));
    memset((char *)dhcp_message_repository->options, 0,
                dhcp_message_total_options_lenth);
    memcpy((char *)dhcp_message_repository->options, (char *)dhcp_magic_cookie,
                sizeof(dhcp_magic_cookie));
}

/**
  * @brief  init and fill in  the needed content of dhcp offer message.
  * @param  packet_buffer packet buffer for UDP.
  * @retval None.
  */
static void dhcps_send_offer(struct pbuf *packet_buffer, struct udp_pcb *udp_pcb)
{
    uint8_t temp_ip = 0;
    struct pbuf *newly_malloc_packet_buffer = NULL;
    // newly malloc a longer pbuf for dhcp offer rather than using the short pbuf from dhcp discover
    newly_malloc_packet_buffer = pbuf_alloc(PBUF_TRANSPORT, DHCP_MSG_LEN + DHCP_OFFER_OPTION_TOTAL_LENGTH_MAX, PBUF_RAM);
    if(newly_malloc_packet_buffer == NULL)
    {
        lwip_printf("[%s] error: pbuf alloc fail\n", __func__);
        return;
    }
    if(pbuf_copy(newly_malloc_packet_buffer, packet_buffer) != ERR_OK)
    {
        lwip_printf("[%s] error: pbuf copy fail\n", __func__);
        pbuf_free(newly_malloc_packet_buffer);
        return;
    }
    dhcp_message_total_options_lenth = DHCP_OFFER_OPTION_TOTAL_LENGTH_MAX;
    dhcp_message_repository = (struct dhcp_msg *)newly_malloc_packet_buffer->payload;
#if (!IS_USE_FIXED_IP)
#ifdef CONFIG_DHCPS_KEPT_CLIENT_INFO
    temp_ip = check_client_request_ip(&client_request_ip, client_addr);
#endif
    /* create new client ip */
    if(temp_ip == 0)
        temp_ip = search_next_ip();
#if (debug_dhcps)
    lwip_printf("temp_ip = %d\n",temp_ip);
#endif
    if (temp_ip == 0) {
        lwip_printf("No useable ip\n");
    }
    lwip_printf("DHCP assign ip = %d.%d.%d.%d\n", ip4_addr1(&dhcps_network_id),ip4_addr2(&dhcps_network_id),ip4_addr3(&dhcps_network_id),temp_ip);
                IP4_ADDR(&dhcps_allocated_client_address, (ip4_addr1(&dhcps_network_id)),
                ip4_addr2(&dhcps_network_id), ip4_addr3(&dhcps_network_id) , temp_ip);
#endif
    dhcps_initialize_message(dhcp_message_repository);
    if(add_offer_options(add_msg_type(&dhcp_message_repository->options[4], DHCP_MESSAGE_TYPE_OFFER)) == 0)
    {
        udp_sendto_if(udp_pcb, newly_malloc_packet_buffer, &dhcps_send_broadcast_address, DHCP_CLIENT_PORT, dhcps_netif);
    }
    pbuf_free(newly_malloc_packet_buffer);
}

/**
  * @brief  init and fill in  the needed content of dhcp nak message.
  * @param  packet buffer packet buffer for UDP.
  * @retval None.
  */
static void dhcps_send_nak(struct pbuf *packet_buffer, struct udp_pcb *udp_pcb)
{
    lwip_printf("DHCP send nack\n");
    dhcp_message_repository = (struct dhcp_msg *)packet_buffer->payload;
    dhcps_initialize_message(dhcp_message_repository);
    add_msg_type(&dhcp_message_repository->options[4], DHCP_MESSAGE_TYPE_NAK);
    udp_sendto_if(udp_pcb, packet_buffer, &dhcps_send_broadcast_address, DHCP_CLIENT_PORT, dhcps_netif);
}

/**
  * @brief  init and fill in  the needed content of dhcp ack message.
  * @param  packet buffer packet buffer for UDP.
  * @retval None.
  */
static void dhcps_send_ack(struct pbuf *packet_buffer, struct udp_pcb *udp_pcb)
{
    struct pbuf *newly_malloc_packet_buffer = NULL;
    // newly malloc a longer pbuf for dhcp ack rather than using the short pbuf from dhcp request
    newly_malloc_packet_buffer = pbuf_alloc(PBUF_TRANSPORT, DHCP_MSG_LEN + DHCP_OFFER_OPTION_TOTAL_LENGTH_MAX, PBUF_RAM);
    if(newly_malloc_packet_buffer == NULL)
    {
        lwip_printf("[%s] error: pbuf alloc fail\n", __func__);
        return;
    }
    if(pbuf_copy(newly_malloc_packet_buffer, packet_buffer) != ERR_OK)
    {
        lwip_printf("[%s] error: pbuf copy fail\n", __func__);
        pbuf_free(newly_malloc_packet_buffer);
        return;
    }
    dhcp_message_total_options_lenth = DHCP_OFFER_OPTION_TOTAL_LENGTH_MAX;
    dhcp_message_repository = (struct dhcp_msg *)newly_malloc_packet_buffer->payload;
    dhcps_initialize_message(dhcp_message_repository);
    if(add_offer_options(add_msg_type(&dhcp_message_repository->options[4], DHCP_MESSAGE_TYPE_ACK)) == 0)
    {
        udp_sendto_if(udp_pcb, newly_malloc_packet_buffer, &dhcps_send_broadcast_address, DHCP_CLIENT_PORT, dhcps_netif);
    }
    pbuf_free(newly_malloc_packet_buffer);

    lwip_printf("DHCP send ack\n");
}

/**
  * @brief  according by the input message type to reflect the correspond state.
  * @param  option_message_type the input server state
  * @retval the server state which already transfer to.
  */
uint8_t dhcps_handle_state_machine_change(uint8_t option_message_type)
{
    switch (option_message_type) {
    case DHCP_MESSAGE_TYPE_DECLINE:
        //#if (debug_dhcps)
        lwip_printf("DHCP_MESSAGE_TYPE_DECLINE\n");
        //#endif
        dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;
        break;
    case DHCP_MESSAGE_TYPE_DISCOVER:
        //#if (debug_dhcps)
        lwip_printf("DHCP_MESSAGE_TYPE_DISCOVER\n");
        //#endif
        if (dhcp_server_state_machine == DHCP_SERVER_STATE_IDLE) {
            dhcp_server_state_machine = DHCP_SERVER_STATE_OFFER;
        }
        break;
    case DHCP_MESSAGE_TYPE_REQUEST:
        //#if (debug_dhcps)
        lwip_printf("DHCP_MESSAGE_TYPE_REQUEST\n");
        //#endif
#if (!IS_USE_FIXED_IP)
#if (debug_dhcps)
        lwip_printf("dhcp_server_state_machine=%d\n", dhcp_server_state_machine);
        lwip_printf("dhcps_allocated_client_address=%d.%d.%d.%d\n",
                    ip4_addr1(&dhcps_allocated_client_address),
                    ip4_addr2(&dhcps_allocated_client_address),
                    ip4_addr3(&dhcps_allocated_client_address),
                    ip4_addr4(&dhcps_allocated_client_address));
        lwip_printf("client_request_ip=%d.%d.%d.%d\n",
                    ip4_addr1(&client_request_ip),
                    ip4_addr2(&client_request_ip),
                    ip4_addr3(&client_request_ip),
                    ip4_addr4(&client_request_ip));
#endif
        if (dhcp_server_state_machine == DHCP_SERVER_STATE_OFFER) {
            if (ip4_addr4(&dhcps_allocated_client_address) != 0) {
                if (memcmp((void *)&dhcps_allocated_client_address, (void *)&client_request_ip, 4) == 0) {
                    dhcp_server_state_machine = DHCP_SERVER_STATE_ACK;
                  } else {
                      dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
                  }
            } else {
                  dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
            }
#ifdef CONFIG_DHCPS_KEPT_CLIENT_INFO
        } else if(dhcp_server_state_machine == DHCP_SERVER_STATE_IDLE){
            uint8_t ip_addr4 = check_client_request_ip(&client_request_ip, client_addr);
            if(ip_addr4 > 0){
                IP4_ADDR(&dhcps_allocated_client_address, (ip4_addr1(&dhcps_network_id)),
                        ip4_addr2(&dhcps_network_id), ip4_addr3(&dhcps_network_id), ip_addr4);
                dhcp_server_state_machine = DHCP_SERVER_STATE_ACK;
            }
#if (!IS_USE_FIXED_IP)
            else if(ip4_addr1(&dhcps_network_id) == ip4_addr1(&client_request_ip) &&
                     ip4_addr2(&dhcps_network_id) == ip4_addr2(&client_request_ip) &&
                     ip4_addr3(&dhcps_network_id) == ip4_addr3(&client_request_ip) &&
                     search_assigned_ip(ip4_addr4(&client_request_ip))){
                IP4_ADDR(&dhcps_allocated_client_address, (ip4_addr1(&dhcps_network_id)),
                        ip4_addr2(&dhcps_network_id), ip4_addr3(&dhcps_network_id), ip4_addr4(&client_request_ip));
                dhcp_server_state_machine = DHCP_SERVER_STATE_ACK;
            }
#endif
            else{
                dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
            }
#endif
        } else {
            dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
        }
#ifdef WIFI_API
//        wifi_sap_save_sta_ip(client_addr, dhcps_allocated_client_address.addr);
#endif
#else
        if (!(dhcp_server_state_machine == DHCP_SERVER_STATE_ACK ||
            dhcp_server_state_machine == DHCP_SERVER_STATE_NAK)) {
                dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
        }
#endif
        break;
    case DHCP_MESSAGE_TYPE_RELEASE:
        lwip_printf("DHCP_MESSAGE_TYPE_RELEASE\n");
        dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;
        break;
    default:
        break;
    }

    return dhcp_server_state_machine;
}
/**
  * @brief  parse the dhcp message option part.
  * @param  optptr: the addr of the first option field.
  *         len: the total length of all option fields.
  * @retval dhcp server state.
  */
static uint8_t dhcps_handle_msg_options(uint8_t *option_start, int16_t total_option_length)
{

    int16_t option_message_type = 0;
    uint8_t *option_end = option_start + total_option_length;
    //dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;

    /* begin process the dhcp option info */
    while (option_start < option_end) {
        switch ((uint8_t)*option_start) {
        case DHCP_OPTION_CODE_MSG_TYPE:
            option_message_type = *(option_start + 2); // 2 => code(1)+lenth(1)
            break;
        case DHCP_OPTION_CODE_REQUEST_IP_ADDRESS :
#if IS_USE_FIXED_IP
            if (memcmp((char *)&dhcps_allocated_client_address,
                    (char *)option_start + 2, 4) == 0)
                dhcp_server_state_machine = DHCP_SERVER_STATE_ACK;
            else
                dhcp_server_state_machine = DHCP_SERVER_STATE_NAK;
#else
            memcpy((char *)&client_request_ip, (char *)option_start + 2, 4);
#endif
            break;
        default:
            break;
        }
        // calculate the options offset to get next option's base addr
        option_start += option_start[1] + 2; // optptr[1]: length value + (code(1)+ Len(1))
    }
    return dhcps_handle_state_machine_change(option_message_type);
}

/**
  * @brief  get message from buffer then check whether it is dhcp related or not.
  *         if yes , parse it more to undersatnd the client's request.
  * @param  same as recv callback function definition
  * @retval if message is dhcp related then return dhcp server state,
  *        otherwise return 0
  */
static uint8_t dhcps_check_msg_and_handle_options(struct pbuf *packet_buffer)
{
    int dhcp_message_option_offset;
    dhcp_message_repository = (struct dhcp_msg *)packet_buffer->payload;
    dhcp_message_option_offset = ((int)dhcp_message_repository->options
                        - (int)packet_buffer->payload);
    dhcp_message_total_options_lenth = (packet_buffer->len
                        - dhcp_message_option_offset);
    memcpy(client_addr, dhcp_message_repository->chaddr, 6);
    dhcps_reply_use_broadcast = ((BOOTP_BROADCAST & ntohs(dhcp_message_repository->flags)) != 0);
    /* check the magic number,if correct parse the content of options */
    if (memcmp((char *)dhcp_message_repository->options,
        (char *)dhcp_magic_cookie, sizeof(dhcp_magic_cookie)) == 0) {
                return dhcps_handle_msg_options(&dhcp_message_repository->options[4],
                            (dhcp_message_total_options_lenth - 4));
    }
    return 0;
}

/**
  * @brief  handle imcoming dhcp message and response message to client
  * @param  same as recv callback function definition
  * @retval None
  */
static void dhcps_receive_udp_packet_handler(void *arg, struct udp_pcb *udp_pcb,
struct pbuf *udp_packet_buffer, const struct ip4_addr *sender_addr, uint16_t sender_port)
{
    int16_t total_length_of_packet_buffer;
    struct pbuf *merged_packet_buffer = NULL;
    struct netif *input_if = netif_get_by_index(udp_packet_buffer->if_idx);

    if (input_if != dhcps_netif) {
        pbuf_free(udp_packet_buffer);
        return;
    }

    dhcp_message_repository = (struct dhcp_msg *)udp_packet_buffer->payload;
    if (udp_packet_buffer == NULL) {
        lwip_printf("System doesn't allocate any buffer\n");
        return;
    }
    //lwip_printf("\n\r dhcps_receive_udp_packet_handler \n\r");
    if (sender_port == DHCP_CLIENT_PORT) {
        total_length_of_packet_buffer = udp_packet_buffer->tot_len;
        if (udp_packet_buffer->next != NULL) {
            merged_packet_buffer = pbuf_coalesce(udp_packet_buffer,
                                PBUF_TRANSPORT);
            if (merged_packet_buffer->tot_len !=
                        total_length_of_packet_buffer) {
                pbuf_free(udp_packet_buffer);
                return;
            }
            udp_packet_buffer = merged_packet_buffer;
        }
        //lwip_printf("\n\r dhcps_receive_udp_packet_handler receive dhcp packet \n\r");
        switch (dhcps_check_msg_and_handle_options(udp_packet_buffer)) {
        case  DHCP_SERVER_STATE_OFFER:
            #if (debug_dhcps)
            lwip_printf("DHCP_SERVER_STATE_OFFER\n");
            #endif
            dhcps_send_offer(udp_packet_buffer, udp_pcb);
            break;
        case DHCP_SERVER_STATE_ACK:
            #if (debug_dhcps)
            lwip_printf("DHCP_SERVER_STATE_ACK\n");
            #endif
            dhcps_send_ack(udp_packet_buffer, udp_pcb);
#if (!IS_USE_FIXED_IP)
            mark_ip_in_table((uint8_t)ip4_addr4(&dhcps_allocated_client_address));
    #ifdef CONFIG_DHCPS_KEPT_CLIENT_INFO
            save_client_addr(&dhcps_allocated_client_address, client_addr);
            memset(&client_request_ip, 0, sizeof(client_request_ip));
            memset(&client_addr, 0, sizeof(client_addr));
            memset(&dhcps_allocated_client_address, 0, sizeof(dhcps_allocated_client_address));
            #if (debug_dhcps)
            dump_client_table();
            #endif
    #endif
#endif
            dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;
            break;
        case DHCP_SERVER_STATE_NAK:
            #if (debug_dhcps)
            lwip_printf("DHCP_SERVER_STATE_NAK\n");
            #endif
            dhcps_send_nak(udp_packet_buffer, udp_pcb);
            dhcp_server_state_machine = DHCP_SERVER_STATE_IDLE;
            break;
        case DHCP_OPTION_CODE_END:
            #if (debug_dhcps)
            lwip_printf("DHCP_OPTION_CODE_END\n");
            #endif
            break;
        default:
            break;
        }
    }

    /* free the UDP connection, so we can accept new clients */
    udp_disconnect(udp_pcb);


    //lwip_printf("%s free packet buffer\n",__func__);
    /* Free the packet buffer */
    if (merged_packet_buffer != NULL)
        pbuf_free(merged_packet_buffer);
    else
        pbuf_free(udp_packet_buffer);
}

void dhcps_set_addr_pool(int addr_pool_set, ip_addr_t * addr_pool_start, ip_addr_t *addr_pool_end)
{
    //uint8_t *ip;
    if(addr_pool_set){
        dhcps_addr_pool_set = 1;

        memcpy(&dhcps_addr_pool_start, addr_pool_start,
                            sizeof(ip_addr_t));
        //ip = &dhcps_addr_pool_start;
        //ip[3] = 100;
        memcpy(&dhcps_addr_pool_end, addr_pool_end,
                            sizeof(ip_addr_t));
        //ip = &dhcps_addr_pool_end;
        //ip[3] = 200;
    }else{
        dhcps_addr_pool_set = 0;
    }
}
/**
  * @brief  Initialize dhcp server.
  * @param  None.
  * @retval None.
  * Note, for now,we assume the server latch ip 192.168.1.1 and support dynamic
  *       or fixed IP allocation.
  */
void dhcps_init(struct netif * pnetif)
{
    uint8_t *ip;

    dhcps_netif = pnetif;

#ifdef CONFIG_DHCPS_KEPT_CLIENT_INFO
    memset(&ip_table, 0, sizeof(struct table));
#if 0
    int i = 0;
    for(i=0; i< DHCPS_MAX_CLIENT_NUM+2; i++)
        memset(ip_table.client_mac[i], 0, 6);
    dump_client_table();
#endif
#endif

    if (dhcps_pcb != NULL) {
        udp_remove(dhcps_pcb);
        dhcps_pcb = NULL;
    }

    dhcps_pcb = udp_new();
    if (dhcps_pcb == NULL) {
        lwip_printf("upd_new error\n");
        return;
    }
    IP4_ADDR(&dhcps_send_broadcast_address, 255, 255, 255, 255);
    /* get net info from net interface */

    memcpy(&dhcps_local_address, &pnetif->ip_addr,
                            sizeof( ip_addr_t));
    memcpy(&dhcps_local_mask, &pnetif->netmask,
                        sizeof(ip_addr_t));

    memcpy(&dhcps_local_gateway, &pnetif->gw,
                        sizeof(ip_addr_t));

    /* calculate the usable network ip range */
    dhcps_network_id.addr = ((pnetif->ip_addr.addr) &
                    (pnetif->netmask.addr));

    dhcps_subnet_broadcast.addr = ((dhcps_network_id.addr |
                    ~(pnetif->netmask.addr)));

    dhcps_owned_first_ip.addr = htonl((ntohl(dhcps_network_id.addr) + 1));
    dhcps_owned_last_ip.addr = htonl(ntohl(dhcps_subnet_broadcast.addr) - 1);
    dhcps_num_of_available_ips = ((ntohl(dhcps_owned_last_ip.addr)
                - ntohl(dhcps_owned_first_ip.addr)) + 1);

#if CONFIG_EXAMPLE_UART_ATCMD || CONFIG_EXAMPLE_SPI_ATCMD
#if IP_SOF_BROADCAST
  dhcps_pcb->so_options|=SOF_BROADCAST;
#endif /* IP_SOF_BROADCAST */
#endif

#if IS_USE_FIXED_IP
    IP4_ADDR(&dhcps_allocated_client_address, ip4_addr1(&dhcps_local_address)
        , ip4_addr2(&dhcps_local_address), ip4_addr3(&dhcps_local_address),
                    (ip4_addr4(&dhcps_local_address)) + 1 );
#else

    if (dhcps_ip_table_semaphore!= NULL) {
        sys_sem_free(&dhcps_ip_table_semaphore);
        dhcps_ip_table_semaphore = NULL;
    }
    sys_sem_new(&dhcps_ip_table_semaphore, 1);

    //dhcps_ip_table = (struct ip_table *)(pvPortMalloc(sizeof(struct ip_table)));
    memset(&ip_table, 0, sizeof(struct table));
    mark_ip_in_table((uint8_t)ip4_addr4(&dhcps_local_address));
    mark_ip_in_table((uint8_t)ip4_addr4(&dhcps_local_gateway));
#if 0
    for (i = 1; i < ip4_addr4(&dhcps_local_address); i++) {
        mark_ip_in_table(i);
    }
#endif
#endif
    memset(&dhcps_addr_pool_start, 0, sizeof(dhcps_addr_pool_start));
    memset(&dhcps_addr_pool_end, 0, sizeof(dhcps_addr_pool_end));
    if(dhcps_addr_pool_start.addr== 0 && dhcps_addr_pool_end.addr == 0)
    {
        memcpy(&dhcps_pool_start,&dhcps_local_address,sizeof(ip_addr_t));
        ip = (uint8_t *)&dhcps_pool_start;

        ip[3] = DHCP_POOL_START;

        memcpy(&dhcps_pool_end,&dhcps_local_address,sizeof(ip_addr_t));
        ip = (uint8_t *)&dhcps_pool_end;

        ip[3] = DHCP_POOL_END;

        dhcps_set_addr_pool(1,&dhcps_pool_start,&dhcps_pool_end);
    }
    udp_bind(dhcps_pcb, &dhcps_local_address, DHCP_SERVER_PORT);

    udp_recv(dhcps_pcb, dhcps_receive_udp_packet_handler, NULL);
}

void dhcps_deinit(void)
{
    dhcps_netif = NULL;

    if (dhcps_pcb != NULL) {
        udp_remove(dhcps_pcb);
        dhcps_pcb = NULL;
    }

    if (dhcps_ip_table_semaphore!= NULL) {
        sys_sem_free(&dhcps_ip_table_semaphore);
        dhcps_ip_table_semaphore = NULL;
    }
}

void dhcps_start(struct netif * pnetif)
{
    struct ip4_addr ipaddr = {0};
    struct ip4_addr netmask = {0};
    struct ip4_addr gw = {0};

    dhcps_deinit();

    //use user config when ip/gw/netmask all be setted
    if(!(ipaddr.addr && gw.addr && netmask.addr))
    {
        IP4_ADDR(&ipaddr, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
        IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
        IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
    }

    //netifapi_netif_set_up(pnetif);  //up after ap start done
    netif_set_addr(pnetif, &ipaddr, &netmask, &gw);
    dhcps_init(pnetif);

    lwip_printf("dhcps start\n");
}

void dhcps_stop(void)
{
    if (dhcps_netif)
        netif_set_addr(dhcps_netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4);

    dhcps_deinit();
    memset(&dhcps_addr_pool_start, 0, sizeof(dhcps_addr_pool_start));
    memset(&dhcps_addr_pool_end, 0, sizeof(dhcps_addr_pool_end));

    lwip_printf("dhcps stop\n");
}

