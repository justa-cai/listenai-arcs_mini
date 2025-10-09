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


#ifndef _DHCP_STATE_H_
#define _DHCP_STATE_H_

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

bool dhcp_ip_addr_restore(void *netif);

void dhcp_ip_addr_store(void *netif);

void dhcp_ip_addr_erase(void *netif);

bool get_ssid_config(void);

void set_ssid_config(bool enabled);

uint32_t get_restored_ip_addr(void);
void set_restored_ip_addr(const uint8_t *ip_bytes);


#ifdef __cplusplus
}
#endif

#endif /*  _DHCP_STATE_H_ */