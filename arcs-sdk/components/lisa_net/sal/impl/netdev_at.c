/**
 * @file netdev_at.c
 * @brief Network device adapter for AT-based (4G) module interface
 */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>

#include "netdev.h"
#include "netdev_at.h"
#include "sal_socket.h"
#include "sal_netdb.h"
#include "sal_low_lvl.h"
#include "lisa_modem_module.h"

/* Define missing EAI error codes if not already defined */
#ifndef EAI_BADFLAGS
#define EAI_BADFLAGS    205
#endif

#ifndef EAI_SOCKTYPE
#define EAI_SOCKTYPE    206
#endif

/* Maximum number of AT sockets */
#ifndef NETDEV_AT_SOCKETS_NUM
#define NETDEV_AT_SOCKETS_NUM  8
#endif

/* AT socket type */
enum netdev_at_socket_type
{
    NETDEV_AT_SOCKET_NONE = 0,
    NETDEV_AT_SOCKET_TCP,
    NETDEV_AT_SOCKET_UDP,
};

/* AT socket structure */
struct netdev_at_socket
{
    int socket_id;                           /* Socket ID from 4G module */
    enum netdev_at_socket_type type;         /* Socket type (TCP/UDP) */
    bool is_ssl;                             /* SSL/TLS enabled */
    bool in_use;                             /* Socket in use flag */
};

/* AT socket table */
static struct netdev_at_socket g_at_sockets[NETDEV_AT_SOCKETS_NUM];

/* Network device instance for AT (4G) */
static struct netdev g_at_netdev;

/* Helper functions */
static int netdev_at_alloc_socket(enum netdev_at_socket_type type)
{
    for (int i = 0; i < NETDEV_AT_SOCKETS_NUM; i++)
    {
        if (!g_at_sockets[i].in_use)
        {
            g_at_sockets[i].type = type;
            g_at_sockets[i].is_ssl = false;
            g_at_sockets[i].in_use = true;
            g_at_sockets[i].socket_id = -1;
            return i;
        }
    }
    return -1;
}

static void netdev_at_free_socket(int sock_idx)
{
    if (sock_idx >= 0 && sock_idx < NETDEV_AT_SOCKETS_NUM)
    {
        memset(&g_at_sockets[sock_idx], 0, sizeof(struct netdev_at_socket));
    }
}

static struct netdev_at_socket *netdev_at_get_socket(int sock_idx)
{
    if (sock_idx >= 0 && sock_idx < NETDEV_AT_SOCKETS_NUM && g_at_sockets[sock_idx].in_use)
    {
        return &g_at_sockets[sock_idx];
    }
    return NULL;
}

/* Socket operations for 4G module */
static int netdev_at_socket(int domain, int type, int protocol)
{
    int sock_idx;
    enum netdev_at_socket_type sock_type;

    /* Validate parameters and determine socket type */
    if (domain != AF_INET && domain != AF_INET6)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (type == SOCK_STREAM)
    {
        /* TCP socket */
        if (protocol != 0 && protocol != IPPROTO_TCP)
        {
            errno = EPROTONOSUPPORT;
            return -1;
        }
        sock_type = NETDEV_AT_SOCKET_TCP;
    }
    else if (type == SOCK_DGRAM)
    {
        /* UDP socket */
        if (protocol != 0 && protocol != IPPROTO_UDP)
        {
            errno = EPROTONOSUPPORT;
            return -1;
        }
        sock_type = NETDEV_AT_SOCKET_UDP;
    }
    else
    {
        errno = EPROTOTYPE;
        return -1;
    }

    /* Allocate socket from pool */
    sock_idx = netdev_at_alloc_socket(sock_type);
    if (sock_idx < 0)
    {
        errno = ENOMEM;
        return -1;
    }

    /* Create the actual 4G socket */
    if (sock_type == NETDEV_AT_SOCKET_TCP)
    {
        g_at_sockets[sock_idx].socket_id = lisa_modem_tcp_socket(false);
    }
    else /* UDP */
    {
        g_at_sockets[sock_idx].socket_id = lisa_modem_udp_socket(domain, type, protocol);
    }

    if (g_at_sockets[sock_idx].socket_id < 0)
    {
        netdev_at_free_socket(sock_idx);
        errno = ENOMEM;
        return -1;
    }

    return sock_idx;
}

