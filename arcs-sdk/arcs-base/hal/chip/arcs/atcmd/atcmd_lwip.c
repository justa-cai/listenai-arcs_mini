/**
 ****************************************************************************************
 *
 * @file atcmd_lwip.c
 *
 * @brief
 *
 * Copyright (C) ListenAI  2023-2024
 *
 ****************************************************************************************
 */
#include <stdbool.h>
#include "atcmd.h"
#include "log_print.h"
#include "lwip/inet.h"
#include "ping.h"
#include "lwip/dns.h"

#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "atcmd_lwip.h"
#include "task.h"
#include "FreeRTOS.h"
#include "ls_err.h"

#include "net_ip.h"
#include "ls_wifi_type.h"

#define ATCMD_LWIP_TASK_DEFAULT_STACK_SIZE  296
#define ATCMD_LWIP_DEFAULT_TASK_PRIO        1
#define MAX_BUFFER ETH_MAX_MTU
#define ETH_MAX_MTU                         1500
#define RECV_SELECT_TIMEOUT_SEC		        (0)
#define RECV_SELECT_TIMEOUT_USEC		    (10000) //10ms

skt_node_t node_pool[NUM_NS];
skt_node_t* mainlist;

// static unsigned char _tx_buffer[MAX_BUFFER];
static unsigned char _rx_buffer[MAX_BUFFER];
// static unsigned char *tx_buffer = _tx_buffer;
static unsigned char *rx_buffer = _rx_buffer;

static int atcmd_lwip_multi_conn = false;
TaskHandle_t atcmd_lwip_auto_recv_task = NULL;

atcmd_lwip_pcb_t atcmd_lwip_ctrl = 
{
    .atcmd_lwip_auto_recv = false,
    .atcmd_lwip_multi_conn = true,
    .atcmd_lwip_tt_mode = false,
    .atcmd_lwip_tt_mode_ready = false,
    .atcmd_lwip_test_mode = false,
};


static int create_client_socket(int proto, int *keepalive);
static int setup_udp_socket(int sockfd, struct sockaddr_in *addr, skt_node_t *node);
static void send_client_response(skt_node_t *node, bool is_error);

/**************************************************************************
 * @brief 将节点插入主节点列表
 * 
 * @param new_node 要插入的新节点指针
 * @return int 成功返回0，失败返回-1
 **************************************************************************/
static int add_node_to_mainlist(skt_node_t* new_node) 
{
    skt_node_t* current_node = mainlist;
    struct in_addr ip_addr;

    SYS_ARCH_DECL_PROTECT(lev);
    SYS_ARCH_PROTECT(lev);

    while (current_node->next != NULL) 
    {
        current_node = current_node->next;

        if (new_node->role == NODE_ROLE_SERVER) 
        {

            if ((current_node->local_port == new_node->local_port) &&
                (current_node->local_ip_addr == new_node->local_ip_addr) &&
                (current_node->role == new_node->role) &&
                (current_node->protocol == new_node->protocol)) 
            {
                SYS_ARCH_UNPROTECT(lev);

                ip_addr.s_addr = htonl(new_node->local_ip_addr);
                AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "Connection (IP:%s Port:%d) already exists",
                           inet_ntoa(ip_addr), new_node->local_port);
                return -1;
            }
        }
    }

    current_node->next = new_node;
    SYS_ARCH_UNPROTECT(lev);
    return 0;
}

/**************************************************************************
 * @brief 将种子节点插入主节点的种子节点列表
 * 
 * @param main_node 主节点指针
 * @param new_seed_node 新种子节点指针
 * @return int 成功返回0，失败返回-1
 **************************************************************************/
static int add_seednode_to_mainnode(skt_node_t* main_node, skt_node_t* new_seed_node) 
{
    skt_node_t* current_node = main_node;

	SYS_ARCH_DECL_PROTECT(lev);
	SYS_ARCH_PROTECT(lev);	

    while (current_node->nextseed != NULL) 
    {
        current_node = current_node->nextseed;

        if ((current_node->remote_port == new_seed_node->remote_port) &&
            (current_node->remote_ip_addr == new_seed_node->remote_ip_addr)) 
        {

			SYS_ARCH_UNPROTECT(lev);

            struct in_addr ip_addr;
            ip_addr.s_addr = htonl(new_seed_node->remote_ip_addr);
            AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "Seed node (IP:%s Port:%d) already exists",
                       inet_ntoa(ip_addr), new_seed_node->remote_port);
            return -1;
        }
    }

    current_node->nextseed = new_seed_node;
	SYS_ARCH_UNPROTECT(lev);    
    return 0;
}

/**************************************************************************
 * @brief 创建并初始化一个新的节点
 * 
 * @param protocol 节点使用的协议（如TCP或UDP）
 * @param role 节点的角色（如服务器或客户端）
 * @return skt_node_t* 指向新创建节点的指针，如果创建失败则返回NULL
 **************************************************************************/
static skt_node_t* create_node(int protocol, int8_t role) 
{
    SYS_ARCH_DECL_PROTECT(lev);

    for (int i = 0; i < NUM_NS; ++i) 
    {
        SYS_ARCH_PROTECT(lev);
        if (node_pool[i].con_id == INVALID_CON_ID) 
        {
            node_pool[i].con_id = i;
            SYS_ARCH_UNPROTECT(lev);
            node_pool[i].sockfd = INVALID_SOCKET_ID;
            node_pool[i].protocol = protocol;
            node_pool[i].role = role;
            node_pool[i].remote_ip_addr = 0;
            node_pool[i].remote_port = -1;
            node_pool[i].handletask = NULL;
            node_pool[i].next = NULL;
            node_pool[i].nextseed = NULL;

            return &node_pool[i];
        }
        SYS_ARCH_UNPROTECT(lev);
    }

    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "No available connection ID");
    return NULL;
}

/**************************************************************************
 * @brief 根据连接ID查找节点
 * 
 * @param connection_id 要查找的节点的连接ID
 * @return skt_node_t* 找到的节点指针，如果未找到则返回NULL
 **************************************************************************/
static skt_node_t* find_node_by_id(int connection_id) 
{
    skt_node_t* current_node = mainlist;

    while (current_node->next != NULL) 
    {
        current_node = current_node->next;
        if (current_node->con_id == connection_id) 
        {

            return current_node;  
        }
        if (current_node->nextseed != NULL) {

            skt_node_t* seed_node = current_node;
            do {
                seed_node = seed_node->nextseed;
                if (seed_node->con_id == connection_id) 
                {
                    
                    return seed_node;  
                }
            } while (seed_node->nextseed != NULL);
        }
    }
    return NULL;  
}

/**************************************************************************
 * @brief 根据索引尝试获取节点
 * 
 * @param index 要获取的节点的索引
 * @return skt_node_t* 找到的节点指针，如果未找到则返回NULL
 **************************************************************************/
static skt_node_t* try_get_node_by_index(int index) 
{
	SYS_ARCH_DECL_PROTECT(lev);

    if ((index <= 0) || (index > NUM_NS)) 
    {
        return NULL;
    }

	SYS_ARCH_PROTECT(lev);

    if (node_pool[index].con_id == INVALID_CON_ID || node_pool[index].sockfd == INVALID_SOCKET_ID) 
    {
		SYS_ARCH_UNPROTECT(lev);
        return NULL;
    }

	SYS_ARCH_UNPROTECT(lev);
    return &node_pool[index];
}

/**************************************************************************
 * @brief 删除指定的节点并释放相关资源
 * 
 * @param node_to_remove 要删除的节点指针
 **************************************************************************/
