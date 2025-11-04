/**
 *
 * @file sim_socket.c
 *
 * @brief Implementation of simulate socket
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 11, 2023
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <stdbool.h>
#include <co_list.h>
#include <lwip/sockets.h>
#include <lwip/ip_addr.h>
#include "sim_socket.h"
#include "sys_arch.h"
#include "rtos_al.h"
#include "core_compatiable.h"

#define LOCAL_PORT_RANGE_START  0xc000
#define LOCAL_PORT_RANGE_END    0xffff


#define SELECT_SEM_PTR(sem) (&(sem))

struct sim_select_cb {
    /** Pointer to the next waiting task */
    struct sim_select_cb *next;
    /** Pointer to the previous waiting task */
    struct sim_select_cb *prev;
    /** readset passed to select */
    fd_set *readset;
    /** writeset passed to select */
    fd_set *writeset;
    /** unimplemented: exceptset passed to select */
    fd_set *exceptset;
    /** don't signal the same semaphore twice: set to 1 when signalled */
    int32_t sem_signalled;
    /** semaphore to wake up a task waiting for select */
    rtos_semaphore sem;
};

static struct sim_sock sockets[NUM_SOCKETS];
static struct sim_select_cb *select_cb_list;
/** This counter is increased from lwip_select when the list is changed
    and checked in event_callback to see if it has changed. */
static volatile int select_cb_ctr = 0;

static in_port_t sim_port = LOCAL_PORT_RANGE_START;


static void socket_recv_event(struct sim_sock *sock)
{
    int32_t s, last_select_cb_ctr;
    struct sim_select_cb *scb;

    __AMOADD_W((volatile int32_t *)&sock->rcvevent, 1);
    if (!(sock->flag & MSG_DONTWAIT))
        rtos_semaphore_signal(sock->signal, 0);

    if (sock->select_waiting == 0)
        return;

    s = sock->socket;
again:
    for (scb = select_cb_list; scb != NULL; scb = scb->next) {
        /* remember the state of select_cb_list to detect changes */
        last_select_cb_ctr = select_cb_ctr;
        if (scb->sem_signalled == 0) {
            /* semaphore not signalled yet */
            int do_signal = 0;
            /* Test this select call for our socket */
            if (sock->rcvevent > 0) {
                if (scb->readset && FD_ISSET(s, scb->readset)) {
                    do_signal = 1;
                }
            }
            if (1)//sock->sendevent != 0)
            {
                if (!do_signal && scb->writeset && FD_ISSET(s, scb->writeset)) {
                    do_signal = 1;
                }
            }

            if (0)//sock->errevent != 0)
            {
                if (!do_signal && scb->exceptset && FD_ISSET(s, scb->exceptset)) {
                    do_signal = 1;
                }
            }

            if (do_signal) {
                scb->sem_signalled = 1;
                /* Don't call SYS_ARCH_UNPROTECT() before signaling the semaphore, as this might
                   lead to the select thread taking itself off the list, invalidating the semaphore. */
                sys_sem_signal(SELECT_SEM_PTR(scb->sem));
                SIMS_DEBUG("%s: signal to select\n", __func__);
            }
        }
        /* unlock interrupts with each step */
        //SYS_ARCH_UNPROTECT(lev);
        /* this makes sure interrupt protection time is short */
        //SYS_ARCH_PROTECT(lev);
        if (last_select_cb_ctr != select_cb_ctr) {
            /* someone has changed select_cb_list, restart at the beginning */
            goto again;
        }
    }
    //SYS_ARCH_UNPROTECT(lev);
}

