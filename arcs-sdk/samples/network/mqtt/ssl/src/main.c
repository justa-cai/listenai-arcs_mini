#include "lisa_thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "mqtt-ssl-test"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "user_fs.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"

#include "core_mqtt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* mbedTLS 头文件 */
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

// ============================================================
// 重要:使用前请修改以下配置!
// ============================================================
#define MQTT_BROKER_HOST   "broker.emqx.io"
#define MQTT_BROKER_PORT   8883                    // SSL端口
#define MQTT_CLIENT_ID     "arcs_mqtt_ssl_client"

#define TARGET_WIFI_SSID   "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD    "a12345678"

#define MQTT_TOPIC_PUB     "arcs/test/pub"
#define MQTT_TOPIC_SUB     "arcs/test/sub"

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

#define NETWORK_BUFFER_SIZE 2048

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

/* SSL/TLS 上下文 */
struct NetworkContext {
    mbedtls_net_context server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
};

/* MQTT 缓冲区 */
static uint8_t g_network_buffer[NETWORK_BUFFER_SIZE];

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;
    if (!mac_addr)
        return -1;
    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret,
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                  uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success: IP=%d.%d.%d.%d",
             ip_addr & 0xff, (ip_addr >> 8) & 0xff,
             (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
        g_get_ip_success = true;
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;
    LOGI("WiFi connection status: %d", connection_info->status);

    switch (connection_info->status) {
    case WIFI_MGR_STA_CONNECTED:
        LOGI("WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
        break;
    case WIFI_MGR_STA_DISCONNECTED:
        LOGI("WiFi disconnected from AP");
        g_wifi_connected = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;
    default:
        break;
    }
}

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };
    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                      mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LOGI("mac_manager_init failed\n");
    }
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,
        .pwd = TARGET_WIFI_PWD,
    };
    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
    LOGI("WiFi auto-connect started");
}

static int wait_for_wifi_connection(void)
{
    int timeout = 30;
    LOGI("Waiting for WiFi connection and IP address...");

    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained");
            vTaskDelay(pdMS_TO_TICKS(500));
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if (timeout % 5 == 0) {
            LOGI("Waiting... (WiFi:%d, IP:%d, timeout:%d)", g_wifi_connected, g_get_ip_success, timeout);
        }
    }
    LOGI("WiFi connection timeout");
    return -1;
}

/* mbedTLS debug 回调 */
static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void) level);
    LOGI("%s:%04d: %s", file, line, str);
}

/* Transport 接口实现 (SSL版本) */
static int32_t transport_recv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    int ret = mbedtls_ssl_read(&pNetworkContext->ssl, pBuffer, bytesToRecv);

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return 0;  /* 非阻塞,需要重试 */
    }

    if (ret < 0) {
        LOGE("mbedtls_ssl_read failed: -0x%04x", -ret);
        return -1;
    }

    return ret;
}

static int32_t transport_send(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    int ret = mbedtls_ssl_write(&pNetworkContext->ssl, pBuffer, bytesToSend);

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return 0;  /* 非阻塞,需要重试 */
    }

    if (ret < 0) {
        LOGE("mbedtls_ssl_write failed: -0x%04x", -ret);
        return -1;
    }

    return ret;
}

static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* MQTT 事件回调 */
static void mqtt_event_callback(MQTTContext_t *pContext, MQTTPacketInfo_t *pPacketInfo,
                                 MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
        LOGI("Received PUBLISH: topic=%.*s, payload=%.*s",
             (int)pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
             (int)pPublishInfo->payloadLength, (char *)pPublishInfo->pPayload);
    } else if (pPacketInfo->type == MQTT_PACKET_TYPE_SUBACK) {
        LOGI("Received SUBACK");
    } else if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBACK) {
        LOGI("Received PUBACK");
    }
}