static void delete_node(skt_node_t* node_to_remove) 
{
    if (node_to_remove == NULL || node_to_remove->con_id == INVALID_CON_ID) 
    {
        return; 
    }

    skt_node_t *current_node, *previous_node, *current_seed, *previous_seed;

	SYS_ARCH_DECL_PROTECT(lev);
	SYS_ARCH_PROTECT(lev);

    for (current_node = mainlist; current_node != NULL; previous_node = current_node, current_node = current_node->next) 
    {
        if (current_node == node_to_remove) 
        {
            previous_node->next = current_node->next;
        }

        if (current_node->role != NODE_ROLE_SERVER) 
        {
            continue;
        }

        previous_seed = current_node;
        current_seed = current_node->nextseed;
        while (current_seed != NULL) 
        {
            if (current_seed == node_to_remove) 
            {
                
                previous_seed->nextseed = node_to_remove->nextseed; 
            }
            previous_seed = current_seed;
            current_seed = current_seed->nextseed;
        }
    }

	SYS_ARCH_UNPROTECT(lev);

    // 如果节点是服务器节点，清理其种子节点
    if (node_to_remove->role == NODE_ROLE_SERVER) 
    {
        while (node_to_remove->nextseed != NULL) 
        {
            current_seed = node_to_remove->nextseed;

            if (current_seed->protocol == NODE_MODE_TCP && current_seed->sockfd != INVALID_SOCKET_ID) 
            {
                close(current_seed->sockfd); 
                current_seed->sockfd = INVALID_SOCKET_ID;
            }

            node_to_remove->nextseed = current_seed->nextseed; 
            current_seed->con_id = INVALID_CON_ID;
        }
    }

    // 如果不是UDP种子节点，关闭套接字
    if (!((node_to_remove->protocol == NODE_MODE_UDP) && (node_to_remove->role == NODE_ROLE_SEED))) 
    {
        if (node_to_remove->sockfd != INVALID_SOCKET_ID) 
        {
            close(node_to_remove->sockfd);  
            node_to_remove->sockfd = INVALID_SOCKET_ID;  
        }
    }

    if (node_to_remove->handletask) 
    {
        vTaskDelete(node_to_remove->handletask); 
        node_to_remove->handletask = NULL;
    }

    node_to_remove->con_id = INVALID_CON_ID;
    return;
}

/**************************************************************************
 * @brief 初始化节点池
 * 
 * 该函数用于初始化节点池，将节点池的内存清零，并将所有节点的连接ID标记为无效。
 * 这样可以确保在使用节点池之前，所有节点都处于初始状态。
 **************************************************************************/
static void init_node_pool(void) 
{
    memset(node_pool, 0, sizeof(node_pool));

    for (int i = 0; i < NUM_NS; i++) 
    {
        // 将每个节点的连接ID标记为无效，表明该节点当前未被使用
        node_pool[i].con_id = INVALID_CON_ID;
    }
}

/**************************************************************************
 * @brief 记录LWIP模块的错误信息
 * 
 * 该函数根据传入的错误代码，从错误消息数组中查找对应的错误消息，并使用日志打印函数输出错误信息。
 * 如果错误代码超出有效范围或者为无错误状态，则不进行任何操作。
 * 
 * @param err_code 要记录的错误代码，类型为at_lwip_errcode_e
**************************************************************************/
static void at_lwip_log_error(at_lwip_errcode_e err_code, const char* function_name)
{
    if ((err_code >= ERR_ATLWIP_MAX) || (err_code <= ERR_NONE))
        return;
    
    if (at_lwip_error_messages[err_code] != NULL)
    {
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "error %d: %s [%s]",
            err_code, at_lwip_error_messages[err_code], function_name);
    }
}

/**************************************************************************
 * @brief 处理ping命令
 * 
 * 该函数用于处理ping命令，支持通过IP地址或域名进行ping操作。
 * 它解析传入的参数，将域名解析为IP地址（如果需要），并调用ping_start函数执行ping操作。
 * 根据ping操作的结果，输出相应的信息。
 * 
 * @param type 命令类型（未使用）
 * @param arg 包含ping命令参数的字符串
 **************************************************************************/