static int32_t socket_alloc(void)
{
    int32_t i;

    sim_sock_protect(0);

    for (i = 0; i < NUM_SOCKETS; i++)
    {
        if (sockets[i].state == SIM_SOCKET_STATE_EMPTY && sockets[i].select_waiting == 0)
        {
            memset(&sockets[i], 0, sizeof(struct sim_sock));
            sockets[i].state  = SIM_SOCKET_STATE_READY;
            sockets[i].socket = i + SIM_SOCKET_OFFSET;
            ls_list_init(&sockets[i].msg);
            rtos_semaphore_create(&sockets[i].signal, 1, 0);
            sockets[i].recv_event = socket_recv_event;
            break;
        }
    }

    sim_sock_unprotect(0);

    if (i >= NUM_SOCKETS)
    {
        ASSERT_ERR(0);
    }

    SIMS_DEBUG("%s: port:%d\n", __func__, i + SIM_SOCKET_OFFSET);
    return (i + SIM_SOCKET_OFFSET);
}

static struct sim_sock *socket_get(int s)
{
    struct sim_sock *sock;

    s -= SIM_SOCKET_OFFSET;

    if ((s < 0) || (s >= NUM_SOCKETS))
    {
        SIMS_DEBUG("socket_get(%d): invalid\n", s + SIM_SOCKET_OFFSET);
        return NULL;
    }

    sock = &sockets[s];

    if (!sock->state)
    {
        SIMS_DEBUG("socket_get(%d): not active\n", s + SIM_SOCKET_OFFSET);
        return NULL;
    }

    return sock;
}

static struct sim_sock * socket_tryget(int s)
{
    s -= SIM_SOCKET_OFFSET;
    if ((s < 0) || (s >= NUM_SOCKETS)) {
        return NULL;
    }

    if (!sockets[s].state) {
        return NULL;
    }

    return &sockets[s];
}

static struct sim_sock *socket_lookup(in_port_t port)
{
    int32_t i;
    struct sim_sock *sock = NULL;

    for (i = 0; i < NUM_SOCKETS; i++)
    {
        if ((sockets[i].state & SIM_SOCKET_STATE_READY) && (port == sockets[i].conn.local_port))
        {
            sock = &sockets[i];
            break;
        }
    }

    return sock;
}

static in_port_t socket_new_port(void)
{
    uint32_t n = 0;
    in_port_t port;

again:

    if (sim_port++ == LOCAL_PORT_RANGE_END)
        sim_port = LOCAL_PORT_RANGE_START;
    port = sim_port;

    if (socket_lookup(port))
    {
        if (++n > (LOCAL_PORT_RANGE_END - LOCAL_PORT_RANGE_START))
            return 0;
        goto again;
    }

    return port;
}

static void socket_free_msg(struct sim_sock *sock)
{
    struct sim_sock_msg *msg = NULL;

    while (!ls_list_is_empty(&sock->msg))
    {
        msg = (struct sim_sock_msg*)ls_list_pop_front(&sock->msg);
        if (msg && msg->buf.data)
            rtos_free(msg->buf.data);
        if (msg)
            rtos_free(msg);
    }
}

static struct sim_sock_msg *socket_read_msg(struct sim_sock *sock, int32_t flags)
{
    struct ls_list_hdr *list = NULL;

    GLOBAL_INT_DISABLE();
    if (!(flags & MSG_PEEK))
        list = ls_list_pop_front(&sock->msg);
    else
    	list = ls_list_pick(&sock->msg);
    GLOBAL_INT_RESTORE();

    return (struct sim_sock_msg*)list;
}

static void socket_write_msg(struct sim_sock *sock, struct sim_sock_msg *msg)
{
    GLOBAL_INT_DISABLE();
    ls_list_push_back(&sock->msg, (struct ls_list_hdr*)&msg->list);
    GLOBAL_INT_RESTORE();
}

static int32_t socket_forward(struct sim_sock_msg *msg, in_port_t port)
{
    int32_t ret = -1;
    struct sim_sock *remote_sock;

    sim_sock_protect(0);
    if ((remote_sock = socket_lookup(port)))
    {
        socket_write_msg(remote_sock, msg);
        if (remote_sock->recv_event)
             remote_sock->recv_event(remote_sock);
        ret = 0;
    }
    sim_sock_unprotect(0);

    return ret;
}

