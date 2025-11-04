#include <stdbool.h>
#include <stdio.h>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <netdb.h>
#include "atcmd_socket_test.h"
#include "atcmd_tcpip.h"
#include  "rtos_al.h"
#include "atcmd.h"
#include "log_print.h"


#define INVALID_INDEX 0xFF

uint8_t recv_print_enable = 1;

extern ssc_mutex_t ssc_tcpip_mutex;
extern ssc_tcpip_entity g_ssc_tcpip_table[SSC_MAX_SOC_ALLOWED];

static bool noblk_used = false;
static uint8_t *tmp_buffer;
static uint16_t tmp_buffer_len;

#define is_server_type(type)    ((type) & SSC_SOC_SERVER)
#define mask_server_bit(type)   ((type) & (~SSC_SOC_SERVER))

#define BYTE_TO_BIT(byte)   (byte * 8)

typedef struct {
    int err;
    char *reason;
} ssc_soc_reason_t;

static ssc_soc_reason_t s_soc_reason[] = {
    {0,             "reason: other reason"},
    {ENOMEM,        "reason: out of memory"},
    {ENOBUFS,       "reason: buffer error"},
    {EWOULDBLOCK,   "reason: timeout, try again"},
    {EHOSTUNREACH,  "reason: routing problem"},
    {EINPROGRESS,   "reason: operation in progress"},
    {EINVAL,        "reason: invalid value"},
    {EADDRINUSE,    "reason: address in use"},
    {EALREADY,      "reason: conn already connected"},
    {EISCONN,       "reason: conn already established"},
    {ECONNABORTED,  "reason: connection aborted"},
    {ECONNRESET,    "reason: connection is reset"},
    {ENOTCONN,      "reason: connection closed"},
    {EIO,           "reason: invalid argument"},
    {-1,            "reason: low level netif error"}
};

char *ssc_tcpip_get_reason(int err)
{
    int i = 0;

    for (i = 0;  i < sizeof(s_soc_reason) / sizeof(ssc_soc_reason_t); i++) {
        if (err == s_soc_reason[i].err) {
            return s_soc_reason[i].reason;
        }
    }

    return s_soc_reason[0].reason;
}

void get_socket_error_code(int fd, int *result, const int select_reason, bool is_connecting)
{
    if (result != NULL && *result != 0) {
        if ((true == noblk_used) && (select_reason != INVALID_SELECT_REASON)) {
            *result = select_reason;
        } else {
            *result = errno;
        }
        // allow EINPROGRESS error in connecting
        if (is_connecting) {
            if (*result == EINPROGRESS ) {
                *result = 0;
            }
        }
        if (*result != ENOMEM && *result != ENOTCONN && *result != 0) {
            ssc_print("socket %d error %d reason: %s\n", fd, *result, ssc_tcpip_get_reason(*result));
        }
    }
}

void ssc_tcpip_init_internal(void)
{
    int i;

    int snd_buf = SSC_TCPIP_TMP_BUFF_SIZE;

    for (i = 0; i < SSC_MAX_SOC_ALLOWED; i++) {
        (g_ssc_tcpip_table[i]).socket_id = -1;
    }
    tmp_buffer = ssc_os_malloc(snd_buf);
    if (tmp_buffer == NULL) {
        // ssc_handle_malloc_fail();
    }
    tmp_buffer_len = snd_buf;

// #ifdef __SSC_SOCKET_NON_BLOCKING__
//     ssc_soc_config_testcase(true, false, 1, 0);
// #else
//     ssc_soc_config_testcase(false, false, 1, 0);
// #endif
}

ssc_tcpip_entity *find_entity(int socket_id)
{
    uint8_t i;
    uint8_t index = INVALID_INDEX;
    ssc_tcpip_entity *entity = NULL;

    ssc_tcpip_trace("enter find_entity,%d\n", socket_id);

    ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
    for (i = 0; i < SSC_MAX_SOC_ALLOWED; i++) {
        if ((g_ssc_tcpip_table[i]).socket_id == socket_id) {
            index = i;
            break;
        }
    }
    ssc_mutex_give(ssc_tcpip_mutex);
    if (index != INVALID_INDEX) {
        entity = &(g_ssc_tcpip_table[i]);
    }

    ssc_tcpip_trace("exit find_entity,%p\n", entity);
    return entity;
}


ssc_tcpip_entity *add_new_entity(int socket_id, uint8_t type, TaskHandle_t async_task_hd, void *content, struct test_param *param)
{
    ssc_tcpip_entity *entity = find_entity(-1);

    ssc_tcpip_trace("enter add_new_entity, %d\n", socket_id);

    if (!entity) {
        // ssc_print("max entity\n");
        return 0;
    }
    ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
    entity->socket_id = socket_id;
    entity->type = type;
    entity->async_task_hd = async_task_hd;
    entity->content = content;
    if (param != NULL) {
        ssc_os_memcpy( &(entity->param), param, sizeof(struct test_param));
    } else {
        bzero(&(entity->param), sizeof(struct test_param));
    }

    ssc_mutex_give(ssc_tcpip_mutex);

    ssc_tcpip_trace("exit add_new_entity\n");

    return entity;
}

ssc_tcpip_entity *get_entity(int socket_id)
{
    return find_entity(socket_id);
}


