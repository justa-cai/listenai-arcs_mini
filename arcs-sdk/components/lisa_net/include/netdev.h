/**
 * @file netdev.h
 * @brief Network Device Abstraction Layer
 * @details Multi-network interface management for FreeRTOS
 *
 * Adapted from RT-Thread netdev component for FreeRTOS environment
 */

#ifndef __NETDEV_H__
#define __NETDEV_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum hardware address length (MAC, IMEI, etc.) */
#ifndef NETDEV_HWADDR_MAX_LEN
#define NETDEV_HWADDR_MAX_LEN          8U
#endif

/* Maximum DNS server number supported */
#ifndef NETDEV_DNS_SERVERS_NUM
#define NETDEV_DNS_SERVERS_NUM         2U
#endif

/* Maximum network device name length */
#ifndef NETDEV_NAME_MAX
#define NETDEV_NAME_MAX                16
#endif

/* Network device status flags */
#define NETDEV_FLAG_UP                 0x01U   /**< Device is up */
#define NETDEV_FLAG_LINK_UP            0x04U   /**< Link is active */
#define NETDEV_FLAG_ETHARP             0x08U   /**< ARP enabled */
#define NETDEV_FLAG_INTERNET_UP        0x80U   /**< Internet connected */
#define NETDEV_FLAG_DHCP               0x100U  /**< DHCP enabled */

/* Network device callback event types */
enum netdev_cb_type
{
    NETDEV_CB_ADDR_IP,                 /**< IP address changed */
    NETDEV_CB_ADDR_NETMASK,            /**< Subnet mask changed */
    NETDEV_CB_ADDR_GATEWAY,            /**< Gateway changed */
    NETDEV_CB_ADDR_DNS_SERVER,         /**< DNS server changed */
    NETDEV_CB_STATUS_UP,               /**< Device up */
    NETDEV_CB_STATUS_DOWN,             /**< Device down */
    NETDEV_CB_STATUS_LINK_UP,          /**< Link up */
    NETDEV_CB_STATUS_LINK_DOWN,        /**< Link down */
    NETDEV_CB_STATUS_INTERNET_UP,      /**< Internet up */
    NETDEV_CB_STATUS_INTERNET_DOWN,    /**< Internet down */
    NETDEV_CB_STATUS_DHCP_ENABLE,      /**< DHCP enabled */
    NETDEV_CB_STATUS_DHCP_DISABLE,     /**< DHCP disabled */
};

/* Forward declarations */
struct netdev;
struct netdev_ops;

/* Netdev IP address structure (IPv4 only for simplicity) */
/* Renamed to avoid conflict with lwIP's ip_addr_t */
typedef struct {
    uint32_t addr;                     /**< IPv4 address in network byte order */
} netdev_ip_addr_t;

/* Network device callback function prototype */
typedef void (*netdev_callback_fn)(struct netdev *netdev, enum netdev_cb_type type);

/* Network device operations */
struct netdev_ops
{
    /* Hardware control operations */
    int (*set_up)(struct netdev *netdev);
    int (*set_down)(struct netdev *netdev);

    /* Address configuration operations */
    int (*set_addr_info)(struct netdev *netdev, netdev_ip_addr_t *ip_addr, netdev_ip_addr_t *netmask, netdev_ip_addr_t *gw);
    int (*set_dns_server)(struct netdev *netdev, uint8_t dns_num, netdev_ip_addr_t *dns_server);
    int (*set_dhcp)(struct netdev *netdev, bool is_enabled);

    /* Default network device operations */
    int (*set_default)(struct netdev *netdev);
};

/* Default priority value (lower number = higher priority) */
#ifndef NETDEV_PRIORITY_DEFAULT
#define NETDEV_PRIORITY_DEFAULT            100
#endif

/* Network device object */
struct netdev
{
    struct netdev *next;                                   /**< Next device in list */

    char name[NETDEV_NAME_MAX];                            /**< Device name */
    int ifindex;                                           /**< Interface index */
    netdev_ip_addr_t ip_addr;                                     /**< IP address */
    netdev_ip_addr_t netmask;                                     /**< Subnet mask */
    netdev_ip_addr_t gw;                                          /**< Gateway */
    netdev_ip_addr_t dns_servers[NETDEV_DNS_SERVERS_NUM];         /**< DNS servers */