static int32_t socket_wait_msg(struct sim_sock *sock, int32_t flags)
{
    int32_t ret = -1;

    if (!((sock->flag | flags) & MSG_DONTWAIT))
    {
        rtos_semaphore_wait(sock->signal, -1);
        ret = 0;
    }

    return ret;
}

static int32_t socket_msghdr_buf_get(const struct msghdr *msghdr, void **buf)
{
    int32_t i, size = 0, offset = 0;

    for (i = 0; i < msghdr->msg_iovlen; i++)
        size += msghdr->msg_iov[i].iov_len;
    *buf = rtos_malloc(size);
    if (!*buf)
    {
        SIMS_ERR("%s: err\n", __func__);
        return 0;
    }

    for (i = 0; i < msghdr->msg_iovlen; i++)
    {
        memcpy((*buf + offset), msghdr->msg_iov[i].iov_base, msghdr->msg_iov[i].iov_len);
        offset += msghdr->msg_iov[i].iov_len;
    }

    return size;
}

int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    struct sim_sock *sock;

    sock = socket_get(s);
    if (!sock)
        return -1;

    if (sock->conn.local_port == 0)
    {
        sim_sock_protect(0);
        sock->conn.local_port = socket_new_port();
        sim_sock_unprotect(0);
        if (sock->conn.local_port == 0)
        {
            SIMS_DEBUG("%s: invalid remote port\n", __func__);
            return -1;
        }
    }

    sock->conn.remote_ip.addr = ntohl(((const struct sockaddr_in*)name)->sin_addr.s_addr);
    sock->conn.remote_port    = ntohs(((const struct sockaddr_in*)name)->sin_port);
    SIMS_DEBUG("%s: lport:%d, rport:%d\n", __func__, sock->conn.local_port, sock->conn.remote_port);

    return 0;
}

int lwip_close(int s)
{
    struct sim_sock *sock = NULL;

    sock = socket_get(s);
    if (!sock)
        return -1;

    sim_sock_protect(0);
    sock->state = SIM_SOCKET_STATE_EMPTY;
    socket_free_msg(sock);
    rtos_semaphore_delete(sock->signal);
    sim_sock_unprotect(0);
    SIMS_DEBUG("%s: port:%d\n", __func__, sock->conn.local_port);
    return 0;
}

int lwip_socket(int domain, int type, int protocol)
{
    (void) domain;
    (void) type;
    (void) protocol;

    if (type != SOCK_DGRAM)
    {
        SIMS_DEBUG("SIMS only supports UDP\n");
        return -1;
    }

    return socket_alloc();
}

int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    in_port_t port;
    struct sim_sock *sock;
    int32_t ret = ERR_OK;

    sock = socket_get(s);
    if (!sock)
        return ERR_ARG;

    port = ntohs(((const struct sockaddr_in*)(const void*)name)->sin_port);

    sim_sock_protect(0);
    if (!port)
    {
        port = socket_new_port();
        if (!port)
        {
            sim_sock_unprotect(0);
            return ERR_ARG;
        }
    }
    else
    {
        if (socket_lookup(port))
        {
            sim_sock_unprotect(0);
            SIMS_DEBUG("%s: local port %d already bound by another socket\n", __func__, port);
            return ERR_USE;
        }
    }
    sim_sock_unprotect(0);
    sock->conn.local_port = port;
    sock->conn.local_ip.addr = ntohl(((const struct sockaddr_in*)(const void*)name)->sin_addr.s_addr);
    SIMS_DEBUG("%s: ip=%x, port=%d\n", __func__, sock->conn.local_ip.addr, port);

    return 0;
}

