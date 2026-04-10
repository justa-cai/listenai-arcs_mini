/**
 * @file netdev.c
 * @brief Network Device Abstraction Layer Implementation
 * @details Multi-network interface management for FreeRTOS
 *
 * Adapted from RT-Thread netdev component for FreeRTOS environment
 */

#include "netdev.h"
#include "netdev_lwip.h"
#include "netdev_at.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "semphr.h"

/* Logging macros (can be adapted to your logging system) */
#define NETDEV_LOG_E(fmt, ...) printf("[NETDEV_E] " fmt "\n", ##__VA_ARGS__)
#define NETDEV_LOG_W(fmt, ...) printf("[NETDEV_W] " fmt "\n", ##__VA_ARGS__)
#define NETDEV_LOG_I(fmt, ...) printf("[NETDEV_I] " fmt "\n", ##__VA_ARGS__)
#define NETDEV_LOG_D(fmt, ...) printf("[NETDEV_D] " fmt "\n", ##__VA_ARGS__)

/* Global variables */
struct netdev *netdev_list = NULL;
struct netdev *netdev_default = NULL;
static SemaphoreHandle_t netdev_lock = NULL;
static bool netdev_initialized = false;
static int next_ifindex = 1;  /* Interface index counter */

/* Helper functions for IP address comparison */
static inline bool ip_addr_cmp(const netdev_ip_addr_t *addr1, const netdev_ip_addr_t *addr2)
{
    return (addr1->addr == addr2->addr);
}

static inline bool ip_addr_isany(const netdev_ip_addr_t *ipaddr)
{
    return (ipaddr->addr == 0);
}

static inline void ip_addr_set_zero(netdev_ip_addr_t *ipaddr)
{
    ipaddr->addr = 0;
}

static inline void ip_addr_copy(netdev_ip_addr_t *dest, const netdev_ip_addr_t *src)
{
    dest->addr = src->addr;
}

/* Lock/unlock functions */
static inline void netdev_core_lock(void)
{
    if (netdev_lock) {
        xSemaphoreTake(netdev_lock, portMAX_DELAY);
    }
}

static inline void netdev_core_unlock(void)
{
    if (netdev_lock) {
        xSemaphoreGive(netdev_lock);
    }
}

/**
 * @brief Initialize netdev subsystem
 */
int netdev_init(void)
{
    if (netdev_initialized) {
        NETDEV_LOG_D("Network device subsystem already initialized");
        return 0;
    }

    /* Create mutex for thread-safe operations */
    netdev_lock = xSemaphoreCreateMutex();
    if (netdev_lock == NULL) {
        NETDEV_LOG_E("Failed to create netdev mutex");
        return -1;
    }

    netdev_initialized = true;
    NETDEV_LOG_I("Network device subsystem initialized successfully");
    return 0;
}

/**
 * @brief Register network interface device
 */