static int atcmd_ping(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int error_code = ERR_NONE;
    ip_addr_t ping_ip_addr;
	char* domain_name;
    uint32_t dns_timeout = 80000;
    uint32_t ping_cnt = 1;
    int32_t time;
    int8_t err;

    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
		"%s", __FUNCTION__);

    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
					if(inet_aton((char *)cur, &ping_ip_addr) == 0) 
                    {
                        domain_name = (char *)cur;
                        // while((!(dns_gethostbyname(domain_name, &ping_ip_addr, NULL, NULL) == ERR_OK)) && dns_timeout--);
                        
                        #if LWIP_DNS 
                        err = dns_gethostbyname(domain_name, &ping_ip_addr, NULL, NULL);
                        if(err == ERR_OK) {
                            AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS,
                                " Found name '%s' = %s", 
                                *domain_name,
                                inet_ntoa(ping_ip_addr)
                            );
                        }
                        else
                        #endif // LWIP_DNS

                        {
                            error_code = ERR_ATCMD_PING_HOST_FOUND_FAILED;
                            goto err_exit; 
                        }
                    }
                    argc_ok = true;		
					break;
				}
                case 1:
                {
                    ping_cnt = atoi((char *)cur);
                    if(ping_cnt <= 0)
                    {
                        error_code = ERR_ATCMD_PARAMETER_INVALID;
					    goto err_exit;
                    }
                    argc_ok = true;
                        
                    break;
                }
				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while (next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if (argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	time = ping_start(&ping_ip_addr, ping_cnt);
	if (time != -1) {
		atcmd_rspinfor("+%d\r\n",time);
    } else {
		atcmd_rspinfor("+timeout\r\n");
		error_code = ERR_ATCMD_PING_FAILED;
	}

err_exit:
	if (error_code) {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
	} else {
        atcmd_rspinfor("[%s] success", __FUNCTION__);
    }
	return ATCMD_OK;
}

/**************************************************************************
 * @brief 检查自动接收模式是否启用
 * 
 * 该函数用于检查AT命令LWIP模块的自动接收模式是否已经启用。
 * 通过访问全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_auto_recv字段，
 * 返回该字段的值以表示自动接收模式的状态。
 * 
 * @return bool 如果自动接收模式已启用，返回true；否则返回false。
**************************************************************************/
static bool atcmd_lwip_is_autorecv_mode(void)
{
	return atcmd_lwip_ctrl.atcmd_lwip_auto_recv;
}

/**************************************************************************
 * @brief 设置AT命令LWIP模块的自动接收模式
 * 
 * 该函数用于设置AT命令LWIP模块的自动接收模式是否启用。
 * 通过修改全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_auto_recv字段，
 * 可以控制自动接收模式的状态。
 * 
 * @param enable 布尔值，true表示启用自动接收模式，false表示禁用。
**************************************************************************/
static void atcmd_lwip_set_autorecv_mode(bool enable)
{
	atcmd_lwip_ctrl.atcmd_lwip_auto_recv = enable;
}

/**************************************************************************
 * @brief 检查AT命令LWIP模块是否启用多连接模式
 * 
 * 该函数用于检查AT命令LWIP模块的多连接模式是否已经启用。
 * 通过访问全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_multi_conn字段，
 * 返回该字段的值以表示多连接模式的状态。
 * 
 * @return bool 如果多连接模式已启用，返回true；否则返回false。
**************************************************************************/
static bool atcmd_lwip_is_multi_conn(void)
{
	return atcmd_lwip_ctrl.atcmd_lwip_multi_conn;
}

/**************************************************************************
 * @brief 检查AT命令LWIP模块是否启用透传模式（TT模式）
 * 
 * 该函数用于检查AT命令LWIP模块的透传模式（TT模式）是否已经启用。
 * 通过访问全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_tt_mode字段，
 * 返回该字段的值以表示透传模式（TT模式）的状态。
 * 
 * @return bool 如果透传模式（TT模式）已启用，返回true；否则返回false。
**************************************************************************/
static bool atcmd_lwip_is_tt_mode(void)
{
	return atcmd_lwip_ctrl.atcmd_lwip_tt_mode;
}

/**************************************************************************
 * @brief 设置AT命令LWIP模块的透传模式（TT模式）
 * 
 * 该函数用于设置AT命令LWIP模块的透传模式（TT模式）是否启用。
 * 通过修改全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_tt_mode字段，
 * 可以控制透传模式（TT模式）的状态。
 * 
 * @param enable 布尔值，true表示启用透传模式（TT模式），false表示禁用。
**************************************************************************/
static void atcmd_lwip_set_tt_mode(bool enable)
{
	atcmd_lwip_ctrl.atcmd_lwip_tt_mode = enable;
}

/**************************************************************************
 * @brief 检查AT命令LWIP模块的透传模式（TT模式）是否准备就绪
 * 
 * 该函数用于检查AT命令LWIP模块的透传模式（TT模式）是否已经准备好。
 * 通过访问全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_tt_mode_ready字段，
 * 返回该字段的值以表示透传模式（TT模式）的准备状态。
 * 
 * @return bool 如果透传模式（TT模式）已准备好，返回true；否则返回false。
**************************************************************************/
static bool atcmd_lwip_is_tt_mode_ready(void)
{
	return atcmd_lwip_ctrl.atcmd_lwip_tt_mode_ready;
}

/**************************************************************************
 * @brief 设置AT命令LWIP模块的透传模式（TT模式）准备状态
 * 
 * 该函数用于设置AT命令LWIP模块的透传模式（TT模式）是否准备就绪。
 * 通过修改全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_tt_mode_ready字段，
 * 可以控制透传模式（TT模式）的准备状态。
 * 
 * @param enable 整数值，非零值表示启用透传模式（TT模式）准备状态，零值表示禁用。
**************************************************************************/
static void atcmd_lwip_set_tt_mode_ready(int enable)
{
	atcmd_lwip_ctrl.atcmd_lwip_tt_mode_ready = enable;
}

/**************************************************************************
 * @brief 检查AT命令LWIP模块的测试模式是否已启用
 * 
 * 该函数用于检查AT命令LWIP模块的测试模式是否已经启用。
 * 通过访问全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_test_mode字段，
 * 返回该字段的值以表示测试模式的状态。
 * 
 * @return bool 如果测试模式已启用，返回true；否则返回false。
**************************************************************************/
static bool atcmd_lwip_is_test_mode(void)
{
	return atcmd_lwip_ctrl.atcmd_lwip_test_mode;
}

/**************************************************************************
 * @brief 设置AT命令LWIP模块的测试模式
 * 
 * 该函数用于设置AT命令LWIP模块的测试模式是否启用。
 * 通过修改全局的atcmd_lwip_ctrl结构体中的atcmd_lwip_test_mode字段，
 * 可以控制测试模式的状态。
 * 
 * @param enable 布尔值，true表示启用测试模式，false表示禁用。
**************************************************************************/
static void atcmd_lwip_set_test_mode(bool enable)
{
	atcmd_lwip_ctrl.atcmd_lwip_test_mode = enable;
}

/**************************************************************************
 * @brief 接收LWIP模块的数据
 * 
 * 该函数用于从指定的套接字节点接收数据。根据节点的协议类型（TCP或UDP），
 * 采用不同的接收方式，并处理可能出现的错误。
 * 
 * @param curnode 指向要接收数据的套接字节点的指针
 * @param buffer 用于存储接收到的数据的缓冲区
 * @param buffer_size 缓冲区的大小
 * @param recv_size 指向一个整数的指针，用于存储实际接收到的数据大小
 * @param udp_clientaddr 用于存储UDP客户端地址的缓冲区
 * @param udp_clientport 指向一个无符号16位整数的指针，用于存储UDP客户端端口
 * @return int 错误代码，如果没有错误则返回ERR_NONE
**************************************************************************/
static int atcmd_lwip_receive_data(skt_node_t *curnode, uint8_t *buffer, uint16_t buffer_size, 
    int *recv_size, uint8_t *udp_clientaddr, uint16_t *udp_clientport)
{
	struct timeval tv;
	fd_set readfds;
	int error_code = ERR_NONE, ret = 0, size = 0;

	FD_ZERO(&readfds);
	FD_SET(curnode->sockfd, &readfds);
	tv.tv_sec = RECV_SELECT_TIMEOUT_SEC;
	tv.tv_usec = RECV_SELECT_TIMEOUT_USEC;

	ret = select(curnode->sockfd + 1, &readfds, NULL, NULL, &tv);
	if (!((ret > 0)&&(FD_ISSET(curnode->sockfd, &readfds))))
	{
		goto err_exit;
	}

    memset(buffer, 0, buffer_size);

	if (curnode->protocol == NODE_MODE_UDP) //udp server receive from client
	{
		if (curnode->role == NODE_ROLE_SERVER) {
			struct sockaddr_in client_addr;
			socklen_t addr_len = sizeof(struct sockaddr_in);
			memset((char *)&client_addr, 0, sizeof(client_addr));

			if ((size = recvfrom(curnode->sockfd, buffer, buffer_size, 0, (struct sockaddr *) &client_addr, &addr_len)) <= 0) 
            {
                error_code = ERR_ATLWIP_RECV_DATA_UDPSER_FAILED;
                goto err_exit;
			}

			inet_ntoa_r(client_addr.sin_addr.s_addr, (char *)udp_clientaddr, 16);
			*udp_clientport = ntohs(client_addr.sin_port);

		} else {
			struct sockaddr_in serv_addr;
			socklen_t addr_len = sizeof(struct sockaddr_in);  
			memset((char *) &serv_addr, 0, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(curnode->remote_port);
			serv_addr.sin_addr.s_addr = htonl(curnode->remote_ip_addr);
			
			if ((size = recvfrom(curnode->sockfd, buffer, buffer_size, 0, (struct sockaddr *) &serv_addr, &addr_len)) <= 0) 
            {
                error_code = ERR_ATLWIP_RECV_DATA_UDPCLI_FAILED;
                goto err_exit;
			}
		}
	} else {
		size = read(curnode->sockfd,buffer,buffer_size);
		
		if (size == 0) {
			error_code = ERR_ATLWIP_RECV_DATA_TCP_CLOSED;
            goto err_exit;
		} else if(size < 0) {
			error_code = ERR_ATLWIP_RECV_DATA_TCP_FAILED;
            goto err_exit;
		}
	}

err_exit:
	if (error_code == 0) {
        *recv_size = size;
    } else {
        at_lwip_log_error(error_code, __FUNCTION__);
    }
	return error_code;
}

/**************************************************************************
 * @brief AT命令LWIP模块的自动接收任务
 * 
 * 该任务用于在自动接收模式下，循环检查节点池中的每个节点，
 * 并尝试从这些节点接收数据。如果接收到数据，将数据通过AT命令响应发送出去。
 * 如果接收过程中出现错误，将删除对应的节点。
 * 
 * @param param 任务参数，当前未使用
**************************************************************************/
static void atcmd_lwip_receive_task(void *param)
{
	int i;
	int packet_size = ETH_MAX_MTU;
    int ret=0;

	while (1)
	{
		for (i = 0; i < NUM_NS; ++i) 
        {
			skt_node_t* curnode = NULL;
			int error_code = ERR_NONE;
			int recv_size = 0;	
			u8_t udp_clientaddr[16] = {0};
			u16_t udp_clientport = 0;		
			struct sockaddr_in cli_addr;
			curnode = try_get_node_by_index(i);
			if (curnode == NULL)
			{
				// vTaskDelay(20);
				continue;
			}
				
			if ((curnode->protocol == NODE_MODE_TCP )
				&& curnode->role == NODE_ROLE_SERVER)
            {
				// vTaskDelay(20);
				continue;
			}

			if (atcmd_lwip_is_autorecv_mode())
			{
				error_code = atcmd_lwip_receive_data(curnode, rx_buffer, packet_size, &recv_size, udp_clientaddr, &udp_clientport);
				if (error_code == 0) {
					if (recv_size)
					{
						if (!atcmd_lwip_is_tt_mode_ready())
						{
							if (atcmd_lwip_is_multi_conn()) {
								atcmd_rspinfor("+IPD,%d,%d:", curnode->con_id, recv_size);
							} else {
								atcmd_rspinfor("+IPD,%d:", recv_size);
							}
						}
                        // atcmd_print_data(rx_buffer, recv_size);
                        if (atcmd_lwip_is_test_mode())
						{
							ret = write(curnode->sockfd, rx_buffer, recv_size);
							if(ret < 0)
							{
								AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
								"\r\nring back data failed!\r\n");
							}				
						}
					}
				} else {
                    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
                        "\r\nlwip_receive_data err:%d\r\n", error_code);
					vTaskDelay(10);
					delete_node(curnode);
				}
			}
		}
	}

	AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
			"Leave auto receive mode");
	
	vTaskDelete(NULL);
	atcmd_lwip_auto_recv_task = NULL;
}