int32_t lwip_forwardmsg(int32_t to, struct msghdr *msghdr)
{
    int32_t ret = -1, size;
    void *buf = NULL;
    struct sim_sock *sock;
    struct sim_sock_msg *msg;

    sock = socket_get(to);
    if (!sock)
    {
        SIMS_DEBUG("%s: invalid socket\n", __func__);
        return -1;
    }

    msg = (struct sim_sock_msg*)rtos_malloc(sizeof(struct sim_sock_msg));
    if (!msg)
    {
        SIMS_DEBUG("%s: malloc fail\n", __func__);
        return -1;
    }

    memset(msg, 0, sizeof(struct sim_sock_msg));
    size = socket_msghdr_buf_get(msghdr, &buf);
    if (!size)
    {
        rtos_free(msg);
        SIMS_DEBUG("%s: invalid param\n", __func__);
        return -1;
    }

    msg->buf.data = buf;
    msg->buf.len  = size;
    msg->buf.peer_port = (in_port_t)(-1);
    sim_sock_protect(0);
    sock = socket_get(to);
    if (sock)
    {
        socket_write_msg(sock, msg);
        if (sock->recv_event)
        {
            sock->recv_event(sock);
            ret = 0;
        }
    }
    sim_sock_unprotect(0);
    SIMS_DEBUG("%s: rport=%d\n", __func__, sock->conn.local_port);
    return ret;
}

ssize_t lwip_sendto(int s, const void *data, size_t size, int flags,
       const struct sockaddr *to, socklen_t tolen)
{
    uint8_t *buf;
    struct sim_sock *sock;
    struct sim_sock_msg *msg;
    in_port_t port;

    sock = socket_get(s);
    if (!sock)
    {
        SIMS_DEBUG("%s: invalid socket\n", __func__);
        return -1;
    }

    if (to && tolen)
        port = ((struct sockaddr_in*)to)->sin_port;
    if (!to || !tolen || !port)
        port = sock->conn.remote_port;

    if (!port)
    {
        SIMS_DEBUG("%s: invalid remote port\n", __func__);
        return -1;
    }

    msg = (struct sim_sock_msg*)rtos_malloc(sizeof(struct sim_sock_msg));
    if (!msg)
    {
        SIMS_DEBUG("%s: malloc fail\n", __func__);
        return -1;
    }

    buf = rtos_malloc(size);
    if (!buf)
    {
        SIMS_DEBUG("%s: malloc fail\n", __func__);
        rtos_free(msg);
        return -1;
    }
    memcpy(buf, data, size);
    msg->buf.data = buf;
    msg->buf.len  = size;
    msg->buf.peer_ip   = sock->conn.local_ip;
    msg->buf.peer_port = sock->conn.local_port;

    socket_forward(msg, port);

    return 0;
}

ssize_t lwip_send(int s, const void *data, size_t size, int flags)
{
    return lwip_sendto(s, data, size, flags, NULL, 0);
}

ssize_t lwip_sendmsg(int s, const struct msghdr *msghdr, int flags)
{
    int32_t size;
    struct sim_sock *sock;
    struct sim_sock_msg *msg;
    void *buf = NULL;

    sock = socket_get(s);
    if (!sock)
        return -1;

    msg = (struct sim_sock_msg*)rtos_malloc(sizeof(struct sim_sock_msg));
    if (!msg)
        return -1;
    memset(msg, 0, sizeof(struct sim_sock_msg));
    size = socket_msghdr_buf_get(msghdr, &buf);
    if (!size)
    {
        rtos_free(msg);
        return -1;
    }

    msg->buf.data = buf;
    msg->buf.len  = size;
    msg->buf.peer_ip   = sock->conn.local_ip;
    msg->buf.peer_port = sock->conn.local_port;

    socket_forward(msg, sock->conn.remote_port);
    SIMS_DEBUG("%s: lport=%d, rport=%d len=%d\n", __func__, sock->conn.local_port, sock->conn.remote_port, size);

    return 0;
}

ssize_t lwip_recvfrom(int s, void *mem, size_t len, int flags,
              struct sockaddr *from, socklen_t *fromlen)
{
    struct sim_sock *sock;
    struct sim_sock_msg *msg;
    int32_t nretry = 0, nread = 0;

    sock = socket_get(s);
    if (!sock)
        return -1;