int netdev_register(struct netdev *netdev, const char *name, uint8_t priority, void *user_data)
{
    uint16_t flags_mask;
    uint8_t index;
    struct netdev *curr, *prev;

    if (!netdev || !name) {
        NETDEV_LOG_E("Invalid parameters for netdev_register");
        return -1;
    }

    /* Clean network interface device */
    flags_mask = NETDEV_FLAG_UP | NETDEV_FLAG_LINK_UP | NETDEV_FLAG_INTERNET_UP | NETDEV_FLAG_DHCP;
    netdev->flags &= ~flags_mask;

    /* Initialize IP addresses to zero */
    ip_addr_set_zero(&netdev->ip_addr);
    ip_addr_set_zero(&netdev->netmask);
    ip_addr_set_zero(&netdev->gw);

    for (index = 0; index < NETDEV_DNS_SERVERS_NUM; index++) {
        ip_addr_set_zero(&netdev->dns_servers[index]);
    }

    netdev->status_callback = NULL;
    netdev->addr_callback = NULL;

    /* Check name length */
    if (strlen(name) >= NETDEV_NAME_MAX) {
        NETDEV_LOG_W("Network device name too long: %s", name);
        strncpy(netdev->name, name, NETDEV_NAME_MAX - 1);
        netdev->name[NETDEV_NAME_MAX - 1] = '\0';
    } else {
        strncpy(netdev->name, name, NETDEV_NAME_MAX);
    }

    /* Set priority (use default if 0) */
    netdev->priority = (priority == 0) ? NETDEV_PRIORITY_DEFAULT : priority;

    /* Assign interface index */
    netdev->ifindex = next_ifindex++;

    netdev->user_data = user_data;
    netdev->next = NULL;

    netdev_core_lock();

    /* Insert into device list sorted by priority (higher priority value = higher priority) */
    if (netdev_list == NULL) {
        /* Empty list - insert as first */
        netdev_list = netdev;
    } else if (netdev->priority > netdev_list->priority) {
        /* New device has higher priority than head - insert at head */
        netdev->next = netdev_list;
        netdev_list = netdev;
    } else {
        /* Find insertion point to maintain sorted order */
        prev = netdev_list;
        curr = netdev_list->next;

        while (curr != NULL && curr->priority >= netdev->priority) {
            prev = curr;
            curr = curr->next;
        }

        /* Insert between prev and curr */
        netdev->next = curr;
        prev->next = netdev;
    }

    netdev_core_unlock();

    /* Set as default if no default exists */
    if (netdev_default == NULL) {
        netdev_set_default(netdev);
    }

    NETDEV_LOG_I("Network device '%s' registered successfully with priority %d",
                 netdev->name, netdev->priority);
    return 0;
}

/**
 * @brief Unregister network interface device
 */
int netdev_unregister(struct netdev *netdev)
{
    struct netdev *curr, *prev;
    struct netdev *best_candidate;

    if (!netdev) {
        return -1;
    }

    if (netdev_list == NULL) {
        return -1;
    }

    netdev_core_lock();

    /* Find and remove from list */
    prev = NULL;
    for (curr = netdev_list; curr != NULL; prev = curr, curr = curr->next) {
        if (curr == netdev) {
            if (prev == NULL) {
                /* Remove head */
                netdev_list = curr->next;
            } else {
                prev->next = curr->next;
            }

            /* If this was the default, clear it */
            if (netdev_default == netdev) {
                netdev_default = NULL;
            }
            break;
        }
    }

    netdev_core_unlock();

    /* Set new default if needed - select by priority */
    if (netdev_default == NULL && netdev_list != NULL) {
        /* Since the list is already sorted by priority (lower value = higher priority),
         * find the first device that is UP or the first device if none are UP */
        best_candidate = NULL;

        /* First try to find a device that is UP */
        for (curr = netdev_list; curr != NULL; curr = curr->next) {
            if (curr->flags & NETDEV_FLAG_UP) {
                best_candidate = curr;
                break;
            }
        }

        /* If no UP device found, use the first device (highest priority) */
        if (best_candidate == NULL) {
            best_candidate = netdev_list;
        }

        netdev_set_default(best_candidate);
        NETDEV_LOG_I("New default network device: %s (priority %d)",
                     best_candidate->name, best_candidate->priority);
    }

    if (curr == netdev) {
        memset(netdev, 0, sizeof(struct netdev));
        NETDEV_LOG_I("Network device unregistered successfully");
        return 0;
    }

    return -1;
}

/**
 * @brief Get first network device with specified flags
 */
struct netdev *netdev_get_first_by_flags(uint16_t flags)
{
    struct netdev *netdev;

    if (netdev_list == NULL) {
        return NULL;
    }

    netdev_core_lock();

    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next) {
        if ((netdev->flags & flags) != 0) {
            netdev_core_unlock();
            return netdev;
        }
    }

    netdev_core_unlock();
    return NULL;
}

/**
 * @brief Get network device by IP address
 */
struct netdev *netdev_get_by_ipaddr(netdev_ip_addr_t *ip_addr)
{
    struct netdev *netdev;

    if (netdev_list == NULL || !ip_addr) {
        return NULL;
    }

    netdev_core_lock();

    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next) {
        if (ip_addr_cmp(&netdev->ip_addr, ip_addr)) {
            netdev_core_unlock();
            return netdev;
        }
    }

    netdev_core_unlock();
    return NULL;
}