uint8_t update_entity(int socket_id, uint8_t type, TaskHandle_t async_task_hd, void *content)
{
    uint8_t ret = SSC_ERROR;
    ssc_tcpip_entity *entity = find_entity(socket_id);

    ssc_tcpip_trace("enter update_entity, %d\n", socket_id);

    if (entity != NULL) {
        ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
        entity->socket_id = socket_id;
        entity->type = type;
        entity->async_task_hd = async_task_hd;
        entity->content = content;
        ssc_mutex_give(ssc_tcpip_mutex);
    }
    ssc_tcpip_trace("exit update_entity, %d\n", ret);
    return ret;
}

static int ssc_socket_recvfrom(int s, void *mem, size_t len, int flags,
                                                 struct sockaddr *from, socklen_t *fromlen)
{
    // int ret, select_result;
     int ret = -1;
    // uint64_t time_start = 0;
    // uint64_t time_end = 0;
    // if (true == noblk_used) {
    //     fd_set rfds;
    //     fd_set exfds;
    //     struct timeval *tval = NULL;

    //     FD_ZERO(&rfds);
    //     FD_SET(s, &rfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }

    //     time_start = esp_timer_get_time();
    //     select_result = select(s + 1, &rfds, NULL, &exfds, tval);
    //     time_end = esp_timer_get_time();
    //     if (select_result <= 0) {
    //         if (select_result == 0) {
    //             ssc_print("+SELECT_TIMEOUT:%"PRIu32, (uint32_t)(time_end - time_start));
    //             errno = EWOULDBLOCK;
    //         }
    //         return -1;
    //     }

    //     if (FD_ISSET(s, &exfds) || !FD_ISSET(s, &rfds)) {
    //         return -1;
    //     }
    // }

    ret = recvfrom(s, mem, len, flags, from, fromlen);

    return ret;

}

static int ssc_socket_recv(int s, void *data, size_t size, int flags, int *tcp_select_reason)
{
    // int ret = -1, select_result;
    int ret = -1;

    // uint64_t time_start = 0;
    // uint64_t time_end = 0;
    // if (true == noblk_used) {
    //     fd_set rfds;
    //     fd_set exfds;
    //     struct timeval *tval = NULL;
    //     *tcp_select_reason = INVALID_SELECT_REASON;

    //     FD_ZERO(&rfds);
    //     FD_SET(s, &rfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }
    //     time_start = esp_timer_get_time();
    //     select_result = select(s + 1, &rfds, NULL, &exfds, tval);
    //     time_end = esp_timer_get_time();

    //     if (select_result <= 0) {

    //         if (select_result == 0) {
    //             ssc_print("+SELECT_TIMEOUT:%" PRIu32, (uint32_t)(time_end - time_start));
    //             if (!FD_ISSET(s, &exfds) || !FD_ISSET(s, &rfds)) { // If there is an error, can't set Error here.
    //                 errno = EWOULDBLOCK;
    //             }
    //         }
    //         return -1;
    //     }
    //     if (FD_ISSET(s, &exfds)) {
    //         ssc_print("The TCP connection was closed because of a link error\n"); // Turn off the TCP connection actively, and soc -T will have an error.
    //         u32_t optlen = sizeof(int);
    //         getsockopt(s, SOL_SOCKET, SO_ERROR, tcp_select_reason, &optlen);
    //         errno = ENOTCONN;
    //         return -1;
    //     }

    //     if (FD_ISSET(s, &rfds)) {
    //         ret = recv(s, data, size, flags);
    //         return ret;
    //     }
    // } else {
        ret = recv(s, data, size, flags);
    // }
    return ret;
}

static int ssc_socket_connect(int s, const struct sockaddr *name, socklen_t namelen, int *tcp_select_reason)
{
    int ret = -1;
    fd_set wfds;
    fd_set exfds;
    struct timeval *tval = NULL;
    *tcp_select_reason = INVALID_SELECT_REASON;

    ret = connect(s, name, namelen);
    // if (true == noblk_used) {
    //     get_socket_error_code(s, &ret, INVALID_SELECT_REASON, true);
    //     if (ret != 0 && ret != EINPROGRESS) {
    //         return -1;
    //     }

    //     FD_ZERO(&wfds);
    //     FD_SET(s, &wfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }

    //     if (select(s + 1, NULL, &wfds, &exfds, tval) <= 0) {
    //         return -1;
    //     }
    //     if (FD_ISSET(s, &exfds) || !FD_ISSET(s, &wfds)) {
    //         u32_t optlen = sizeof(int);
    //         getsockopt(s, SOL_SOCKET, SO_ERROR, tcp_select_reason, &optlen);
    //         return -1;
    //     }
    //     ret = 0;
    // }

    return ret;
}