static int netdev_at_closesocket(int socket)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);
    int result = 0;

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    /* Close the 4G module socket */
    if (sock->type == NETDEV_AT_SOCKET_TCP)
    {
        result = lisa_modem_tcp_closesocket(sock->socket_id);
        lisa_modem_tcp_deinit(sock->socket_id);
    }
    else /* UDP */
    {
        result = lisa_modem_udp_closesocket(sock->socket_id);
        lisa_modem_udp_deinit(sock->socket_id);
    }

    /* Free the socket */
    netdev_at_free_socket(socket);

    return result;
}

static int netdev_at_bind(int socket, const struct sockaddr *name, socklen_t namelen)
{
    /* 4G module typically doesn't support explicit bind */
    errno = EOPNOTSUPP;
    return 0;
}

static int netdev_at_listen(int socket, int backlog)
{
    /* 4G module typically doesn't support listen/accept */
    errno = EOPNOTSUPP;
    return 0;
}

static int netdev_at_connect(int socket, const struct sockaddr *name, socklen_t namelen)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);
    struct sockaddr_in *addr_in;
    char ip_str[16];
    int port;

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    if (!name || namelen < sizeof(struct sockaddr_in))
    {
        errno = EINVAL;
        return -1;
    }

    /* Only TCP sockets can connect */
    if (sock->type != NETDEV_AT_SOCKET_TCP)
    {
        errno = EOPNOTSUPP;
        return -1;
    }

    addr_in = (struct sockaddr_in *)name;
    inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
    port = ntohs(addr_in->sin_port);

    /* Connect using 4G module TCP interface */
    if (!lisa_modem_tcp_connect(sock->socket_id, ip_str, port, sock->is_ssl))
    {
        errno = ECONNREFUSED;
        return -1;
    }

    return 0;
}

static int netdev_at_accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    /* 4G module typically doesn't support listen/accept */
    errno = EOPNOTSUPP;
    return 0;
}

static int netdev_at_sendto(int socket, const void *data, size_t size, int flags,
                             const struct sockaddr *to, socklen_t tolen)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);
    int result;

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    if (sock->type == NETDEV_AT_SOCKET_TCP)
    {
        /* TCP send */
        result = lisa_modem_tcp_send(sock->socket_id, data, size, 5000);
    }
    else /* UDP */
    {
        /* UDP sendto */
        result = lisa_modem_udp_sendto(sock->socket_id, data, size, flags, to, tolen);
    }

    if (result < 0)
    {
        errno = EIO;
    }

    return result;
}

static int netdev_at_recvfrom(int socket, void *mem, size_t len, int flags,
                               struct sockaddr *from, socklen_t *fromlen)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);
    int result;

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    if (sock->type == NETDEV_AT_SOCKET_TCP)
    {
        /* TCP receive */
        result = lisa_modem_tcp_recv(sock->socket_id, mem, len, 5000);
    }
    else /* UDP */
    {
        /* UDP recvfrom */
        result = lisa_modem_udp_recvform(sock->socket_id, mem, len, flags, from, (int *)fromlen);
    }

    return result;
}

static int netdev_at_sendmsg(int socket, const struct msghdr *message, int flags)
{
    /* AT device does not support sendmsg */
    errno = EOPNOTSUPP;
    return 0;
}

static int netdev_at_recvmsg(int socket, struct msghdr *message, int flags)
{
    /* AT device does not support recvmsg */
    errno = EOPNOTSUPP;
    return 0;
}

static int netdev_at_getsockopt(int socket, int level, int optname,
                                 void *optval, socklen_t *optlen)
{
    return 0;
}

static int netdev_at_setsockopt(int socket, int level, int optname,
                                 const void *optval, socklen_t optlen)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    /* Limited sockopt support via 4G module */
    return lisa_modem_setsockopt(sock->socket_id, level, optname, optval, optlen);
}

static int netdev_at_shutdown(int socket, int how)
{
    
    return 0;
}

static int netdev_at_getpeername(int socket, struct sockaddr *name, socklen_t *namelen)
{

    return 0;
}

static int netdev_at_getsockname(int socket, struct sockaddr *name, socklen_t *namelen)
{
    return 0;
}

static int netdev_at_ioctlsocket(int socket, long cmd, void *arg)
{
    struct netdev_at_socket *sock = netdev_at_get_socket(socket);

    if (!sock)
    {
        errno = EBADF;
        return -1;
    }

    return lisa_modem_ioctlsocket(sock->socket_id, cmd, arg);
}

