/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-05-23     ChenYong     First version
 * 2018-11-12     ChenYong     Add TLS support
 */

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>  /* For snprintf */
#include <lisa_mem.h>
#include <lisa_mutex.h>
#include <lisa_thread.h>
#include <sys/time.h>
/* Note: sys/ioctl.h, unistd.h, fcntl.h not needed for FreeRTOS/lwIP */

/* Include lwIP headers BEFORE sal_socket.h to avoid incomplete type issues */
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

#include <sal_socket.h>
#include <sal_netdb.h>
#ifdef SAL_USING_TLS
#include <sal_tls.h>
#endif
#include <sal_low_lvl.h>
#include <netdev.h>


#ifdef RT_USING_LWP
#include <lwp_sys_socket.h>
#endif

#include <lisa_log.h>

#define TAG "sal_socket"

/* Map RT-Thread logging macros to lisa_log macros */
#define LOG_D(fmt, ...) LISA_LOGD(TAG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) LISA_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) LISA_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) LISA_LOGE(TAG, fmt, ##__VA_ARGS__)

#define VALID_PROTOCOL(protocol) ((protocol) >= 0 && (protocol) <= IPPROTO_RAW)
#define VALID_COMBO(domain, type, protocol)                                                                                         \
    (                                                                                                                               \
        (((domain) == AF_INET || (domain) == AF_INET6) &&                                                                           \
         (((type) == SOCK_STREAM && ((protocol) == 0 || (protocol) == IPPROTO_TCP)) ||                                              \
          ((type) == SOCK_DGRAM && ((protocol) == 0 || (protocol) == IPPROTO_UDP)) ||                                               \
          ((type) == SOCK_RAW && ((protocol) == IPPROTO_RAW))                                                                       \
          )) ||                                                                                                                     \
        ((domain) == AF_UNIX && (type) == SOCK_STREAM && (protocol) == 0) ||                                                        \
        ((domain) == AF_NETLINK && (type) == SOCK_RAW && (protocol) == 0)                                                           \
    )

/* the socket table used to dynamic allocate sockets */
struct sal_socket_table
{
    uint32_t max_socket;
    struct sal_socket **sockets;
};

/* record the netdev and res table*/
struct sal_netdev_res_table
{
    struct addrinfo *res;
    struct netdev *netdev;
};

struct ifconf
{
    int ifc_len;            /* Size of buffer.  */
    union
    {
        char *ifcu_buf;
        struct sal_ifreq *ifcu_req;
    } ifc_ifcu;
};

#ifdef SAL_USING_TLS
/* The global TLS protocol options */
static struct sal_proto_tls *proto_tls;
#endif

/* The global socket table */
static struct sal_socket_table socket_table;
static lisa_mutex_t *sal_core_lock = NULL;
static bool init_ok = false;
static struct sal_netdev_res_table sal_dev_res_tbl[SAL_SOCKETS_NUM];

#define IS_SOCKET_PROTO_TLS(sock) (((sock)->protocol == PROTOCOL_TLS) || \
                                   ((sock)->protocol == PROTOCOL_DTLS))
#define SAL_SOCKOPS_PROTO_TLS_VALID(sock, name) (proto_tls && (proto_tls->ops->name) && IS_SOCKET_PROTO_TLS(sock))

#define SAL_SOCKOPT_PROTO_TLS_EXEC(sock, name, optval, optlen)                      \
    do                                                                              \
    {                                                                               \
        if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, name))                                \
        {                                                                           \
            return proto_tls->ops->name((sock)->user_data_tls, (optval), (optlen)); \
        }                                                                           \
    } while (0)

#define SAL_SOCKET_OBJ_GET(sock, socket) \
    do                                   \
    {                                    \
        (sock) = sal_get_socket(socket); \
        if ((sock) == NULL)           \
        {                                \
            return -1;                   \
        }                                \
    } while (0)

/* Removed netdev_is_up check - allow operations during initialization */
#define SAL_NETDEV_IS_UP(netdev)   \
    do                             \
    {                              \
        /* No-op: removed UP check to allow socket operations during init */ \
    } while (0)

#define SAL_NETDEV_SOCKETOPS_VALID(netdev, pf, ops)              \
    do                                                           \
    {                                                            \
        (pf) = (struct sal_proto_family *)netdev->sal_user_data; \
        if ((pf)->skt_ops->ops == NULL)                       \
        {                                                        \
            return -1;                                           \
        }                                                        \
    } while (0)

/* Removed netdev_is_up check - allow netdb operations during initialization */
#define SAL_NETDEV_NETDBOPS_VALID(netdev, pf, ops)                             \
    ((netdev) &&                                                               \
     ((pf) = (struct sal_proto_family *)(netdev)->sal_user_data) != NULL && \
     (pf)->netdb_ops->ops)

#define SAL_NETDBOPS_VALID(netdev, pf, ops)                                    \
    ((netdev) &&                                                               \
     ((pf) = (struct sal_proto_family *)(netdev)->sal_user_data) != NULL && \
     (pf)->netdb_ops->ops)

/**
 * SAL (Socket Abstraction Layer) initialize.
 *
 * @return result  0: initialize success
 *                -1: initialize failed
 */
int sal_init(void)
{
    int cn;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    if (init_ok)
    {
        LOG_D("Socket Abstraction Layer is already initialized.");
        return 0;
    }

    /* init sal socket table */
    cn = SOCKET_TABLE_STEP_LEN < SAL_SOCKETS_NUM ? SOCKET_TABLE_STEP_LEN : SAL_SOCKETS_NUM;
    socket_table.max_socket = cn;
    socket_table.sockets = lisa_mem_calloc(1, cn * sizeof(struct sal_socket *));
    if (socket_table.sockets == NULL)
    {
        LOG_E("No memory for socket table.\n");
        return -1;
    }

    /*init the dev_res table */
    memset(sal_dev_res_tbl, 0, sizeof(sal_dev_res_tbl));

    /* create sal socket lock */
    sal_core_lock = lisa_mutex_create();
    if (sal_core_lock == NULL)
    {
        LOG_E("No memory for sal mutex.\n");
        lisa_mem_free(socket_table.sockets);
        return -1;
    }

    LOG_I("Socket Abstraction Layer initialize success.");
    init_ok = true;

    return 0;
}

