/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool random_mac_if_mac_invalid;
} mac_manager_config_t;

typedef struct {
    /**
     * @brief Set the MAC address
     * @param mac[in]: MAC address in uint8_t array
     * @param mac_len[in]: MAC address length
     * @retval The result execute.
     */
    int (*set)(const uint8_t *mac, size_t mac_len);

    /**
     * @brief Get the MAC address
     * @param mac[out]: output buffer
     * @param mac_len[in, out]: input and output buffer length of mac
     * @retval The result execute.
     */
    int (*get)(uint8_t *mac, size_t *mac_len);

    /**
     * @brief Generate random MAC address
     * @param mac[out]: generated mac address
     * @param mac_len[in, out]: input and output buffer length of mac
     * @retval The result execute.
     */
    int (*random)(uint8_t *mac, size_t *mac_len);

    /**
     * @brief Delete saved MAC address
     * @retval The result execute.
     */
    int (*del)(void);
} mac_manager_content_ops_t;

typedef struct {
    void *(*malloc)(size_t size);
    void (*free)(void *ptr);
} mac_manager_mem_ops_t;

typedef struct mac_manager_s mac_manager_t;

mac_manager_t *mac_manager_init(mac_manager_mem_ops_t *mem_ops, mac_manager_content_ops_t *ops, mac_manager_config_t *config);

int mac_manager_get(mac_manager_t *obj, uint8_t *mac_addr, size_t mac_addr_len);

int mac_manager_set(mac_manager_t *obj, const uint8_t *mac_addr, size_t mac_addr_len);

int mac_manager_del(mac_manager_t *obj);

void mac_manager_deinit(mac_manager_t *obj);

#ifdef __cplusplus
}
#endif