/**************************************************************************
 * @brief 启动AT命令LWIP自动接收任务
 * 
 * 该函数用于启动AT命令LWIP模块的自动接收任务。如果任务尚未启动，则创建一个新的任务。
 * 如果任务创建失败，将输出错误信息并返回 -1；如果任务已经启动或创建成功，则返回 0。
 * 
 * @return int 任务创建成功返回 0，失败返回 -1
**************************************************************************/
static int atcmd_lwip_start_autorecv_task(void)
{
	if (atcmd_lwip_auto_recv_task == NULL)
	{
		if (xTaskCreate(atcmd_lwip_receive_task, 
            ((const char*)"atcmd_lwip_receive_task"), 
            ATCMD_LWIP_TASK_DEFAULT_STACK_SIZE, 
            NULL, 
            ATCMD_LWIP_DEFAULT_TASK_PRIO, 
            &atcmd_lwip_auto_recv_task) != pdPASS)
		{
			AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
				"ERROR: Create atcmd lwip receive task failed.");
			return -1;
		}
	}
	return 0;
}

/**************************************************************************
 * @brief 启动服务器任务
 * 
 * 该函数用于启动一个服务器，根据传入的参数创建一个TCP或UDP服务器。
 * 处理服务器的套接字创建、绑定、监听等操作，并处理客户端连接。
 * 
 * @param param 指向服务器节点的指针，包含服务器的配置信息
**************************************************************************/
static void server_start(void *param) 
{
    int protocol_mode;
    int server_socket_fd, client_socket_fd;
    socklen_t client_address_length;
    struct sockaddr_in server_address, client_address;
    int local_port;
    int error_code = ERR_NONE;
    int reuse_address_option = 1;
    struct ip_addr_cfg cfg = {0};

    int result;
    int keepalive_enable = 1;
    skt_node_t *server_node = (skt_node_t *)param;

    if (server_node) {
        protocol_mode = server_node->protocol;
        local_port = server_node->local_port;
    } else {
        error_code = ERR_ATLWIP_SERVER_START_NODE_IS_NULL;
        goto err_exit;
    }

    if (protocol_mode == NODE_MODE_UDP) {
        server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    } else if (protocol_mode == NODE_MODE_TCP) {
        server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        error_code = ERR_ATLWIP_SERVER_START_NODE_MODE_INVALID;
        goto err_exit;
    }

    if (server_socket_fd == INVALID_SOCKET_ID) 
    {
        error_code = ERR_ATLWIP_SERVER_START_SOCKET_CREATE_FAILED;
        goto err_exit;
    }

    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_address_option, sizeof(reuse_address_option)) < 0) 
    {
        close(server_socket_fd);
        error_code = ERR_ATLWIP_SERVER_START_SOCKET_OPTION_FAILED;
        goto err_exit;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(local_port);

    if (bind(server_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) 
    {
        close(server_socket_fd);
        error_code = ERR_ATLWIP_SERVER_START_SOCKET_BIND_FAILED;
        goto err_exit;
    }

    if (server_node) 
    {
	ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
        server_node->sockfd = server_socket_fd;
        server_node->local_ip_addr = ntohl(cfg.ipv4.addr);
    }

    if (protocol_mode == NODE_MODE_TCP) 
    {
        if (listen(server_socket_fd, 5) < 0) 
        {
            error_code = ERR_ATLWIP_SERVER_START_SOCKET_LISTEN_FAILED;
            goto err_exit;
        }

        if (add_node_to_mainlist(server_node) < 0) 
        {
            error_code = ERR_ATLWIP_SERVER_START_TCP_NODE_ADD_FAILED;
            goto err_exit;
        }

        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "The tcp server start ok, con_id:%d, ip:%s, port:%d, socket id:%d", 
            server_node->con_id, inet_ntoa(cfg.ipv4.addr), server_node->local_port, server_node->sockfd);

        client_address_length = sizeof(client_address);

        while (1) 
        {
            client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_address, &client_address_length);
            if (client_socket_fd < 0) 
            {
                AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "Failed to accept client connection");
                continue; // 重试 accept
            }

            skt_node_t *seed_node = create_node(protocol_mode, NODE_ROLE_SEED);
            if (!seed_node) 
            {
                close(client_socket_fd);
                error_code = ERR_ATLWIP_SERVER_START_SEED_CREATE_FAILED;
                goto err_exit;
            }

            seed_node->sockfd = client_socket_fd;
            seed_node->remote_port = ntohs(client_address.sin_port);
            seed_node->remote_ip_addr = ntohl(client_address.sin_addr.s_addr);

            if (add_seednode_to_mainnode(server_node, seed_node) < 0)
            {
                delete_node(seed_node);
                close(client_socket_fd);
                error_code = ERR_ATLWIP_SERVER_START_SEED_ADD_FAILED;
                goto err_exit;
            }

            AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "A client connected to server[%d]\r\n"
                        "con_id:%d, seed, tcp, address:%s, port:%d, socket:%d",
                        server_node->con_id, seed_node->con_id,
                        inet_ntoa(client_address.sin_addr.s_addr), ntohs(client_address.sin_port), seed_node->sockfd);
            
            atcmd_rspinfor("CONNECT OK\r\n");
            atcmd_rspinfor("server_id:%d, con_id:%d, seed, tcp, address:%s, port:%d, socket:%d, OK\r\n",
                            server_node->con_id, 
                            seed_node->con_id,
                            inet_ntoa(client_address.sin_addr.s_addr), 
                            ntohs(client_address.sin_port), 
                            seed_node->sockfd);
        }
    } else {
#if IP_SOF_BROADCAST && IP_SOF_BROADCAST_RECV
        int broadcast_enable = 1;
        if (setsockopt(server_socket_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) 
        {
            AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, "Failed to enable broadcast on UDP socket");
            error_code = ERR_ATLWIP_SERVER_START_BROADCAST_OPTION_FAILED;
            goto err_exit;
        }
#endif
        if (add_node_to_mainlist(server_node) < 0) 
        {
            error_code = ERR_ATLWIP_SERVER_START_UDP_NODE_ADD_FAILED;
            goto err_exit;
        }

        server_node->handletask = NULL;
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "The udp server start ok, con_id:%d, ip:%s, port:%d, socket id:%d", 
            server_node->con_id, inet_ntoa(cfg.ipv4.addr), server_node->local_port, server_node->sockfd);
    }

err_exit:
    if (error_code != ERR_NONE) 
    {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("ERROR:%d", error_code);

        if (server_node) 
        {
            server_node->handletask = NULL;
            delete_node(server_node);
        }

        if (server_socket_fd != INVALID_SOCKET_ID) 
        {
            close(server_socket_fd);
        }    
    }

}