#ifdef SAL_INTERNET_CHECK
/* check SAL network interface device internet status */
static void check_netdev_internet_up_work(void *work_data)
{
#define SAL_INTERNET_VERSION  0x00
#define SAL_INTERNET_BUFF_LEN 12
#define SAL_INTERNET_TIMEOUT  (2)

#define SAL_INTERNET_HOST "link.rt-thread.org"
#define SAL_INTERNET_PORT 8101

#define SAL_INTERNET_MONTH_LEN 4
#define SAL_INTERNET_DATE_LEN  16
    LOG_I("%d, %s---\r\n", __LINE__, __func__);
    unsigned int index;
    int sockfd = -1, result = 0;
    struct sockaddr_in server_addr;
    struct hostent *host;
    struct timeval timeout;
    struct netdev *netdev = (struct netdev *)work_data;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char send_data[SAL_INTERNET_BUFF_LEN], recv_data = 0;

    const char month[][SAL_INTERNET_MONTH_LEN] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    char date[SAL_INTERNET_DATE_LEN];
    unsigned int moth_num = 0;

    struct sal_proto_family *pf = (struct sal_proto_family *)netdev->sal_user_data;
    const struct sal_socket_ops *skt_ops;

    /* get network interface socket operations */
    if (pf == NULL || pf->skt_ops == NULL)
    {
        result = -1;
        goto __exit;
    }

    host = (struct hostent *)pf->netdb_ops->gethostbyname(SAL_INTERNET_HOST);
    if (host == NULL)
    {
        result = -1;
        goto __exit;
    }

    skt_ops = pf->skt_ops;
    if ((sockfd = skt_ops->socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        result = -1;
        goto __exit;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SAL_INTERNET_PORT);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    timeout.tv_sec = SAL_INTERNET_TIMEOUT;
    timeout.tv_usec = 0;

    /* set receive and send timeout */
    skt_ops->setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout));
    skt_ops->setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout));

    /* get build moth value*/
    memset(date, 0x00, SAL_INTERNET_DATE_LEN);
    snprintf(date, SAL_INTERNET_DATE_LEN, "%s", __DATE__);

    for (index = 0; index < sizeof(month) / SAL_INTERNET_MONTH_LEN; index++)
    {
        if (memcmp(date, month[index], SAL_INTERNET_MONTH_LEN - 1) == 0)
        {
            moth_num = index + 1;
            break;
        }
    }

    /* not find build month */
    if (moth_num == 0 || moth_num > sizeof(month) / SAL_INTERNET_MONTH_LEN)
    {
        result = -1;
        goto __exit;
    }

    memset(send_data, 0x00, SAL_INTERNET_BUFF_LEN);
    send_data[0] = SAL_INTERNET_VERSION;
    for (index = 0; index < netdev->hwaddr_len; index++)
    {
        send_data[index + 1] = netdev->hwaddr[index] + moth_num;
    }
    send_data[9] = 0;  /* Version major */
    send_data[10] = 0; /* Version minor */
    send_data[11] = 0; /* Version patch */

    skt_ops->sendto(sockfd, send_data, SAL_INTERNET_BUFF_LEN, 0,
                    (struct sockaddr *)&server_addr, sizeof(struct sockaddr));

    result = skt_ops->recvfrom(sockfd, &recv_data, sizeof(recv_data), 0, (struct sockaddr *)&server_addr, &addr_len);
    if (result < 0)
    {
        goto __exit;
    }

    if (recv_data == false)
    {
        result = -1;
        goto __exit;
    }

__exit:
    if (result > 0)
    {
        LOG_D("Set network interface device(%s) internet status up.", netdev->name);
        netdev_low_level_set_internet_status(netdev, true);
    }
    else
    {
        LOG_D("Set network interface device(%s) internet status down.", netdev->name);
        netdev_low_level_set_internet_status(netdev, false);
    }

    if (sockfd >= 0)
    {
        skt_ops->closesocket(sockfd);
    }
}
#endif /* SAL_INTERNET_CHECK */

/**
 * This function will check SAL network interface device internet status.
 *
 * @param netdev the network interface device to check
 */
int sal_check_netdev_internet_up(struct netdev *netdev)
{
    assert(netdev);
    LOG_I("%d, %s---\r\n", __LINE__, __func__);
#ifdef SAL_INTERNET_CHECK
    /* Note: Workqueue functionality needs to be integrated with project's workqueue system */
    /* The project's workqueue doesn't support delayed submission like RT-Thread's rt_work */
    /* For now, directly call the check function */
    check_netdev_internet_up_work((void *)netdev);
#endif /* SAL_INTERNET_CHECK */
    return 0;
}

/**
 * This function will register TLS protocol to the global TLS protocol.
 *
 * @param pt TLS protocol object
 *
 * @return 0: TLS protocol object register success
 */
#ifdef SAL_USING_TLS
int sal_proto_tls_register(const struct sal_proto_tls *pt)
{
    assert(pt);
    proto_tls = (struct sal_proto_tls *)pt;

    return 0;
}
#endif

/**
 * This function will get sal socket object by sal socket descriptor.
 *
 * @param socket sal socket index
 *
 * @return sal socket object of the current sal socket index
 */
struct sal_socket *sal_get_socket(int socket)
{
    struct sal_socket_table *st = &socket_table;
    int idx;

    /* Convert socket descriptor to index */
    idx = socket - SAL_SOCKET_OFFSET;

    /* Validate socket index BEFORE accessing array */
    if (idx < 0 || idx >= (int)st->max_socket)
    {
        LOG_I("%d, %s, invalid socket: %d (idx: %d)---\r\n", __LINE__, __func__, socket, idx);
        return NULL;
    }

