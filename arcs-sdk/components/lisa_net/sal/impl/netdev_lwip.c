/**
 * @file netdev_lwip.c
 * @brief Network device adapter for lwIP (WiFi) interface
 */

#include <string.h>
#include <stdbool.h>

/* Include lwIP headers FIRST to avoid type conflicts */
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/api.h>
#include <lwip/init.h>
#include <lwip/netif.h>

#include "netdev.h"
#include "sal_socket.h"
#include "sal_netdb.h"
#include "sal_low_lvl.h"
#include <lwip/sockets.h>

#if LWIP_VERSION < 0x2000000
#define SELWAIT_T int
#else
#ifndef SELWAIT_T
#define SELWAIT_T u8_t
#endif
#endif

/* Socket operations for lwIP */
static int lwip_netdev_socket(int domain, int type, int protocol)
{
    return lwip_socket(domain, type, protocol);
}

static int lwip_netdev_closesocket(int socket)
{
    return lwip_close(socket);
}

static int lwip_netdev_bind(int socket, const struct sockaddr *name, socklen_t namelen)
{
    return lwip_bind(socket, name, namelen);
}

static int lwip_netdev_listen(int socket, int backlog)
{
    return lwip_listen(socket, backlog);
}

static int lwip_netdev_connect(int socket, const struct sockaddr *name, socklen_t namelen)
{
    return lwip_connect(socket, name, namelen);
}

static int lwip_netdev_accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    return lwip_accept(socket, addr, addrlen);
}

static int lwip_netdev_sendto(int socket, const void *data, size_t size, int flags,
                               const struct sockaddr *to, socklen_t tolen)
{
    return lwip_sendto(socket, data, size, flags, to, tolen);
}

static int lwip_netdev_recvfrom(int socket, void *mem, size_t len, int flags,
                                 struct sockaddr *from, socklen_t *fromlen)
{
    return lwip_recvfrom(socket, mem, len, flags, from, fromlen);
}

static int lwip_netdev_sendmsg(int socket, const struct msghdr *message, int flags)
{
    return lwip_sendmsg(socket, message, flags);
}

static int lwip_netdev_recvmsg(int socket, struct msghdr *message, int flags)
{
    return lwip_recvmsg(socket, message, flags);
}

static int lwip_netdev_getsockopt(int socket, int level, int optname,
                                   void *optval, socklen_t *optlen)
{
    return lwip_getsockopt(socket, level, optname, optval, optlen);
}

static int lwip_netdev_setsockopt(int socket, int level, int optname,
                                   const void *optval, socklen_t optlen)
{
    return lwip_setsockopt(socket, level, optname, optval, optlen);
}

static int lwip_netdev_shutdown(int socket, int how)
{
    return lwip_shutdown(socket, how);
}

static int lwip_netdev_getpeername(int socket, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getpeername(socket, name, namelen);
}

static int lwip_netdev_getsockname(int socket, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getsockname(socket, name, namelen);
}

static int lwip_netdev_ioctlsocket(int socket, long cmd, void *arg)
{
    return lwip_ioctl(socket, cmd, arg);
}

static int lwip_netdev_select(int maxfdp1, fd_set *readset, fd_set *writeset, 
                               fd_set *exceptset, struct timeval *timeout)
{
    return lwip_select(maxfdp1, readset, writeset, exceptset, timeout);
}

/* Socket operations table */
static const struct sal_socket_ops lwip_socket_ops =
{
    .socket      = lwip_netdev_socket,
    .closesocket = lwip_netdev_closesocket,
    .bind        = lwip_netdev_bind,
    .listen      = lwip_netdev_listen,
    .connect     = lwip_netdev_connect,
    .accept      = lwip_netdev_accept,
    .sendto      = lwip_netdev_sendto,
    .sendmsg     = lwip_netdev_sendmsg,
    .recvmsg     = lwip_netdev_recvmsg,
    .recvfrom    = lwip_netdev_recvfrom,
    .getsockopt  = lwip_netdev_getsockopt,
    .setsockopt  = lwip_netdev_setsockopt,
    .shutdown    = lwip_netdev_shutdown,
    .getpeername = lwip_netdev_getpeername,
    .getsockname = lwip_netdev_getsockname,
    .ioctlsocket = lwip_netdev_ioctlsocket,
    .socketpair  = NULL,
    .select      = lwip_netdev_select,
};

/* Network database operations for lwIP */
static const struct sal_netdb_ops lwip_netdb_ops =
{
    .gethostbyname   = lwip_gethostbyname,
    .gethostbyname_r = lwip_gethostbyname_r,
    .getaddrinfo     = lwip_getaddrinfo,
    .freeaddrinfo    = lwip_freeaddrinfo,
};

/* Protocol family definition for lwIP */
static const struct sal_proto_family lwip_proto_family =
{
    .family     = AF_INET,
#if LWIP_VERSION > 0x2000000
    .sec_family = AF_INET6,
#else
    .sec_family = AF_INET,
#endif
    .skt_ops    = &lwip_socket_ops,
    .netdb_ops  = &lwip_netdb_ops,
};

/* Network device instance for lwIP */
static struct netdev lwip_netdev;

/* Network device operations for lwIP */
static int lwip_netdev_set_up(struct netdev *netdev)
{
    /* WiFi bring up is handled by WiFi manager */
    netdev_low_level_set_status(netdev, true);
    return 0;
}