/**************************************************************************
 * @brief 创建服务器任务
 * 
 * 该函数用于创建一个服务器任务，接收一个指向服务器节点的指针作为参数。
 * 首先检查参数是否为空，如果为空则直接返回。
 * 然后调用 server_start 函数启动服务器，最后删除当前任务。
 * 
 * @param param 指向服务器节点的指针，包含服务器的配置信息
**************************************************************************/
static void creat_server_task(void *param)
{
    if(!param)
        return;
	server_start(param);
	vTaskDelete(NULL);
	return;
}

/**************************************************************************
 * @brief 创建一个服务器节点并启动服务器任务
 * 
 * 该函数用于解析传入的参数，创建一个服务器节点，并启动一个新的任务来运行服务器。
 * 参数应包含服务器模式（TCP或UDP）和本地端口号。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_server_create(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;

	skt_node_t* servernode = NULL;
	int server_mode;
	int local_port;
	int error_code = ERR_NONE;
    int8_t argc_ok = false;

	AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
		"%s", __FUNCTION__);
	
    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    if (!cur)
                    {                   
                        error_code = ERR_ATCMD_PARAMETER_IS_NULL;
                        goto err_exit;
                    }

                    if (!strcmp("TCP", cur)) {
                        server_mode = NODE_MODE_TCP;
                    }
                    else if (!strcmp("UDP", cur)) {
                        server_mode = NODE_MODE_UDP;
                    } else {
                        error_code = ERR_ATCMD_SERVER_CREATE_MODE_INVALID;
                        goto err_exit;
                    }

					break;
				}
                case 1:
                {
                    if(!cur)
                    {                   
                        error_code = ERR_ATCMD_PARAMETER_IS_NULL;
                        goto err_exit;
                    }
                   	local_port = atoi((char*)cur);
                    if (!IS_VALID_PORT(local_port))
                    {
                        error_code = ERR_ATCMD_SERVER_CREATE_PORT_INVALID;
                        goto err_exit;
                    }
                    argc_ok = true;
                    break;
                }
				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if (argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	servernode = create_node(server_mode, NODE_ROLE_SERVER);
	if (servernode == NULL)
    {
		error_code = ERR_ATCMD_SERVER_CREATE_PORT_INVALID;
		goto err_exit;
	}
	servernode->local_port = local_port;

	if (xTaskCreate(creat_server_task, 
        ((const char*)"creat_server_task"), 
        ATCMD_LWIP_TASK_DEFAULT_STACK_SIZE, 
        servernode, 
        ATCMD_LWIP_DEFAULT_TASK_PRIO, 
        &servernode->handletask) != pdPASS)
	{	
		error_code = ERR_ATCMD_SERVER_CREATE_TASK_FAILED;
		goto err_exit;
	}

err_exit:
	if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        if(servernode)
		    delete_node(servernode);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<mode>,"
            "<port>");

    } else {
        atcmd_rspinfor("[%s] success, con_id:%d", __FUNCTION__, servernode->con_id);
    }
    return ATCMD_OK;
}

/**************************************************************************
 * @brief 启动客户端连接
 * 
 * 该函数用于启动一个客户端连接，根据传入的客户端节点参数创建套接字，
 * 并尝试连接到服务器。如果连接成功，将客户端节点添加到主列表中。
 * 如果连接过程中出现错误，将记录错误信息并删除客户端节点。
 * 
 * @param param 指向客户端节点的指针，包含客户端的配置信息
**************************************************************************/
static void client_start(void *param) 
{
    skt_node_t *client_node = (skt_node_t *)param;
    int error_code = ERR_NONE;
    int enable_flag = 0;
    struct sockaddr_in server_addr;
    char remote_ip_str[16];
    
    /* 参数提取 */
    const int protocol_type = client_node->protocol;
    const int remote_port = client_node->remote_port;
    struct in_addr ip_addr_struct = {.s_addr = htonl(client_node->remote_ip_addr)};

    /* IP地址转换 */
    if (!inet_ntoa_r(ip_addr_struct, remote_ip_str, sizeof(remote_ip_str))) 
    {
        error_code = ERR_ATLWIP_CLIENT_START_IP_CONVERSION_FAILED;
        goto err_exit;
    }

    /* 创建套接字 */
    int client_sockfd = create_client_socket(protocol_type, &enable_flag);
    if (client_sockfd == INVALID_SOCKET_ID) 
    {
        error_code = ERR_ATLWIP_CLIENT_START_SOCKET_CREATE_FAILED;
        goto err_exit;
    }

    /* 初始化服务器地址结构 */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_port);
    server_addr.sin_addr.s_addr = inet_addr(remote_ip_str);

    /* 更新节点套接字描述符 */
    client_node->sockfd = client_sockfd;

    /* 协议特定处理 */
    if (protocol_type == NODE_MODE_TCP) 
    {
        if (connect(client_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) 
        {
            error_code = ERR_ATLWIP_CLIENT_START_CONNECT_FAILED;
            goto err_exit;
        }
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "Connected to TCP server");
    } else {
        /* UDP特定设置 */
        if (setup_udp_socket(client_sockfd, &server_addr, client_node) != 0)
        {
            error_code = ERR_ATLWIP_CLIENT_START_MULTICAST_SETUP_FAILED;
            goto err_exit;
        }
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "UDP client initialized");
    }

    /* 添加节点到主列表 */
    if (add_node_to_mainlist(client_node) < 0) 
    {
        error_code = ERR_ATLWIP_CLIENT_START_NODE_ADD_FAILED;
        goto err_exit;
    }

    /* 成功响应 */
    send_client_response(client_node, false);

err_exit:
    if (error_code != ERR_NONE) 
    {
        send_client_response(client_node, true);
        at_lwip_log_error(error_code, __FUNCTION__);
        if (client_node) 
        {
            delete_node(client_node);
        }
    }
}

/**************************************************************************
 * @brief 创建客户端套接字
 * 
 * 该函数根据传入的协议类型创建一个客户端套接字，并根据需要设置TCP Keepalive选项。
 * 
 * @param proto 协议类型，取值为 NODE_MODE_TCP 或 NODE_MODE_UDP
 * @param keepalive 指向一个整数的指针，用于指示是否启用TCP Keepalive
 * @return int 创建的套接字描述符，如果创建失败则返回 INVALID_SOCKET_ID
**************************************************************************/
static int create_client_socket(int proto, int *keepalive)
{
    int sockfd = socket(AF_INET, 
        (proto == NODE_MODE_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
    
    if (sockfd == INVALID_SOCKET_ID) return sockfd;

    /* TCP Keepalive设置 */
    if (proto == NODE_MODE_TCP && *keepalive) {
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 
                 keepalive, sizeof(*keepalive));
    }
    return sockfd;
}

/**************************************************************************
 * @brief 设置UDP套接字
 * 
 * 该函数用于对UDP套接字进行配置，包括设置广播、多播选项以及绑定本地端口。
 * 
 * @param sockfd UDP套接字描述符
 * @param addr 指向目标地址结构体的指针
 * @param node 指向套接字节点结构体的指针
 * @return int 操作结果，成功返回0，失败返回非0值
**************************************************************************/
static int setup_udp_socket(int sockfd, struct sockaddr_in *addr, skt_node_t *node) 
{
    int ret = 0;

#if IP_SOF_BROADCAST
    if (addr->sin_addr.s_addr == htonl(INADDR_BROADCAST)) 
    {
        int broadcast_enable = 1;
        ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, 
                       &broadcast_enable, sizeof(broadcast_enable));
    }
#endif

#if LWIP_IGMP
    if (ip_addr_ismulticast((ip_addr_t *)&addr->sin_addr)) 
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = addr->sin_addr.s_addr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        ret = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                       &mreq, sizeof(mreq));
    }
#endif

    if (node->local_port > 0) 
    {
        struct sockaddr_in local_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(node->local_port),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };
        ret = bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
    }
    return ret;
}