    /* Check if socket entry exists */
    if (st->sockets[idx] == NULL)
    {
        LOG_I("%d, %s, socket entry is NULL: %d---\r\n", __LINE__, __func__, socket);
        return NULL;
    }

    /* Check socket structure valid or not */
    // LOG_I("%d, %s, socket: %d, magic: %04x---\r\n", __LINE__, __func__, socket, st->sockets[idx]->magic);
    return st->sockets[idx];
}

/**
 * This function will lock sal socket.
 *
 * @note please don't invoke it on ISR.
 */
static void sal_lock(void)
{
    if (sal_core_lock)
    {
        lisa_mutex_lock(sal_core_lock, -1);
    }
}

/**
 * This function will lock sal socket.
 *
 * @note please don't invoke it on ISR.
 */
static void sal_unlock(void)
{
    if (sal_core_lock)
    {
        lisa_mutex_unlock(sal_core_lock);
    }
}

/**
 * This function will clean the netdev.
 *
 * @note please don't invoke it on ISR.
 */
int sal_netdev_cleanup(struct netdev *netdev)
{
    uint32_t idx = 0;
    int find_dev;
    LOG_I("%d, %s---\r\n", __LINE__, __func__);
    do
    {
        find_dev = 0;
        sal_lock();
        for (idx = 0; idx < socket_table.max_socket; idx++)
        {
            if (socket_table.sockets[idx] && socket_table.sockets[idx]->netdev == netdev)
            {
                find_dev = 1;
                break;
            }
        }
        sal_unlock();
        if (find_dev)
        {
            lisa_thread_mdelay(100);
        }
    } while (find_dev);

    return 0;
}

/**
 * This function will initialize sal socket object and set socket options
 *
 * @param family    protocol family
 * @param type      socket type
 * @param protocol  transfer Protocol
 * @param res       sal socket object address
 *
 * @return  0 : socket initialize success
 *         -1 : input the wrong family
 *         -2 : input the wrong socket type
 *         -3 : get network interface failed
 *         -4 : invalid protocol or combo
 */
static int socket_init(int family, int type, int protocol, struct sal_socket **res)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    struct netdev *netdev = NULL;
    bool flag = false;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* Existing range checks for family and type */
    if (family < 0 || family > AF_MAX)
    {
        LOG_E("Invalid family: %d (must be 0 ~ %d)", family, AF_MAX);
        return -1;
    }

    if (type < 0 || type > SOCK_MAX)
    {
        LOG_E("Invalid type: %d (must be 0 ~ %d)", type, SOCK_MAX);
        return -1;
    }

    /* Range check for protocol */
    if (!VALID_PROTOCOL(protocol))
    {
        LOG_E("Invalid protocol: %d (must be 0 ~ %d)", protocol, IPPROTO_RAW);
        errno =(EINVAL);
        return -1;
    }

    sock = *res;
    sock->domain = family;
    sock->type = type;
    sock->protocol = protocol;

    /* Combo compatibility check */
    if (!VALID_COMBO(family, type, protocol))
    {
        LOG_E("Invalid combo: domain=%d, type=%d, protocol=%d", family, type, protocol);
        errno =(EINVAL);
        return -1;
    }

    /* Get network interface device by priority (first in list has highest priority) */
    /* Traverse the priority-sorted netdev_list to find matching protocol family */
    /* Note: Do NOT check netdev_is_up() to allow socket creation during initialization */
    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next)
    {
        pf = (struct sal_proto_family *)netdev->sal_user_data;
        if (pf != NULL && pf->skt_ops && (pf->family == family || pf->sec_family == family))
        {
            sock->netdev = netdev;
            flag = true;
            break;
        }
    }

    if (flag == false)
    {
        LOG_E("not find network interface device by protocol family(%d).", family);
        return -1;
    }

    LOG_D("Socket init success: domain=%d, type=%d, protocol=%d, netdev=%s",
          family, type, protocol, sock->netdev ? sock->netdev->name : "default");
    return 0;
}

static int socket_alloc(struct sal_socket_table *st, int f_socket)
{
    int idx;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* find an empty socket entry */
    for (idx = f_socket; idx < (int)st->max_socket; idx++)
    {
        if (st->sockets[idx] == NULL)
        {
            break;
        }
    }

    /* allocate a larger sockte container */
    if (idx == (int)st->max_socket && st->max_socket < SAL_SOCKETS_NUM)
    {
        int cnt, index;
        struct sal_socket **sockets;

        /* increase the number of socket with 4 step length */
        cnt = st->max_socket + SOCKET_TABLE_STEP_LEN;
        cnt = cnt > SAL_SOCKETS_NUM ? SAL_SOCKETS_NUM : cnt;

        sockets = lisa_mem_realloc(st->sockets, cnt * sizeof(struct sal_socket *));
        if (sockets == NULL)
            goto __result; /* return st->max_socket */

        /* clean the new allocated fds */
        for (index = st->max_socket; index < cnt; index++)
        {
            sockets[index] = NULL;
        }

        st->sockets = sockets;
        st->max_socket = cnt;
    }

    /* allocate  'struct sal_socket' */
    if (idx < (int)st->max_socket && st->sockets[idx] == NULL)
    {
        st->sockets[idx] = lisa_mem_calloc(1, sizeof(struct sal_socket));
        if (st->sockets[idx] == NULL)
        {
            idx = st->max_socket;
        }
    }

__result:
    return idx;
}

static void socket_free(struct sal_socket_table *st, int idx)
{
    struct sal_socket *sock;

    /* Safety check: ensure index is valid and slot is not NULL */
    if (idx < 0 || idx >= (int)st->max_socket)
    {
        LOG_W("socket_free: invalid index %d", idx);
        return;
    }

    sock = st->sockets[idx];
    if (sock == NULL)
    {
        LOG_W("socket_free: socket at index %d already freed", idx);
        return;
    }

    /* Clear the slot before freeing to prevent double-free */
    st->sockets[idx] = NULL;

    /* Free the socket structure */
    lisa_mem_free(sock);
}