static int lwip_netdev_set_down(struct netdev *netdev)
{
    /* WiFi bring down is handled by WiFi manager */
    netdev_low_level_set_status(netdev, false);
    return 0;
}

static int lwip_netdev_set_addr_info(struct netdev *netdev, netdev_ip_addr_t *ip_addr,
                                      netdev_ip_addr_t *netmask, netdev_ip_addr_t *gw)
{
    /* Address configuration is handled by lwIP/WiFi manager */
    if (ip_addr)
    {
        netdev_low_level_set_ipaddr(netdev, ip_addr);
    }
    if (netmask)
    {
        netdev_low_level_set_netmask(netdev, netmask);
    }
    if (gw)
    {
        netdev_low_level_set_gw(netdev, gw);
    }
    return 0;
}

static int lwip_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, netdev_ip_addr_t *dns_server)
{
    /* DNS configuration is handled by lwIP */
    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);
    return 0;
}

static int lwip_netdev_set_dhcp(struct netdev *netdev, bool is_enabled)
{
    /* DHCP configuration is handled by lwIP */
    netdev_low_level_set_dhcp_status(netdev, is_enabled);
    return 0;
}

static int lwip_netdev_set_default(struct netdev *netdev)
{
    /* Set as default network interface */
    return 0;
}

static const struct netdev_ops lwip_netdev_ops =
{
    .set_up         = lwip_netdev_set_up,
    .set_down       = lwip_netdev_set_down,
    .set_addr_info  = lwip_netdev_set_addr_info,
    .set_dns_server = lwip_netdev_set_dns_server,
    .set_dhcp       = lwip_netdev_set_dhcp,
    .set_default    = lwip_netdev_set_default,
};

/**
 * @brief Register lwIP network device
 *
 * This function should be called after WiFi initialization is complete
 *
 * @param name Device name (e.g., "wifi0", "wlan0")
 * @param priority Device priority (lower value = higher priority)
 * @return 0 on success, -1 on failure
 */
int netdev_lwip_register(const char *name, uint8_t priority)
{
    int result;

    if (!name)
    {
        printf("[NETDEV] Invalid device name\n");
        return -1;
    }

    /* Initialize netdev structure */
    memset(&lwip_netdev, 0, sizeof(struct netdev));

    /* Set operations */
    lwip_netdev.ops = &lwip_netdev_ops;

    /* Set protocol family user data */
    lwip_netdev.sal_user_data = (void *)&lwip_proto_family;

    /* Set default MTU */
    lwip_netdev.mtu = 1500;

    /* Register the network device with specified name and priority */
    result = netdev_register(&lwip_netdev, name, priority, (void *)&lwip_proto_family);

    if (result == 0)
    {
        printf("[NETDEV] lwIP network device '%s' registered successfully (priority %d)\n", name, priority);
    }
    else
    {
        printf("[NETDEV] Failed to register lwIP network device '%s'\n", name);
    }

    return result;
}

/**
 * @brief Unregister lwIP network device
 *
 * This function should be called during WiFi deinitialization
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_lwip_unregister(const char *name)
{
    int result;
    struct netdev *netdev;

    if (!name)
    {
        printf("[NETDEV] Invalid device name\n");
        return -1;
    }

    /* Find the network device by name */
    netdev = netdev_get_by_name(name);
    if (!netdev)
    {
        printf("[NETDEV] lwIP network device '%s' not found\n", name);
        return -1;
    }

    result = netdev_unregister(netdev);

    if (result == 0)
    {
        printf("[NETDEV] lwIP network device '%s' unregistered successfully\n", name);
    }
    else
    {
        printf("[NETDEV] Failed to unregister lwIP network device '%s'\n", name);
    }

    return result;
}

/**
 * @brief Update lwIP netdev status from WiFi events
 *
 * @param link_up Link status
 * @param internet_up Internet connectivity status
 */
void netdev_lwip_update_status(bool link_up, bool internet_up)
{
    netdev_low_level_set_link_status(&lwip_netdev, link_up);
    netdev_low_level_set_internet_status(&lwip_netdev, internet_up);
}

/**
 * @brief Update lwIP netdev IP address information
 *
 * @param ip_addr IP address (network byte order)
 * @param netmask Netmask (network byte order)
 * @param gw Gateway (network byte order)
 */
void netdev_lwip_update_ip_info(uint32_t ip_addr, uint32_t netmask, uint32_t gw)
{
    netdev_ip_addr_t addr;

    addr.addr = ip_addr;
    netdev_low_level_set_ipaddr(&lwip_netdev, &addr);

    addr.addr = netmask;
    netdev_low_level_set_netmask(&lwip_netdev, &addr);

    addr.addr = gw;
    netdev_low_level_set_gw(&lwip_netdev, &addr);
}

/**
 * @brief Update lwIP netdev DNS server
 *
 * @param dns_num DNS server index (0 or 1)
 * @param dns_server DNS server address (network byte order)
 */
void netdev_lwip_update_dns(uint8_t dns_num, uint32_t dns_server)
{
    netdev_ip_addr_t addr;

    if (dns_num < NETDEV_DNS_SERVERS_NUM)
    {
        addr.addr = dns_server;
        netdev_low_level_set_dns_server(&lwip_netdev, dns_num, &addr);
    }
}