/* Socket operations table */
static const struct sal_socket_ops netdev_at_socket_ops =
{
    .socket      = netdev_at_socket,
    .closesocket = netdev_at_closesocket,
    .bind        = netdev_at_bind,
    .listen      = netdev_at_listen,
    .connect     = netdev_at_connect,
    .accept      = netdev_at_accept,
    .sendto      = netdev_at_sendto,
    .recvfrom    = netdev_at_recvfrom,
    .sendmsg     = netdev_at_sendmsg,
    .recvmsg     = netdev_at_recvmsg,
    .getsockopt  = netdev_at_getsockopt,
    .setsockopt  = netdev_at_setsockopt,
    .shutdown    = netdev_at_shutdown,
    .getpeername = netdev_at_getpeername,
    .getsockname = netdev_at_getsockname,
    .ioctlsocket = netdev_at_ioctlsocket,
    .socketpair  = NULL,
    .select      = NULL,  /* AT device does not support select */
};

/* Network database operations for 4G module */
static struct hostent *netdev_at_gethostbyname(const char *name)
{
    static struct hostent hostent_data;
    static char *aliases[1] = {NULL};
    static uint32_t addr_list_data[2];
    static char *addr_list[2];
    char ip_str[16];

    if (!lisa_modem_dns_resolve(name, ip_str, sizeof(ip_str)))
    {
        return NULL;
    }

    /* Convert IP string to address */
    if (inet_pton(AF_INET, ip_str, &addr_list_data[0]) != 1)
    {
        return NULL;
    }

    /* Fill in hostent structure */
    hostent_data.h_name = (char *)name;
    hostent_data.h_aliases = aliases;
    hostent_data.h_addrtype = AF_INET;
    hostent_data.h_length = 4;
    addr_list[0] = (char *)&addr_list_data[0];
    addr_list[1] = NULL;
    hostent_data.h_addr_list = addr_list;

    return &hostent_data;
}

static int netdev_at_gethostbyname_r(const char *name, struct hostent *ret, char *buf,
                                      size_t buflen, struct hostent **result, int *h_errnop)
{
    char ip_str[16];
    size_t name_len;
    uint32_t addr;

    if (!lisa_modem_dns_resolve(name, ip_str, sizeof(ip_str)))
    {
        *result = NULL;
        *h_errnop = HOST_NOT_FOUND;
        return -1;
    }

    /* Convert IP string to address */
    if (inet_pton(AF_INET, ip_str, &addr) != 1)
    {
        *result = NULL;
        *h_errnop = NO_DATA;
        return -1;
    }

    /* Check buffer size */
    name_len = strlen(name) + 1;
    if (buflen < name_len + sizeof(char *) * 2 + sizeof(uint32_t) + sizeof(char *) * 2)
    {
        *result = NULL;
        *h_errnop = NO_RECOVERY;
        errno = ERANGE;
        return -1;
    }

    /* Fill in hostent structure using provided buffer */
    ret->h_name = buf;
    strcpy(ret->h_name, name);
    buf += name_len;

    ret->h_aliases = (char **)buf;
    ret->h_aliases[0] = NULL;
    buf += sizeof(char *) * 2;

    ret->h_addrtype = AF_INET;
    ret->h_length = 4;

    memcpy(buf, &addr, sizeof(uint32_t));
    ret->h_addr_list = (char **)(buf + sizeof(uint32_t));
    ret->h_addr_list[0] = buf;
    ret->h_addr_list[1] = NULL;

    *result = ret;
    return 0;
}