static int socket_new(void)
{
    struct sal_socket *sock;
    struct sal_socket_table *st = &socket_table;
    int idx;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    sal_lock();

    /* find an empty sal socket entry */
    idx = socket_alloc(st, 0);

    /* can't find an empty sal socket entry */
    if (idx == (int)st->max_socket)
    {
        idx = -(1 + SAL_SOCKET_OFFSET);
        goto __result;
    }

    sock = st->sockets[idx];
    sock->socket = idx + SAL_SOCKET_OFFSET;
    sock->magic = SAL_SOCKET_MAGIC;
    sock->netdev = NULL;
    sock->user_data = NULL;
#ifdef SAL_USING_TLS
    sock->user_data_tls = NULL;
#endif

__result:
    sal_unlock();
    return idx + SAL_SOCKET_OFFSET;
}

static void socket_delete(int socket)
{
    struct sal_socket *sock;
    struct sal_socket_table *st = &socket_table;
    int idx;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    idx = socket - SAL_SOCKET_OFFSET;
    if (idx < 0 || idx >= (int)st->max_socket)
    {
        LOG_W("socket_delete: invalid socket descriptor %d (idx: %d)", socket, idx);
        return;
    }
    sal_lock();
    sock = sal_get_socket(socket);
    if (sock == NULL)
    {
        sal_unlock();
        return;
    }
    sock->magic = 0;
    sock->netdev = NULL;
    socket_free(st, idx);
    sal_unlock();
}

int sal_accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    int new_socket;
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status */
    SAL_NETDEV_IS_UP(sock->netdev);

    /* check the network interface socket operations */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, accept);

    new_socket = pf->skt_ops->accept((int)(size_t)sock->user_data, addr, addrlen);
    if (new_socket != -1)
    {
        int retval;
        int new_sal_socket;
        struct sal_socket *new_sock;

        /* allocate a new socket structure and registered socket options */
        new_sal_socket = socket_new();
        new_sock = sal_get_socket(new_sal_socket);
        if (new_sock == NULL)
        {
            pf->skt_ops->closesocket(new_socket);
            return -1;
        }

        retval = socket_init(sock->domain, sock->type, sock->protocol, &new_sock);
        if (retval < 0)
        {
            pf->skt_ops->closesocket(new_socket);
            memset(new_sock, 0x00, sizeof(struct sal_socket));
            /* socket init failed, delete socket */
            socket_delete(new_sal_socket);
            LOG_E("New socket registered failed, return error %d.", retval);
            return -1;
        }

        /* new socket create by accept should have the same netdev with server*/
        new_sock->netdev = sock->netdev;
        /* socket structure user_data used to store the acquired new socket */
        new_sock->user_data = (void *)(size_t)new_socket;

        return new_sal_socket;
    }

    return -1;
}

static void sal_sockaddr_to_ipaddr(const struct sockaddr *name, netdev_ip_addr_t *local_ipaddr)
{
    const struct sockaddr_in *svr_addr = (const struct sockaddr_in *)name;

#if NETDEV_IPV4 && NETDEV_IPV6
    local_ipaddr->u_addr.ip4.addr = svr_addr->sin_addr.s_addr;
    local_ipaddr->type = IPADDR_TYPE_V4;
#elif NETDEV_IPV4
    local_ipaddr->addr = svr_addr->sin_addr.s_addr;
#elif NETDEV_IPV6
#error "not only support IPV6"
#endif /* NETDEV_IPV4 && NETDEV_IPV6*/
}

int sal_bind(int socket, const struct sockaddr *name, socklen_t namelen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    netdev_ip_addr_t input_ipaddr;

    assert(name);
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

#define IS_INET_ADDR_FAMILY(_af) ((_af) == AF_INET) || ((_af) == AF_INET6)
    if (IS_INET_ADDR_FAMILY(name->sa_family))
    {
        /* bind network interface by ip address */
        sal_sockaddr_to_ipaddr(name, &input_ipaddr);

        /* check input ipaddr is default netdev ipaddr */
        if (!ip_addr_isany_val(input_ipaddr))
        {
            struct sal_proto_family *input_pf = NULL, *local_pf = NULL;
            struct netdev *new_netdev = NULL;

            new_netdev = netdev_get_by_ipaddr(&input_ipaddr);
            if (new_netdev == NULL)
            {
                return -1;
            }

            /* get input and local ip address proto_family */
            SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, local_pf, bind);
            SAL_NETDEV_SOCKETOPS_VALID(new_netdev, input_pf, bind);

            /* check the network interface protocol family type */
            if (input_pf->family != local_pf->family)
            {
                int new_socket = -1;

                /* protocol family is different, close old socket and create new socket by input ip address */
                local_pf->skt_ops->closesocket(socket);

                new_socket = input_pf->skt_ops->socket(input_pf->family, sock->type, sock->protocol);
                if (new_socket < 0)
                {
                    return -1;
                }
                sock->netdev = new_netdev;
                sock->user_data = (void *)(size_t)new_socket;
            }
        }
    }
    /* check and get protocol families by the network interface device */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, bind);
    return pf->skt_ops->bind((int)(size_t)sock->user_data, name, namelen);
}

int sal_shutdown(int socket, int how)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    int error = 0;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* shutdown operation not need to check network interface status */
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, shutdown);

    if (pf->skt_ops->shutdown((int)(size_t)sock->user_data, how) == 0)
    {
#ifdef SAL_USING_TLS
        if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, closesocket))
        {
            if (proto_tls->ops->closesocket(sock->user_data_tls) < 0)
            {
                return -1;
            }
        }
#endif
        error = 0;
    }
    else
    {
        error = -1;
    }


    return error;
}

int sal_getpeername(int socket, struct sockaddr *name, socklen_t *namelen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, getpeername);

    return pf->skt_ops->getpeername((int)(size_t)sock->user_data, name, namelen);
}