/**
 * @brief Get network device by name
 */
struct netdev *netdev_get_by_name(const char *name)
{
    struct netdev *netdev;
    size_t name_len;

    if (netdev_list == NULL || !name) {
        return NULL;
    }

    name_len = strlen(name);
    if (name_len >= NETDEV_NAME_MAX) {
        name_len = NETDEV_NAME_MAX - 1;
    }

    netdev_core_lock();

    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next) {
        if (strncmp(netdev->name, name, name_len) == 0) {
            netdev_core_unlock();
            return netdev;
        }
    }

    netdev_core_unlock();
    return NULL;
}

/**
 * @brief Set default network device
 */
void netdev_set_default(struct netdev *netdev)
{
    if (netdev && netdev != netdev_default) {
        netdev_default = netdev;

        /* Execute the default network interface device operations */
        if (netdev->ops && netdev->ops->set_default) {
            netdev->ops->set_default(netdev);
        }

        NETDEV_LOG_D("Set default network device: %s", netdev->name);
    }
}

/**
 * @brief Enable network device
 */
int netdev_set_up(struct netdev *netdev)
{
    if (!netdev) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_up) {
        NETDEV_LOG_E("Network device '%s' does not support set_up operation", netdev->name);
        return -1;
    }

    /* Check if already up */
    if (netdev_is_up(netdev)) {
        return 0;
    }

    /* Execute driver set_up operation */
    return netdev->ops->set_up(netdev);
}

/**
 * @brief Disable network device
 */
int netdev_set_down(struct netdev *netdev)
{
    if (!netdev) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_down) {
        NETDEV_LOG_E("Network device '%s' does not support set_down operation", netdev->name);
        return -1;
    }

    /* Check if already down */
    if (!netdev_is_up(netdev)) {
        return 0;
    }

    /* Execute driver set_down operation */
    return netdev->ops->set_down(netdev);
}

/**
 * @brief Enable/disable DHCP
 */
int netdev_dhcp_enabled(struct netdev *netdev, bool is_enabled)
{
    if (!netdev) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_dhcp) {
        NETDEV_LOG_E("Network device '%s' does not support DHCP operations", netdev->name);
        return -1;
    }

    /* Check if already in requested state */
    if (netdev_is_dhcp_enabled(netdev) == is_enabled) {
        return 0;
    }

    /* Execute DHCP control operations */
    return netdev->ops->set_dhcp(netdev, is_enabled);
}

/**
 * @brief Set device IP address
 */
int netdev_set_ipaddr(struct netdev *netdev, const netdev_ip_addr_t *ipaddr)
{
    if (!netdev || !ipaddr) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_addr_info) {
        NETDEV_LOG_E("Network device '%s' does not support IP address setting", netdev->name);
        return -1;
    }

    if (netdev_is_dhcp_enabled(netdev)) {
        NETDEV_LOG_E("Network device '%s' DHCP is enabled, cannot set static IP", netdev->name);
        return -1;
    }

    /* Execute set IP address operation */
    return netdev->ops->set_addr_info(netdev, (netdev_ip_addr_t *)ipaddr, NULL, NULL);
}

/**
 * @brief Set device netmask
 */
int netdev_set_netmask(struct netdev *netdev, const netdev_ip_addr_t *netmask)
{
    if (!netdev || !netmask) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_addr_info) {
        NETDEV_LOG_E("Network device '%s' does not support netmask setting", netdev->name);
        return -1;
    }

    if (netdev_is_dhcp_enabled(netdev)) {
        NETDEV_LOG_E("Network device '%s' DHCP is enabled, cannot set static netmask", netdev->name);
        return -1;
    }

    /* Execute set netmask operation */
    return netdev->ops->set_addr_info(netdev, NULL, (netdev_ip_addr_t *)netmask, NULL);
}

/**
 * @brief Set device gateway
 */
