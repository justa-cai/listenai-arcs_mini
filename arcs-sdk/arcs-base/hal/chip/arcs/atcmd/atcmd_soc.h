#ifndef __SSC_CMD_SOC_H_
#define __SSC_CMD_SOC_H_

#define SSC_MAX_SOC_ALLOWED                 16

#define INVALID_SELECT_REASON -1

#define SSC_TCPIP_TMP_BUFF_SIZE             (1500)

enum soc_error_code {
    SSC_SOC_SUCCEED = 0,
    SSC_SOC_COMMON_ERROR,
    SSC_SOC_TABLE_FULL,
    SSC_SOC_ID_NOT_EXIST,
    SSC_SOC_UNSUPPORTED_TYPE,
    SSC_SOC_INVALID_TYPE,
    SSC_SOC_SEND_TOO_LONG,
    SSC_SOC_INVALID_PARAM,
    SSC_SOC_SEND_FAIL,
    SSC_SOC_OOM,
    SSC_SOC_ERROR_END,
};

enum soc_action {
    SSC_SOC_ACTION_NONE = 0,
    SSC_SOC_BIND,
    SSC_SOC_CONNECT,
    SSC_SOC_LISTEN,
    SSC_SOC_SEND_TO,
    SSC_SOC_SHUTDOWN,
    SSC_SOC_CLOSE,
    SSC_SOC_WORKTHREAD,
    SSC_SOC_INFO,
    SSC_SOC_GET_ADDR_INFO,
    SSC_SOC_SET_RECV_PRINT_FLAG,
    SSC_SOC_CONFIG_DATA_VALIDATION,
    SSC_SOC_CHANGE_BUFFER_SIZE,
    SSC_SOC_QUERY,
    SSC_SOC_CONFIG_SELECT_TIME,
    SSC_SOC_ABORT, // for espconn
    SSC_SOC_GET_OPTION,
    SSC_SOC_SET_OPTION,
    SSC_SOC_IGMP_JOIN,
    SSC_SOC_IGMP_HANDLE,
    SSC_SOC_END,
};


enum soc_type {
    SSC_SOC_TYPE_NONE = 0x00,
    SSC_SOC_UDP = 0x01,
    SSC_SOC_TCP = 0x02,
    SSC_SOC_SSL = 0x03,
    SSC_SOC_UDP_IPV6 = 0x04,
    SSC_SOC_TCP_IPV6 = 0x05,
    SSC_SOC_SERVER = 0x80,
};


enum soc_ssl_record_size {
    SSL_RECORD_SIZE_DEFAULT = 2048,
    SSL_RECORD_SIZE_512 = 512,
    SSL_RECORD_SIZE_1024 = 1024,
    SSL_RECORD_SIZE_2048 = 2048,
    SSL_RECORD_SIZE_4096 = 4096,
    SSL_RECORD_SIZE_8192 = 8192,
};

enum soc_query_option {
    SSC_SOC_QUERY_RECV_DATA = 1,
};


enum soc_validate_data_option {
    SSC_SOC_VALIDATE_DATA_NONE = 0x00,
    SSC_SOC_VALIDATE_DATA_RX = 0x01,
    SSC_SOC_VALIDATE_DATA_TX = 0x02,
};

enum ssc_ssl_context_type {
    SSC_SSL_CONTEXT_TYPE_CLIENT = 1,
    SSC_SSL_CONTEXT_TYPE_SERVER = 2,
};

enum ssc_ssl_context_action {
    SSC_SSL_CONTEXT_NONE,
    SSC_SSL_CONTEXT_INIT,
    SSC_SSL_CONTEXT_DEINIT,
    SSC_SSL_CONTEXT_QUERY,
    SSC_SSL_CONTEXT_END,
};

enum ssc_ssl_option {
    SSC_SSL_OPTION_NONE = 0x00,
    SSC_SSL_OPTION_VERIFY_SERVER = 0x01,
    SSC_SSL_OPTION_VERIFY_CLIENT = 0x02,
};

enum ssc_ssl_pki_type {
    SSC_SSL_PKI_X509_PEM = 0x0001,
    SSC_SSL_PKI_X509_DER = 0x0002,
    SSC_SSL_PKI_PKCS7 = 0x0004,
    SSC_SSL_PKI_PKCS8 = 0x0008,
    SSC_SSL_PKI_PKCS12 = 0x0010,
    SSC_SSL_PKI_ENCRYPTED = 0x0100,
};

enum ssc_ssl_version {
    SSC_SSLv23 = 0x1F,
    SSC_SSLv23_2 = 0x0E,  // current target ssl implementation do not support SSLv20 and TLSv12
    SSC_SSLv2_0 = 0x01,
    SSC_SSLv3_0 = 0x02,
    SSC_TLSv1_0 = 0x04,
    SSC_TLSv1_1 = 0x08,
    SSC_TLSv1_2 = 0x10,
};

enum ssc_ssl_error_code {
    SSC_SSL_ERROR_START = 0,
    SSC_SSL_ERROR_SET_FRAGMENT_SIZE = 1,
    SSC_SSL_ERROR_SET_CA = 2,
    SSC_SSL_ERROR_SET_CERT_KEY = 3,
};

enum ssc_ret {
    SSC_ERROR,
    SSC_OK,
};


struct test_param {
    int loop_back;   // if this socket is loopback
};

struct ip_total {
    uint32_t ip;  //ipv4
};


typedef struct {
    uint16_t len;
    uint16_t remote_port;
    int socket_id;
    uint32_t send_count;
    struct ip_total remote_ip;
    uint32_t interval;
    uint16_t rate;
} send_to_parameters;

typedef struct {
    int socket_id;
    struct ip_total remote_ip;
    uint16_t remote_port;
} connect_parameters;

typedef struct {
    int socket_id;
} listen_parameters;

typedef struct {
    uint32_t total_len; /* total struct len */
    uint32_t item_count;
} ssl_pki_array_header;


typedef struct {
    uint32_t len_type;  // high 16 bits, length; low 16 bits, type
    uint32_t *data;
} ssl_pki_item;


typedef struct {
    uint16_t item_count;
    ssl_pki_item *item_list;
} ssl_pki_item_list;

#endif /*__SSC_CMD_SOC_H_*/
