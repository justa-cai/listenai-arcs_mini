#include "lisa_thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "IOMuxManager.h"
#include "lisa_log.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "wifi_api.h"
#include "net_al.h"
#include "net_def.h"
#include "ls_event.h"
#include "core_mqtt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define TAG "wifi_pm"

#define LOW_POWER_ENABLED   1
#define MQTT_ENABLED        1

#if LOW_POWER_ENABLED
#include "pm_impl.h"
#include "vrtc.h"
#endif

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================

#define MQTT_BROKER_HOST   "192.168.32.192"
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "arcs_mqtt_client"

#define TARGET_WIFI_SSID   "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD    "a12345678"

#define MQTT_TOPIC_PUB     "arcs/test/pub"
#define MQTT_TOPIC_SUB     "arcs/test/sub"

#define MQTT_KEEPALIVE_S   120

#define NETWORK_BUFFER_SIZE 1024
#define WIFI_PS_LISTEN_INTERVAL  10
#define WIFI_RECONNECT_DELAY_MS  3000
#define WIFI_RECONNECT_TASK_STACK_SIZE 1024
#define WIFI_RECONNECT_TASK_PRIORITY   4
#define MQTT_RECONNECT_DELAY_MS  2000
#define MQTT_CONNECT_RETRY_COUNT 3
#define MQTT_CONNECT_RETRY_DELAY_MS 1000
#define MSG_QUEUE_LENGTH    5
#define MSG_PAYLOAD_MAX     128
#define MSG_TASK_STACK_SIZE 2048
#define MSG_TASK_PRIORITY   3

static mac_manager_t *m_mac_manager;

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;
#if LOW_POWER_ENABLED
static bool g_low_power_enabled = false;
#endif

/* 网络上下文 */
struct NetworkContext {
    int socket;
};

typedef struct {
    MQTTContext_t context;
    MQTTFixedBuffer_t network_buffer;
    TransportInterface_t transport;
    struct NetworkContext network_context;
} mqtt_client_runtime_t;

/* MQTT 缓冲区 */
static uint8_t g_network_buffer[NETWORK_BUFFER_SIZE];
static mqtt_client_runtime_t g_mqtt_client;

/* 全局 MQTT Context，供消息处理任务发布响应使用 */
static MQTTContext_t *g_mqtt_ctx = NULL;

/* 消息队列：回调 -> 处理任务 */
typedef struct {
    char payload[MSG_PAYLOAD_MAX];
    size_t len;
} mqtt_msg_t;

static QueueHandle_t g_msg_queue = NULL;

/* 互斥锁：保护 MQTT 上下文的并发访问 */
static SemaphoreHandle_t g_mqtt_mutex = NULL;
static TaskHandle_t g_wifi_reconnect_task = NULL;
static volatile bool g_wifi_reconnect_requested = false;

static ls_err_t user_wifi_direct_connect(void);
#if MQTT_ENABLED
static int mqtt_connect_session(mqtt_client_runtime_t *mqtt_client);
static int mqtt_subscribe_default_topic(MQTTContext_t *mqtt_context);
static int mqtt_publish_boot_message(MQTTContext_t *mqtt_context);
static void mqtt_cleanup(mqtt_client_runtime_t *mqtt_client);
#endif
static void set_low_power_enabled(bool enable);

void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER3);
}