int netdev_set_gw(struct netdev *netdev, const netdev_ip_addr_t *gw)
{
    if (!netdev || !gw) {
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_addr_info) {
        NETDEV_LOG_E("Network device '%s' does not support gateway setting", netdev->name);
        return -1;
    }

    if (netdev_is_dhcp_enabled(netdev)) {
        NETDEV_LOG_E("Network device '%s' DHCP is enabled, cannot set static gateway", netdev->name);
        return -1;
    }

    /* Execute set gateway operation */
    return netdev->ops->set_addr_info(netdev, NULL, NULL, (netdev_ip_addr_t *)gw);
}

/**
 * @brief Set DNS server
 */
int netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, const netdev_ip_addr_t *dns_server)
{
    if (!netdev || !dns_server) {
        return -1;
    }

    if (dns_num >= NETDEV_DNS_SERVERS_NUM) {
        NETDEV_LOG_E("DNS server number %d exceeds maximum %d", dns_num, NETDEV_DNS_SERVERS_NUM);
        return -1;
    }

    if (!netdev->ops || !netdev->ops->set_dns_server) {
        NETDEV_LOG_E("Network device '%s' does not support DNS server setting", netdev->name);
        return -1;
    }

    /* Execute set DNS server operation */
    return netdev->ops->set_dns_server(netdev, dns_num, (netdev_ip_addr_t *)dns_server);
}

/**
 * @brief Set status change callback
 */
void netdev_set_status_callback(struct netdev *netdev, netdev_callback_fn status_callback)
{
    if (netdev && status_callback) {
        netdev->status_callback = status_callback;
    }
}

/**
 * @brief Set address change callback
 */
void netdev_set_addr_callback(struct netdev *netdev, netdev_callback_fn addr_callback)
{
    if (netdev && addr_callback) {
        netdev->addr_callback = addr_callback;
    }
}

/* ===== Low-Level Functions (Driver Use Only) ===== */

/**
 * @brief Set IP address (driver use only)
 */
void netdev_low_level_set_ipaddr(struct netdev *netdev, const netdev_ip_addr_t *ipaddr)
{
    if (!netdev || !ipaddr) {
        return;
    }

    if (!ip_addr_cmp(&netdev->ip_addr, ipaddr)) {
        ip_addr_copy(&netdev->ip_addr, ipaddr);

        /* Execute IP address change callback */
        if (netdev->addr_callback) {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_IP);
        }
    }
}

/**
 * @brief Set netmask (driver use only)
 */
void netdev_low_level_set_netmask(struct netdev *netdev, const netdev_ip_addr_t *netmask)
{
    if (!netdev || !netmask) {
        return;
    }

    if (!ip_addr_cmp(&netdev->netmask, netmask)) {
        ip_addr_copy(&netdev->netmask, netmask);

        /* Execute netmask change callback */
        if (netdev->addr_callback) {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_NETMASK);
        }
    }
}

/**
 * @brief Set gateway (driver use only)
 */
void netdev_low_level_set_gw(struct netdev *netdev, const netdev_ip_addr_t *gw)
{
    if (!netdev || !gw) {
        return;
    }

    if (!ip_addr_cmp(&netdev->gw, gw)) {
        ip_addr_copy(&netdev->gw, gw);

        /* Execute gateway change callback */
        if (netdev->addr_callback) {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_GATEWAY);
        }
    }
}

/**
 * @brief Set DNS server (driver use only)
 */
void netdev_low_level_set_dns_server(struct netdev *netdev, uint8_t dns_num, const netdev_ip_addr_t *dns_server)
{
    uint8_t index;

    if (!netdev || !dns_server) {
        return;
    }

    /* Check if DNS server already exists */
    for (index = 0; index < NETDEV_DNS_SERVERS_NUM; index++) {
        if (ip_addr_cmp(&netdev->dns_servers[index], dns_server)) {
            return;
        }
    }

    if (dns_num < NETDEV_DNS_SERVERS_NUM) {
        ip_addr_copy(&netdev->dns_servers[dns_num], dns_server);

        /* Execute DNS server change callback */
        if (netdev->addr_callback) {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_DNS_SERVER);
        }
    }
}

/**
 * @brief Set device status (driver use only)
 */
