//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
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
#include "nvs.h"
#include "nvds_tag_def.h"

#include "lwip/prot/iana.h"
#include "lwip/prot/dhcp.h"

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
        restored_ip_addr = 0;  // 直接赋值为 0
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

    if (restored_ip_addr != 0) {
        CLOGI("restore ip addr\n");
        dhcp->offered_ip_addr.addr = restored_ip_addr;
        err = true;
    }
    return err;
}

// void dhcp_ip_addr_store(void *netif)
// {
//     struct netif *net = (struct netif *)netif;
//     struct dhcp *dhcp = netif_dhcp_data(net);
//     uint32_t ip_addr = dhcp->offered_ip_addr.addr;
//     CLOGI("store ip addr\n");
//     print_ip(restored_ip_addr);
//     print_ip(ip_addr);
//     if (restored_ip_addr != ip_addr) {
//         if (nvds_put(NVDS_LEN_IP_ADDR_ADDR, NVDS_LEN_IP_ADDR_ADDR, (uint8_t *)(&ip_addr)) == NVDS_OK) {
//             CLOGI("store ip addr success\n");
//         }
//     }
// }

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
    *options++ = netif->hwaddr_len + 1; /* option size */
    *options++ = LWIP_IANA_HWTYPE_ETHERNET;
    for (i = 0; i < netif->hwaddr_len; i++) {
      *options++ = netif->hwaddr[i];
    }
  }
#endif /* LWIP_DHCP_ENABLE_CLIENT_ID */
}