static void receive_task(void *pvParameters)
{
    ssc_tcpip_entity *entity = (ssc_tcpip_entity *) pvParameters;
    bool close_flag = false;
    uint8_t type = entity->type;
    // uint8_t validate_data = entity->validate_data & SSC_SOC_VALIDATE_DATA_RX;
    // uint8_t level_option = entity->level_option;
    uint16_t recv_buffer_len;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    uint32_t recv_index = 0;
    int recv_result;
    int tcp_select_reason = INVALID_SELECT_REASON;
    int socket_id = entity->socket_id;
    struct sockaddr_in remote_addr;
    // uint8_t *recv_buffer_validate = NULL;
    uint8_t *recv_buffer;
    // fd_set rfds;
    // struct timeval tval;

    ssc_tcpip_trace("enter recv task\n");

    // if (validate_data && entity != NULL) {
    //     recv_buffer_validate = ssc_os_malloc(720);
    //     if (recv_buffer_validate == NULL) {
    //         ssc_handle_malloc_fail();
    //         goto NO_MEMORY;
    //     }
    //     recv_buffer = recv_buffer_validate;
    //     recv_buffer_len = 720;
    // } else {
        recv_buffer = tmp_buffer;
        recv_buffer_len = tmp_buffer_len;
    // }

    while (!close_flag) {
        switch (type) {
        case SSC_SOC_TCP:
            recv_result = ssc_socket_recv(socket_id, recv_buffer, recv_buffer_len, 0, &tcp_select_reason);
            if (recv_result > 0) {
                entity->recv_data_len += recv_result;
                //recv succeed
                if (recv_print_enable) {
                    ssc_print("+RECV:%d,%d", socket_id, recv_result);
                }
                // if (validate_data) {
                //     if (ssc_do_data_validation(recv_buffer, recv_result, recv_index) != true) {
                //         ssc_print("+DATA_ERROR:%d", socket_id);
                //     }
                //     recv_index += recv_result;
                // }
            } else {
                if (errno == EWOULDBLOCK) {
                    ssc_print("+SOCKET_TIMEOUT,CONTINUE_RECV\n");
                    continue;
                }
                //closed by remote side or error
                // get_socket_error_code(socket_id, &recv_result, tcp_select_reason, false);
                close_flag = true;
                ssc_print("+CLOSED:%d,%d\n", socket_id, recv_result);
            }
            break;
        case SSC_SOC_UDP:
            recv_result = ssc_socket_recvfrom(socket_id, recv_buffer, recv_buffer_len, 0,
                                              (struct sockaddr *)&remote_addr, &addr_len);
            if (recv_result >= 0) {
                entity->recv_data_len += recv_result;
                if (recv_print_enable) {
                    ssc_print("+RECVFROM:%d,%d,%s,%d\n", socket_id, recv_result,
                              inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));
                }
                // if (validate_data) {
                //     if (ssc_do_data_validation(recv_buffer, recv_result, recv_index) != true) {
                //         ssc_print("+DATA_ERROR:%d", socket_id);
                //     }
                //     recv_index += recv_result;
                // }
            } else {
                if (errno == EWOULDBLOCK) {
                    ssc_print("+SOCKET_TIMEOUT,CONTINUE_RECV\n");
                    continue;
                }
                get_socket_error_code(socket_id, &recv_result, INVALID_SELECT_REASON, false);

                ssc_tcpip_trace("recvfrom error, %d, %d", socket_id, recv_result);
                close_flag = true;
            }
            break;
        default:
            close_flag = true;
            break;
        }
    }

NO_MEMORY:
    ssc_tcpip_trace("recv end, %d\n", socket_id);
    // if (recv_buffer_validate != NULL) {
    //     ssc_os_free(recv_buffer_validate);
    // }
    ssc_tcpip_trace("exit recv task\n");

    vTaskDelete(NULL);
}

static void ssc_soc_connect_task(void *pvParameters)
{
    connect_parameters *param = (connect_parameters *)pvParameters;
    int tcp_select_reason = INVALID_SELECT_REASON;
    int connect_result = SSC_SOC_ID_NOT_EXIST;
    TaskHandle_t new_task_handle;
    ssc_tcpip_entity *entity = find_entity(param->socket_id);
    struct sockaddr_in sock_addr;
    //  printf("%s,%d param->socket_id is %d\n",__FUNCTION__,__LINE__,param->socket_id);

    // ssc_tcpip_trace("enter ssc_soc_connect_task\n");

    if ((param->remote_ip.ip == IPADDR_ANY) || param->remote_port == 0) {
        connect_result = SSC_SOC_INVALID_PARAM;
    } else if (entity != NULL) {
        switch (entity->type) {
        case SSC_SOC_TCP:
            ssc_os_memset(&sock_addr, 0, sizeof(sock_addr));
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = param->remote_ip.ip;
            sock_addr.sin_port = htons(param->remote_port);
            // printf("%s,%d\n",__FUNCTION__,__LINE__);
            connect_result = ssc_socket_connect(param->socket_id, (struct sockaddr *)&sock_addr, sizeof(sock_addr), &tcp_select_reason);
            if (connect_result < 0) {
                // printf("%s,%d\n",__FUNCTION__,__LINE__);
                get_socket_error_code(param->socket_id, &connect_result, tcp_select_reason, true);
            }
            break;
        default:
            connect_result = SSC_SOC_UNSUPPORTED_TYPE;
            break;
        }
    }

    if (connect_result == SSC_SOC_SUCCEED) {
        // ssc_os_sprintf(temp, "+CONNECT:%d,OK", param->socket_id);
        ssc_print("+CONNECT:%d,OK\n", param->socket_id);
        if (entity->start_workthread == 1) {
            xTaskCreate(receive_task, SSC_TCPIP_RECV_TASK_NAME,SSC_TCPIP_TASK_STACK_DEPTH, entity, SSC_TCPIP_RECV_TASK_PRIORITY, &new_task_handle);
            update_entity(param->socket_id, entity->type, new_task_handle, NULL);
        }
    } else {
        ssc_print("+CONNECT:%d,ERROR,%d\n", param->socket_id, connect_result);
        // ssc_os_sprintf(temp, "+CONNECT:%d,ERROR,%d", param->socket_id, connect_result);
    }
    ssc_tcpip_trace("exit ssc_soc_connect_task\n");

    free(param);
    vTaskDelete(NULL);
}