int sal_getsockname(int socket, struct sockaddr *name, socklen_t *namelen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, getsockname);

    return pf->skt_ops->getsockname((int)(size_t)sock->user_data, name, namelen);
}

int sal_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, getsockopt);

    return pf->skt_ops->getsockopt((int)(size_t)sock->user_data, level, optname, optval, optlen);
}

int sal_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;

    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, setsockopt);

#ifdef SAL_USING_TLS
    if (level == SOL_TLS)
    {
        switch (optname)
        {
        case TLS_CRET_LIST:
            SAL_SOCKOPT_PROTO_TLS_EXEC(sock, set_cret_list, optval, optlen);
            break;

        case TLS_CIPHERSUITE_LIST:
            SAL_SOCKOPT_PROTO_TLS_EXEC(sock, set_ciphersurite, optval, optlen);
            break;

        case TLS_PEER_VERIFY:
            SAL_SOCKOPT_PROTO_TLS_EXEC(sock, set_peer_verify, optval, optlen);
            break;

        case TLS_DTLS_ROLE:
            SAL_SOCKOPT_PROTO_TLS_EXEC(sock, set_dtls_role, optval, optlen);
            break;

        default:
            return -1;
        }

        return 0;
    }
    else
    {
        return pf->skt_ops->setsockopt((int)sock->user_data, level, optname, optval, optlen);
    }
#else
    return pf->skt_ops->setsockopt((int)(size_t)sock->user_data, level, optname, optval, optlen);
#endif /* SAL_USING_TLS */
}

int sal_connect(int socket, const struct sockaddr *name, socklen_t namelen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    int ret;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, connect);

    ret = pf->skt_ops->connect((int)(size_t)sock->user_data, name, namelen);
#ifdef SAL_USING_TLS
    if (ret >= 0 && SAL_SOCKOPS_PROTO_TLS_VALID(sock, connect))
    {
        if (proto_tls->ops->connect(sock->user_data_tls) < 0)
        {
            return -1;
        }

        return ret;
    }
#endif

    return ret;
}

int sal_listen(int socket, int backlog)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, listen);

    return pf->skt_ops->listen((int)(size_t)sock->user_data, backlog);
}

int sal_sendmsg(int socket, const struct msghdr *message, int flags)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status  */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, sendmsg);

#ifdef SAL_USING_TLS
    if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, send))
    {
        int ret;

        if ((ret = proto_tls->ops->send(sock->user_data_tls, message, flags)) < 0)
        {
            return -1;
        }
        return ret;
    }
    else
    {
        return pf->skt_ops->sendmsg((int)(size_t)sock->user_data, message, flags);
    }
#else
    return pf->skt_ops->sendmsg((int)(size_t)sock->user_data, message, flags);
#endif
}

int sal_recvmsg(int socket, struct msghdr *message, int flags)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status  */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, recvmsg);

#ifdef SAL_USING_TLS
    if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, recv))
    {
        int ret;

        if ((ret = proto_tls->ops->recv(sock->user_data_tls, message, flags)) < 0)
        {
            return -1;
        }
        return ret;
    }
    else
    {
        return pf->skt_ops->recvmsg((int)(size_t)sock->user_data, message, flags);
    }
#else
    return pf->skt_ops->recvmsg((int)(size_t)sock->user_data, message, flags);
#endif
}

int sal_recvfrom(int socket, void *mem, size_t len, int flags,
                 struct sockaddr *from, socklen_t *fromlen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status  */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, recvfrom);

#ifdef SAL_USING_TLS
    if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, recv))
    {
        int ret;

        if ((ret = proto_tls->ops->recv(sock->user_data_tls, mem, len)) < 0)
        {
            return -1;
        }
        return ret;
    }
    else
    {
        return pf->skt_ops->recvfrom((int)(size_t)sock->user_data, mem, len, flags, from, fromlen);
    }
#else
    return pf->skt_ops->recvfrom((int)(size_t)sock->user_data, mem, len, flags, from, fromlen);
#endif
}

int sal_sendto(int socket, const void *dataptr, size_t size, int flags,
               const struct sockaddr *to, socklen_t tolen)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status  */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, sendto);

#ifdef SAL_USING_TLS
    if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, send))
    {
        int ret;

        if ((ret = proto_tls->ops->send(sock->user_data_tls, dataptr, size)) < 0)
        {
            return -1;
        }
        return ret;
    }
    else
    {
        return pf->skt_ops->sendto((int)sock->user_data, dataptr, size, flags, to, tolen);
    }
#else
    return pf->skt_ops->sendto((int)(size_t)sock->user_data, dataptr, size, flags, to, tolen);
#endif
}

int sal_socket(int domain, int type, int protocol)
{
    int retval;
    int socket, proto_socket;
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);

    /* allocate a new socket and registered socket options */
    socket = socket_new();
    if (socket < 0)
    {
        LOG_E("Failed to allocate new socket");
        return -1;
    }

    /* get sal socket object by socket descriptor */
    sock = sal_get_socket(socket);
    if (sock == NULL)
    {
        LOG_E("Failed to get socket object for descriptor %d", socket);
        socket_delete(socket);
        return -1;
    }

    /* Initialize sal socket object */
    retval = socket_init(domain, type, protocol, &sock);
    if (retval < 0)
    {
        LOG_E("SAL socket protocol family input failed, return error %d.", retval);
        socket_delete(socket);
        return retval;
    }
    /* valid the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, socket);

    proto_socket = pf->skt_ops->socket(domain, type, protocol);
    LOG_I("%d, %s, %d---\r\n", __LINE__, __func__, proto_socket);
    if (proto_socket >= 0)
    {
#ifdef SAL_USING_TLS
        if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, socket))
        {
            sock->user_data_tls = proto_tls->ops->socket(socket);
            if (sock->user_data_tls == NULL)
            {
                pf->skt_ops->closesocket(proto_socket);
                socket_delete(socket);
                return -1;
            }
        }
#endif
        sock->user_data = (void *)(size_t)proto_socket;
        return sock->socket;
    }
    socket_delete(socket);
    return -1;
}

int sal_socketpair(int domain, int type, int protocol, int *fds)
{
    int unix_fd[2];
    struct sal_socket *socka;
    struct sal_socket *sockb;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    if (domain == AF_UNIX)
    {
        /* get the socket object by socket descriptor */
        SAL_SOCKET_OBJ_GET(socka, fds[0]);
        SAL_SOCKET_OBJ_GET(sockb, fds[1]);

        /* valid the network interface socket opreation */
        SAL_NETDEV_SOCKETOPS_VALID(socka->netdev, pf, socket);

        unix_fd[0] = (int)(size_t)socka->user_data;
        unix_fd[1] = (int)(size_t)sockb->user_data;

        if (pf->skt_ops->socketpair)
        {
            return pf->skt_ops->socketpair(domain, type, protocol, unix_fd);
        }
    }

    errno =(EINVAL);

    return -1;
}