/**************************************************************************
 * @brief 发送客户端连接响应信息
 * 
 * 该函数用于向客户端发送连接响应信息，根据连接是否成功，发送不同的响应消息。
 * 
 * @param node 指向客户端节点的指针，包含客户端的配置信息
 * @param is_error 布尔值，表示连接是否出错。true 表示出错，false 表示成功
**************************************************************************/
static void send_client_response(skt_node_t *node, bool is_error) 
{
    const char *proto_str = (node->protocol == NODE_MODE_TCP) ? "TCP" : "UDP";
    char ip_str[16];
    struct in_addr addr;
    addr.s_addr = htonl(node->remote_ip_addr);
    inet_ntoa_r(addr, ip_str, sizeof(ip_str));

    if (is_error) {
        atcmd_rspinfor("+LINK_ERROR,con_id:%d,proto:\"%s\",ip:%s,remote_port:%d,local_port:%d,socket:%d\r\n",
                     node->con_id, proto_str, ip_str, 
                     node->remote_port, node->local_port, node->sockfd);
    } else {
        atcmd_rspinfor("+LINK_CONN,con_id:%d,proto:\"%s\",ip:%s,remote_port:%d,local_port:%d,socket:%d\r\n",
                     node->con_id, proto_str, ip_str,
                     node->remote_port, node->local_port, node->sockfd);
    }
}

/**************************************************************************
 * @brief 客户端启动任务函数
 * 
 * 该函数用于启动一个客户端连接任务。它接收一个指向客户端节点的指针作为参数，
 * 首先检查参数是否为空，如果为空则直接返回。然后调用 client_start 函数来启动客户端连接，
 * 最后删除当前任务。
 * 
 * @param param 指向客户端节点的指针，包含客户端的配置信息
**************************************************************************/
static void client_start_task(void *param)
{
	if(!param)
		return;
    client_start(param);
	vTaskDelete(NULL);
}

/**************************************************************************
 * @brief 创建一个客户端节点并启动客户端任务
 * 
 * 该函数用于解析传入的参数，创建一个客户端节点，并启动一个新的任务来运行客户端。
 * 参数应包含客户端模式（TCP或UDP）、远程地址、远程端口和本地端口（可选）。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_client_create(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	skt_node_t* clientnode = NULL;
	int mode = 0;
	int remote_port;
	int local_port = 0;
	struct in_addr addr;
	int error_code = ERR_NONE;

#if LWIP_DNS  
	struct hostent *server_host;
#endif

	AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
		    "%s", __FUNCTION__);
		
    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
                /*  mode, remote_addr, remote_port, local_port */
				case 0:
				{
                    if (!strcmp("TCP", cur)) {
                        mode = NODE_MODE_TCP;
                    }
                    else if (!strcmp("UDP", cur)) {
                        mode = NODE_MODE_UDP;
                    } else {
                        error_code = ERR_ATCMD_CLIENT_CREATE_MODE_INVALID;
                        goto err_exit;
                    }

					break;
				}
                case 1:
                {
                   	if (inet_aton((char*)cur, &addr) == 0) 
                    {
                    #if LWIP_DNS  
                        server_host = gethostbyname((char*)cur);
                        if (server_host) {
                            memcmp(&addr, server_host->h_addr, 4);
                            AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS,
                                "Found name '%s' = %s", 
                                (char*)cur,
                                inet_ntoa(addr)
                            );
                        }
                        else
                    #endif //LWIP_DNS
                        {
                            error_code = ERR_ATCMD_CLIENT_CREATE_HOST_FOUND_FAILED;
                            goto err_exit; 
                        }
                    } else {
                        if (0 == addr.s_addr) 
                        {
                            error_code = ERR_ATCMD_CLIENT_CREATE_ADDR_INVALID;
                            goto err_exit; 
                        }
                    }
                    break;
                }
                case 2:
                {
                    remote_port = atoi((char*)cur);
                    if (!IS_VALID_PORT(remote_port)) 
                    {
                        error_code = ERR_ATCMD_CLIENT_CREATE_REMOTE_PORT_INVALID;
                        goto err_exit;
                    }
                    break;
                }
                case 3:
                {
                    if (cur)
                    {
                        local_port = atoi((char*)cur);
                        if (!IS_VALID_PORT(local_port)) 
                        {
                            error_code = ERR_ATCMD_CLIENT_CREATE_LOCAL_PORT_INVALID;
                            goto err_exit;
                        }
                        argc_ok = true;
                    }
                    break;
                }
				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if (argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	clientnode = create_node(mode, NODE_ROLE_CLIENT);
	if (clientnode == NULL)
    {
		error_code = ERR_ATCMD_CLIENT_CREATE_NODE_FAILED;
		goto err_exit;
	}

	clientnode->remote_port = remote_port;
	clientnode->remote_ip_addr = ntohl(addr.s_addr);
	clientnode->local_port = local_port;

	if (xTaskCreate(client_start_task, 
        ((const char*)"client_start_task"), 
        ATCMD_LWIP_TASK_DEFAULT_STACK_SIZE, 
        clientnode, 
        ATCMD_LWIP_DEFAULT_TASK_PRIO, 
        NULL) != pdPASS)
	{	
		error_code = ERR_ATCMD_CLIENT_CREATE_TASK_FAILED;
		goto err_exit;
	}

err_exit:
    if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        if(clientnode)
		    delete_node(clientnode);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<mode>,"
            "<remote_ip>,"
            "<remote_port>,"
            "<local_port>");
    } else {
        atcmd_rspinfor("[%s] success, con_id:%d", __FUNCTION__, clientnode->con_id);
    }

    return ATCMD_OK;
}