    uint8_t hwaddr_len;                                    /**< Hardware address length */
    uint8_t hwaddr[NETDEV_HWADDR_MAX_LEN];                 /**< Hardware address */

    uint16_t flags;                                        /**< Status flags */
    uint16_t mtu;                                          /**< Maximum transfer unit */
    uint8_t priority;                                      /**< Device priority (lower = higher priority) */

    const struct netdev_ops *ops;                          /**< Device operations */
    void *sal_user_data;                               /* 保存对应的网卡操作接口，user-specific data for SAL */

    netdev_callback_fn status_callback;                    /**< Status change callback */
    netdev_callback_fn addr_callback;                      /**< Address change callback */

    void *user_data;                                       /**< User-specific data */
};

/* ===== Global Variables ===== */

/* The list of network interface devices */
extern struct netdev *netdev_list;
/* The default network interface device */
extern struct netdev *netdev_default;

/* ===== Device Registration ===== */

/**
 * @brief Register network interface device
 *
 * @param netdev Network device object
 * @param name Device name
 * @param priority Device priority (lower value = higher priority, 0-255)
 * @param user_data User-specific data
 * @return 0 on success, -1 on failure
 *
 * @note Devices are sorted by priority in the list.
 *       If priority is 0, NETDEV_PRIORITY_DEFAULT (100) is used.
 */
int netdev_register(struct netdev *netdev, const char *name, uint8_t priority, void *user_data);

/**
 * @brief Unregister network interface device
 *
 * @param netdev Network device object
 * @return 0 on success, -1 on failure
 */
int netdev_unregister(struct netdev *netdev);

/* ===== Priority Management ===== */

/**
 * @brief Set network device priority
 *
 * @param name Device name
 * @param priority New priority value (lower = higher priority, 0-255)
 * @return 0 on success, -1 on failure
 *
 * @note The device list will be re-sorted after priority change.
 *       If priority is 0, NETDEV_PRIORITY_DEFAULT (100) is used.
 */
int netdev_set_priority(const char *name, uint8_t priority);

/**
 * @brief Get network device priority
 *
 * @param name Device name
 * @return Priority value (0-255), or -1 on failure
 */
int netdev_get_priority(const char *name);

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
int app_netdev_register(const char *name, uint32_t priority);

/**
 * @brief Generic network device unregistration dispatcher
 *
 * This function dispatches to the appropriate network device implementation
 * based on the device name prefix.
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_unregister_by_name(const char *name);

/**
 * @brief Check if the highest priority network device is WiFi
 *
 * This function checks if the first device in the priority-sorted list
 * (highest priority) is a WiFi device based on name prefix ("wifi" or "wlan").
 *
 * @return true if highest priority device is WiFi, false otherwise
 */
bool netdev_is_wifi(void);

/* ===== Device Lookup ===== */

/**
 * @brief Get first network device with specified flags
 *
 * @param flags Status flags to match
 * @return Network device object or NULL
 */
struct netdev *netdev_get_first_by_flags(uint16_t flags);

/**
 * @brief Get network device by IP address
 *
 * @param ip_addr IP address to match
 * @return Network device object or NULL
 */
struct netdev *netdev_get_by_ipaddr(netdev_ip_addr_t *ip_addr);

/**
 * @brief Get network device by name
 *
 * @param name Device name
 * @return Network device object or NULL
 */
struct netdev *netdev_get_by_name(const char *name);

/* ===== Default Device Management ===== */

/**
 * @brief Set default network device
 *
 * @param netdev Network device to set as default
 */
void netdev_set_default(struct netdev *netdev);

/* ===== Device Status Control ===== */

/**
 * @brief Enable network device
 *
 * @param netdev Network device object
 * @return 0 on success, -1 on failure
 */
int netdev_set_up(struct netdev *netdev);

/**
 * @brief Disable network device
 *
 * @param netdev Network device object
 * @return 0 on success, -1 on failure
 */
int netdev_set_down(struct netdev *netdev);

/**
 * @brief Enable/disable DHCP
 *
 * @param netdev Network device object
 * @param is_enabled DHCP enable flag
 * @return 0 on success, -1 on failure
 */
int netdev_dhcp_enabled(struct netdev *netdev, bool is_enabled);

/* ===== Device Status Query ===== */

/**
 * @brief Check if device is up
 */