int sal_closesocket(int socket)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    int error = 0;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);

    /* Check if socket descriptor is valid (not negative) */
    if (socket < SAL_SOCKET_OFFSET) {
        LOG_E("Invalid socket descriptor: %d (must be >= %d)", socket, SAL_SOCKET_OFFSET);
        return -1;
    }

    /* get the socket object by socket descriptor */
    sock = sal_get_socket(socket);
    if (sock == NULL) {
        LOG_W("Socket %d already closed or invalid", socket);
        return -1;
    }

    /* clsoesocket operation not need to vaild network interface status */
    /* valid the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, closesocket);

    if (pf->skt_ops->closesocket((int)(size_t)sock->user_data) == 0)
    {
#ifdef SAL_USING_TLS
        if (SAL_SOCKOPS_PROTO_TLS_VALID(sock, closesocket))
        {
            if (proto_tls->ops->closesocket(sock->user_data_tls) < 0)
            {
                LOG_E("TLS closesocket failed for socket %d", socket);
                error = -1;
            }
        }
#endif
        if (error == 0)
        {
            error = 0;
        }
    }
    else
    {
        LOG_E("Protocol closesocket failed for socket %d", socket);
        error = -1;
    }

    /* delete socket */
    socket_delete(socket);

    return error;
}

#define ARPHRD_ETHER    1      /* Ethernet 10/100Mbps. */
#define ARPHRD_LOOPBACK 772    /* Loopback device.  */
#define IFF_UP          0x1
#define IFF_RUNNING     0x40
#define IFF_NOARP       0x80

int sal_ioctlsocket(int socket, long cmd, void *arg)
{
    struct netdev *netdev = NULL;
    struct netdev *cur_netdev_list = netdev_list;
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    struct sockaddr_in *addr_in = NULL;
    struct sockaddr *addr = NULL;
    netdev_ip_addr_t input_ipaddr;

    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    struct sal_ifreq *ifr = (struct sal_ifreq *)arg;

    if (ifr != NULL)
    {
        switch (cmd)
        {
        case SIOCGIFADDR:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                addr_in = (struct sockaddr_in *)&(ifr->ifr_ifru.ifru_addr);
#if NETDEV_IPV4 && NETDEV_IPV6
                addr_in->sin_addr.s_addr = sock->netdev->ip_addr.u_addr.ip4.addr;
#elif NETDEV_IPV4
                addr_in->sin_addr.s_addr = sock->netdev->ip_addr.addr;
#elif NETDEV_IPV6
#error "not only support IPV6"
#endif /* NETDEV_IPV4 && NETDEV_IPV6*/
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        addr_in = (struct sockaddr_in *)&(ifr->ifr_ifru.ifru_addr);
#if NETDEV_IPV4 && NETDEV_IPV6
                        addr_in->sin_addr.s_addr = netdev->ip_addr.u_addr.ip4.addr;
#elif NETDEV_IPV4
                        addr_in->sin_addr.s_addr = netdev->ip_addr.addr;
#elif NETDEV_IPV6
#error "Do not only support IPV6"
#endif /* NETDEV_IPV4 && NETDEV_IPV6 */

                        return 0;
                    }
                }
                return -1;
            }

        case SIOCSIFADDR:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_addr);
                sal_sockaddr_to_ipaddr(addr, &input_ipaddr);
                netdev_set_ipaddr(sock->netdev, &input_ipaddr);
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_addr);
                        sal_sockaddr_to_ipaddr(addr, &input_ipaddr);
                        netdev_set_ipaddr(netdev, &input_ipaddr);
                        return 0;
                    }
                }
                return -1;
            }

        case SIOCGIFNETMASK:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                addr_in = (struct sockaddr_in *)&(ifr->ifr_ifru.ifru_netmask);
#if NETDEV_IPV4 && NETDEV_IPV6
                addr_in->sin_addr.s_addr = sock->netdev->netmask.u_addr.ip4.addr;
#elif NETDEV_IPV4
                addr_in->sin_addr.s_addr = sock->netdev->netmask.addr;
#elif NETDEV_IPV6
#error "not only support IPV6"
#endif /* NETDEV_IPV4 && NETDEV_IPV6*/
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        addr_in = (struct sockaddr_in *)&(ifr->ifr_ifru.ifru_netmask);
#if NETDEV_IPV4 && NETDEV_IPV6
                        addr_in->sin_addr.s_addr = netdev->netmask.u_addr.ip4.addr;
#elif NETDEV_IPV4
                        addr_in->sin_addr.s_addr = netdev->netmask.addr;
#elif NETDEV_IPV6
#error "not only support IPV6"
#endif /* NETDEV_IPV4 && NETDEV_IPV6*/
                        return 0;
                    }
                }
                return -1;
            }

        case SIOCSIFNETMASK:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_netmask);
                sal_sockaddr_to_ipaddr(addr, &input_ipaddr);
                netdev_set_netmask(sock->netdev, &input_ipaddr);
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_netmask);
                        sal_sockaddr_to_ipaddr(addr, &input_ipaddr);
                        netdev_set_netmask(netdev, &input_ipaddr);
                        return 0;
                    }
                }
                return -1;
            }

        case SIOCGIFHWADDR:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_hwaddr);
