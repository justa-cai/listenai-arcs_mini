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

#ifdef CONFIG_VENDOR_STORAGE_ENABLE
#include "vendor_ops.h"

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

static int vendor_storage_random(uint8_t *mac, size_t *mac_len)
{
    unsigned int curTickCount = (unsigned int)xTaskGetTickCount();

    LISA_LOGI(TAG, "vendor_storage_random curTickCount:%d\n", curTickCount);
    srand(curTickCount);

    mac[0] = 0x26;
    mac[1] = 0x48;
    for (size_t i = 2; i < *mac_len; i++) {
        mac[i] = rand() % 256;
    }

    return 0;
}

static int vendor_storage_get(uint8_t *mac_buf, size_t *mac_buf_len)
{
    if (!mac_buf || !mac_buf_len || *mac_buf_len < 6) {
        return -1;
    }

    if (vendor_storage_read(VENDOR_WIFI_MAC_ID, mac_buf, *mac_buf_len) == 0) {
        return 0;
    }

    efuse_init();
    uint64_t uuid = efuse_read_uuid();
    if (uuid == 0) {
        LISA_LOGE(TAG, "efuse read empty uuid");
        return -1;
    }
    mac_buf[0] = ARCS_MAC_HEADER_0;
    mac_buf[1] = ARCS_MAC_HEADER_1;
    mac_buf[2] = (uuid & 0xFF);
    mac_buf[3] = (uuid >> 32 >> 16) & 0xFF;
    mac_buf[4] = (uuid >> 32 >> 8) & 0xFF;
    mac_buf[5] = (uuid >> 32) & 0xFF;

    *mac_buf_len = 6;
    return 0;
}

static int vendor_storage_set(const uint8_t *mac, size_t mac_len)
{
    if (!mac || mac_len != 6) {
        return -1;
    }

    return vendor_storage_write(VENDOR_WIFI_MAC_ID, (void *)mac, mac_len);
}

static int vendor_storage_del(void)
{
    return -1;
}

static mac_manager_content_ops_t mac_manager_venor_ops = {
    .random = vendor_storage_random,
    .get    = vendor_storage_get,
    .set    = vendor_storage_set,
    .del    = vendor_storage_del,
};
#endif

int listen_mac_manager_init(void)
{
    int ret = 0;
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = true,
    };
    mac_manager = mac_manager_init(
        mac_manager_ops_get()->mem_ops,
#ifdef CONFIG_VENDOR_STORAGE_ENABLE
        &mac_manager_venor_ops,
#else
        mac_manager_ops_get()->content_ops,
#endif
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