#define netdev_is_up(netdev) (((netdev)->flags & NETDEV_FLAG_UP) ? 1 : 0)

/**
 * @brief Check if link is up
 */
#define netdev_is_link_up(netdev) (((netdev)->flags & NETDEV_FLAG_LINK_UP) ? 1 : 0)

/**
 * @brief Check if internet is up
 */
#define netdev_is_internet_up(netdev) (((netdev)->flags & NETDEV_FLAG_INTERNET_UP) ? 1 : 0)

/**
 * @brief Check if DHCP is enabled
 */
#define netdev_is_dhcp_enabled(netdev) (((netdev)->flags & NETDEV_FLAG_DHCP) ? 1 : 0)

/* ===== Address Configuration ===== */

/**
 * @brief Set device IP address
 *
 * @param netdev Network device object
 * @param ipaddr IP address
 * @return 0 on success, -1 on failure
 */
int netdev_set_ipaddr(struct netdev *netdev, const netdev_ip_addr_t *ipaddr);

/**
 * @brief Set device netmask
 *
 * @param netdev Network device object
 * @param netmask Subnet mask
 * @return 0 on success, -1 on failure
 */
int netdev_set_netmask(struct netdev *netdev, const netdev_ip_addr_t *netmask);

/**
 * @brief Set device gateway
 *
 * @param netdev Network device object
 * @param gw Gateway address
 * @return 0 on success, -1 on failure
 */
int netdev_set_gw(struct netdev *netdev, const netdev_ip_addr_t *gw);

/**
 * @brief Set DNS server
 *
 * @param netdev Network device object
 * @param dns_num DNS server number (0 or 1)
 * @param dns_server DNS server address
 * @return 0 on success, -1 on failure
 */
int netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, const netdev_ip_addr_t *dns_server);

/* ===== Callback Registration ===== */

/**
 * @brief Set status change callback
 *
 * @param netdev Network device object
 * @param status_callback Callback function
 */
void netdev_set_status_callback(struct netdev *netdev, netdev_callback_fn status_callback);

/**
 * @brief Set address change callback
 *
 * @param netdev Network device object
 * @param addr_callback Callback function
 */
void netdev_set_addr_callback(struct netdev *netdev, netdev_callback_fn addr_callback);

/* ===== Low-Level Functions (Driver Use Only) ===== */

/**
 * @brief Set IP address (driver use only)
 *
 * @param netdev Network device object
 * @param ipaddr IP address
 */
void netdev_low_level_set_ipaddr(struct netdev *netdev, const netdev_ip_addr_t *ipaddr);

/**
 * @brief Set netmask (driver use only)
 *
 * @param netdev Network device object
 * @param netmask Subnet mask
 */
void netdev_low_level_set_netmask(struct netdev *netdev, const netdev_ip_addr_t *netmask);

/**
 * @brief Set gateway (driver use only)
 *
 * @param netdev Network device object
 * @param gw Gateway address
 */
void netdev_low_level_set_gw(struct netdev *netdev, const netdev_ip_addr_t *gw);

/**
 * @brief Set DNS server (driver use only)
 *
 * @param netdev Network device object
 * @param dns_num DNS server number
 * @param dns_server DNS server address
 */
void netdev_low_level_set_dns_server(struct netdev *netdev, uint8_t dns_num, const netdev_ip_addr_t *dns_server);

/**
 * @brief Set device status (driver use only)
 *
 * @param netdev Network device object
 * @param is_up Status flag
 */
void netdev_low_level_set_status(struct netdev *netdev, bool is_up);

/**
 * @brief Set link status (driver use only)
 *
 * @param netdev Network device object
 * @param is_up Link status flag
 */
void netdev_low_level_set_link_status(struct netdev *netdev, bool is_up);

/**
 * @brief Set internet status (driver use only)
 *
 * @param netdev Network device object
 * @param is_up Internet status flag
 */
void netdev_low_level_set_internet_status(struct netdev *netdev, bool is_up);

/**
 * @brief Set DHCP status (driver use only)
 *
 * @param netdev Network device object
 * @param is_enable DHCP enable flag
 */
void netdev_low_level_set_dhcp_status(struct netdev *netdev, bool is_enable);

/* ===== Utility Functions ===== */

/**
 * @brief Initialize netdev subsystem
 *
 * @return 0 on success, -1 on failure
 */
int netdev_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __NETDEV_H__ */