static int connect_to_broker_ssl(struct NetworkContext *pNetworkContext)
{
    int ret;
    char port_str[8];

    LOGI("Initializing SSL/TLS connection to %s:%d...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    /* 1. 初始化 mbedTLS 结构 */
    mbedtls_net_init(&pNetworkContext->server_fd);
    mbedtls_ssl_init(&pNetworkContext->ssl);
    mbedtls_ssl_config_init(&pNetworkContext->conf);
    mbedtls_x509_crt_init(&pNetworkContext->cacert);
    mbedtls_ctr_drbg_init(&pNetworkContext->ctr_drbg);
    mbedtls_entropy_init(&pNetworkContext->entropy);

    /* 2. 初始化随机数生成器 */
    const char *pers = "mqtt_ssl_client";
    ret = mbedtls_ctr_drbg_seed(&pNetworkContext->ctr_drbg, mbedtls_entropy_func,
                                  &pNetworkContext->entropy, (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        LOGE("mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        return -1;
    }

    /* 3. 连接到服务器 */
    snprintf(port_str, sizeof(port_str), "%d", MQTT_BROKER_PORT);
    ret = mbedtls_net_connect(&pNetworkContext->server_fd, MQTT_BROKER_HOST,
                               port_str, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        LOGE("mbedtls_net_connect failed: -0x%04x", -ret);
        return -1;
    }

    LOGI("TCP connection established");

    /* 4. 设置非阻塞模式 */
    ret = mbedtls_net_set_nonblock(&pNetworkContext->server_fd);
    if (ret != 0) {
        LOGE("mbedtls_net_set_nonblock failed: -0x%04x", -ret);
        return -1;
    }

    /* 5. 设置 SSL/TLS 配置 */
    ret = mbedtls_ssl_config_defaults(&pNetworkContext->conf,
                                       MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        LOGE("mbedtls_ssl_config_defaults failed: -0x%04x", -ret);
        return -1;
    }

    /* 配置为不验证证书 (用于测试,生产环境应验证证书) */
    mbedtls_ssl_conf_authmode(&pNetworkContext->conf, MBEDTLS_SSL_VERIFY_NONE);

    mbedtls_ssl_conf_rng(&pNetworkContext->conf, mbedtls_ctr_drbg_random,
                          &pNetworkContext->ctr_drbg);

    /* 启用调试输出 (可选) */
    mbedtls_ssl_conf_dbg(&pNetworkContext->conf, my_debug, NULL);

    /* 6. 设置 SSL 上下文 */
    ret = mbedtls_ssl_setup(&pNetworkContext->ssl, &pNetworkContext->conf);
    if (ret != 0) {
        LOGE("mbedtls_ssl_setup failed: -0x%04x", -ret);
        return -1;
    }

    ret = mbedtls_ssl_set_hostname(&pNetworkContext->ssl, MQTT_BROKER_HOST);
    if (ret != 0) {
        LOGE("mbedtls_ssl_set_hostname failed: -0x%04x", -ret);
        return -1;
    }

    mbedtls_ssl_set_bio(&pNetworkContext->ssl, &pNetworkContext->server_fd,
                         mbedtls_net_send, mbedtls_net_recv, NULL);

    /* 7. 执行 SSL/TLS 握手 */
    LOGI("Performing SSL/TLS handshake...");
    while ((ret = mbedtls_ssl_handshake(&pNetworkContext->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            LOGE("mbedtls_ssl_handshake failed: -0x%04x", -ret);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOGI("SSL/TLS handshake completed successfully");
    LOGI("Connected to MQTT broker %s:%d via SSL/TLS", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    return 0;
}

static void cleanup_ssl(struct NetworkContext *pNetworkContext)
{
    mbedtls_ssl_close_notify(&pNetworkContext->ssl);
    mbedtls_net_free(&pNetworkContext->server_fd);
    mbedtls_x509_crt_free(&pNetworkContext->cacert);
    mbedtls_ssl_free(&pNetworkContext->ssl);
    mbedtls_ssl_config_free(&pNetworkContext->conf);
    mbedtls_ctr_drbg_free(&pNetworkContext->ctr_drbg);
    mbedtls_entropy_free(&pNetworkContext->entropy);
}

int main(int argc, char **argv)
{
    MQTTContext_t mqttContext;
    MQTTConnectInfo_t connectInfo;
    MQTTSubscribeInfo_t subscribeInfo;
    MQTTPublishInfo_t publishInfo;
    MQTTFixedBuffer_t networkBuffer;
    TransportInterface_t transport;
    struct NetworkContext networkContext;
    MQTTStatus_t mqttStatus;
    bool sessionPresent;

    LOGI("MQTT SSL/TLS Test Starting...");

    /* 初始化 */
    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = { .custom_mac = custom_get_wifi_mac };
    lisa_wifi_init(&ops);

    user_fs_init();
    lisa_kv_init();
    user_wifi_manager_init();

    if (wait_for_wifi_connection() != 0) {
        LOGE("Failed to connect to WiFi");
        return -1;
    }

    LOGI("\n========================================");
    LOGI("MQTT SSL/TLS 功能测试");
    LOGI("========================================\n");

    /* 连接到 MQTT Broker (SSL) */
    if (connect_to_broker_ssl(&networkContext) != 0) {
        return -1;
    }

    /* 设置 Transport 接口 */
    transport.recv = transport_recv;
    transport.send = transport_send;
    transport.pNetworkContext = &networkContext;
    transport.writev = NULL;

    /* 设置网络缓冲区 */
    networkBuffer.pBuffer = g_network_buffer;
    networkBuffer.size = NETWORK_BUFFER_SIZE;

    /* 初始化 MQTT Context */
    mqttStatus = MQTT_Init(&mqttContext, &transport, get_time_ms, mqtt_event_callback, &networkBuffer);
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Init failed: %s", MQTT_Status_strerror(mqttStatus));
        goto cleanup;
    }

    /* 建立 MQTT 连接 */
    memset(&connectInfo, 0, sizeof(connectInfo));
    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = MQTT_CLIENT_ID;
    connectInfo.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connectInfo.keepAliveSeconds = 60;

    LOGI("=== 测试 1: MQTT CONNECT ===");
    mqttStatus = MQTT_Connect(&mqttContext, &connectInfo, NULL, 5000, &sessionPresent);
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Connect failed: %s", MQTT_Status_strerror(mqttStatus));
        goto cleanup;
    }
    LOGI("MQTT Connected successfully");

    vTaskDelay(pdMS_TO_TICKS(500));

    /* 订阅主题 */
    LOGI("\n=== 测试 2: MQTT SUBSCRIBE ===");
    memset(&subscribeInfo, 0, sizeof(subscribeInfo));
    subscribeInfo.qos = MQTTQoS0;
    subscribeInfo.pTopicFilter = MQTT_TOPIC_SUB;
    subscribeInfo.topicFilterLength = strlen(MQTT_TOPIC_SUB);

    mqttStatus = MQTT_Subscribe(&mqttContext, &subscribeInfo, 1, MQTT_GetPacketId(&mqttContext));
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Subscribe failed: %s", MQTT_Status_strerror(mqttStatus));
        goto cleanup;
    }
    LOGI("Subscribe request sent for topic: %s", MQTT_TOPIC_SUB);

    /* 处理 SUBACK */
    mqttStatus = MQTT_ProcessLoop(&mqttContext);
    if (mqttStatus != MQTTSuccess && mqttStatus != MQTTNeedMoreBytes) {
        LOGE("MQTT_ProcessLoop failed: %s", MQTT_Status_strerror(mqttStatus));
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    /* 发布消息 */
    LOGI("\n=== 测试 3: MQTT PUBLISH ===");
    memset(&publishInfo, 0, sizeof(publishInfo));
    publishInfo.qos = MQTTQoS0;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = "Hello from ARCS via SSL!";
    publishInfo.payloadLength = strlen("Hello from ARCS via SSL!");

    mqttStatus = MQTT_Publish(&mqttContext, &publishInfo, MQTT_GetPacketId(&mqttContext));
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Publish failed: %s", MQTT_Status_strerror(mqttStatus));
        goto cleanup;
    }
    LOGI("Published message to topic: %s", MQTT_TOPIC_PUB);

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 处理接收的消息 */
    LOGI("\n=== 测试 4: 接收消息 ===");
    while (1) {
        mqttStatus = MQTT_ProcessLoop(&mqttContext);
        if (mqttStatus != MQTTSuccess && mqttStatus != MQTTNeedMoreBytes) {
            LOGE("MQTT_ProcessLoop failed: %s", MQTT_Status_strerror(mqttStatus));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 断开连接 */
    LOGI("\n=== 测试 5: MQTT DISCONNECT ===");
    mqttStatus = MQTT_Disconnect(&mqttContext);
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Disconnect failed: %s", MQTT_Status_strerror(mqttStatus));
    } else {
        LOGI("MQTT Disconnected successfully");
    }

    LOGI("\n========================================");
    LOGI("所有 MQTT SSL/TLS 测试完成");
    LOGI("========================================\n");

cleanup:
    cleanup_ssl(&networkContext);
    return 0;
}
