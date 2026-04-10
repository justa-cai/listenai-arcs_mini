/**
 * @file netdev_at.h
 * @brief Network device adapter for AT-based (4G) module interface
 */

#ifndef __NETDEV_AT_H__
#define __NETDEV_AT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register AT-based network device
 *
 * This function should be called after 4G module initialization is complete
 *
 * @param name Device name (e.g., "4g0", "lte0")
 * @param priority Device priority (lower value = higher priority)
 * @return 0 on success, -1 on failure
 */
int netdev_at_register(const char *name, uint8_t priority);

/**
 * @brief Unregister AT-based network device
 *
 * This function should be called during 4G module deinitialization
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_at_unregister(const char *name);

/**
 * @brief Update 4G netdev status
 *
 * @param link_up Link status
 * @param internet_up Internet connectivity status
 */
void netdev_4g_update_status(bool link_up, bool internet_up);

/**
 * @brief Update 4G netdev IP address information
 *
 * @param ip_addr IP address (network byte order)
 * @param netmask Netmask (network byte order)
 * @param gw Gateway (network byte order)
 */
void netdev_4g_update_ip_info(uint32_t ip_addr, uint32_t netmask, uint32_t gw);

/**
 * @brief Update 4G netdev DNS server
 *
 * @param dns_num DNS server index (0 or 1)
 * @param dns_server DNS server address (network byte order)
 */
void netdev_4g_update_dns(uint8_t dns_num, uint32_t dns_server);

#ifdef __cplusplus
}
#endif

#endif /* __NETDEV_AT_H__ */
