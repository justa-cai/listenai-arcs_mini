#include "lisa_thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#define TAG "mqtt-agent-test"
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

#include "core_mqtt_agent.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// ============================================================
// 重要:使用前请修改以下配置!
// ============================================================
#define MQTT_BROKER_HOST   "broker.emqx.io"
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "arcs_mqtt_agent_client"

#define TARGET_WIFI_SSID   "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD    "a12345678"

#define MQTT_TOPIC_PUB     "arcs/test/pub"
#define MQTT_TOPIC_SUB     "arcs/test/sub"

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

#define NETWORK_BUFFER_SIZE 2048
#define AGENT_CMD_QUEUE_LENGTH 25
#define AGENT_TASK_STACK_SIZE 4096
#define AGENT_TASK_PRIORITY 5

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;
static volatile bool g_agent_connected = false;

/* 网络上下文 */
struct NetworkContext {
    int socket;
};

/* 定义 MQTTAgentMessageContext 结构体 */
struct MQTTAgentMessageContext {
    QueueHandle_t queue;
};

/* 定义 MQTTAgentCommandContext 结构体 */
struct MQTTAgentCommandContext {
    SemaphoreHandle_t semaphore;
    MQTTStatus_t returnStatus;
};

/* MQTT Agent 上下文和缓冲区 */
static MQTTAgentContext_t g_agent_context;
static uint8_t g_network_buffer[NETWORK_BUFFER_SIZE];
static struct MQTTAgentMessageContext g_command_queue_context;
static MQTTAgentCommand_t g_command_queue[AGENT_CMD_QUEUE_LENGTH];
static struct NetworkContext g_network_context = {0};

/* FreeRTOS 队列用于命令传递 */
static QueueHandle_t g_agent_queue;
static SemaphoreHandle_t g_connect_semaphore;

/* MQTT Agent 任务句柄 */
static TaskHandle_t g_agent_task_handle = NULL;

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
        g_agent_connected = false;
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

/* Transport 接口实现 - 处理非阻塞 socket */
static int32_t transport_recv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    int bytes_received = recv(pNetworkContext->socket, pBuffer, bytesToRecv, 0);
    
    /* 非阻塞 socket: EAGAIN/EWOULDBLOCK 表示暂时没有数据 */
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* 没有数据可读，返回 0 让 MQTT 库知道需要稍后重试 */
            return 0;
        }
        /* 其他错误返回负值 */
        return bytes_received;
    }
    
    return (int32_t)bytes_received;
}

static int32_t transport_send(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    int bytes_sent = send(pNetworkContext->socket, pBuffer, bytesToSend, 0);
    return (int32_t)bytes_sent;
}

static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* Agent 消息队列接口实现 */
static bool agent_send_command(MQTTAgentMessageContext_t *pMsgCtx,
                                MQTTAgentCommand_t * const * pCommandToSend,
                                uint32_t blockTimeMs)
{
    BaseType_t ret;
    TickType_t xTicksToWait = pdMS_TO_TICKS(blockTimeMs);

    ret = xQueueSend(g_agent_queue, pCommandToSend, xTicksToWait);
    return (ret == pdPASS);
}

static bool agent_recv_command(MQTTAgentMessageContext_t *pMsgCtx,
                                MQTTAgentCommand_t **pReceivedCommand,
                                uint32_t blockTimeMs)
{
    BaseType_t ret;
    TickType_t xTicksToWait = pdMS_TO_TICKS(blockTimeMs);

    ret = xQueueReceive(g_agent_queue, pReceivedCommand, xTicksToWait);
    return (ret == pdPASS);
}

static MQTTAgentCommand_t *agent_get_command(uint32_t blockTimeMs)
{
    /* 简单实现:从静态数组中分配 */
    static uint32_t cmd_index = 0;
    MQTTAgentCommand_t *pCommand = NULL;

    /* 在实际应用中,这里应该使用内存池或其他线程安全的分配机制 */
    taskENTER_CRITICAL();
    if (cmd_index < AGENT_CMD_QUEUE_LENGTH) {
        pCommand = &g_command_queue[cmd_index];
        cmd_index++;
    }
    taskEXIT_CRITICAL();

    return pCommand;
}