void netdev_low_level_set_status(struct netdev *netdev, bool is_up)
{
    if (!netdev) {
        return;
    }

    if (netdev_is_up(netdev) != is_up) {
        if (is_up) {
            netdev->flags |= NETDEV_FLAG_UP;
        } else {
            netdev->flags &= ~NETDEV_FLAG_UP;
        }

        /* Execute status change callback */
        if (netdev->status_callback) {
            netdev->status_callback(netdev, is_up ? NETDEV_CB_STATUS_UP : NETDEV_CB_STATUS_DOWN);
        }
    }
}

/**
 * @brief Set link status (driver use only)
 */
void netdev_low_level_set_link_status(struct netdev *netdev, bool is_up)
{
    if (!netdev) {
        return;
    }

    if (netdev_is_link_up(netdev) != is_up) {
        if (is_up) {
            netdev->flags |= NETDEV_FLAG_LINK_UP;
        } else {
            netdev->flags &= ~NETDEV_FLAG_LINK_UP;
            /* Link down also means internet down */
            netdev->flags &= ~NETDEV_FLAG_INTERNET_UP;
        }

        /* Execute link status change callback */
        if (netdev->status_callback) {
            netdev->status_callback(netdev, is_up ? NETDEV_CB_STATUS_LINK_UP : NETDEV_CB_STATUS_LINK_DOWN);
        }
    }
}

/**
 * @brief Set internet status (driver use only)
 */
void netdev_low_level_set_internet_status(struct netdev *netdev, bool is_up)
{
    if (!netdev) {
        return;
    }

    if (netdev_is_internet_up(netdev) != is_up) {
        if (is_up) {
            netdev->flags |= NETDEV_FLAG_INTERNET_UP;
        } else {
            netdev->flags &= ~NETDEV_FLAG_INTERNET_UP;
        }

        /* Execute internet status change callback */
        if (netdev->status_callback) {
            netdev->status_callback(netdev, is_up ? NETDEV_CB_STATUS_INTERNET_UP : NETDEV_CB_STATUS_INTERNET_DOWN);
        }
    }
}

/**
 * @brief Set DHCP status (driver use only)
 */
void netdev_low_level_set_dhcp_status(struct netdev *netdev, bool is_enable)
{
    if (!netdev) {
        return;
    }

    if (netdev_is_dhcp_enabled(netdev) != is_enable) {
        if (is_enable) {
            netdev->flags |= NETDEV_FLAG_DHCP;
        } else {
            netdev->flags &= ~NETDEV_FLAG_DHCP;
        }

        /* Execute DHCP status change callback */
        if (netdev->status_callback) {
            netdev->status_callback(netdev, is_enable ? NETDEV_CB_STATUS_DHCP_ENABLE : NETDEV_CB_STATUS_DHCP_DISABLE);
        }
    }
}

/* ===== Priority Management ===== */

/**
 * @brief Set network device priority
 */
int netdev_set_priority(const char *name, uint8_t priority)
{
    struct netdev *netdev, *curr, *prev;
    uint8_t new_priority;

    if (!name) {
        NETDEV_LOG_E("Invalid name parameter");
        return -1;
    }

    /* Find device by name */
    netdev = netdev_get_by_name(name);
    if (!netdev) {
        NETDEV_LOG_E("Network device '%s' not found", name);
        return -1;
    }

    /* Use default priority if 0 */
    new_priority = (priority == 0) ? NETDEV_PRIORITY_DEFAULT : priority;

    /* If priority unchanged, nothing to do */
    if (netdev->priority == new_priority) {
        return 0;
    }

    netdev_core_lock();

    /* Remove device from current position in list */
    if (netdev_list == netdev) {
        /* Device is at head */
        netdev_list = netdev->next;
    } else {
        /* Find and remove from middle/tail */
        prev = NULL;
        for (curr = netdev_list; curr != NULL; prev = curr, curr = curr->next) {
            if (curr == netdev) {
                if (prev) {
                    prev->next = curr->next;
                }
                break;
            }
        }

        if (curr == NULL) {
            /* Device not found in list - should not happen */
            netdev_core_unlock();
            NETDEV_LOG_E("Device '%s' not found in list", netdev->name);
            return -1;
        }
    }

    /* Update priority */
    netdev->priority = new_priority;
    netdev->next = NULL;

    /* Re-insert into sorted list */
    if (netdev_list == NULL) {
        /* List is now empty - insert as first */
        netdev_list = netdev;
    } else if (netdev->priority > netdev_list->priority) {
        /* New priority is higher than head - insert at head */
        netdev->next = netdev_list;
        netdev_list = netdev;
    } else {
        /* Find insertion point to maintain sorted order */
        prev = netdev_list;
        curr = netdev_list->next;

        while (curr != NULL && curr->priority >= netdev->priority) {
            prev = curr;
            curr = curr->next;
        }

        /* Insert between prev and curr */
        netdev->next = curr;
        prev->next = netdev;
    }

    netdev_core_unlock();

    NETDEV_LOG_I("Network device '%s' priority changed to %d", netdev->name, new_priority);
    return 0;
}

