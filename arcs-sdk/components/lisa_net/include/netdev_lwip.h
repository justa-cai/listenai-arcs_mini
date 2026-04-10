/**
 * @file netdev_lwip.h
 * @brief Network device adapter for lwIP (WiFi) interface
 */

#ifndef __NETDEV_LWIP_H__
#define __NETDEV_LWIP_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register lwIP network device
 *
 * This function should be called after WiFi initialization is complete
 *
 * @param name Device name (e.g., "wifi0", "wlan0")
 * @param priority Device priority (lower value = higher priority)
 * @return 0 on success, -1 on failure
 */
int netdev_lwip_register(const char *name, uint8_t priority);

/**
 * @brief Unregister lwIP network device
 *
 * This function should be called during WiFi deinitialization
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_lwip_unregister(const char *name);

/**
 * @brief Update lwIP netdev status from WiFi events
 *
 * @param link_up Link status
 * @param internet_up Internet connectivity status
 */
void netdev_lwip_update_status(bool link_up, bool internet_up);

/**
 * @brief Update lwIP netdev IP address information
 *
 * @param ip_addr IP address (network byte order)
 * @param netmask Netmask (network byte order)
 * @param gw Gateway (network byte order)
 */
void netdev_lwip_update_ip_info(uint32_t ip_addr, uint32_t netmask, uint32_t gw);

/**
 * @brief Update lwIP netdev DNS server
 *
 * @param dns_num DNS server index (0 or 1)
 * @param dns_server DNS server address (network byte order)
 */
void netdev_lwip_update_dns(uint8_t dns_num, uint32_t dns_server);

#ifdef __cplusplus
}
#endif

#endif /* __NETDEV_LWIP_H__ */
