// Copyright 2024-2025 ListenAI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _ATCMD_LWIP_H_
#define _ATCMD_LWIP_H_

#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sockets.h"

// 定义宏：判断端口号是否合法
#define IS_VALID_PORT(port)     ((port) >= 0 && (port) <= 65535)

#define NUM_NS                  (10) 
#define MAX_CON_ID			    (10)

#define INVALID_CON_ID		    (-1)
#define INVALID_SOCKET_ID	    (-1)

typedef enum {
    ERR_NONE = 0,
    ERR_ATCMD_PARAMETER_IS_NULL,         
    ERR_ATCMD_PARAMETER_COUNT_EXCEED,
    ERR_ATCMD_PARAMETER_COUNT_MISSING,
    ERR_ATCMD_PARAMETER_INVALID,
    ERR_ATCMD_PING_HOST_FOUND_FAILED,
    ERR_ATCMD_PING_FAILED,
    ERR_ATLWIP_RECV_DATA_UDPSER_FAILED,
    ERR_ATLWIP_RECV_DATA_UDPCLI_FAILED,
    ERR_ATLWIP_RECV_DATA_TCP_CLOSED,
    ERR_ATLWIP_RECV_DATA_TCP_FAILED,
    ERR_ATLWIP_SERVER_START_NODE_IS_NULL,
    ERR_ATLWIP_SERVER_START_NODE_MODE_INVALID,
    ERR_ATLWIP_SERVER_START_SOCKET_CREATE_FAILED,
    ERR_ATLWIP_SERVER_START_SOCKET_OPTION_FAILED,
    ERR_ATLWIP_SERVER_START_SOCKET_BIND_FAILED,
    ERR_ATLWIP_SERVER_START_SOCKET_LISTEN_FAILED,
    ERR_ATLWIP_SERVER_START_TCP_NODE_ADD_FAILED,
    ERR_ATLWIP_SERVER_START_SEED_CREATE_FAILED,
    ERR_ATLWIP_SERVER_START_SEED_ADD_FAILED,
    ERR_ATLWIP_SERVER_START_BROADCAST_OPTION_FAILED,
    ERR_ATLWIP_SERVER_START_UDP_NODE_ADD_FAILED,
    ERR_ATCMD_SERVER_CREATE_MODE_INVALID,
    ERR_ATCMD_SERVER_CREATE_PORT_INVALID,
    ERR_ATCMD_SERVER_CREATE_TASK_FAILED,
    ERR_ATLWIP_CLIENT_START_IP_CONVERSION_FAILED,
    ERR_ATLWIP_CLIENT_START_SOCKET_CREATE_FAILED,
    ERR_ATLWIP_CLIENT_START_CONNECT_FAILED,
    ERR_ATLWIP_CLIENT_START_MULTICAST_SETUP_FAILED,
    ERR_ATLWIP_CLIENT_START_NODE_ADD_FAILED,
    ERR_ATCMD_CLIENT_CREATE_MODE_INVALID,
    ERR_ATCMD_CLIENT_CREATE_HOST_FOUND_FAILED,
    ERR_ATCMD_CLIENT_CREATE_ADDR_INVALID,
    ERR_ATCMD_CLIENT_CREATE_REMOTE_PORT_INVALID,
    ERR_ATCMD_CLIENT_CREATE_LOCAL_PORT_INVALID,
    ERR_ATCMD_CLIENT_CREATE_NODE_FAILED,
    ERR_ATCMD_CLIENT_CREATE_TASK_FAILED,
    ERR_ATLWIP_SEND_DATA_UDPSER_FAILED,
    ERR_ATLWIP_SEND_DATA_UDPCLI_FAILED,
    ERR_ATLWIP_SEND_DATA_TCP_ROLE_INVALID,
    ERR_ATLWIP_SEND_DATA_TCP_INVALID,
    ERR_ATCMD_SEND_DATA_ARG_INVALID,
    ERR_ATCMD_SEND_DATA_PTR_INVALID,
    ERR_ATCMD_SEND_DATA_DATA_SIZE_INVALID,
    ERR_ATCMD_SEND_DATA_CON_ID_INVALID,
    ERR_ATCMD_SEND_DATA_UDPSER_IP_ADDR_INVALID,
    ERR_ATCMD_SEND_DATA_PARAM_INVALID,
    ERR_ATCMD_CLOSE_CON_ID_INVALID,
    ERR_ATCMD_AUTO_RECV_TASK_FAILED,
    ERR_ATCMD_RECV_DATA_AUTO_RECV_MODE_INVALID,
    ERR_ATCMD_RECV_DATA_CON_ID_INVALID,
    ERR_ATCMD_RECV_DATA_PACKET_SIZE_INVALID,
    ERR_ATCMD_RECV_DATA_NODE_INVALID,
    ERR_ATCMD_RECV_DATA_ROLE_INVALID,
    ERR_ATLWIP_MAX,
} at_lwip_errcode_e;