/**************************************************************************
 * @brief 通过LWIP协议栈发送数据
 * 
 * 该函数根据传入的套接字节点信息、数据、数据大小和客户端地址，选择合适的协议和方式发送数据。
 * 支持TCP和UDP协议，根据节点的角色（服务器或客户端）进行不同的处理。
 * 
 * @param curnode 指向套接字节点的指针，包含节点的协议、角色、端口等信息
 * @param data 指向要发送的数据的指针
 * @param data_sz 要发送的数据的大小
 * @param cli_addr 客户端地址结构体，包含客户端的IP地址和端口号
 * @return int 错误码，ERR_NONE表示成功，其他值表示相应的错误
**************************************************************************/
static int atcmd_lwip_send_data(skt_node_t *curnode, uint8_t *data, uint16_t data_sz, struct sockaddr_in cli_addr)
{
	int error_code = ERR_NONE;
	int ret;

	if ((curnode->protocol == NODE_MODE_UDP) && (curnode->role == NODE_ROLE_SERVER)) {
		if (sendto(curnode->sockfd, data, data_sz, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) <= 0) 
        {
			error_code = ERR_ATLWIP_SEND_DATA_UDPSER_FAILED;
            goto err_exit;
		}
	} else {
		if (curnode->protocol == NODE_MODE_UDP) {
			struct sockaddr_in serv_addr;
			memset(&serv_addr, 0, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET; 
			serv_addr.sin_port = htons(curnode->remote_port);
			serv_addr.sin_addr.s_addr = htonl(curnode->remote_ip_addr);

			#ifdef UDP_TEST
            while(1)
            {
                sendto(curnode->sockfd, data, data_sz, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            }
			#else
			if (sendto( curnode->sockfd, data, data_sz, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <= 0)
            {
				error_code = ERR_ATLWIP_SEND_DATA_UDPCLI_FAILED;
                goto err_exit;
			}
			#endif
		} else if ((curnode->protocol == NODE_MODE_TCP)) {
            if (curnode->role == NODE_ROLE_SERVER) 
            {
                error_code = ERR_ATLWIP_SEND_DATA_TCP_ROLE_INVALID;
                goto err_exit;
            }

            ret = write(curnode->sockfd, data, data_sz);
            if (ret <= 0) 
            {
                error_code = ERR_ATLWIP_SEND_DATA_TCP_INVALID;
                goto err_exit;
            }
        }
	}
	
err_exit:
	if (error_code!= ERR_NONE)
    {
        at_lwip_log_error(error_code, __FUNCTION__);
    }
	return error_code;
}

/**************************************************************************
 * @brief 处理发送数据的AT命令
 * 
 * 该函数用于解析AT命令参数，验证参数的有效性，并调用atcmd_lwip_send_data函数发送数据。
 * 参数应包含数据大小、连接ID，对于UDP服务器，还应包含目标IP地址和端口号。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_send_data(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int con_id = INVALID_CON_ID;
	int error_code = ERR_NONE;
	skt_node_t* curnode = NULL;
	struct sockaddr_in cli_addr;
	int data_sz;
	uint8_t *data;
    char udp_clientaddr[16]={0};

    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
        "%s", __FUNCTION__);

    if (!arg)
    {
        error_code = ERR_ATCMD_SEND_DATA_ARG_INVALID;
        goto err_exit;
    }
    char *p = arg;
	if (NULL != (p = strtok(p, ":"))) {
        data = (uint8_t *)arg + strlen(p) + 1;
    } else {
        error_code = ERR_ATCMD_SEND_DATA_PTR_INVALID;
        goto err_exit;
    }
	
    if(arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    data_sz = atoi((char*)cur);
                    if ((data_sz > MAX_BUFFER) || (data_sz <= 0)) 
                    {
                        error_code = ERR_ATCMD_SEND_DATA_DATA_SIZE_INVALID;
                        goto err_exit;
                    }
					break;
				}
                case 1:
                {
                    con_id = atoi((char*)cur);
                    curnode = find_node_by_id(con_id);
                    if (curnode == NULL) 
                    {
                        error_code = ERR_ATCMD_SEND_DATA_CON_ID_INVALID;
                        goto err_exit;
                    }
                    argc_ok = true;
                    break;
                }
                case 2:
                {
                    if ((curnode->protocol == NODE_MODE_UDP)
                        &&(curnode->role == NODE_ROLE_SERVER)) {
                        strcpy((char*)udp_clientaddr, (char*)cur);
                        if (inet_aton(udp_clientaddr , &cli_addr.sin_addr) == 0) 
                        {
                            error_code = ERR_ATCMD_SEND_DATA_UDPSER_IP_ADDR_INVALID;
                            goto err_exit;
                        }    
                    } else {
                        error_code = ERR_ATCMD_SEND_DATA_PARAM_INVALID;
                        goto err_exit;
                    }
                    argc_ok = false;
                    break;
                }
                case 3:
                {
                    if ((curnode->protocol == NODE_MODE_UDP)
                        &&(curnode->role == NODE_ROLE_SERVER)) {
                        cli_addr.sin_family = AF_INET;
                        cli_addr.sin_port = htons(atoi((char*)cur));
                    } else {
                        error_code = ERR_ATCMD_SEND_DATA_PARAM_INVALID;
                        goto err_exit;
                    }
                    argc_ok = true;
                    break;
                }
				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if (argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	error_code = atcmd_lwip_send_data(curnode, data, data_sz, cli_addr);

err_exit:
	if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("%s:ERROR:%d,%d", __FUNCTION__, error_code, con_id);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<data_size>,"
            "<con_id>[,<dst_ip>,<dst_port>]"
            ":<data>(MAX %d)", 
            MAX_BUFFER);
    } else {
        atcmd_rspinfor("[%s] sucess, con_id:%d", __FUNCTION__, con_id);
    }

    return ATCMD_OK;
}

/**************************************************************************
 * @brief 关闭所有套接字节点
 * 
 * 该函数遍历主列表中的所有套接字节点，并调用 delete_node 函数删除每个节点，
 * 以释放相关资源并关闭对应的套接字连接。
**************************************************************************/
static void socket_close_all(void)
{
	skt_node_t *currNode = mainlist->next;
	
	while(currNode)
	{
		delete_node(currNode);
		currNode = mainlist->next;
	}
	currNode = NULL;
}

/**************************************************************************
 * @brief 关闭指定连接或所有连接
 * 
 * 该函数用于解析传入的参数，根据连接ID关闭指定的连接，或者在连接ID为0时关闭所有连接。
 * 参数应包含一个有效的连接ID。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_close_connect(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int con_id = INVALID_CON_ID;
	int error_code = ERR_NONE;
	skt_node_t *del_node;

	AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
        "%s", __FUNCTION__);

    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    con_id = atoi((char*)cur);
                    argc_ok = true;
					break;
				}

				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if(argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	if (con_id == 0) {
		if (atcmd_lwip_is_autorecv_mode())
        {
			atcmd_lwip_set_autorecv_mode(false);
		}
		socket_close_all();
	} else {
        del_node = find_node_by_id(con_id);
        if(del_node == NULL) {
            error_code = ERR_ATCMD_CLOSE_CON_ID_INVALID;
            goto err_exit;
        } else {
            delete_node(del_node);
            del_node = NULL;
        }
    }

err_exit:

	if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<con_id>");

    } else {
        atcmd_rspinfor("[%s] sucess, con_id:%d", __FUNCTION__, con_id);
    }

    return ATCMD_OK;
}

/**************************************************************************
 * @brief 处理自动接收数据的AT命令
 * 
 * 该函数用于解析AT命令参数，根据参数值启用或禁用自动接收数据模式，并启动或停止相应的任务。
 * 参数应包含一个整数值，0表示禁用自动接收，非0表示启用自动接收。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_auto_receive_data(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int error_code = ERR_NONE;
	int enable = 0;

    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
        "%s", __FUNCTION__);

    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    enable = atoi((char*)cur);
                    argc_ok = true;
					break;
				}

				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if(argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	if (enable) {
		if (atcmd_lwip_is_autorecv_mode()) {
			AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "already enter auto receive mode");
		} else {
            atcmd_lwip_set_autorecv_mode(true);
			if (atcmd_lwip_start_autorecv_task() != 0)
            {
                error_code = ERR_ATCMD_AUTO_RECV_TASK_FAILED;
                goto err_exit;
            }
		}
	} else {
		if (atcmd_lwip_is_autorecv_mode()) {
			atcmd_lwip_set_autorecv_mode(false);
        } else {
			AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS,"already leave auto receive mode");
		}
	}

err_exit:
	if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<enable/disable>");

    } else {
        atcmd_rspinfor("[%s] sucess, set:%d", __FUNCTION__, enable);
    }

    return ATCMD_OK;
}

/**************************************************************************
 * @brief 处理接收数据的AT命令
 * 
 * 该函数用于解析AT命令参数，根据参数接收指定连接的数据。
 * 支持多连接模式，会检查自动接收模式、连接ID、数据包大小等参数的有效性。
 * 若接收数据未达到指定大小，会尝试多次接收。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
**************************************************************************/
static int atcmd_receive_data(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int con_id = INVALID_CON_ID;
	int error_code = ERR_NONE;
	int recv_size = 0;	
	int packet_size = 0;
	skt_node_t* curnode = NULL;
	struct sockaddr_in cli_addr;
	int data_sz;
	uint8_t *data;
    char udp_clientaddr[16]={0};
	uint16_t udp_clientport = 0;

    int total_recv_size = 0;
    int next_expected_size = 0;
    int fetch_counter = 0;

	AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
        "%s", __FUNCTION__);

	if (atcmd_lwip_is_autorecv_mode())
	{
		error_code = ERR_ATCMD_RECV_DATA_AUTO_RECV_MODE_INVALID; 
		goto err_exit;
	}
	
    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    if (atcmd_lwip_is_multi_conn()) {
                        con_id = atoi((char*)cur);
                        if (con_id > MAX_CON_ID)
                        {
                            error_code = ERR_ATCMD_RECV_DATA_CON_ID_INVALID;
                            goto err_exit;
                        }
                    } else {
                        con_id = 0;
                        packet_size = atoi((char*)cur);
                        argc_ok = true;           
                    }
					break;
				}
                case 1:
                {
                    if (atcmd_lwip_is_multi_conn()) {
                        packet_size = atoi((char*)cur);
                        argc_ok = true;
                    } else {
                        argc_ok = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
                    }
                    break;
                }
				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if(argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

    if(packet_size <= 0 || packet_size > MAX_BUFFER) 
    {
		AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR, 
			"Recv Size(%d) exceeds MAX_BUFFER(%d)", packet_size, 
			MAX_BUFFER);
		error_code = ERR_ATCMD_RECV_DATA_PACKET_SIZE_INVALID;
		goto err_exit;
	}

	curnode = find_node_by_id(con_id);
	if(curnode == NULL)
    {
		error_code = ERR_ATCMD_RECV_DATA_NODE_INVALID;
		goto err_exit;
	}

	if(curnode->protocol == NODE_MODE_TCP && curnode->role == NODE_ROLE_SERVER)
    {
		AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
			"ERROR: TCP Server must receive data from the seed");
		error_code = ERR_ATCMD_RECV_DATA_ROLE_INVALID;
		goto err_exit;
	}

	memset(rx_buffer, 0, MAX_BUFFER);
	error_code = atcmd_lwip_receive_data(curnode, rx_buffer, packet_size, &recv_size, udp_clientaddr, &udp_clientport);
    atcmd_rspinfor("1:%d", recv_size);
err_exit:
	if (error_code == ERR_NONE) {
        total_recv_size = recv_size;
        fetch_counter = 0;
        while (total_recv_size < packet_size) 
        { 
            atcmd_rspinfor("2:%d", total_recv_size);

            next_expected_size = packet_size - total_recv_size;
            if (next_expected_size > TCP_MSS) 
            {
                next_expected_size = TCP_MSS;   
            }
			vTaskDelay(10);
            error_code = atcmd_lwip_receive_data(curnode, rx_buffer + total_recv_size, next_expected_size, &recv_size, udp_clientaddr, &udp_clientport);
            fetch_counter = (recv_size == 0) ? (fetch_counter+1) : 0;
            if (fetch_counter >= 5) 
            {
                break;
            }
            total_recv_size += recv_size;
            if (error_code != 0) 
            {
                break;
            }
        }

		if (total_recv_size)
		{
			atcmd_rspinfor("+CIPRECVDATA,%d:", total_recv_size);
            atcmd_print_data(rx_buffer, total_recv_size);
            atcmd_rspinfor("\r\n");
		}

	} else {
        at_lwip_log_error(error_code, __FUNCTION__);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<con_id>,"
            "<len>(MAX %d)", 
            MAX_BUFFER);
    }

    if (error_code != ERR_NONE) {
        atcmd_rspinfor("[%s] ERROR:%d, con_id:%d\r\n", __FUNCTION__, error_code, con_id);
    } else {
        atcmd_rspinfor("[%s] sucess, con_id:%d", __FUNCTION__, con_id);
    }

    return ATCMD_OK;
}