#ifdef RT_USING_LWP
                if (!strcmp("lo", sock->netdev->name))
                {
                    struct musl_ifreq *musl_ifreq_tmp = (struct musl_ifreq *)arg;
                    musl_ifreq_tmp->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_LOOPBACK;
                }
                else
                {
                    struct musl_ifreq *musl_ifreq_tmp = (struct musl_ifreq *)arg;
                    musl_ifreq_tmp->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
                }
#endif
                memcpy(addr->sa_data, sock->netdev->hwaddr, sock->netdev->hwaddr_len);
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        addr = (struct sockaddr *)&(ifr->ifr_ifru.ifru_hwaddr);
#ifdef RT_USING_LWP
                        if (!strcmp("lo", netdev->name))
                        {
                            struct musl_ifreq *musl_ifreq_tmp = (struct musl_ifreq *)arg;
                            musl_ifreq_tmp->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_LOOPBACK;
                        }
                        else
                        {
                            struct musl_ifreq *musl_ifreq_tmp = (struct musl_ifreq *)arg;
                            musl_ifreq_tmp->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
                        }
#endif
                        memcpy(addr->sa_data, netdev->hwaddr, netdev->hwaddr_len);
                        return 0;
                    }
                }
                return -1;
            }

        case SIOCGIFMTU:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                ifr->ifr_ifru.ifru_mtu = sock->netdev->mtu;
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        ifr->ifr_ifru.ifru_mtu = netdev->mtu;
                        return 0;
                    }
                }
                return -1;
            }
        case SIOCGIFFLAGS:
            if (!strcmp(ifr->ifr_ifrn.ifrn_name, sock->netdev->name))
            {
                uint16_t flags_tmp = 0;
                if (sock->netdev->flags & NETDEV_FLAG_UP)
                    flags_tmp = flags_tmp | IFF_UP;
                if (!(sock->netdev->flags & NETDEV_FLAG_ETHARP))
                    flags_tmp = flags_tmp | IFF_NOARP;
                flags_tmp = flags_tmp | IFF_RUNNING;
                ifr->ifr_ifru.ifru_flags = flags_tmp;
                return 0;
            }
            else
            {
                if (cur_netdev_list == NULL)
                {
                    LOG_E("ifconfig: network interface device list error.\n");
                    return -1;
                }

                for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
                {
                    if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                    {
                        uint16_t flags_tmp = 0;
                        if (netdev->flags & NETDEV_FLAG_UP)
                            flags_tmp = flags_tmp | IFF_UP;
                        if (!(netdev->flags & NETDEV_FLAG_ETHARP))
                            flags_tmp = flags_tmp | IFF_NOARP;
                        ifr->ifr_ifru.ifru_flags = flags_tmp;
                        return 0;
                    }
                }
                return -1;
            }


        case SIOCSIFFLAGS:
            for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
            {
                if (!strcmp(ifr->ifr_ifrn.ifrn_name, netdev->name))
                {
                    if ((ifr->ifr_ifru.ifru_flags & IFF_UP) == 0)
                    {
                        netdev_set_down(netdev);
                    }
                    else
                    {
                        netdev_set_up(netdev);
                    }
                    return 0;
                }
            }
            return -1;

        case SIOCGIFCONF:
        {
            struct ifconf *ifconf_tmp;
            ifconf_tmp = (struct ifconf *)arg;
            int count_size = 0;

            for (netdev = cur_netdev_list; netdev != NULL; netdev = netdev->next)
            {
                struct sal_ifreq sal_ifreq_temp;
                count_size++;
                strcpy(sal_ifreq_temp.ifr_ifrn.ifrn_name, netdev->name);
                memcpy(ifconf_tmp->ifc_ifcu.ifcu_buf, &sal_ifreq_temp, sizeof(struct sal_ifreq));
                ifconf_tmp->ifc_ifcu.ifcu_buf += sizeof(struct sal_ifreq);
            }
            ifconf_tmp->ifc_len = sizeof(struct sal_ifreq) * count_size;
            ifconf_tmp->ifc_ifcu.ifcu_buf = ifconf_tmp->ifc_ifcu.ifcu_buf - sizeof(struct sal_ifreq) * count_size;
            return 0;
        }
        case SIOCGIFINDEX:
        {
            netdev = netdev_get_by_name(ifr->ifr_ifrn.ifrn_name);
            if (netdev)
            {
                ifr->ifr_ifru.ifru_ivalue = netdev->ifindex;
                return 0;
            }
            return -ENODEV;
        }
        default:
            break;
        }
    }

    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, ioctlsocket);

    return pf->skt_ops->ioctlsocket((int)(size_t)sock->user_data, cmd, arg);
}

#ifdef SAL_USING_POSIX
int sal_poll(struct dfs_file *file, struct rt_pollreq *req)
{
    struct sal_socket *sock;
    struct sal_proto_family *pf;
    int socket = (int)(size_t)file->vnode->data;

    /* get the socket object by socket descriptor */
    SAL_SOCKET_OBJ_GET(sock, socket);

    /* check the network interface is up status  */
    SAL_NETDEV_IS_UP(sock->netdev);
    /* check the network interface socket opreation */
    SAL_NETDEV_SOCKETOPS_VALID(sock->netdev, pf, poll);

    return pf->skt_ops->poll(file, req);
}
#endif /* SAL_USING_POSIX */

int sal_select(int maxfdp1, fd_set *readset, fd_set *writeset, 
               fd_set *exceptset, struct timeval *timeout)
{
    struct netdev *netdev = NULL;
    struct sal_proto_family *pf;

    /* Get network interface by priority - traverse priority-sorted list */
    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next)
    {
        pf = (struct sal_proto_family *)netdev->sal_user_data;
        if (pf && pf->skt_ops && pf->skt_ops->select)
        {
            return pf->skt_ops->select(maxfdp1, readset, writeset, exceptset, timeout);
        }
    }

    /* No network device supports select */
    errno = ENOSYS;
    return -1;
}