const char *at_lwip_error_messages[] = {
    [ERR_ATCMD_PARAMETER_IS_NULL]                       = "parameter is null",
    [ERR_ATCMD_PARAMETER_COUNT_EXCEED]                  = "parameter count exceed",
    [ERR_ATCMD_PARAMETER_COUNT_MISSING]                 = "parameter count missing",
    [ERR_ATCMD_PARAMETER_INVALID]                       = "parameter invalid",
    [ERR_ATCMD_PING_HOST_FOUND_FAILED]                  = "ping host name not found",
    [ERR_ATCMD_PING_FAILED]                             = "ping timeout",
    [ERR_ATLWIP_RECV_DATA_UDPSER_FAILED]                = "udp server receive data failed",
    [ERR_ATLWIP_RECV_DATA_UDPCLI_FAILED]                = "udp client receive data failed",
    [ERR_ATLWIP_RECV_DATA_TCP_CLOSED]                   = "tcp receive socket closed",
    [ERR_ATLWIP_RECV_DATA_TCP_FAILED]                   = "tcp receive data failed",
    [ERR_ATLWIP_SERVER_START_NODE_IS_NULL]              = "server_start node is null",
    [ERR_ATLWIP_SERVER_START_NODE_MODE_INVALID]         = "server_start node mode invalid",
    [ERR_ATLWIP_SERVER_START_SOCKET_CREATE_FAILED]      = "server_start socket create failed",
    [ERR_ATLWIP_SERVER_START_SOCKET_OPTION_FAILED]      = "server_start socket option failed",
    [ERR_ATLWIP_SERVER_START_SOCKET_BIND_FAILED]        = "server_start socket bind failed",
    [ERR_ATLWIP_SERVER_START_SOCKET_LISTEN_FAILED]      = "server_start socket listen failed",
    [ERR_ATLWIP_SERVER_START_TCP_NODE_ADD_FAILED]       = "server_start node add to mainlist failed",
    [ERR_ATLWIP_SERVER_START_SEED_CREATE_FAILED]        = "server_start seed create failed",
    [ERR_ATLWIP_SERVER_START_SEED_ADD_FAILED]           = "server_start seed add to server node failed",
    [ERR_ATLWIP_SERVER_START_BROADCAST_OPTION_FAILED]   = "server_start broadcast option failed",
    [ERR_ATLWIP_SERVER_START_UDP_NODE_ADD_FAILED]       = "server_start udp node add to mainlist failed",
    [ERR_ATCMD_SERVER_CREATE_MODE_INVALID]              = "server_create mode invalid",
    [ERR_ATCMD_SERVER_CREATE_PORT_INVALID]              = "server_create port invalid",
    [ERR_ATCMD_SERVER_CREATE_TASK_FAILED]               = "server_create task failed",
    [ERR_ATLWIP_CLIENT_START_IP_CONVERSION_FAILED]      = "client_start ip conversion failed",
    [ERR_ATLWIP_CLIENT_START_SOCKET_CREATE_FAILED]      = "client_start socket create failed",
    [ERR_ATLWIP_CLIENT_START_CONNECT_FAILED]            = "client_start connect failed",
    [ERR_ATLWIP_CLIENT_START_MULTICAST_SETUP_FAILED]    = "client_start multicast setup failed",
    [ERR_ATLWIP_CLIENT_START_NODE_ADD_FAILED]           = "client_start node add to mainlist failed",
    [ERR_ATCMD_CLIENT_CREATE_MODE_INVALID]              = "client_create mode invalid",
    [ERR_ATCMD_CLIENT_CREATE_HOST_FOUND_FAILED]         = "client_create host name not found",
    [ERR_ATCMD_CLIENT_CREATE_ADDR_INVALID]              = "client_create ip addr invalid",
    [ERR_ATCMD_CLIENT_CREATE_REMOTE_PORT_INVALID]       = "client_create remote port invalid",
    [ERR_ATCMD_CLIENT_CREATE_LOCAL_PORT_INVALID]        = "client_create local port invalid",
    [ERR_ATCMD_CLIENT_CREATE_NODE_FAILED]               = "client_create node failed",
    [ERR_ATCMD_CLIENT_CREATE_TASK_FAILED]               = "client_create task failed",
    [ERR_ATLWIP_SEND_DATA_UDPSER_FAILED]                = "lwip send_data udp server failed",
    [ERR_ATLWIP_SEND_DATA_UDPCLI_FAILED]                = "lwip send_data udp client failed",
    [ERR_ATLWIP_SEND_DATA_TCP_ROLE_INVALID]             = "lwip send_data tcp role invalid",
    [ERR_ATLWIP_SEND_DATA_TCP_INVALID]                  = "lwip send_data tcp invalid",
    [ERR_ATCMD_SEND_DATA_ARG_INVALID]                   = "atcmd send_data arg invalid ",
    [ERR_ATCMD_SEND_DATA_PTR_INVALID]                   = "atcmd send_data ptr invalid ",
    [ERR_ATCMD_SEND_DATA_DATA_SIZE_INVALID]             = "atcmd send_data data size invalid ",
    [ERR_ATCMD_SEND_DATA_CON_ID_INVALID]                = "atcmd send_data con_id invalid ",
    [ERR_ATCMD_SEND_DATA_UDPSER_IP_ADDR_INVALID]        = "atcmd send_data udp server ip addr invalid ",
    [ERR_ATCMD_SEND_DATA_PARAM_INVALID]                 = "atcmd send_data param invalid ",
    [ERR_ATCMD_CLOSE_CON_ID_INVALID]                    = "atcmd close con_id invalid ",
    [ERR_ATCMD_AUTO_RECV_TASK_FAILED]                   = "atcmd auto receive task failed ",
    [ERR_ATCMD_RECV_DATA_AUTO_RECV_MODE_INVALID]        = "atcmd receive data rejected by auto receive mode  ",
    [ERR_ATCMD_RECV_DATA_CON_ID_INVALID]                = "atcmd receive data con_id invalid ",
    [ERR_ATCMD_RECV_DATA_PACKET_SIZE_INVALID]           = "atcmd receive data packet size invalid ",
    [ERR_ATCMD_RECV_DATA_NODE_INVALID]                  = "atcmd receive data node invalid ",
    [ERR_ATCMD_RECV_DATA_ROLE_INVALID]                  = "atcmd receive data role invalid ",
    [ERR_ATLWIP_MAX]                                    = "lwip error max",
};

