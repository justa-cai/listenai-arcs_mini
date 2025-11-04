#ifndef __SSC_SOCKET_H__
#define __SSC_SOCKET_H__

#include "atcmd_soc.h"

#define SSC_TCPIP_LISTEN_TASK_NAME          "SscTcpipListen"
#define SSC_TCPIP_RECV_TASK_NAME            "SscTcpipRecv"
#define SSC_TCPIP_SERVER_TASK_NAME          "SscTcpipServer"
#define SSC_TCPIP_CONNECT_TASK_NAME         "SscTcpipConnect"
#define SSC_TCPIP_CLOSE_TASK_NAME           "SscTcpipClose"
#define SSC_TCPIP_SEND_TASK_NAME            "SscTcpipSend"
#define SSC_TCPIP_DNS_TASK_NAME             "SscTcpipDns"

// #define SSC_TCPIP_TASK_STACK_DEPTH          (2048+256)

#define SSC_TCPIP_TASK_STACK_DEPTH          (1024+256)

#define SSC_TCPIP_TASK_PRIORITY               (3)
#define SSC_TCPIP_RECV_TASK_PRIORITY          (3)
#define SSC_TCPIP_TASK_SEND_PRIORITY          (3)

#define SSC_TCPIP_LISTEN_BACKLOG              5

typedef struct {
    uint8_t level_option;
    uint8_t type;
    uint8_t validate_data;
    uint8_t start_workthread;
    uint32_t recv_data_len;
    TaskHandle_t async_task_hd;
    int socket_id;
    void *content;
    struct test_param param;
} ssc_tcpip_entity;

typedef union {
    int val;
    struct linger linger;
    struct timeval tv;
    unsigned char str[16];
} ssc_soc_optval;

typedef enum {
    VALINT,
    VALLINGER,
    VALTIMEVAL,
    VALUCHAR,
    VALMAX,
} ssc_soc_opt_val_type;

void ssc_soc_config_select_time(uint32_t timeout);
void ssc_soc_setopt(int socket_id, char *opt_name, void *p);
void ssc_soc_getopt(int socket_id, const char *opt_name);
void ssc_tcpip_init_internal(void);
void ssc_soc_bind(uint8_t type, struct ip_total *local_ip, uint16_t local_port,
                  uint8_t start_workthread, struct test_param *param);
void ssc_soc_connect(int socket_id, struct ip_total *remote_ip, uint16_t remote_port);
void ssc_soc_listen(int socket_id);
void ssc_soc_send_to(int socket_id, uint16_t len, uint32_t send_count,
                     struct ip_total *remote_ip, uint16_t remote_port, uint32_t interval, uint16_t rate);
void ssc_soc_shutdown(int socket_id, char how);
void ssc_soc_close(int socket_id, bool abort);
void ssc_soc_work_thread(int socket_id, uint8_t option);
void ssc_soc_info(int socket_id);
void ssc_soc_getaddrinfo(char *domain);
void ssc_soc_config_data_validation(int socket_id, uint8_t option);
void ssc_soc_query(int socket_id, uint8_t option);
bool ssc_ssl_init_context(int16_t cert_id, int16_t private_key_id, int16_t ca_id,
                          uint8_t type, uint8_t version, uint8_t option, uint16_t record_frag_size);
bool ssc_ssl_deinit_context(void);
void ssc_soc_change_buffer_size(uint16_t len);

#endif /* __SSC_SOCKET_H__ */
