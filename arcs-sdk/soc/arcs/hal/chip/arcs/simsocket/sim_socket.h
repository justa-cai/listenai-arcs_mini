/**
 ****************************************************************************************
 *
 * @file sim_socket.h
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
#ifndef _SIM_SOCKET_H_
#define _SIM_SOCKET_H_

#include "co_list.h"

#define NUM_SOCKETS  9

struct net_buf
{
    uint8_t *data;
    int32_t len;
    ip_addr_t peer_ip;
    in_port_t peer_port;
};

struct sim_sock_msg
{
    struct  ls_list_hdr list;
    struct  net_buf buf;
};

struct sock_conn
{
    ip_addr_t local_ip;
    ip_addr_t remote_ip;
    in_port_t local_port;
    in_port_t remote_port;
    int32_t remote_idx;
};

struct sim_sock
{
    int32_t state;
    int32_t flag;
    struct ls_list msg;
    struct sock_conn conn;
    rtos_semaphore signal;
    void (*recv_event)(struct sim_sock *sock);
    uint16_t select_waiting;
    uint16_t socket;
    volatile uint32_t rcvevent;
};


#define SIM_SOCKET_OFFSET         0

#define SIM_SOCKET_STATE_EMPTY    0
#define SIM_SOCKET_STATE_READY    1

int32_t lwip_forwardmsg(int32_t to, struct msghdr *msghdr);

#endif