void lisa_adc_pinmux()
{
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 7, 3);
}

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
    (void)netmask;
    (void)gateway;
    (void)arg;

    if (success) {
        LOGI("DHCP Success: IP=%d.%d.%d.%d",
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, 
             (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
        g_get_ip_success = true;
        if (g_wifi_reconnect_requested && (g_wifi_reconnect_task != NULL)) {
            xTaskNotifyGive(g_wifi_reconnect_task);
        }
    } else {
        g_get_ip_success = false;
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static bool wifi_is_link_busy(void)
{
    wifi_link_status_t link_status = {0};

    if (wifi_get_link_status(&link_status) != LS_OK) {
        return false;
    }

    return (link_status.state == STA_IN_CONNECTING) ||
           (link_status.state == STA_CONNECTED);
}

static void schedule_wifi_reconnect(void)
{
    g_wifi_reconnect_requested = true;
    if (g_wifi_reconnect_task != NULL) {
        xTaskNotifyGive(g_wifi_reconnect_task);
    }
}

static void wifi_reconnect_task(void *arg)
{
    uint32_t wifi_attempt = 0;

    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (g_wifi_reconnect_requested) {
            if (!g_wifi_connected) {
                if (wifi_is_link_busy()) {
                    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                    continue;
                }

                wifi_attempt++;
                LOGI("WiFi reconnect attempt %lu", (unsigned long)wifi_attempt);

                if (user_wifi_direct_connect() != LS_OK) {
                    LOGW("Failed to trigger WiFi reconnect");
                }

                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
                continue;
            }

            if (!g_get_ip_success) {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                continue;
            }

#if MQTT_ENABLED
            if (g_mqtt_mutex == NULL) {
                g_wifi_reconnect_requested = false;
                wifi_attempt = 0;
                continue;
            }

            LOGI("MQTT reconnect attempt");
            mqtt_cleanup(&g_mqtt_client);

            if (mqtt_connect_session(&g_mqtt_client) == 0) {
                vTaskDelay(pdMS_TO_TICKS(500));

                if ((mqtt_subscribe_default_topic(&g_mqtt_client.context) == 0) &&
                    (mqtt_publish_boot_message(&g_mqtt_client.context) == 0)) {
#if LOW_POWER_ENABLED
                    set_low_power_enabled(true);
#endif
                    LOGI("WiFi and MQTT reconnect success");
                    g_wifi_reconnect_requested = false;
                    wifi_attempt = 0;
                    break;
                }
            }

            mqtt_cleanup(&g_mqtt_client);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
            continue;
#endif

#if LOW_POWER_ENABLED
            set_low_power_enabled(true);
#endif
            g_wifi_reconnect_requested = false;
            wifi_attempt = 0;
        }
    }
}

static int app_wifi_event_handler(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    (void)arg;
    (void)event_module;
    (void)event_data;
    net_if_t *net_if;

    switch (event_id) {
    case EVENT_WIFI_CONNECTED:
        LOGI("WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        } else {
            g_get_ip_success = true;
            if (g_wifi_reconnect_requested && (g_wifi_reconnect_task != NULL)) {
                xTaskNotifyGive(g_wifi_reconnect_task);
            }
        }
        break;
    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        LOGI("WiFi disconnected or connect failed (event=%d)", event_id);
        g_wifi_connected = false;
        g_get_ip_success = false;
#if LOW_POWER_ENABLED
        set_low_power_enabled(false);
#endif
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
#if MQTT_ENABLED
        if (g_mqtt_mutex != NULL) {
            mqtt_cleanup(&g_mqtt_client);
        }
#endif
        schedule_wifi_reconnect();
        break;
    default:
        break;
    }
    return 0;
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

static ls_err_t user_wifi_direct_connect(void)
{
    wifi_connect_cfg_t cfg = {0};

    strncpy((char *)cfg.ssid, TARGET_WIFI_SSID, WIFI_SSID_LEN);
    strncpy((char *)cfg.key, TARGET_WIFI_PWD, WIFI_PASSWORD_LEN);
    cfg.dhcp_mode = DHCP_CLIENT;
    cfg.sec = WIFI_SEC_AUTO;

    ls_err_t ret = wifi_sta_connect(&cfg);
    if (ret != LS_OK) {
        LOGE("wifi_sta_connect failed: %d", ret);
    } else {
        LOGI("WiFi connecting to %s ...", TARGET_WIFI_SSID);
    }

    return ret;
}

static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");

    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();
    int ret = wifi_sta_auto_reconnect_enable();
    if (ret != LS_OK) {
        LOGW("Failed to enable WiFi auto reconnect: %d", ret);
    }

    #if LOW_POWER_ENABLED
    ret = wifi_sta_set_listen_itv(WIFI_PS_LISTEN_INTERVAL);
    if (ret != 0) {
        LOGE("Failed to set listen interval: %d", ret);
        return;
    }
    #endif
    
    user_wifi_direct_connect();
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

#if LOW_POWER_ENABLED
static void enable_wifi_power_save(void)
{
    int ret = wifi_ps_mode_set(WIFI_PS_MODE_DTIM);
    if (ret != 0) {
        LOGE("Failed to enable WiFi power save mode: %d", ret);
        return;
    }
    net_enable_keep_alive(); 
    LOGI("WiFi power save enabled (Listen mode, interval=%d)", WIFI_PS_LISTEN_INTERVAL);
}

static void enable_system_low_power(void)
{
    pm_config_t config = {
        .mode       = PM_MODE_LIGHT_SLEEP,
    };
    pm_set_config(&config);

    LOGI("System power management enabled (Light Sleep mode with keep alive)");
}

static void disable_wifi_power_save(void)
{
    int ret = wifi_ps_mode_set(WIFI_PS_MODE_OFF);
    if (ret != 0) {
        LOGW("Failed to disable WiFi power save mode: %d", ret);
        return;
    }
    net_disable_keep_alive();   
    LOGI("WiFi power save disabled");
}

static void disable_system_low_power(void)
{
    pm_config_t config = {
        .mode       = PM_MODE_ACTIVE,
    };

    pm_set_config(&config);

    LOGI("System power management disabled");
}

static void set_low_power_enabled(bool enable)
{
    if (enable) {
        if (g_low_power_enabled) {
            return;
        }

        enable_wifi_power_save();
        enable_system_low_power();
        g_low_power_enabled = true;
        return;
    }

    if (!g_low_power_enabled) {
        return;
    }

    disable_wifi_power_save();
    disable_system_low_power();
    g_low_power_enabled = false;
}
#endif

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

/**
 * 发布文本响应到 MQTT_TOPIC_PUB
 *
 * 简单协议格式: "<cmd>:<code>:<message>"
 */
static void mqtt_publish_response(const char *cmd, int code, const char *message)
{
    if (!g_mqtt_ctx) return;

    char buf[MSG_PAYLOAD_MAX];
    int str_cnt = snprintf(buf, sizeof(buf), "%s:%d:%s", cmd, code, message ? message : "");
    if (str_cnt <= 0 || str_cnt >= (int)sizeof(buf)) return;

    MQTTPublishInfo_t pub = {
        .qos             = MQTTQoS0,
        .pTopicName      = MQTT_TOPIC_PUB,
        .topicNameLength = strlen(MQTT_TOPIC_PUB),
        .pPayload        = buf,
        .payloadLength   = (size_t)str_cnt,
    };

    if (xSemaphoreTake(g_mqtt_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        MQTTStatus_t status = MQTT_Publish(g_mqtt_ctx, &pub, MQTT_GetPacketId(g_mqtt_ctx));
        xSemaphoreGive(g_mqtt_mutex);
        if (status != MQTTSuccess) {
            LOGE("Publish failed: %s", MQTT_Status_strerror(status));
        } else {
            LOGI("Response: %s", buf);
        }
    } else {
        LOGW("Failed to acquire MQTT mutex for publish");
    }
}

/*
 * 处理接收到的简单文本命令
 *
 * 协议格式: "<cmd>" 或 "<cmd>:<arg>"
 */
static void handle_mqtt_message(const char *payload, size_t len)
{
#if LOW_POWER_ENABLED
    wifi_ps_lock_acquire(WIFI_PS_LOCK_BIT_APP, false);
#endif

    LOGI("CMD: %.*s", (int)len, payload);

    if (len >= 6 && strncmp(payload, "status", 6) == 0) {
        char resp[128];
        snprintf(resp, sizeof(resp), "status:0:wifi=%d,ip=%d,uptime=%lu",
                 (int)g_wifi_connected, (int)g_get_ip_success,
                 (unsigned long)get_time_ms());
        mqtt_publish_response("status", 0, resp + 9);

    } else if (len >= 4 && strncmp(payload, "ping", 4) == 0) {
        mqtt_publish_response("pong", 0, "ok");

    } else if (len >= 6 && strncmp(payload, "reboot", 6) == 0) {
        mqtt_publish_response("reboot", 0, "rebooting");
        LOGI("Reboot requested, delaying 1s...");
        vTaskDelay(pdMS_TO_TICKS(1000));

    } else {
        LOGW("Unknown cmd: %.*s", (int)len, payload);
        mqtt_publish_response("error", -1, "unknown cmd");
    }

#if LOW_POWER_ENABLED
    wifi_ps_lock_release(WIFI_PS_LOCK_BIT_APP);
#endif
}

/* 消息处理任务 */
static void msg_handler_task(void *arg)
{
    mqtt_msg_t msg;

    LOGI("Message handler task started");

    while (1) {
        if (xQueueReceive(g_msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
            handle_mqtt_message(msg.payload, msg.len);
        }
    }
}

/* MQTT 事件回调（仅入队，不做耗时处理） */
static void mqtt_event_callback(MQTTContext_t *pContext, MQTTPacketInfo_t *pPacketInfo,
                                 MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
        if (!pPublishInfo) return;

        LOGI("Received PUBLISH: topic=%.*s, payload=%.*s",
             (int)pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
             (int)pPublishInfo->payloadLength, (char *)pPublishInfo->pPayload);

        /* 丢弃超长 payload */
        if (pPublishInfo->payloadLength >= MSG_PAYLOAD_MAX) {
            LOGW("Payload too large (%d bytes, max %d), dropped",
                 (int)pPublishInfo->payloadLength, MSG_PAYLOAD_MAX - 1);
            return;
        }

        /* 拷贝 payload 并发送到消息队列 */
        mqtt_msg_t msg;
        memcpy(msg.payload, pPublishInfo->pPayload, pPublishInfo->payloadLength);
        msg.payload[pPublishInfo->payloadLength] = '\0';
        msg.len = pPublishInfo->payloadLength;

        if (xQueueSend(g_msg_queue, &msg, 0) != pdTRUE) {
            LOGW("Message queue full, dropping message");
        }

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

static int mqtt_create_runtime_resources(void)
{
    BaseType_t task_ret;

    g_mqtt_mutex = xSemaphoreCreateMutex();
    if (!g_mqtt_mutex) {
        LOGE("Failed to create MQTT mutex");
        return -1;
    }

    g_msg_queue = xQueueCreate(MSG_QUEUE_LENGTH, sizeof(mqtt_msg_t));
    if (!g_msg_queue) {
        LOGE("Failed to create message queue");
        return -1;
    }

    task_ret = xTaskCreate(msg_handler_task, "msg_handler",
                           MSG_TASK_STACK_SIZE, NULL,
                           MSG_TASK_PRIORITY, NULL);
    if (task_ret != pdPASS) {
        LOGE("Failed to create msg_handler task");
        return -1;
    }

    return 0;
}

static void mqtt_prepare_runtime(mqtt_client_runtime_t *mqtt_client)
{
    memset(mqtt_client, 0, sizeof(*mqtt_client));

    mqtt_client->network_context.socket = -1;
    mqtt_client->transport.recv = transport_recv;
    mqtt_client->transport.send = transport_send;
    mqtt_client->transport.pNetworkContext = &mqtt_client->network_context;
    mqtt_client->transport.writev = NULL;
    mqtt_client->network_buffer.pBuffer = g_network_buffer;
    mqtt_client->network_buffer.size = NETWORK_BUFFER_SIZE;
}

static int mqtt_connect_session(mqtt_client_runtime_t *mqtt_client)
{
    MQTTConnectInfo_t connect_info = {0};
    MQTTStatus_t mqtt_status;
    bool session_present;
    uint32_t attempt;

    connect_info.cleanSession = true;
    connect_info.pClientIdentifier = MQTT_CLIENT_ID;
    connect_info.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connect_info.keepAliveSeconds = MQTT_KEEPALIVE_S;

    if (connect_to_broker(&mqtt_client->network_context) != 0) {
        return -1;
    }

    mqtt_status = MQTT_Init(&mqtt_client->context, &mqtt_client->transport,
                            get_time_ms, mqtt_event_callback,
                            &mqtt_client->network_buffer);
    if (mqtt_status != MQTTSuccess) {
        LOGE("MQTT_Init failed: %s", MQTT_Status_strerror(mqtt_status));
        return -1;
    }

    for (attempt = 1; attempt <= MQTT_CONNECT_RETRY_COUNT; ++attempt) {
        mqtt_status = MQTT_Connect(&mqtt_client->context, &connect_info, NULL,
                                   5000, &session_present);
        if (mqtt_status == MQTTSuccess) {
            g_mqtt_ctx = &mqtt_client->context;
            LOGI("MQTT Connected successfully");
            return 0;
        }

        LOGW("MQTT_Connect attempt %lu/%d failed: %s",
             (unsigned long)attempt, MQTT_CONNECT_RETRY_COUNT,
             MQTT_Status_strerror(mqtt_status));

        if (attempt < MQTT_CONNECT_RETRY_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(MQTT_CONNECT_RETRY_DELAY_MS));
        }
    }

    LOGE("MQTT_Connect failed after %d attempts", MQTT_CONNECT_RETRY_COUNT);
    return -1;
}

static int mqtt_subscribe_default_topic(MQTTContext_t *mqtt_context)
{
    MQTTSubscribeInfo_t subscribe_info = {0};
    MQTTStatus_t mqtt_status;

    subscribe_info.qos = MQTTQoS0;
    subscribe_info.pTopicFilter = MQTT_TOPIC_SUB;
    subscribe_info.topicFilterLength = strlen(MQTT_TOPIC_SUB);

    mqtt_status = MQTT_Subscribe(mqtt_context, &subscribe_info, 1,
                                 MQTT_GetPacketId(mqtt_context));
    if (mqtt_status != MQTTSuccess) {
        LOGE("MQTT_Subscribe failed: %s", MQTT_Status_strerror(mqtt_status));
        return -1;
    }

    LOGI("Subscribe request sent for topic: %s", MQTT_TOPIC_SUB);

    mqtt_status = MQTT_ProcessLoop(mqtt_context);
    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes) {
        LOGE("MQTT_ProcessLoop failed: %s", MQTT_Status_strerror(mqtt_status));
    }

    return 0;
}

static int mqtt_publish_boot_message(MQTTContext_t *mqtt_context)
{
    MQTTPublishInfo_t publish_info = {0};
    MQTTStatus_t mqtt_status;

    publish_info.qos = MQTTQoS0;
    publish_info.pTopicName = MQTT_TOPIC_PUB;
    publish_info.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publish_info.pPayload = "Hello from ARCS!";
    publish_info.payloadLength = strlen("Hello from ARCS!");

    mqtt_status = MQTT_Publish(mqtt_context, &publish_info,
                               MQTT_GetPacketId(mqtt_context));
    if (mqtt_status != MQTTSuccess) {
        LOGE("MQTT_Publish failed: %s", MQTT_Status_strerror(mqtt_status));
        return -1;
    }

    LOGI("Published message to topic: %s", MQTT_TOPIC_PUB);
    return 0;
}

static int mqtt_start(mqtt_client_runtime_t *mqtt_client)
{
    mqtt_prepare_runtime(mqtt_client);

    if (mqtt_create_runtime_resources() != 0) {
        return -1;
    }

    if (mqtt_connect_session(mqtt_client) != 0) {
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    if (mqtt_subscribe_default_topic(&mqtt_client->context) != 0) {
        return -1;
    }

    if (mqtt_publish_boot_message(&mqtt_client->context) != 0) {
        return -1;
    }

    return 0;
}

static void mqtt_process_once(void)
{
    MQTTStatus_t mqtt_status;
    MQTTContext_t *active_context = NULL;

    if (!g_wifi_connected || !g_get_ip_success || (g_mqtt_ctx == NULL) || (g_mqtt_mutex == NULL)) {
        return;
    }

    if (xSemaphoreTake(g_mqtt_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }

    active_context = g_mqtt_ctx;
    if (active_context != NULL) {
        mqtt_status = MQTT_ProcessLoop(active_context);
    } else {
        xSemaphoreGive(g_mqtt_mutex);
        return;
    }
    xSemaphoreGive(g_mqtt_mutex);

    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes) {
        LOGE("MQTT_ProcessLoop failed: %s", MQTT_Status_strerror(mqtt_status));
    }
}

static void mqtt_cleanup(mqtt_client_runtime_t *mqtt_client)
{
    BaseType_t locked = pdFALSE;

    if (g_mqtt_mutex != NULL) {
        locked = xSemaphoreTake(g_mqtt_mutex, pdMS_TO_TICKS(1000));
    }

    g_mqtt_ctx = NULL;

    if (mqtt_client->network_context.socket >= 0) {
        close(mqtt_client->network_context.socket);
        mqtt_client->network_context.socket = -1;
    }

    mqtt_prepare_runtime(mqtt_client);

    if (locked == pdTRUE) {
        xSemaphoreGive(g_mqtt_mutex);
    }
}

int main(int argc, char **argv)
{
    BaseType_t task_ret;

#if LOW_POWER_ENABLED
    vrtc_init();
    pm_init();
    pm_register_gpio_retention(0, 4);
    pm_register_gpio_retention(0, 5);
#endif

    /* 初始化 */
    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    task_ret = xTaskCreate(wifi_reconnect_task, "wifi_reconnect",
                           WIFI_RECONNECT_TASK_STACK_SIZE, NULL,
                           WIFI_RECONNECT_TASK_PRIORITY, &g_wifi_reconnect_task);
    if (task_ret != pdPASS) {
        LOGE("Failed to create wifi_reconnect task");
        return -1;
    }

    lisa_wifi_ops_t ops = {
        .init_done  = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    if (wait_for_wifi_connection() != 0) {
        LOGE("Failed to connect to WiFi");
        return -1;
    }

#if MQTT_ENABLED
    if (mqtt_start(&g_mqtt_client) != 0) {
        goto cleanup;
    }
#endif // MQTT_ENABLED

#if LOW_POWER_ENABLED
    set_low_power_enabled(true);
#endif
    while (1) {
        #if MQTT_ENABLED
        mqtt_process_once();
        #endif

        // LOGI("main test---------\r\n");
        vTaskDelay(pdMS_TO_TICKS(300));
    }

cleanup:
#if MQTT_ENABLED
    mqtt_cleanup(&g_mqtt_client);
#endif
    return 0;
}