    sock->flag = flags;
retry:
    if (sock->rcvevent)
    {
        msg = socket_read_msg(sock, flags);
        if (msg)
        {
            if (!(flags & MSG_PEEK) && (msg->buf.len > len))
                SIMS_ERR("%s: buff size: %d > %d\n", __func__, msg->buf.len, len);

            nread = msg->buf.len > len ? len : msg->buf.len;
            memcpy(mem, msg->buf.data, nread);
            if (from && fromlen)
            {
                ((struct sockaddr_in*)from)->sin_len    = sizeof(struct sockaddr_in);
                ((struct sockaddr_in*)from)->sin_family = AF_INET;
                ((struct sockaddr_in*)from)->sin_port   = msg->buf.peer_port;
                ((struct sockaddr_in*)from)->sin_addr.s_addr   = msg->buf.peer_ip.addr;
                memset(((struct sockaddr_in*)from)->sin_zero, 0, SIN_ZERO_LEN);
                if (*fromlen > ((struct sockaddr_in*)from)->sin_len)
                    *fromlen = ((struct sockaddr_in*)from)->sin_len;
            }
            if (!(flags & MSG_PEEK))
            {
                __AMOADD_W((volatile int32_t *)&sock->rcvevent, -1);
                rtos_free(msg->buf.data);
                rtos_free(msg);
            }
      //      SIMS_DEBUG("%s: rport=%d, len=%d\n", __func__, msg->buf.peer_port, len);
        }
        else
        {
            __AMOADD_W((volatile int32_t *)&sock->rcvevent, -1);
            SIMS_ERR("%s: no data %d\n", __func__, sock->rcvevent);
        }
    }
    else
    {
        if (nretry)
            return -1;
        if (!socket_wait_msg(sock, flags))
        {
            SIMS_DEBUG("%s: retry\n", __func__);
            nretry++;
            goto retry;
        }
    }

 //   SIMS_DEBUG("%s: lport=%d\n", __func__, sock->conn.local_port);

    return nread;
}

ssize_t lwip_recv(int s, void *mem, size_t len, int flags)
{
    return lwip_recvfrom(s, mem, len, flags, NULL, NULL);
}

static int32_t
lwip_selscan(int32_t maxfdp1, fd_set *readset_in, fd_set *writeset_in, fd_set *exceptset_in,
             fd_set *readset_out, fd_set *writeset_out, fd_set *exceptset_out)
{
  int i, nready = 0;
  fd_set lreadset, lwriteset, lexceptset;
  struct sim_sock *sock;

  FD_ZERO(&lreadset);
  FD_ZERO(&lwriteset);
  FD_ZERO(&lexceptset);

  /* Go through each socket in each list to count number of sockets which
     currently match */
  for (i = LWIP_SOCKET_OFFSET; i < maxfdp1; i++) {
    /* if this FD is not in the set, continue */
    if (!(readset_in && FD_ISSET(i, readset_in)) &&
        !(writeset_in && FD_ISSET(i, writeset_in)) &&
        !(exceptset_in && FD_ISSET(i, exceptset_in))) {
      continue;
    }
    /* First get the socket's status (protected)... */
    sim_sock_protect(0);
    sock = socket_tryget(i);
    if (sock != NULL) {
      void* lastdata  = NULL;//sock->lastdata;
      s16_t rcvevent  = sock->rcvevent;
      u16_t sendevent = 1;//sock->sendevent;
      u16_t errevent  = 0;//sock->errevent;
      sim_sock_unprotect(0);

      /* ... then examine it: */
      /* See if netconn of this socket is ready for read */
      if (readset_in && FD_ISSET(i, readset_in) && ((lastdata != NULL) || (rcvevent > 0))) {
        FD_SET(i, &lreadset);
      //  SIMS_DEBUG("sim_selscan: fd=%d ready for reading\n", i);
        nready++;
      }
      /* See if netconn of this socket is ready for write */
      if (writeset_in && FD_ISSET(i, writeset_in) && (sendevent != 0)) {
        FD_SET(i, &lwriteset);
        SIMS_DEBUG("sim_selscan: fd=%d ready for writing\n", i);
        nready++;
      }
      /* See if netconn of this socket had an error */
      if (exceptset_in && FD_ISSET(i, exceptset_in) && (errevent != 0)) {
        FD_SET(i, &lexceptset);
        SIMS_DEBUG("sim_selscan: fd=%d ready for exception\n", i);
        nready++;
      }
    } else {
      sim_sock_unprotect(0);
      /* continue on to next FD in list */
    }
  }
  /* copy local sets to the ones provided as arguments */
  *readset_out = lreadset;
  *writeset_out = lwriteset;
  *exceptset_out = lexceptset;

  SIMS_ASSERT("nready >= 0", nready >= 0);

  return nready;
}

