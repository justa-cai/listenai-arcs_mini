/**
 ****************************************************************************************
 *
 * @file ipc_master_utils.c
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdbool.h>
#include "rtos_al.h"
#include "ls_rtos.h"
#include "ipc_master.h"
#ifdef CFG_AMP_IPC_WIFI_CHAN
#include "wlif.h"
#include "mrpc_wifi_api_client.h"
#endif
#include "ls_event.h"


#define IPC_INIT_WIFI_TASK_STACK_SIZE      512
#define IPC_INIT_WIFI_TASK_PRIORITY        RTOS_TASK_PRIORITY(2)
#define IPC_SLAVE_TIMEOUT                  10000

static bool mac_addr_is_zero(uint8_t mac_addr[6]);

static uint8_t g_mac_addr[6] = {0};

static void ipc_event_handler(struct cfg_ind_event *event)
{
    ls_event_post(event->module_id, event->event_id, event->event_data, event->event_data_size, LS_NEVER_TIMEOUT, false);
}

int32_t ipc_indication_handler(struct ipc_msg_desc *desc, void *arg)
{
    char *buf;
    uint16_t id;
    uint32_t res, len;
    struct ipc_msg_hdr *msg = (struct ipc_msg_hdr*)desc->data;

    switch (msg->id)
    {
        case IPC_IND_EVENT:
            ipc_event_handler((struct cfg_ind_event*)msg);
            break;
        case IPC_IND_PRINT:
            if (msg->len >= (IPC_A2C_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr)))
                len = IPC_A2C_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr) - 1;
            else
                len = msg->len;
#if 0
            id  = (uint16_t)msg->data[0];
            buf = (char*)msg->data;
            buf[len] = 0;
            buf += sizeof(msg->id);
#else
            buf = (char*)msg->data;
            buf[len] = 0;
#endif
            logDbg("%s", buf);
            break;
        default:
            break;
    }

    return IPC_MSG_RELEASE;
}

static bool ipc_wait_linkup(void)
{
    int32_t i = 0;

    CLOGI("WiFi IPC startup ...");
    while (!ipc_master_get_link_status() && i++ < IPC_SLAVE_TIMEOUT)
        rtos_delay(1);

    if (ipc_master_get_link_status())
    {
        CLOGI("WiFi IPC Done");
        return true;
    }
    else
    {
        CLOGE("WiFi IPC Timeout");
        ASSERT_ERR(0);
        return false;
    }
}

static void ipc_init_config(void)
{
    struct ipc_config config;

    memset(&config, 0, sizeof(config));
    ipc_master_init_config(&config);
}
#ifdef CFG_AMP_IPC_WIFI_CHAN
static RTOS_TASK_FCT(ipc_wifi_init_task)
{
    uint8_t mac_addr[6];

    if (ipc_wait_linkup())
    {
        if (g_mac_addr != NULL && !mac_addr_is_zero(g_mac_addr)) {
            if (wifi_mac_set(g_mac_addr) != LS_OK) {
                CLOGE("Set MAC address failed\r\n");
                assert(0);
                return;
            }
        }

        if (wifi_get_sta_mac(mac_addr) == LS_OK)
        {
            for (int32_t i = 0; i < WLIF_IDX_MAX; i++)
                wlif_vif_init(i, mac_addr);
            CLOGI("mac: %02x-%02x-%02x-%02x-%02x-%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
    }
    rtos_task_delete(NULL);
}

int32_t ipc_wifi_init(void)
{
    ipc_init_config();
    wlif_init();

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type ipc_wifi_init_task_stack_buf[IPC_INIT_WIFI_TASK_STACK_SIZE];
    static rtos_static_task_tcb ipc_wifi_init_task_control;
    rtos_task_create_static(ipc_wifi_init_task, "wifi_init", INIT_WIFI_TASK, IPC_INIT_WIFI_TASK_STACK_SIZE, NULL,
                           IPC_INIT_WIFI_TASK_PRIORITY, NULL, ipc_wifi_init_task_stack_buf, &ipc_wifi_init_task_control);
#else
    rtos_task_create(ipc_wifi_init_task, "wifi_init", INIT_WIFI_TASK, IPC_INIT_WIFI_TASK_STACK_SIZE, NULL,
                           IPC_INIT_WIFI_TASK_PRIORITY, NULL);
#endif
    return 0;
}

void ipc_wifi_mac_pre_set(uint8_t mac_addr[6])
{
    memcpy(g_mac_addr, mac_addr, 6);
}

static bool mac_addr_is_zero(uint8_t mac_addr[6])
{
    for (int32_t i = 0; i < 6; i++)
    {
        if (mac_addr[i] != 0)
            return false;
    }
    return true;
}

#endif