static int netdev_at_getaddrinfo(const char *nodename, const char *servname,
                                  const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *ai = NULL;
    struct sockaddr_in *sa = NULL;
    char ip_str[16];
    uint32_t addr;
    int port = 0;
    int socktype = SOCK_STREAM;
    int protocol = IPPROTO_TCP;

    /* Validate input parameters */
    if (!nodename || !res)
    {
        return EAI_BADFLAGS;
    }

    *res = NULL;

    /* Parse hints if provided */
    if (hints)
    {
        /* Validate address family */
        if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
        {
            return EAI_FAMILY;
        }

        /* Get socket type from hints */
        if (hints->ai_socktype != 0)
        {
            socktype = hints->ai_socktype;
            if (socktype == SOCK_DGRAM)
            {
                protocol = IPPROTO_UDP;
            }
            else if (socktype != SOCK_STREAM)
            {
                return EAI_SOCKTYPE;
            }
        }
    }

    /* Parse service/port if provided */
    if (servname)
    {
        char *endptr;
        long port_long = strtol(servname, &endptr, 10);
        if (*endptr == '\0' && port_long > 0 && port_long <= 65535)
        {
            port = (int)port_long;
        }
        else
        {
            return EAI_SERVICE;
        }
    }

    /* Resolve DNS using 4G module */
    if (!lisa_modem_dns_resolve(nodename, ip_str, sizeof(ip_str)))
    {
        return EAI_NONAME;
    }

    /* Convert IP string to binary address */
    if (inet_pton(AF_INET, ip_str, &addr) != 1)
    {
        return EAI_NONAME;
    }

    /* Allocate addrinfo structure */
    ai = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    if (!ai)
    {
        return EAI_MEMORY;
    }
    memset(ai, 0, sizeof(struct addrinfo));

    /* Allocate sockaddr_in structure */
    sa = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (!sa)
    {
        free(ai);
        return EAI_MEMORY;
    }
    memset(sa, 0, sizeof(struct sockaddr_in));

    /* Fill in sockaddr_in */
    sa->sin_family = AF_INET;
    sa->sin_port = htons(port);
    sa->sin_addr.s_addr = addr;

    /* Fill in addrinfo */
    ai->ai_family = AF_INET;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr = (struct sockaddr *)sa;
    ai->ai_canonname = NULL;
    ai->ai_next = NULL;

    /* Optionally allocate canonical name if AI_CANONNAME is set */
    if (hints && (hints->ai_flags & AI_CANONNAME))
    {
        size_t name_len = strlen(nodename) + 1;
        ai->ai_canonname = (char *)malloc(name_len);
        if (ai->ai_canonname)
        {
            strcpy(ai->ai_canonname, nodename);
        }
        /* Continue even if canonname allocation fails */
    }

    *res = ai;
    return 0;
}

static void netdev_at_freeaddrinfo(struct addrinfo *ai)
{
    struct addrinfo *next;

    /* Free the entire chain of addrinfo structures */
    while (ai)
    {
        next = ai->ai_next;

        /* Free the sockaddr structure if allocated */
        if (ai->ai_addr)
        {
            free(ai->ai_addr);
        }

        /* Free the canonical name if allocated */
        if (ai->ai_canonname)
        {
            free(ai->ai_canonname);
        }

        /* Free the addrinfo structure itself */
        free(ai);

        ai = next;
    }
}

static const struct sal_netdb_ops netdev_at_netdb_ops =
{
    .gethostbyname   = netdev_at_gethostbyname,
    .gethostbyname_r = netdev_at_gethostbyname_r,
    .getaddrinfo     = netdev_at_getaddrinfo,
    .freeaddrinfo    = netdev_at_freeaddrinfo,
};

/* Protocol family definition for 4G module */
static const struct sal_proto_family netdev_at_proto_family =
{
    .family     = AF_INET,
    .sec_family = AF_INET,
    .skt_ops    = &netdev_at_socket_ops,
    .netdb_ops  = &netdev_at_netdb_ops,
};

/* Network device operations for 4G */
static int netdev_at_set_up(struct netdev *netdev)
{
    /* 4G bring up is handled by 4G module initialization */
    netdev_low_level_set_status(netdev, true);
    return 0;
}

static int netdev_at_set_down(struct netdev *netdev)
{
    /* 4G bring down is handled by 4G module deinitialization */
    netdev_low_level_set_status(netdev, false);
    return 0;
}

static int netdev_at_set_addr_info(struct netdev *netdev, netdev_ip_addr_t *ip_addr,
                                    netdev_ip_addr_t *netmask, netdev_ip_addr_t *gw)
{
    /* Address configuration is handled by 4G module/network */
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

static int netdev_at_set_dns_server(struct netdev *netdev, uint8_t dns_num, netdev_ip_addr_t *dns_server)
{
    /* DNS configuration is handled by 4G module/network */
    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);
    return 0;
}