int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
            struct timeval *timeout)
{
    uint32_t waitres = 0;
    int32_t nready;
    fd_set lreadset, lwriteset, lexceptset;
    uint32_t msectimeout;
    struct sim_select_cb select_cb;
    int32_t i;
    int32_t maxfdp2;

    /* Go through each socket in each list to count number of sockets which
       currently match */
    nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);

    /* If we don't have any current events, then suspend if we are supposed to */
    if (!nready) {
        if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
            SIMS_DEBUG("sim_select: no timeout, returning 0\n");
            /* This is OK as the local fdsets are empty and nready is zero,
               or we would have returned earlier. */
            goto return_copy_fdsets;
        }

        /* None ready: add our semaphore to list:
           We don't actually need any dynamic memory. Our entry on the
           list is only valid while we are in this function, so it's ok
           to use local variables. */

        select_cb.next = NULL;
        select_cb.prev = NULL;
        select_cb.readset = readset;
        select_cb.writeset = writeset;
        select_cb.exceptset = exceptset;
        select_cb.sem_signalled = 0;
        if (sys_sem_new(&select_cb.sem, 0) != ERR_OK) {
            /* failed to create semaphore */
            set_errno(ENOMEM);
            return -1;
        }

        /* Protect the select_cb_list */
        sim_sock_protect(0);

        /* Put this select_cb on top of list */
        select_cb.next = select_cb_list;
        if (select_cb_list != NULL) {
            select_cb_list->prev = &select_cb;
        }
        select_cb_list = &select_cb;
        /* Increasing this counter tells event_callback that the list has changed. */
        select_cb_ctr++;

        /* Now we can safely unprotect */
        sim_sock_unprotect(0);

        /* Increase select_waiting for each socket we are interested in */
        maxfdp2 = maxfdp1;
        for (i = SIM_SOCKET_OFFSET; i < maxfdp1; i++) {
            if ((readset && FD_ISSET(i, readset)) ||
                    (writeset && FD_ISSET(i, writeset)) ||
                    (exceptset && FD_ISSET(i, exceptset))) {
                struct sim_sock *sock;
                sim_sock_protect(0);
                sock = socket_tryget(i);
                if (sock != NULL) {
                    sock->select_waiting++;
                    SIMS_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
                } else {
                    /* Not a valid socket */
                    nready = -1;
                    maxfdp2 = i;
                    sim_sock_unprotect(0);
                    break;
                }
                sim_sock_unprotect(0);
            }
        }

        if (nready >= 0) {
            /* Call lwip_selscan again: there could have been events between
               the last scan (without us on the list) and putting us on the list! */
            nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);
            if (!nready) {
                /* Still none ready, just wait to be woken */
                if (timeout == 0) {
                    /* Wait forever */
                    msectimeout = 0;
                } else {
                    msectimeout =  ((timeout->tv_sec * 1000) + ((timeout->tv_usec + 500)/1000));
                    if (msectimeout == 0) {
                        /* Wait 1ms at least (0 means wait forever) */
                        msectimeout = 1;
                    }
                }

                waitres = sys_arch_sem_wait(SELECT_SEM_PTR(select_cb.sem), msectimeout);
            }
        }


        /* Decrease select_waiting for each socket we are interested in */
        for (i = SIM_SOCKET_OFFSET; i < maxfdp2; i++) {
            if ((readset && FD_ISSET(i, readset)) ||
                    (writeset && FD_ISSET(i, writeset)) ||
                    (exceptset && FD_ISSET(i, exceptset))) {
                struct sim_sock *sock;
                sim_sock_protect(0);
                sock = socket_tryget(i);
                if (sock != NULL) {
                    /* for now, handle select_waiting==0... */
                    SIMS_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
                    if (sock->select_waiting > 0) {
                        sock->select_waiting--;
                    }
                } else {
                    /* Not a valid socket */
                    nready = -1;
                }
                sim_sock_unprotect(0);
            }
        }
        /* Take us off the list */
        sim_sock_protect(0);
        if (select_cb.next != NULL) {
            select_cb.next->prev = select_cb.prev;
        }
        if (select_cb_list == &select_cb) {
            SIMS_ASSERT("select_cb.prev == NULL", select_cb.prev == NULL);
            select_cb_list = select_cb.next;
        } else {
            SIMS_ASSERT("select_cb.prev != NULL", select_cb.prev != NULL);
            select_cb.prev->next = select_cb.next;
        }
        /* Increasing this counter tells event_callback that the list has changed. */
        select_cb_ctr++;
        sys_sem_free(&select_cb.sem);
        sim_sock_unprotect(0);

        if (nready < 0) {
            /* This happens when a socket got closed while waiting */
            set_errno(EBADF);
            return -1;
        }

        if (waitres == SYS_ARCH_TIMEOUT) {
            /* Timeout */
         //   SIMS_DEBUG("sim_select: timeout expired\n");
            /* This is OK as the local fdsets are empty and nready is zero,
               or we would have returned earlier. */
            goto return_copy_fdsets;
        }

        /* See what's set */
        nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);
    }

   // SIMS_DEBUG("sim_select: nready=%ld\n", nready);