typedef enum {
    NODE_ROLE_SERVER = 0,
    NODE_ROLE_CLIENT,
    NODE_ROLE_SEED
} skt_node_type_e;

typedef enum {
    NODE_MODE_TCP = 0,
    NODE_MODE_UDP,
} node_protocol_type_e;

typedef struct _skt_node {
    int         con_id;          // 连接ID
    int         sockfd;          // 套接字文件描述符
    int8_t      role;            // 节点角色（客户端、服务器、种子节点）
    int         protocol;        // 协议类型（TCP/UDP）
    uint32_t    remote_ip_addr;  // 远程IP地址
    uint16_t    remote_port;     // 远程端口
    uint32_t    local_ip_addr;   // 本地IP地址
    uint16_t    local_port;      // 本地端口
    TaskHandle_t handletask;     // 任务句柄
    struct _skt_node* next;      // 指向下一个主节点
    struct _skt_node* nextseed;  // 指向下一个种子节点
} skt_node_t;

typedef struct _atcmd_lwip_pcb {
    bool atcmd_lwip_auto_recv;
    bool atcmd_lwip_multi_conn;
    bool atcmd_lwip_tt_mode;
    bool atcmd_lwip_tt_mode_ready;
    bool atcmd_lwip_test_mode;
} atcmd_lwip_pcb_t;

#endif //_ATCMD_LWIP_H_
