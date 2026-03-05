//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "lwip/opt.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include <stdbool.h>
#include "netif/dhcp_state.h"
#include "lisa_kv.h"

#include "lwip/prot/iana.h"
#include "lwip/prot/dhcp.h"

#ifndef CONFIG_LWIP_DHCP_IP_ADDR_KV_KEY
#define CONFIG_LWIP_DHCP_IP_ADDR_KV_KEY "lwip.dhcp_ip_addr"
#endif

static uint32_t restored_ip_addr = 0;

/**
 * @brief 获取当前保存的 IP 地址
 * @return 返回 restored_ip_addr 的值
 */
uint32_t get_restored_ip_addr(void) {
    return restored_ip_addr;
}

/**
 * @brief 设置新的 IP 地址
 * @param new_ip 要设置的 IP 地址（32 位无符号整数）
 */
void set_restored_ip_addr(const uint8_t *ip_bytes)
{
    if (ip_bytes == NULL) {
        restored_ip_addr = 0;
        return;
    }
    restored_ip_addr = ((uint32_t)ip_bytes[3] << 24) | ((uint32_t)ip_bytes[2] << 16) | ((uint32_t)ip_bytes[1] << 8) |
                       (uint32_t)ip_bytes[0];
}

void print_ip(uint32_t ip) {
    uint8_t *bytes = (uint8_t *)&ip;
    CLOGI("%d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
}

static bool ssid_config_flag = false;

bool get_ssid_config(void)
{
    return ssid_config_flag;
}

void set_ssid_config(bool enabled)
{
    ssid_config_flag = enabled;
}

bool dhcp_ip_addr_restore(void *netif)
{
    bool err = false;
    struct netif *net = (struct netif *)netif;
    struct dhcp *dhcp = netif_dhcp_data(net);
    uint8_t *bytes;

    CLOGV("netif%d restore ip addr\n", net->num);

    if (restored_ip_addr == 0) {
        uint8_t *blob_data = NULL;
        int blob_len = 0;
        if (lisa_kv_get_blob(CONFIG_LWIP_DHCP_IP_ADDR_KV_KEY, &blob_data, &blob_len) == 0 && blob_len == sizeof(uint32_t)) {
            restored_ip_addr = *(uint32_t *)blob_data;
            bytes = (uint8_t *)&restored_ip_addr;
            CLOGV("netif-%d read restore-ip %d.%d.%d.%d from LISA_KV\n", net->num, bytes[0], bytes[1], bytes[2], bytes[3]);
            lisa_kv_free(blob_data);
        } else {
            CLOGV("netif-%d read store-ip from LISA_KV failed\n", net->num);
            if (blob_data) {
                lisa_kv_free(blob_data);
            }
        }
    }

    if (restored_ip_addr != 0) {
        bytes = (uint8_t *)&restored_ip_addr;
        CLOGV("netif-%d restore ip addr %d.%d.%d.%d\n", net->num, bytes[0], bytes[1], bytes[2], bytes[3]);
        dhcp->offered_ip_addr.addr = restored_ip_addr;
        err = true;
    }

    return err;
}

void dhcp_ip_addr_store(void *netif)
{
    struct netif *net = (struct netif *)netif;
    struct dhcp *dhcp = netif_dhcp_data(net);
    uint32_t ip_addr = dhcp->offered_ip_addr.addr;
    uint8_t *bytes = (uint8_t *)&ip_addr;

    CLOGV("netif%d store ip addr\n", net->num);
    if (restored_ip_addr != ip_addr) {
        restored_ip_addr = dhcp->offered_ip_addr.addr;
        if (lisa_kv_set_blob(CONFIG_LWIP_DHCP_IP_ADDR_KV_KEY, (uint8_t *)(&ip_addr), sizeof(uint32_t)) == 0) {
            CLOGV("netif-%d save store-ip %d.%d.%d.%d success to LISA_KV\n", net->num, bytes[0], bytes[1], bytes[2], bytes[3]);
        } else {
            CLOGE("netif-%d store ip addr to LISA_KV failed\n", net->num);
        }
        bytes = (uint8_t *)&restored_ip_addr;
        CLOGV("netif-%d store ip %d.%d.%d.%d\n", net->num, bytes[0], bytes[1], bytes[2], bytes[3]);
    }
}

void dhcp_ip_addr_clear(void)
{
    uint32_t ip_addr;

    CLOGV("dhcp_ip_addr_clear\n");
    if (restored_ip_addr) {
        restored_ip_addr = ip_addr = 0;
        if (lisa_kv_del(CONFIG_LWIP_DHCP_IP_ADDR_KV_KEY) == 0) {
            CLOGV("clear store-ip addr from LISA_KV success\n");
        }
    }
}

void dhcp_append_extra_opts(struct netif *netif, uint8_t state, struct dhcp_msg *msg_out, uint16_t *options_out_len)
{

    LWIP_UNUSED_ARG(netif);
    LWIP_UNUSED_ARG(state);
    LWIP_UNUSED_ARG(msg_out);
    LWIP_UNUSED_ARG(options_out_len);
#if LWIP_DHCP_ENABLE_CLIENT_ID
    if (state == DHCP_STATE_RENEWING || state == DHCP_STATE_REBINDING ||
      state == DHCP_STATE_REBOOTING || state == DHCP_STATE_OFF ||
      state == DHCP_STATE_REQUESTING || state == DHCP_STATE_BACKING_OFF || state == DHCP_STATE_SELECTING) {
    size_t i;
    u8_t *options = msg_out->options + *options_out_len;
    LWIP_ERROR("dhcp_append(client_id): options_out_len + 3 + netif->hwaddr_len <= DHCP_OPTIONS_LEN",
               *options_out_len + 3U + netif->hwaddr_len <= DHCP_OPTIONS_LEN, return;);
    *options_out_len = *options_out_len + netif->hwaddr_len + 3;
    *options++ = DHCP_OPTION_CLIENT_ID;
    *options++ = netif->hwaddr_len + 1;
    *options++ = LWIP_IANA_HWTYPE_ETHERNET;
    for (i = 0; i < netif->hwaddr_len; i++) {
      *options++ = netif->hwaddr[i];
    }
  }
#endif
}