struct hostent *sal_gethostbyname(const char *name)
{
    struct netdev *netdev = NULL;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);

    /* Get network interface by priority - traverse priority-sorted list */
    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next)
    {
        if (SAL_NETDEV_NETDBOPS_VALID(netdev, pf, gethostbyname))
        {
            return pf->netdb_ops->gethostbyname(name);
        }
    }

    return NULL;
}

int sal_gethostbyname_r(const char *name, struct hostent *ret, char *buf,
                        size_t buflen, struct hostent **result, int *h_errnop)
{
    struct netdev *netdev = NULL;
    struct sal_proto_family *pf;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);

    /* Get network interface by priority - traverse priority-sorted list */
    /* Note: Do NOT check netdev_is_up() to allow DNS operations during initialization */
    /* Always use highest priority device from netdev_list, not netdev_default */
    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next)
    {
        if (SAL_NETDEV_NETDBOPS_VALID(netdev, pf, gethostbyname_r))
        {
            return pf->netdb_ops->gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
        }
    }

    return -1;
}

int sal_getaddrinfo(const char *nodename,
                    const char *servname,
                    const struct addrinfo *hints,
                    struct addrinfo **res)
{
    struct netdev *netdev = NULL;
    struct sal_proto_family *pf;
    int ret = -1;
    uint32_t i = 0;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);

    /* Get network interface by priority - traverse priority-sorted list */
    for (netdev = netdev_list; netdev != NULL; netdev = netdev->next)
    {
        if (SAL_NETDEV_NETDBOPS_VALID(netdev, pf, getaddrinfo))
        {
            ret = pf->netdb_ops->getaddrinfo(nodename, servname, hints, res);
            break;
        }
    }

    if (ret == 0)
    {
        /*record the netdev and res*/
        for (i = 0; i < SAL_SOCKETS_NUM; i++)
        {
            if (sal_dev_res_tbl[i].res == NULL)
            {
                sal_dev_res_tbl[i].res = *res;
                sal_dev_res_tbl[i].netdev = netdev;
                break;
            }
        }

        assert((i < SAL_SOCKETS_NUM));
    }

    return ret;
}

void sal_freeaddrinfo(struct addrinfo *ai)
{
    struct netdev *netdev = NULL;
    struct sal_proto_family *pf = NULL;
    uint32_t i = 0;
    // LOG_I("%d, %s---\r\n", __LINE__, __func__);
    /*when use the multi netdev, it must free the ai use the getaddrinfo netdev */
    for (i = 0; i < SAL_SOCKETS_NUM; i++)
    {
        if (sal_dev_res_tbl[i].res == ai)
        {
            netdev = sal_dev_res_tbl[i].netdev;
            sal_dev_res_tbl[i].res = NULL;
            sal_dev_res_tbl[i].netdev = NULL;
            break;
        }
    }
    assert((i < SAL_SOCKETS_NUM));

    if (SAL_NETDBOPS_VALID(netdev, pf, freeaddrinfo))
    {
        pf->netdb_ops->freeaddrinfo(ai);
    }
}

int sal_socket_to_fd(int scoket)
{
    struct sal_socket *sock;
    sock = sal_get_socket(scoket);
    if (sock == NULL)
    {
        return -1;
    }
    int lwip_fd = (int)(size_t)sock->user_data;

    return lwip_fd;
}

#if CONFIG_SAL_USING_POSIX
int socket(int domain, int type, int protocol)
{
    return sal_socket(domain, type, protocol);
}

int closesocket(int s)
{
    return sal_closesocket(s);
}

int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    return sal_bind(s, name, namelen);
}

int listen(int s, int backlog)
{
    return sal_listen(s, backlog);
}

int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    return sal_connect(s, name, namelen);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return sal_accept(s, addr, addrlen);
}

int send(int s, const void *data, size_t size, int flags)
{
    return sal_sendto(s, data, size, flags, NULL, 0);
}

int sendmsg(int s, const void *data, int flags)
{
    return sal_sendmsg(s, data, flags);
}

int sendto(int s, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
{
    return sal_sendto(s, data, size, flags, to, tolen);
}

int recv(int s, void *mem, size_t len, int flags)
{
    return sal_recvfrom(s, mem, len, flags, NULL, NULL);
}

int recvmsg(int s, void *mem, int flags)
{
    return sal_recvmsg(s, mem, flags);
}

int recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    return sal_recvfrom(s, mem, len, flags, from, fromlen);
}

int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    return sal_getsockopt(s, level, optname, optval, optlen);
}

int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return sal_setsockopt(s, level, optname, optval, optlen);
}

int shutdown(int s, int how)
{
    return sal_shutdown(s, how);
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    return sal_getpeername(s, name, namelen);
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    return sal_getsockname(s, name, namelen);
}

int ioctlsocket(int s, long cmd, void *arg)
{
    return sal_ioctlsocket(s, cmd, arg);
}

// int ioctl(int s, long cmd, void *arg)
// {
//     return sal_ioctlsocket(s, cmd, arg);
// }

int select(int maxfdp1, fd_set *readset, fd_set *writeset, 
           fd_set *exceptset, struct timeval *timeout)
{
    /* Try SAL select first - it will use the appropriate network device */
    int ret = sal_select(maxfdp1, readset, writeset, exceptset, timeout);
    if (ret >= 0 || errno != ENOSYS) {
        return ret;
    }
}

struct hostent *gethostbyname(const char *name)
{
    return sal_gethostbyname(name);
}

int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen,
                    struct hostent **result, int *h_errnop)
{
    return sal_gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
}

int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res)
{
    return sal_getaddrinfo(nodename, servname, hints, res);
}

void freeaddrinfo(struct addrinfo *ai)
{
    sal_freeaddrinfo(ai);
}

#endif /* CONFIG_SAL_USING_POSIX */