return_copy_fdsets:
    set_errno(0);
    if (readset) {
        *readset = lreadset;
    }
    if (writeset) {
        *writeset = lwriteset;
    }
    if (exceptset) {
        *exceptset = lexceptset;
    }

    return nready;
}

#if 0
#define SIM_SOCKOPT_CHECK_OPTLEN(optlen, opttype) do { if ((optlen) < sizeof(opttype)) { return -1; }}while(0)
#define SIM_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, opttype) do { \
  SIM_SOCKOPT_CHECK_OPTLEN(optlen, opttype); \
  }while(0)

/** lwip_getsockopt_impl: the actual implementation of getsockopt:
 * same argument as lwip_getsockopt, either called directly or through callback
 */
static uint32_t socket_getsockopt_impl(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    uint32_t err = 0;
    struct sim_sock *sock = socket_tryget(s);

    if (!sock) {
        return -1;
    }

    switch (level) {
        /* Level: SOL_SOCKET */
        case SOL_SOCKET:
            switch (optname) {
                case SO_CONNINFO:
                    SIM_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, void *);
                    *(void **)optval = sock;
                    break;
                default:
                    SIMS_DEBUG("lwip_getsockopt(%d, SOL_SOCKET, UNIMPL: optname=0x%x, ..)\n", s, optname);
                    err = -1;
                    break;
            }  /* switch (optname) */
            break;
        default:
            SIMS_DEBUG("lwip_getsockopt(%d, level=0x%x, UNIMPL: optname=0x%x, ..)\n", s, level, optname);
            err = ENOPROTOOPT;
            break;
    } /* switch (level) */

    return err;
}

int32_t lwip_getsockopt(int32_t s, int32_t level, int32_t optname, void *optval, socklen_t *optlen)
{
    uint32_t err = 0;
    struct sim_sock *sock = socket_get(s);

    if (!sock) {
        return -1;
    }

    if ((NULL == optval) || (NULL == optlen)) {
        set_errno(EFAULT);
        return -1;
    }
    err = socket_getsockopt_impl(s, level, optname, optval, optlen);

    return err;
}
#endif
int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return 0;
}