static bool agent_release_command(MQTTAgentCommand_t *pCommandToRelease)
{
    /* 简单实现:不做任何事,因为我们使用静态数组 */
    return true;
}

/* MQTT Agent 接收消息回调 */
static void incoming_publish_callback(MQTTAgentContext_t *pAgentContext,
                                       uint16_t packetId,
                                       MQTTPublishInfo_t *pPublishInfo)
{
    LOGI("Incoming publish received: topic=%.*s, payload=%.*s",
         (int)pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
         (int)pPublishInfo->payloadLength, (char *)pPublishInfo->pPayload);
}

/* 命令完成回调 */
static void command_complete_callback(MQTTAgentCommandContext_t *pCommandContext,
                                       MQTTAgentReturnInfo_t *pReturnInfo)
{
    if (pCommandContext != NULL) {
        SemaphoreHandle_t *pSem = (SemaphoreHandle_t *)pCommandContext;
        xSemaphoreGive(*pSem);
    }

    if (pReturnInfo->returnCode != MQTTSuccess) {
        LOGE("Command failed with return code: %d", pReturnInfo->returnCode);
    }
}

/* MQTT 连接到 Broker */
static int connect_to_broker(struct NetworkContext *pNetworkContext)
{
    struct sockaddr_in broker_addr;
    struct hostent *host_entry;

    LOGI("Resolving broker hostname: %s", MQTT_BROKER_HOST);
    host_entry = gethostbyname(MQTT_BROKER_HOST);
    if (host_entry == NULL) {
        LOGE("Failed to resolve hostname");
        return -1;
    }

    pNetworkContext->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (pNetworkContext->socket < 0) {
        LOGE("Failed to create socket");
        return -1;
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(MQTT_BROKER_PORT);
    memcpy(&broker_addr.sin_addr.s_addr, host_entry->h_addr, host_entry->h_length);

    LOGI("Connecting to broker %s:%d...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    if (connect(pNetworkContext->socket, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        LOGE("Failed to connect to broker");
        close(pNetworkContext->socket);
        return -1;
    }

    /* 设置 Socket 为非阻塞模式 - 使用 LwIP 的 ioctl */
    int non_blocking = 1;
    if (ioctlsocket(pNetworkContext->socket, FIONBIO, &non_blocking) < 0) {
        LOGE("Failed to set socket to non-blocking");
        close(pNetworkContext->socket);
        return -1;
    }

    LOGI("Connected to MQTT broker successfully (non-blocking mode)");
    return 0;
}

/* MQTT Agent 任务 */
static void mqtt_agent_task(void *pvParameters)
{
    MQTTStatus_t mqttStatus;
    MQTTConnectInfo_t connectInfo = {0};
    TransportInterface_t transport = {0};
    MQTTFixedBuffer_t networkBuffer;
    bool sessionPresent = false;

    LOGI("MQTT Agent task started");

    /* 连接到 Broker */
    if (connect_to_broker(&g_network_context) != 0) {
        LOGE("Failed to connect to broker, agent task exiting");
        vTaskDelete(NULL);
        return;
    }

    /* 设置 Transport 接口 */
    transport.recv = transport_recv;
    transport.send = transport_send;
    transport.pNetworkContext = &g_network_context;

    /* 设置网络缓冲区 */
    networkBuffer.pBuffer = g_network_buffer;
    networkBuffer.size = NETWORK_BUFFER_SIZE;

    /* 初始化消息接口 */
    MQTTAgentMessageInterface_t messageInterface = {
        .pMsgCtx = &g_command_queue_context,
        .send = agent_send_command,
        .recv = agent_recv_command,
        .getCommand = agent_get_command,
        .releaseCommand = agent_release_command
    };

    /* 初始化 MQTT Agent */
    mqttStatus = MQTTAgent_Init(&g_agent_context,
                                 &messageInterface,
                                 &networkBuffer,
                                 &transport,
                                 get_time_ms,
                                 incoming_publish_callback,
                                 NULL);

    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTTAgent_Init failed: %d", mqttStatus);
        close(g_network_context.socket);
        vTaskDelete(NULL);
        return;
    }

    /* 建立 MQTT 连接 */
    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = MQTT_CLIENT_ID;
    connectInfo.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connectInfo.keepAliveSeconds = 60;

    mqttStatus = MQTT_Connect(&g_agent_context.mqttContext, &connectInfo, NULL, 5000, &sessionPresent);
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTT_Connect failed: %d", mqttStatus);
        close(g_network_context.socket);
        vTaskDelete(NULL);
        return;
    }

    LOGI("MQTT Agent connected to broker");
    g_agent_connected = true;

    /* 通知主任务连接成功 */
    xSemaphoreGive(g_connect_semaphore);

    /* 进入 Agent 命令循环 */
    LOGI("Entering MQTT Agent command loop...");
    mqttStatus = MQTTAgent_CommandLoop(&g_agent_context);

    LOGE("MQTT Agent command loop exited with status: %d", mqttStatus);
    g_agent_connected = false;

    /* 清理 */
    close(g_network_context.socket);
    vTaskDelete(NULL);
}

