/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mac_manager.h"
#include "lisa_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

struct mac_manager_s {
    mac_manager_content_ops_t content_ops;
    mac_manager_mem_ops_t mem_ops;
    mac_manager_config_t config;
    bool init_done;
};

#define TAG             "mac_manager"

#define UINT8_MAC_LEN   6

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

static bool check_mac_is_valid(const uint8_t *mac, size_t mac_len)
{
    static const uint8_t zero_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t ff_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (!mac || mac_len != UINT8_MAC_LEN) {
        return false; 
    }

    // 检查是否为多播地址 (第一个字节的 LSB 为 1)
    // 多播地址保留用于组寻址，不能作为设备的唯一标识符
    if ((mac[0] & 0x01) != 0) {
        return false;
    }

    if (memcmp(mac, zero_mac, UINT8_MAC_LEN) == 0) {
        return false;
    }

    if (memcmp(mac, ff_mac, UINT8_MAC_LEN) == 0) {
         return false;
    }

    return true; 
}

static int check_mac_if_exist(mac_manager_t *obj)
{
    if (!obj || !obj->init_done) {
        return -1;
    }

    uint8_t mac_addr[UINT8_MAC_LEN] = {0};
    size_t mac_addr_len = UINT8_MAC_LEN;

    int ret = obj->content_ops.get(mac_addr, &mac_addr_len);
    if (ret != 0) {
        return -2;
    }

    return 0;
}

static void random_mac(mac_manager_t *obj, char *out_mac)
{
    uint8_t mac[UINT8_MAC_LEN];
    size_t mac_len = UINT8_MAC_LEN;

    if (!obj || !obj->init_done || !out_mac) {
        return;
    }

    obj->content_ops.random(mac, &mac_len);

    memcpy(out_mac, mac, mac_len);
}

int mac_manager_set(mac_manager_t *obj, const uint8_t *mac_addr, size_t mac_addr_len)
{
    if (!obj || !obj->init_done || !mac_addr) {
        return -1;
    }

    if (!check_mac_is_valid(mac_addr, mac_addr_len)) {
        LISA_LOGE(TAG, "mac_manager_set: invalid mac: %02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        return -1;
    }
    
    return obj->content_ops.set(mac_addr, mac_addr_len);
}

int mac_manager_del(mac_manager_t *obj)
{
    if (!obj || !obj->init_done) {
        return -1;
    }

    return obj->content_ops.del();
}

static int gen_mac_and_set(mac_manager_t *obj, char *out_mac, size_t out_mac_len)
{
    if (!obj || !obj->init_done || !out_mac || out_mac_len != UINT8_MAC_LEN) {
        return -1;
    }
    random_mac(obj, out_mac);
    return mac_manager_set(obj, (uint8_t *)out_mac, UINT8_MAC_LEN);
}

mac_manager_t *mac_manager_init(
    mac_manager_mem_ops_t *mem_ops,
    mac_manager_content_ops_t *ops,
    mac_manager_config_t *config
)
{
    int ret = 0;

    if (mem_ops == NULL || ops == NULL || config == NULL) {
        return NULL;
    }
    if (mem_ops->malloc == NULL || mem_ops->free == NULL) {
        return NULL;
    }

    if (ops->set == NULL || ops->get == NULL || ops->del == NULL || ops->random == NULL) {
        return NULL;
    }

    struct mac_manager_s *obj = (struct mac_manager_s *)mem_ops->malloc(sizeof(struct mac_manager_s));
    if (!obj) {
        return NULL;
    }
    memcpy(&(obj->mem_ops), mem_ops, sizeof(mac_manager_mem_ops_t));
    memcpy(&(obj->content_ops), ops, sizeof(mac_manager_content_ops_t));
    memcpy(&(obj->config), config, sizeof(mac_manager_config_t));
    obj->init_done = true;

    if (config->random_mac_if_mac_invalid) {
        if (check_mac_if_exist(obj) != 0) {
            char out_mac[UINT8_MAC_LEN] = {0};
            ret = gen_mac_and_set(obj, out_mac, UINT8_MAC_LEN);
            if (ret != 0) {
                LISA_LOGE(TAG, "mac_manager_init: failed to set mac address");
            }
        }
    }

    return obj;
}

void mac_manager_deinit(mac_manager_t *obj)
{
    if (!obj || !obj->init_done) {
        return;
    }
    obj->init_done = false;
    obj->mem_ops.free(obj);
}

int mac_manager_get(mac_manager_t *obj, uint8_t *mac_addr, size_t mac_addr_len)
{
    if (!obj || !obj->init_done) {
        return -1;
    }

    if (mac_addr == NULL || mac_addr_len != UINT8_MAC_LEN) {
        return -1;
    }

    int r = obj->content_ops.get(mac_addr, &mac_addr_len);
    if (r != 0)
    {
        return r;
    }

    if (!check_mac_is_valid(mac_addr, mac_addr_len)) {
        if (obj->config.random_mac_if_mac_invalid) {
            char out_mac[UINT8_MAC_LEN] = {0};
            int ret = gen_mac_and_set(obj, out_mac, UINT8_MAC_LEN);
            if (ret != 0) {
                return -1;
            } else {
                memcpy(mac_addr, out_mac, UINT8_MAC_LEN);
                return 0;
            }
        } else {
            return -1;
        }
    }

    return 0;
}