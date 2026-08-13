/****************************************************************************************
 *
 * @file mqtt_test.c
 *
 *
 * Copyright (C) ListenAI 2026
 *
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "arcs_ap.h"
#include "rtos_al.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"


#define MQTT_ACK_TIMEOUT_MS          5000
#define MQTT_HEART_BEAT_PERIOD_MS    15000
#define MQTT_KEEP_ALIVE              60//(MQTT_HEART_BEAT_PERIOD_MS*2)


static mqtt_client_t *static_client;
static const char *heartbeat_topic = "listenai/arcsd/heartbeat";
static const char *reply_topic = "listenai/arcsd/reply";
static bool mqtt_enable = 0;
static ip_addr_t mqtt_sever_ip;
static rtos_semaphore mqtt_sem;




static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    if (strncmp((char *)data, "ACK", 3) == 0)
    {
        rtos_semaphore_signal(mqtt_sem, 0);
        CLOGD("Heartbeat ACK received.\n");
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    //CLOGD("Incoming publish at topic %s\n", topic);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        CLOGD("Connected to Broker!\n");

        // 设置收到消息的回调
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);

        // 订阅回复话题
        mqtt_subscribe(client, reply_topic, 0, NULL, NULL);
    }
    else
    {
        CLOGD("Connect failed, status: %d\n", status);
    }
}

static void mqtt_test_start_mqtt_client(ip_addr_t *broker_ip)
{
    static_client = mqtt_client_new();
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = "LS_ARCSD_Board";
    ci.keep_alive = MQTT_KEEP_ALIVE;

    CLOGD("Connecting to Broker %s...\n", ipaddr_ntoa(broker_ip));
    mqtt_client_connect(static_client, broker_ip, 1883, mqtt_connection_cb, NULL, &ci);
}

static void mqtt_test_task(void *pvParameters)
{
    rtos_semaphore_create(&mqtt_sem, 1, 0);
    mqtt_test_start_mqtt_client(&mqtt_sever_ip);

    CLOGD("mqtt test task started\n");
    while (1)
    {
        if (mqtt_client_is_connected(static_client))
        {
            const char *payload = "ping";
            err_t err = mqtt_publish(static_client, heartbeat_topic, payload, strlen(payload), 0, 0, NULL, NULL);

            if (err == ERR_OK)
            {
                if (rtos_semaphore_wait(mqtt_sem, MQTT_ACK_TIMEOUT_MS))
                {
                    CLOGD("[Warning]: Heartbeat Timeout!\n");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(MQTT_HEART_BEAT_PERIOD_MS));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (!mqtt_enable)
            break;
    }
    CLOGD("mqtt test task exited\n");
    mqtt_enable = 0;
    mqtt_disconnect(static_client);
    rtos_task_delete(NULL);
}

static void mqtt_test_start_main(const ip_addr_t *ip)
{
    mqtt_sever_ip = *ip;
    rtos_task_create(mqtt_test_task, "mqtt_test", APP_INIT_TASK, 256, NULL, 3, NULL);
    mqtt_enable = 1;
}

static void mqtt_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
        mqtt_test_start_main(ipaddr);
    else
        CLOGD("DNS: 域名 [%s] 解析失败!\n", name);
}

void mqtt_test_start(char* host)
{
    if (host == NULL)
    {
        mqtt_enable = 0;
    }
    else if (!mqtt_enable)
    {
        ip_addr_t ip;

        if (ipaddr_aton(host, &ip))
        {
            mqtt_test_start_main(&ip);
        }
        else
        {
            err_t err = dns_gethostbyname(host, &ip, mqtt_dns_found_cb, NULL);
            if (err == ERR_OK)
            {
                mqtt_test_start_main(&ip);
            }
            else if (err != ERR_INPROGRESS)
            {
                CLOGD("DNS: 域名 [%s] 解析失败!\n", host);
            }
        }
    }
}