void ssc_soc_connect(int socket_id, struct ip_total *remote_ip, uint16_t remote_port)
{
    unsigned short stack_depth = SSC_TCPIP_TASK_STACK_DEPTH;
    connect_parameters *param = malloc(sizeof(connect_parameters));
    ssc_tcpip_entity *entity = find_entity(socket_id);

    // printf("enter ssc_soc_connect, %d, %lx, %u\n", socket_id, remote_ip->ip, remote_port);

    if (param != NULL && entity != NULL) { 
        param->socket_id = socket_id;
        param->remote_port = remote_port;
        param->remote_ip.ip = remote_ip->ip;
        // create new task for blocking operation
        if (pdPASS != xTaskCreate(ssc_soc_connect_task, SSC_TCPIP_CONNECT_TASK_NAME,
                                  stack_depth, param, SSC_TCPIP_TASK_PRIORITY, NULL)) {
            free(param);
            // ssc_handle_malloc_fail();
        }
    } else if (param != NULL) {
        free(param);
        ssc_print("+CONNECT:%d,ERROR,%d", socket_id, SSC_SOC_INVALID_PARAM);
    } else {
        //ssc_handle_malloc_fail();
    }

    ssc_tcpip_trace("exit ssc_soc_connect\n");
}

void ssc_soc_bind(uint8_t type, struct ip_total *local_ip, uint16_t local_port,
             uint8_t start_workthread, struct test_param *param)
{
    int ret = 0;
    int bind_result = SSC_SOC_TABLE_FULL;
    int socket_id = -1;
#ifdef __SSC_SET_REUSEADDR__
    int opt = 1;
#endif
    TaskHandle_t new_task_handle;
    ssc_tcpip_entity *entity;
    struct sockaddr_in sock_addr;
    ssc_tcpip_trace("enter ssc_soc_bind, %x, %lx, %u\n", type, local_ip->ip, local_port);
    switch (type) {
    case SSC_SOC_SSL:
    case SSC_SOC_TCP:
        socket_id = socket(AF_INET, SOCK_STREAM, 0);
        break;
    case SSC_SOC_UDP:
        socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        printf("%s,%d\n",__FUNCTION__,__LINE__);
        break;
    default:
        bind_result = SSC_SOC_UNSUPPORTED_TYPE;
        break;
    }

    //ret = ssc_socket_config(socket_id);

    if (!ret) {  // create socket succeed
        if (type == SSC_SOC_TCP || type == SSC_SOC_UDP) {
            ssc_os_memset(&sock_addr, 0, sizeof(sock_addr));
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = local_ip->ip;
            sock_addr.sin_port = htons(local_port);
            bind_result = bind(socket_id, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
            printf("%s,%d\n",__FUNCTION__,__LINE__);
        }
        //get_socket_error_code(socket_id, &bind_result, INVALID_SELECT_REASON, false);
        if (bind_result != 0) {
            closesocket(socket_id);
            socket_id = -1;
        } else {
            // bind succeed, add this socket to list
            entity = add_new_entity(socket_id, type, 0, NULL, param);

            if (entity == NULL) {
                bind_result = SSC_SOC_TABLE_FULL;
                closesocket(socket_id);
            } else {
                ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
                entity->start_workthread = start_workthread;
                ssc_mutex_give(ssc_tcpip_mutex);


                if (type == SSC_SOC_UDP && start_workthread == 1) {
                    xTaskCreate(receive_task, SSC_TCPIP_RECV_TASK_NAME,
                                SSC_TCPIP_TASK_STACK_DEPTH,
                                entity, SSC_TCPIP_RECV_TASK_PRIORITY, &new_task_handle);
                    update_entity(socket_id, type, new_task_handle, NULL);
                    printf("%s,%d\n",__FUNCTION__,__LINE__);
                }
            }
        }
    } else {
        closesocket(socket_id);
        socket_id = -1;
    }
    if (bind_result == SSC_SOC_SUCCEED) {
        if (type == SSC_SOC_TCP || type == SSC_SOC_UDP) {
            printf("%s,%d\n",__FUNCTION__,__LINE__);
            ssc_print("+BIND:%d,OK,%s,%d\n", socket_id, inet_ntoa(local_ip->ip), local_port);
        }
    } else {
        ssc_print("+BIND:ERROR,%d", bind_result);
    }
}

static void ssc_soc_close_one(ssc_tcpip_entity *entity)
{
    int close_result = SSC_SOC_ID_NOT_EXIST;
    int socket_id = entity->socket_id;

    if (entity->socket_id != -1) {
        ssc_tcpip_trace("close one socket, %d, %u, %p, %p\n", socket_id, entity->type,
                        entity->async_task_hd, entity->content);

        switch (mask_server_bit(entity->type)) {
        case SSC_SOC_UDP:
        case SSC_SOC_TCP:
            close_result = closesocket(entity->socket_id);
            get_socket_error_code(socket_id, &close_result, INVALID_SELECT_REASON, false);
            break;
        default:
            close_result = SSC_SOC_UNSUPPORTED_TYPE;
            break;
        }

        ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
        ssc_os_memset(entity, 0, sizeof(ssc_tcpip_entity));
        entity->socket_id = -1;
        ssc_mutex_give(ssc_tcpip_mutex);

        if (close_result == SSC_SOC_SUCCEED) {
            ssc_print("+CLOSE:%d,OK\n", socket_id);
        } else {
            ssc_print("+CLOSE:%d,ERROR,%d\n", socket_id, close_result);
        }
    } else {
        ssc_print("+CLOSE:%d,ERROR,%d\n", socket_id, close_result);
    }
}

static void ssc_soc_close_task(void *pvParameters)
{
    connect_parameters *param = (connect_parameters *)pvParameters;
    uint8_t i;
    int socket_id = param->socket_id;
    ssc_tcpip_entity *entity;
    ssc_tcpip_trace("enter ssc_soc_close, %d\n", socket_id);

    if (socket_id < 0) {  //close all
        for (i = 0; i < SSC_MAX_SOC_ALLOWED; i++) {
            entity = &(g_ssc_tcpip_table[i]);
            if (entity->socket_id != -1) {
                ssc_soc_close_one(entity);
            }
        }
        ssc_print("+CLOSEALL\n");
    } else {
        entity = find_entity(socket_id);
        if (entity != NULL) {
            ssc_soc_close_one(entity);
        }
    }

    free(param);
    vTaskDelete(NULL);
}

void ssc_soc_close(int socket_id, bool abort)
{
    // uint8_t i;
    // ssc_tcpip_entity *entity;
    // ssc_tcpip_trace("enter ssc_soc_close, %d\n", socket_id);

    // if (socket_id < 0) {  //close all
    //     for (i = 0; i < SSC_MAX_SOC_ALLOWED; i++) {
    //         entity = &(g_ssc_tcpip_table[i]);
    //         if (entity->socket_id != -1) {
    //             ssc_soc_close_one(entity);
    //         }
    //     }
    //     ssc_print("+CLOSEALL\n");
    // } else {
    //     entity = find_entity(socket_id);
    //     if (entity != NULL) {
    //         ssc_soc_close_one(entity);
    //     }
    // }

    unsigned short stack_depth = 512;
    connect_parameters *param = malloc(sizeof(connect_parameters));

    if (param != NULL) { 
        param->socket_id = socket_id;
        // create new task for close operation
        if (pdPASS != xTaskCreate(ssc_soc_close_task, SSC_TCPIP_CLOSE_TASK_NAME,
                                  stack_depth, param, SSC_TCPIP_TASK_PRIORITY, NULL)) {
            free(param);
            // ssc_handle_malloc_fail();
        }
    } else {
        //ssc_handle_malloc_fail();
    }
    ssc_tcpip_trace("exit ssc_soc_close\n");
}

static int ssc_socket_send(int s, const void *data, size_t size, int flags, int *tcp_select_reason)
{
    int ret = -1;

    // if (true == noblk_used) {
    //     fd_set wfds;
    //     fd_set exfds;
    //     struct timeval *tval = NULL;
    //     *tcp_select_reason = INVALID_SELECT_REASON;

    //     FD_ZERO(&wfds);
    //     FD_SET(s, &wfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }

    //     ret = send(s, data, size, flags);
    //     if (ret > 0) {          //send ok
    //         return ret;
    //     } else {                             //send fail
    //         if (errno == EWOULDBLOCK) {
    //             if (select(s + 1, NULL, &wfds, &exfds, tval) <= 0) {
    //                 return -1;
    //             }
    //             if (FD_ISSET(s, &exfds) || !FD_ISSET(s, &wfds)) {
    //                 u32_t optlen = sizeof(int);
    //                 getsockopt(s, SOL_SOCKET, SO_ERROR, tcp_select_reason, &optlen);
    //                 return -1;
    //             }
    //         } else {   // if errno != EWOULDBLOCK.return ret
    //             return ret;
    //         }
    //     }
    // }

    ret = send(s, data, size, flags);

    return ret;
}

static int ssc_socket_sendto(int s, const void *data, size_t size, int flags,
                                               const struct sockaddr *to, socklen_t tolen)
{
    int ret = -1;

    // if (true == noblk_used) {
    //     fd_set wfds;
    //     fd_set exfds;
    //     struct timeval *tval = NULL;

    //     FD_ZERO(&wfds);
    //     FD_SET(s, &wfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }

    //     if (select(s + 1, NULL, &wfds, &exfds, tval) <= 0) {
    //         return -1;
    //     }

    //     if (FD_ISSET(s, &exfds) || !FD_ISSET(s, &wfds)) {
    //         return -1;
    //     }
    // }

    ret = sendto(s, data, size, flags, to, tolen);
    return ret;
}

static void ssc_soc_send_to_task(void *pvParameters)
{
    send_to_parameters *param = (send_to_parameters *) pvParameters;
    char temp[32];
    // uint8_t level_option;
    uint8_t type;
    uint32_t i;
    uint32_t send_index = 0;
    int send_result = SSC_SOC_ID_NOT_EXIST;
    int tcp_select_reason = INVALID_SELECT_REASON;
    int send_len = -1;
    int to_send = -1;
    int flag = 0;
    int delay = param->interval / portTICK_PERIOD_MS;
    uint8_t *send_buffer;
    uint32_t total_slen_bit = 0;
    uint32_t old_time = 0;
    uint32_t now_time = 0;
    uint32_t tol_length_sec = (param->rate * 1024 * 1024);

    ssc_tcpip_entity *entity = find_entity(param->socket_id);
    struct sockaddr_in sock_addr;
    // fd_set wfds;
    // struct timeval tval;

    if (param->len > tmp_buffer_len) {
        send_result = SSC_SOC_SEND_TOO_LONG;
    } else if (entity != NULL) {
        type = entity->type;
        // level_option = entity->level_option;
        send_buffer = tmp_buffer;

        if (!send_buffer) {
            ssc_print("ssc_soc_send_to_task no m\n");
            goto NO_MEMORY;
        }

        switch (type) {
        case SSC_SOC_UDP:
        case SSC_SOC_UDP_IPV6:
            if (type == SSC_SOC_UDP) {
                if (param->remote_ip.ip == IPADDR_ANY || param->remote_port == 0) {
                    send_result = SSC_SOC_INVALID_PARAM;
                } else {
                    ssc_os_memset(&sock_addr, 0, sizeof(sock_addr));
                    sock_addr.sin_family = AF_INET;
                    sock_addr.sin_addr.s_addr = param->remote_ip.ip;
                    sock_addr.sin_port = htons(param->remote_port);
                    send_result = SSC_SOC_SUCCEED;
                }
            }
            for (i = 0; i < param->send_count; i++) {
#ifdef __SSC_SOCKET_UDP_TX_FLOW_CTRL__
_try_again:
#endif
                if (delay > 0) {
                    vTaskDelay(param->interval / portTICK_PERIOD_MS);
                }

                if (type == SSC_SOC_UDP) {
                    send_len = ssc_socket_sendto(param->socket_id, send_buffer, param->len, flag,
                                                 (struct sockaddr *)&sock_addr, sizeof(sock_addr));
                }
                total_slen_bit += BYTE_TO_BIT(send_len);
                if (send_len != param->len) {
                    send_result = SSC_SOC_SEND_FAIL;
                    get_socket_error_code(param->socket_id, &send_result, INVALID_SELECT_REASON, false);
#ifdef __SSC_SOCKET_UDP_TX_FLOW_CTRL__
                    if (send_result == ENOMEM) {
                        send_result = SSC_SOC_SUCCEED;
                        /* delay one tick use esp_rom_delay_us here, because it will cause taskwdt */
                        vTaskDelay(1);
                        goto _try_again;
                    }
#endif
                    ssc_tcpip_trace("udp send fail %d, %d\n", send_len, param->len);
                    break;
                }

                // if (param->rate) {
                //     ssc_soc_send_limit_rate(&old_time, &now_time, &total_slen_bit, tol_length_sec);
                // }
            }
            break;
        case SSC_SOC_TCP:
        case SSC_SOC_TCP_IPV6:
            send_result = SSC_SOC_SUCCEED;
            for (i = 0; i < param->send_count; i++) {
                if (delay > 0) {
                    vTaskDelay(param->interval / portTICK_PERIOD_MS);
                }

                if (send_result == SSC_SOC_SUCCEED) {
                    to_send = (int) param->len;
                    while (1) {
                        if (type == SSC_SOC_TCP || type == SSC_SOC_TCP_IPV6) {
                            send_len = ssc_socket_send(param->socket_id, &(send_buffer[param->len - to_send]), to_send, 0, &tcp_select_reason);
                        }
                        total_slen_bit += BYTE_TO_BIT(send_len);
                        if (send_len == to_send) {
                            //succeed
                            ssc_tcpip_trace("tcp/ssl send succeed %d bytes\n", send_len);
                            break;
                        } else if (send_len > 0 && send_len < to_send) {
                            to_send -= send_len;
                            // delay 20 ms as send blocks
                            // vTaskDelay(20/portTICK_PERIOD_MS);
                        } else {
                            send_result = SSC_SOC_SEND_FAIL;
                            get_socket_error_code(param->socket_id, &send_result, tcp_select_reason, false);
                            ssc_tcpip_trace("tcp send fail, %d\n", send_len);
                            break;
                        }
                    }

                    // if (param->rate) {
                    //     ssc_soc_send_limit_rate(&old_time, &now_time, &total_slen_bit, tol_length_sec);
                    // }
                } else {
                    //send fail, break;
                    break;
                }
            }
            break;
        default:
            send_result = SSC_SOC_UNSUPPORTED_TYPE;
            break;
        }
    }

    if (send_result != SSC_SOC_SUCCEED) {
        ssc_print("+SEND:%d,ERROR,%d\n", param->socket_id, send_result);
    } else {
        ssc_print("+SEND:%d,OK\n", param->socket_id);
    }

NO_MEMORY:
    ssc_os_free(param);
    ssc_tcpip_trace("exit ssc_soc_send_to_task\n");

    vTaskDelete(NULL);
}

void ssc_soc_send_to(int socket_id, uint16_t len, uint32_t send_count, struct ip_total *remote_ip, uint16_t remote_port, uint32_t interval, uint16_t rate)
{
    unsigned short stack_depth = SSC_TCPIP_TASK_STACK_DEPTH;
    send_to_parameters *param = ssc_os_malloc(sizeof(send_to_parameters));
    ssc_tcpip_entity *entity = find_entity(socket_id);
    ssc_tcpip_trace("enter ssc_soc_send_to, %d, %u, %lu, %lu, %u, %lu, %u\n",
                    socket_id, len, send_count, remote_ip->ip, remote_port, interval, rate);

    if (param != NULL && entity != NULL) {
        param->socket_id = socket_id;
        param->len = len;
        param->send_count = send_count;
        param->remote_port = remote_port;
        param->interval = interval;
        param->rate = rate;

        if (remote_ip->ip) {
            param->remote_ip.ip = remote_ip->ip;
        }
        // create new task for blocking operation
        if (pdPASS != xTaskCreate(ssc_soc_send_to_task, SSC_TCPIP_SEND_TASK_NAME,
                                  stack_depth, param, SSC_TCPIP_TASK_PRIORITY, NULL)) {
            ssc_os_free(param);
            // ssc_handle_malloc_fail();
        }
    } else if (param != NULL) {
        ssc_print("+SEND:%d,ERROR,%d", param->socket_id, SSC_SOC_ID_NOT_EXIST);
        ssc_os_free(param);
    } else {
        // ssc_handle_malloc_fail();
    }

    ssc_tcpip_trace("exit ssc_soc_send_to\n");
}

static int ssc_socket_accept(int s, struct sockaddr *addr, socklen_t *addrlen, int *tcp_select_reason)
{
    int new_s = -1;

    // if (true == noblk_used) {
    //     fd_set rfds;
    //     fd_set exfds;
    //     struct timeval *tval = NULL;
    //     *tcp_select_reason = INVALID_SELECT_REASON;

    //     FD_ZERO(&rfds);
    //     FD_SET(s, &rfds);

    //     FD_ZERO(&exfds);
    //     FD_SET(s, &exfds);

    //     if (true == timeout_used) {
    //         tval = &tmp_timeout;
    //     }

    //     if (select(s + 1, &rfds, NULL, &exfds, tval) <= 0) {
    //         return -1;
    //     }
    //     if (FD_ISSET(s, &exfds) || !FD_ISSET(s, &rfds)) {
    //         u32_t optlen = sizeof(int);
    //         getsockopt(s, SOL_SOCKET, SO_ERROR, tcp_select_reason, &optlen);
    //         return -1;
    //     }
    // }
    new_s = accept(s, addr, addrlen);
    if (new_s < 0) {
        return -1;
    }

    // if (ssc_socket_config(new_s)) {
    //     closesocket(new_s);
    //     new_s = -1;
    // }

    return new_s;
}

static void listen_task(void *pvParameters)
{
    listen_parameters *listen_param = (listen_parameters *)pvParameters;
    ssc_tcpip_entity *entity = find_entity(listen_param->socket_id);
    ssc_tcpip_entity *new_sock_entity;
    bool close_flag = false;
    int tcp_select_reason = INVALID_SELECT_REASON;
    uint8_t type = entity->type;
    //uint8_t level_option = entity->level_option;
    socklen_t addr_len = sizeof(struct sockaddr);

    TaskHandle_t new_task_handle;
    int socket_id = entity->socket_id;
    int new_socket_id;
    struct sockaddr_in remote_addr;

    // int maxfd = socket_id;
    ssc_tcpip_trace("enter listen task\n");

    while (!close_flag && is_server_type(type)) {
        if (mask_server_bit(type) == SSC_SOC_TCP) {
            new_socket_id = ssc_socket_accept(socket_id, (struct sockaddr *)&remote_addr, &addr_len, &tcp_select_reason);
            switch (mask_server_bit(type)) {
            case SSC_SOC_TCP:
                if (new_socket_id >= 0) {
                    ssc_print("+ACCEPT:%d,%d,%s,%d\n", new_socket_id,
                              socket_id,
                              inet_ntoa(remote_addr.sin_addr),
                              htons(remote_addr.sin_port));
                    // create recv task add new socket to global list
                    new_sock_entity = add_new_entity(new_socket_id, SSC_SOC_TCP, 0, NULL, NULL);
                    xTaskCreate(receive_task, SSC_TCPIP_RECV_TASK_NAME,
                                SSC_TCPIP_TASK_STACK_DEPTH,
                                new_sock_entity, SSC_TCPIP_TASK_PRIORITY, &new_task_handle);
                    update_entity(new_socket_id, SSC_SOC_TCP, new_task_handle, NULL);
                } else {
                    // CLI_LOGD("%s,%d\n",__FUNCTION__,__LINE__);
                    ssc_print("+ACCEPT:%d,ERROR\n", socket_id);
                    ssc_tcpip_trace("accept fail, %d\n", (0 - errno));
                    close_flag = true;
                }
                break;
            default:
                break;
            }
        }
        else {
            break;
        }
    }
    // CLI_LOGD("%s,%d\n",__FUNCTION__,__LINE__);
    ssc_tcpip_trace("listen end\n");
    ssc_tcpip_trace("exit listen task\n");
    ssc_os_free(listen_param);
    vTaskDelete(NULL);
}

void ssc_soc_listen(int socket_id)
{
    char temp[32];
    int listen_result = SSC_SOC_ID_NOT_EXIST;
    TaskHandle_t new_task_handle;
    listen_parameters *param = ssc_os_zalloc(sizeof(listen_parameters));
    ssc_tcpip_entity *entity = find_entity(socket_id);

    if (param == NULL) {
        return;
    }

    ssc_tcpip_trace("enter ssc_soc_listen, %d\n", socket_id);
    if (entity != NULL) {
        switch (entity->type) {
        case SSC_SOC_TCP:
        case SSC_SOC_TCP_IPV6:
            param->socket_id = socket_id;
            listen_result = listen(socket_id, SSC_TCPIP_LISTEN_BACKLOG);
            get_socket_error_code(socket_id, &listen_result, INVALID_SELECT_REASON, false);
            break;
        default:
            listen_result = SSC_SOC_UNSUPPORTED_TYPE;
            break;
        }
    }

    if (listen_result == SSC_SOC_SUCCEED) {
        update_entity(socket_id, (entity->type | SSC_SOC_SERVER), NULL, NULL);
        ssc_print("+LISTEN:%d,OK", socket_id);
        if (entity->start_workthread == 1) {
            if (pdPASS == xTaskCreate(listen_task, SSC_TCPIP_LISTEN_TASK_NAME,
                                      SSC_TCPIP_TASK_STACK_DEPTH,
                                      param, SSC_TCPIP_TASK_PRIORITY, &new_task_handle)) {
                update_entity(socket_id, entity->type, new_task_handle, NULL);
            } else {
                ssc_os_free(param);
                // ssc_handle_malloc_fail();
            }
        }
    } else {
        ssc_os_free(param);
        ssc_print("+LISTEN:%d,ERROR,%d", socket_id, listen_result);
    }
    ssc_tcpip_trace("exit ssc_soc_listen\n");
}


static void ssc_soc_one_info(ssc_tcpip_entity *entity)
{
    char remote_addr_low[SSC_IPV6_ADDR_STR_MAX_LEN];
    uint8_t type;
    socklen_t len;
    int socket_id;
    int result = SSC_SOC_COMMON_ERROR;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    if (entity != NULL) {

        ssc_mutex_take(ssc_tcpip_mutex, portMAX_DELAY);
        socket_id = entity->socket_id;
        type = entity->type;
        ssc_mutex_give(ssc_tcpip_mutex);

        if (socket_id != -1) {
            //get info
            len = sizeof(struct sockaddr_in);
            ssc_os_memset(&local_addr, 0, sizeof(struct sockaddr_in));
            result = getsockname(socket_id, (struct sockaddr *)&local_addr, &len);
            if (result == SSC_SOC_SUCCEED && (type == (SSC_SOC_TCP | SSC_SOC_SERVER) || type == (SSC_SOC_UDP | SSC_SOC_SERVER)
                                              || type == (SSC_SOC_TCP) || type == (SSC_SOC_UDP))) {
                ssc_print("+SOCINFO:%d,%x,%s,%d\n", socket_id, type, inet_ntoa(local_addr.sin_addr), htons(local_addr.sin_port));
                if ((type & SSC_SOC_SERVER) == 0 && type != SSC_SOC_UDP) {
                    ssc_os_memset(&remote_addr, 0, sizeof(struct sockaddr_in));
                    result = getpeername(socket_id, (struct sockaddr *)&remote_addr, &len);
                    if (result == SSC_SOC_SUCCEED) {
                        ssc_os_strcpy(remote_addr_low, inet_ntoa(remote_addr.sin_addr));
                        ssc_print("+SOCINFO:%d,%x,%s,%d,%s,%d\n", socket_id, type, inet_ntoa(local_addr.sin_addr),
                                  htons(local_addr.sin_port), remote_addr_low, htons(remote_addr.sin_port));
                    }
                } else {
                    ssc_print("+SOCINFO:%d,%x,%s,%d\n", socket_id, type, inet_ntoa(local_addr.sin_addr), htons(local_addr.sin_port));
                }
            }
        }
    }
}

void ssc_soc_info(int socket_id)
{

    uint8_t i;
    ssc_tcpip_entity *entity;

    ssc_tcpip_trace("enter ssc_soc_info, %d\n", socket_id);

    if (socket_id == -1) {
        for (i = 0; i < SSC_MAX_SOC_ALLOWED; i++) {
            entity = &(g_ssc_tcpip_table[i]);
            ssc_soc_one_info(entity);
        }
    } else {
        entity = find_entity(socket_id);
        ssc_soc_one_info(entity);
    }
    ssc_print("+SOCINFOALL");

    ssc_tcpip_trace("exit ssc_soc_info\n");
}


static void ssc_soc_getaddrinfo_task(void *pvParameters)
{
    char *domain = pvParameters;
    int result = SSC_SOC_INVALID_PARAM;

    char ip4_addr[32];
    struct sockaddr_in *sockaddr_ipv4;
    struct addrinfo *answer, hint;

    bzero(&hint, sizeof(hint));
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    result = getaddrinfo((const char *)domain, NULL, &hint, &answer);
    if (result != SSC_SOC_SUCCEED) {
        ssc_print("+HOSTIP:ERROR,%d", result);
    } else {
        sockaddr_ipv4 = (struct sockaddr_in *)answer->ai_addr;
        inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ip4_addr, sizeof(ip4_addr));
        ssc_print("+HOSTIP:OK,%s", ip4_addr);
        freeaddrinfo(answer);
    }
    ssc_tcpip_trace("exit ssc_soc_getaddrinfo_task\n");
    ssc_os_free(domain);
    vTaskDelete(NULL);
}


void ssc_soc_getaddrinfo(char *domain)
{
    char *param;
    ssc_tcpip_trace("enter ssc_soc_gethostbyname\n");
    param = ssc_os_malloc(ssc_os_strlen(domain) + 1);

    if (!param) {
        return;
    }

    ssc_os_strcpy(param, domain);

    // create new task for blocking operation
    if (pdPASS != xTaskCreate(ssc_soc_getaddrinfo_task, SSC_TCPIP_DNS_TASK_NAME,
                              SSC_TCPIP_TASK_STACK_DEPTH, param, SSC_TCPIP_TASK_PRIORITY, NULL)) {
        ssc_os_free(param);
    }
    ssc_tcpip_trace("exit ssc_soc_gethostbyname\n");
}