static int netdev_at_set_dhcp(struct netdev *netdev, bool is_enabled)
{
    /* DHCP configuration is handled by 4G module/network */
    netdev_low_level_set_dhcp_status(netdev, is_enabled);
    return 0;
}

static int netdev_at_set_default(struct netdev *netdev)
{
    /* Set as default network interface */
    return 0;
}

static const struct netdev_ops netdev_at_ops =
{
    .set_up         = netdev_at_set_up,
    .set_down       = netdev_at_set_down,
    .set_addr_info  = netdev_at_set_addr_info,
    .set_dns_server = netdev_at_set_dns_server,
    .set_dhcp       = netdev_at_set_dhcp,
    .set_default    = netdev_at_set_default,
};

/**
 * @brief Register AT-based network device
 *
 * This function should be called after 4G module initialization is complete
 *
 * @param name Device name (e.g., "4g0", "lte0")
 * @param priority Device priority (lower value = higher priority)
 * @return 0 on success, -1 on failure
 */
int netdev_at_register(const char *name, uint8_t priority)
{
    int result;

    if (!name)
    {
        printf("[NETDEV] Invalid device name\n");
        return -1;
    }

    /* Initialize socket table */
    memset(g_at_sockets, 0, sizeof(g_at_sockets));

    /* Initialize netdev structure */
    memset(&g_at_netdev, 0, sizeof(struct netdev));

    /* Set operations */
    g_at_netdev.ops = &netdev_at_ops;

    /* Set protocol family user data */
    g_at_netdev.sal_user_data = (void *)&netdev_at_proto_family;

    /* Set default MTU for cellular */
    g_at_netdev.mtu = 1500;

    /* Register the network device with specified name and priority */
    result = netdev_register(&g_at_netdev, name, priority, (void *)&netdev_at_proto_family);

    if (result == 0)
    {
        printf("[NETDEV] AT network device '%s' registered successfully (priority %d)\n", name, priority);
    }
    else
    {
        printf("[NETDEV] Failed to register AT network device '%s'\n", name);
    }

    return result;
}

/**
 * @brief Unregister AT-based network device
 *
 * This function should be called during 4G module deinitialization
 *
 * @param name Device name to unregister
 * @return 0 on success, -1 on failure
 */
int netdev_at_unregister(const char *name)
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
        printf("[NETDEV] AT network device '%s' not found\n", name);
        return -1;
    }

    /* Close all open sockets */
    for (int i = 0; i < NETDEV_AT_SOCKETS_NUM; i++)
    {
        if (g_at_sockets[i].in_use)
        {
            netdev_at_closesocket(i);
        }
    }

    result = netdev_unregister(netdev);

    if (result == 0)
    {
        printf("[NETDEV] AT network device '%s' unregistered successfully\n", name);
    }
    else
    {
        printf("[NETDEV] Failed to unregister AT network device '%s'\n", name);
    }

    return result;
}

/**
 * @brief Update 4G netdev status
 *
 * @param link_up Link status
 * @param internet_up Internet connectivity status
 */
void netdev_4g_update_status(bool link_up, bool internet_up)
{
    netdev_low_level_set_link_status(&g_at_netdev, link_up);
    netdev_low_level_set_internet_status(&g_at_netdev, internet_up);
}

/**
 * @brief Update 4G netdev IP address information
 *
 * @param ip_addr IP address (network byte order)
 * @param netmask Netmask (network byte order)
 * @param gw Gateway (network byte order)
 */
void netdev_4g_update_ip_info(uint32_t ip_addr, uint32_t netmask, uint32_t gw)
{
    netdev_ip_addr_t addr;

    addr.addr = ip_addr;
    netdev_low_level_set_ipaddr(&g_at_netdev, &addr);

    addr.addr = netmask;
    netdev_low_level_set_netmask(&g_at_netdev, &addr);

    addr.addr = gw;
    netdev_low_level_set_gw(&g_at_netdev, &addr);
}

/**
 * @brief Update 4G netdev DNS server
 *
 * @param dns_num DNS server index (0 or 1)
 * @param dns_server DNS server address (network byte order)
 */
void netdev_4g_update_dns(uint8_t dns_num, uint32_t dns_server)
{
    netdev_ip_addr_t addr;

    if (dns_num < NETDEV_DNS_SERVERS_NUM)
    {
        addr.addr = dns_server;
        netdev_low_level_set_dns_server(&g_at_netdev, dns_num, &addr);
    }
}
