#include "lisa_thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "mqtt-test"
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

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define MQTT_BROKER_HOST   "broker.emqx.io"
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "arcs_mqtt_client"

#define TARGET_WIFI_SSID   "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD    "a12345678"

#define MQTT_TOPIC_PUB     "arcs/test/pub"
#define MQTT_TOPIC_SUB     "arcs/test/sub"

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

#define NETWORK_BUFFER_SIZE 1024

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

/* 网络上下文 */
struct NetworkContext {
    int socket;
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

/* Transport 接口实现 */
static int32_t transport_recv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    int ret = recv(pNetworkContext->socket, pBuffer, bytesToRecv, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }
        return -1;
    }
    return ret;
}

static int32_t transport_send(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    int ret = send(pNetworkContext->socket, pBuffer, bytesToSend, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }
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

static int connect_to_broker(struct NetworkContext *pNetworkContext)
{
    struct sockaddr_in server_addr;
    struct hostent *host;

    pNetworkContext->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (pNetworkContext->socket < 0) {
        LOGE("Failed to create socket");
        return -1;
    }

    host = gethostbyname(MQTT_BROKER_HOST);
    if (host == NULL) {
        LOGE("Failed to resolve hostname");
        close(pNetworkContext->socket);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MQTT_BROKER_PORT);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    if (connect(pNetworkContext->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("Failed to connect to broker");
        close(pNetworkContext->socket);
        return -1;
    }

    /* 设置非阻塞模式 */
    int flags = fcntl(pNetworkContext->socket, F_GETFL, 0);
    fcntl(pNetworkContext->socket, F_SETFL, flags | O_NONBLOCK);

    LOGI("Connected to MQTT broker %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    return 0;
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

    LOGI("MQTT Test Starting...");

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
    LOGI("MQTT 功能测试");
    LOGI("========================================\n");

    /* 连接到 MQTT Broker */
    if (connect_to_broker(&networkContext) != 0) {
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
    publishInfo.pPayload = "Hello from ARCS!";
    publishInfo.payloadLength = strlen("Hello from ARCS!");

    mqttStatus = MQTT_Publish(&mqttContext, &publishInfo, MQTT_GetPacketId(&mqttContext));
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Publish failed: %s", MQTT_Status_strerror(mqttStatus));
        goto cleanup;
    }
    LOGI("Published message to topic: %s", MQTT_TOPIC_PUB);

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 处理接收的消息 */
    LOGI("\n=== 测试 4: 接收消息 ===");
    // for (int i = 0; i < 5; i++) {
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
    LOGI("所有 MQTT 测试完成");
    LOGI("========================================\n");

cleanup:
    close(networkContext.socket);
    return 0;
}