/**
 * @brief Get network device priority
 */
int netdev_get_priority(const char *name)
{
    struct netdev *netdev;

    if (!name) {
        NETDEV_LOG_E("Invalid name parameter");
        return -1;
    }

    /* Find device by name */
    netdev = netdev_get_by_name(name);
    if (!netdev) {
        NETDEV_LOG_E("Network device '%s' not found", name);
        return -1;
    }

    return (int)netdev->priority;
}

/**
 * @brief Generic network device registration dispatcher
 *
 * This function dispatches to the appropriate network device implementation
 * based on the device name prefix:
 * - "wifi*" or "wlan*" -> lwIP/WiFi
 * - "4g*", "lte*", "ml*" -> AT-based (4G/LTE)
 *
 * @param name Device name (e.g., "wifi0", "4g0", "ml307")
 * @param priority Device priority (lower value = higher priority)
 * @return 0 on success, -1 on failure
 */
int app_netdev_register(const char *name, uint32_t priority)
{
    if (!name) {
        NETDEV_LOG_E("Invalid device name");
        return -1;
    }

    /* Dispatch based on device name prefix */
    if (strncmp(name, "wifi0", 5) == 0 || strncmp(name, "wlan0", 5) == 0) {
        /* WiFi/lwIP device */
        return netdev_lwip_register(name, priority);
    } else if (strncmp(name, "ml307", 5) == 0) {
        /* AT-based 4G/LTE device */
        #if CONFIG_LISA_MODEM
        return netdev_at_register(name, priority);
        #else
        NETDEV_LOG_E("LISA_MODEM is not enabled");
        return -1;
        #endif
    } else {
        NETDEV_LOG_E("Unknown device type for name '%s'", name);
        return -1;
    }
}

/**
 * @brief Generic network device unregistration dispatcher
 *
 * This function dispatches to the appropriate network device implementation
 * based on the device name prefix.
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_unregister_by_name(const char *name)
{
    if (!name) {
        NETDEV_LOG_E("Invalid device name");
        return -1;
    }

    /* Dispatch based on device name prefix */
    if (strncmp(name, "wifi", 4) == 0 || strncmp(name, "wlan", 4) == 0) {
        /* WiFi/lwIP device */
        return netdev_lwip_unregister(name);
    }
    else if (strncmp(name, "4g", 2) == 0 ||
             strncmp(name, "lte", 3) == 0 ||
             strncmp(name, "ml", 2) == 0) {
        /* AT-based 4G/LTE device */
        return netdev_at_unregister(name);
    }
    else {
        NETDEV_LOG_E("Unknown device type for name '%s'", name);
        return -1;
    }
}

/**
 * @brief Check if the highest priority network device is WiFi
 *
 * This function checks if the first device in the priority-sorted list
 * (highest priority) is a WiFi device based on name prefix.
 *
 * @return true if highest priority device is WiFi, false otherwise
 */
bool netdev_is_wifi(void)
{
    struct netdev *netdev;

    netdev_core_lock();

    /* Get the first device in the list (highest priority) */
    netdev = netdev_list;

    netdev_core_unlock();

    if (netdev == NULL) {
        return false;
    }

    /* Check if device name starts with "wifi" or "wlan" */
    if (strncmp(netdev->name, "wifi", 4) == 0 ||
        strncmp(netdev->name, "wlan", 4) == 0) {
        return true;
    }

    return false;
}