/* 订阅回调 */
static void subscribe_command_callback(MQTTAgentCommandContext_t *pCommandContext,
                                        MQTTAgentReturnInfo_t *pReturnInfo)
{
    if (pReturnInfo->returnCode == MQTTSuccess) {
        LOGI("Subscribe command completed successfully");
    } else {
        LOGE("Subscribe command failed: %d", pReturnInfo->returnCode);
    }

    if (pCommandContext != NULL && pCommandContext->semaphore != NULL) {
        pCommandContext->returnStatus = pReturnInfo->returnCode;
        xSemaphoreGive(pCommandContext->semaphore);
    }
}

/* 发布回调 */
static void publish_command_callback(MQTTAgentCommandContext_t *pCommandContext,
                                      MQTTAgentReturnInfo_t *pReturnInfo)
{
    LOGI("[CALLBACK] Publish callback invoked, returnCode=%d", pReturnInfo->returnCode);
    
    if (pReturnInfo->returnCode == MQTTSuccess) {
        LOGI("✓ Publish command completed successfully");
    } else {
        LOGE("✗ Publish command failed with code: %d", pReturnInfo->returnCode);
    }

    if (pCommandContext != NULL && pCommandContext->semaphore != NULL) {
        pCommandContext->returnStatus = pReturnInfo->returnCode;
        LOGI("[CALLBACK] Releasing semaphore");
        xSemaphoreGive(pCommandContext->semaphore);
    } else {
        LOGE("[CALLBACK] Context or semaphore is NULL!");
    }
}

