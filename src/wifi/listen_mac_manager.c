/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_MAC_MANAGER

#define TAG "listen_mac_manager"

#include "mac_manager.h"
#include "mac_manager_ops.h"

#include "assert.h"
#include "lisa_log.h"

#include <stdint.h>
#include <string.h>


static mac_manager_t *mac_manager = NULL;
static uint8_t m_mac_addr[6] = {0};

int listen_mac_manager_init(void)
{
    int ret = 0;
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = true,
    };
    mac_manager = mac_manager_init(
        mac_manager_ops_get()->mem_ops,
        mac_manager_ops_get()->content_ops,
        &config);
    assert(mac_manager);

    ret = mac_manager_get(mac_manager, m_mac_addr, sizeof(m_mac_addr));
    LOGI("MAC Manager init done, ret:%d, mac addr:%02x:%02x:%02x:%02x:%02x:%02x", ret, m_mac_addr[0], m_mac_addr[1], m_mac_addr[2], m_mac_addr[3], m_mac_addr[4], m_mac_addr[5]);
    assert(ret == 0);
    return ret;
}
uint8_t *listen_mac_manager_get(void)
{
    return m_mac_addr;
}

#endif  // CONFIG_MAC_MANAGER