/**************************************************************************
 * @brief 处理LWIP测试模式的AT命令
 * 
 * 该函数用于解析AT命令参数，根据参数值启用或禁用LWIP模块的测试模式。
 * 参数应包含一个整数值，非零表示启用测试模式，零表示禁用。
 * 
 * @param type 命令类型，当前未使用
 * @param arg 指向包含参数的字符串的指针
 * @return int 返回ATCMD_OK表示命令处理成功
**************************************************************************/
static int atcmd_lwip_test_mode(int type, char *arg)
{
    char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    int8_t argc_ok = false;

	int error_code = ERR_NONE;
	int enable = 0;

    AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, 
        "%s", __FUNCTION__);

    if (arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			switch (token_idx)
			{
				case 0:
				{
                    enable = atoi((char*)cur);
                    argc_ok = true;
					break;
				}

				default:
					error_code = ERR_ATCMD_PARAMETER_COUNT_EXCEED;
					goto err_exit;
			}
		} while(next);
	} else {
		error_code = ERR_ATCMD_PARAMETER_IS_NULL;
		goto err_exit;
	}

    if(argc_ok != true)
    {
        error_code = ERR_ATCMD_PARAMETER_COUNT_MISSING;
		goto err_exit;
    }

	if (enable) {
		if (atcmd_lwip_is_test_mode()) {
			AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS, "already enter auto receive mode");
		} else {
            atcmd_lwip_set_test_mode(true);
		}
	} else {
		if (atcmd_lwip_is_test_mode()) {
			atcmd_lwip_set_test_mode(false);
        } else {
			AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ALWAYS,"already leave auto receive mode");
		}
	}

err_exit:
	if (error_code != ERR_NONE) {
        at_lwip_log_error(error_code, __FUNCTION__);
        atcmd_rspinfor("[%s] ERROR:%d", __FUNCTION__, error_code);
        AT_DBG_MSG(AT_LOG_FLAG_LWIP, AT_LOG_LEVEL_ERROR,
            "Usage: =<enable/disable>");

    } else {
        atcmd_rspinfor("[%s] sucess, set:%d", __FUNCTION__, enable);
    }

    return ATCMD_OK;

}

const atcmd_item_t atcmd_lwip_table[] =
{
    {atcmd_ping,       		                "AT+PING", 		        "ping <IP or domain>\r\n"},
    {atcmd_server_create,       		    "AT+CIPSERVER", 		"AT+CIPSERVER=<mode>(TCP/UDP),<local_port>\r\n"},
    {atcmd_client_create,       		    "AT+CIPSTART", 		    "AT+CIPSTART=<mode>(TCP/UDP),<remote_ip>,<remote_port>,<local_port>\r\n"},
    {atcmd_send_data,       		        "AT+CIPSEND", 		    "AT+CIPSEND=<len>,<con_id>[,<udp_dst_ip>,<udp_dst_port>]:<data>\r\n"},
    {atcmd_close_connect,       		    "AT+CIPCLOSE", 		    "AT+CIPCLOSE=<con_id>\r\n"},
    {atcmd_auto_receive_data,       		"AT+CIPAUTORECV", 		"AT+CIPAUTORECV=<set_val>\r\n"},
    {atcmd_receive_data,       		        "AT+CIPRECVDATA", 		"AT+CIPRECVDATA=<con_id>,<read_size>\r\n"},
    {atcmd_lwip_test_mode,       		    "AT+CIPTESTMODE", 		"AT+CIPTESTMODE=<set_val>\r\n"},
};

/************************************************************************** 
 * @brief 注册LWIP相关的AT命令
 *
 * 该函数用于初始化节点池，创建主列表节点，并将LWIP相关的AT命令表添加到系统中。
 * 这样系统就可以识别和处理LWIP相关的AT命令。
 *
 * @return void
**************************************************************************/
void atcmd_lwip_register(void)
{
    init_node_pool();
	mainlist = create_node(-1,-1);
    atcmd_entry_add_table(atcmd_lwip_table, sizeof(atcmd_lwip_table)/sizeof(atcmd_item_t));
}

/**************************************************************************
 * @brief 打印LWIP相关AT命令的帮助信息
 * 
 * 该函数遍历LWIP相关的AT命令表，打印每个命令的名称和对应的帮助信息。
 * 帮助信息可以帮助用户了解每个AT命令的使用方法。
 * 
 * @return void
**************************************************************************/
void atcmd_lwip_help(void)
{
    int i;
    int item_len;
    item_len = sizeof(atcmd_lwip_table)/sizeof(atcmd_item_t);
    for (i = 0; i < item_len; i++)
        CLOGI("%s: %s\n", atcmd_lwip_table[i].atcmd_entry.name, atcmd_lwip_table[i].atcmd_entry.help);
}