int main(int argc, char **argv)
{
    MQTTStatus_t mqttStatus;
    MQTTAgentSubscribeArgs_t subscribeArgs = {0};
    MQTTSubscribeInfo_t subscribeInfo = {0};
    MQTTAgentCommandInfo_t commandInfo = {0};
    MQTTPublishInfo_t publishInfo = {0};
    SemaphoreHandle_t command_semaphore;
    BaseType_t xReturned;

    LOGI("MQTT Agent Test Starting...");

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
    LOGI("MQTT Agent 功能测试");
    LOGI("========================================\n");

    /* 创建命令队列 */
    g_agent_queue = xQueueCreate(AGENT_CMD_QUEUE_LENGTH, sizeof(MQTTAgentCommand_t *));
    if (g_agent_queue == NULL) {
        LOGE("Failed to create agent queue");
        return -1;
    }
    g_command_queue_context.queue = g_agent_queue;

    /* 创建信号量 */
    g_connect_semaphore = xSemaphoreCreateBinary();
    command_semaphore = xSemaphoreCreateBinary();

    if (g_connect_semaphore == NULL || command_semaphore == NULL) {
        LOGE("Failed to create semaphores");
        return -1;
    }

    /* 创建 MQTT Agent 任务 */
    xReturned = xTaskCreate(mqtt_agent_task,
                            "mqtt_agent",
                            AGENT_TASK_STACK_SIZE,
                            NULL,
                            AGENT_TASK_PRIORITY,
                            &g_agent_task_handle);

    if (xReturned != pdPASS) {
        LOGE("Failed to create MQTT Agent task");
        return -1;
    }

    /* 等待 Agent 连接成功 */
    if (xSemaphoreTake(g_connect_semaphore, pdMS_TO_TICKS(10000)) != pdTRUE) {
        LOGE("Timeout waiting for agent connection");
        return -1;
    }

    LOGI("\n=== 测试 1: MQTT SUBSCRIBE (使用 Agent API) ===");

    /* 订阅主题 */
    subscribeInfo.qos = MQTTQoS0;
    subscribeInfo.pTopicFilter = MQTT_TOPIC_SUB;
    subscribeInfo.topicFilterLength = strlen(MQTT_TOPIC_SUB);

    subscribeArgs.pSubscribeInfo = &subscribeInfo;
    subscribeArgs.numSubscriptions = 1;

    struct MQTTAgentCommandContext subscribeContext = {
        .semaphore = command_semaphore,
        .returnStatus = MQTTSuccess
    };

    commandInfo.cmdCompleteCallback = subscribe_command_callback;
    commandInfo.pCmdCompleteCallbackContext = &subscribeContext;
    commandInfo.blockTimeMs = 5000;

    mqttStatus = MQTTAgent_Subscribe(&g_agent_context, &subscribeArgs, &commandInfo);
    if (mqttStatus != MQTTSuccess) {
        LOGE("MQTTAgent_Subscribe failed: %d", mqttStatus);
    } else {
        /* 等待订阅完成 */
        xSemaphoreTake(command_semaphore, pdMS_TO_TICKS(5000));
        LOGI("Subscribed to topic: %s", MQTT_TOPIC_SUB);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    LOGI("\n=== 测试 2: MQTT PUBLISH (使用 Agent API, 无回调) ===");

    /* 发布消息 - 使用 QoS 0，不等待回调 */
    publishInfo.qos = MQTTQoS0;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = "Hello from ARCS MQTT Agent!";
    publishInfo.payloadLength = strlen("Hello from ARCS MQTT Agent!");

    /* 不使用回调，直接发布 */
    commandInfo.cmdCompleteCallback = NULL;
    commandInfo.pCmdCompleteCallbackContext = NULL;
    commandInfo.blockTimeMs = 5000;

    LOGI("Publishing message to: %s", MQTT_TOPIC_PUB);
    LOGI("Payload: %s", (char *)publishInfo.pPayload);

    mqttStatus = MQTTAgent_Publish(&g_agent_context, &publishInfo, &commandInfo);
    if (mqttStatus != MQTTSuccess) {
        LOGE("✗ MQTTAgent_Publish failed: %d", mqttStatus);
    } else {
        LOGI("✓ Publish command queued successfully");
        LOGI("Note: QoS 0 message sent without waiting for ACK");
    }
    
    /* 给 Agent 任务一些时间处理命令 */
    vTaskDelay(pdMS_TO_TICKS(2000));
    LOGI("✓ Message should have been sent to broker");

    LOGI("\n=== 测试 3: 持续接收消息 ===");
    LOGI("Agent 任务将在后台持续处理消息...");

    /* 主任务保持运行,让 Agent 任务在后台处理消息 */
    while (1) {
        if (!g_agent_connected) {
            LOGE("Agent disconnected, exiting");
            break;
        }

        /* 定期发布消息 */
        vTaskDelay(pdMS_TO_TICKS(10000));

        publishInfo.pPayload = "Periodic message from MQTT Agent";
        publishInfo.payloadLength = strlen("Periodic message from MQTT Agent");

        commandInfo.cmdCompleteCallback = NULL;
        commandInfo.pCmdCompleteCallbackContext = NULL;
        commandInfo.blockTimeMs = 1000;

        mqttStatus = MQTTAgent_Publish(&g_agent_context, &publishInfo, &commandInfo);
        if (mqttStatus == MQTTSuccess) {
            LOGI("✓ Sent periodic message to %s", MQTT_TOPIC_PUB);
        } else {
            LOGE("✗ Failed to send periodic message: %d", mqttStatus);
        }
    }

    LOGI("\n========================================");
    LOGI("MQTT Agent 测试完成");
    LOGI("========================================\n");

    return 0;
}
