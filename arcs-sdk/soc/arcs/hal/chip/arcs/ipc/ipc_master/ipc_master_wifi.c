/**
 ****************************************************************************************
 *
 * @file ipc_master_wifi.c
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
#include "wlif.h"
#include "mrpc_wifi_api_client.h"



#define IPC_INIT_WIFI_TASK_STACK_SIZE      512
#define IPC_INIT_WIFI_TASK_PRIORITY        RTOS_TASK_PRIORITY(2)
#define IPC_SLAVE_TIMEOUT                  10000


static struct ipc_ccb *master_txcfm_ccb;
static struct ipc_ccb *slave_txdesc_ccb;
static struct ipc_ccb *master_rxdesc_ccb;
static struct ipc_ccb *slave_rxcfm_ccb;

IPC_FUNC_ATTR int32_t ipc_master_wifi_rxcfm_push(void *data, uint32_t size)
{
    int32_t ret = 0;

    if (ipc_send(slave_rxcfm_ccb, data, size, IPC_TIMEOUT) != IPC_ERR_OK)
        ret = -1;

    return ret;
}

IPC_FUNC_ATTR int32_t ipc_master_wifi_tx_push(void *data, uint32_t size)
{
    int32_t ret = 0;

    if (ipc_send(slave_txdesc_ccb, data, size, IPC_TIMEOUT) != IPC_ERR_OK)
        ret = -1;

    return ret;
}

IPC_FUNC_ATTR static RTOS_TASK_FCT(ipc_master_wifi_rx_task)
{
    struct ipc_rxdesc *rx;
    void (*wifi_rx_data)(void *data);

    wifi_rx_data = env;
    while (1)
    {
        if ((rx = (struct ipc_rxdesc*)ipc_get_rbuffer(master_rxdesc_ccb, NULL, -1)))
        {
            wifi_rx_data(rx->data);
            ipc_free_rbuffer(master_rxdesc_ccb, (uint8_t*)rx, 0);
        }
    }
}

IPC_FUNC_ATTR static RTOS_TASK_FCT(ipc_master_wifi_tx_cfm_task)
{
    struct ipc_txcfm *tx_cfm;
    void (*wifi_tx_data_cfm)(void *data, uint32_t status);

    wifi_tx_data_cfm = env;
    while (1)
    {
        if ((tx_cfm = (struct ipc_txcfm*)ipc_get_rbuffer(master_txcfm_ccb, NULL, -1)))
        {
            wifi_tx_data_cfm(tx_cfm->data, tx_cfm->status);
            ipc_free_rbuffer(master_txcfm_ccb, (uint8_t*)tx_cfm, 0);
        }
    }
}

int32_t ipc_master_wifi_init_tx_chan(struct ipc_master_env_tag *ipc_env)
{
    int32_t res;
    struct ipc_queue *master_txcfm_q, *slave_txdesc_q;

    master_txcfm_q = ipc_get_queue(&ipc_env->shared->txcfm.ring);
    if (master_txcfm_q == NULL)
        goto ERROR1;

    master_txcfm_ccb = ipc_chan_create(IPC_NAME("m_txcfm"), IPC_CHAN_MASTER_TXCFM, master_txcfm_q, ipc_platform_task_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_txcfm_ccb == NULL)
        goto ERROR2;

    slave_txdesc_q = ipc_get_queue(&ipc_env->shared->txdesc.ring);
    if (slave_txdesc_q == NULL)
        goto ERROR3;

    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_TXDESC, slave_txdesc_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_txdesc_ccb == NULL)
        goto ERROR4;

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type wifi_tx_task_stack_buf[LS_IPC_TX_CFM_TASK_STACK_SIZE];
    static rtos_static_task_tcb wifi_tx_task_control;
    res = rtos_task_create_static(ipc_master_wifi_tx_cfm_task, "wifi_txcfm", IPC_WIFI_TX_TASK, LS_IPC_TX_CFM_TASK_STACK_SIZE, NULL,
                           LS_IPC_TXCFM_TASK_PRIORITY, NULL, wifi_tx_task_stack_buf, &wifi_tx_task_control);
#else
    res = rtos_task_create(ipc_master_wifi_tx_cfm_task, "wifi_txcfm", IPC_WIFI_TX_TASK, LS_IPC_TX_CFM_TASK_STACK_SIZE, ipc_env->cb.wifi_tx_data_cfm,
                           LS_IPC_TXCFM_TASK_PRIORITY, NULL);
#endif
    if (!res)
        return 0;

    rtos_free(slave_txdesc_ccb);
ERROR4:
    rtos_free(slave_txdesc_q);
ERROR3:
    rtos_free(master_txcfm_ccb);
ERROR2:
    rtos_free(master_txcfm_q);
ERROR1:
    CLOGE("Failed to init tx data chan");
    return -1;
}

int32_t ipc_master_wifi_init_rx_chan(struct ipc_master_env_tag *ipc_env)
{
    int32_t res;
    struct ipc_queue *master_rxdesc_q, *slave_rxcfm_q;

    master_rxdesc_q = ipc_get_queue(&ipc_env->shared->rxdesc.ring);
    if (master_rxdesc_q == NULL)
        goto ERROR1;

    master_rxdesc_ccb = ipc_chan_create(IPC_NAME("m_rxdesc"), IPC_CHAN_MASTER_RXDESC, master_rxdesc_q, ipc_platform_task_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_rxdesc_ccb == NULL)
        goto ERROR2;

    slave_rxcfm_q = ipc_get_queue(&ipc_env->shared->rxcfm.ring);
    if (slave_rxcfm_q == NULL)
        goto ERROR3;

    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_RXCFM, slave_rxcfm_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_rxcfm_ccb == NULL)
        goto ERROR4;

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type wifi_rx_task_stack_buf[LS_IPC_RX_DATA_TASK_STACK_SIZE];
    static rtos_static_task_tcb wifi_rx_task_control;
    res = rtos_task_create_static(ipc_master_wifi_rx_task, "wifi_rxdesc", IPC_WIFI_RX_TASK, LS_IPC_RX_DATA_TASK_STACK_SIZE, NULL,
                           LS_IPC_RX_TASK_PRIORITY, NULL, wifi_rx_task_stack_buf, &wifi_rx_task_control);
#else
    res = rtos_task_create(ipc_master_wifi_rx_task, "wifi_rxdesc", IPC_WIFI_RX_TASK, LS_IPC_RX_DATA_TASK_STACK_SIZE, ipc_env->cb.wifi_rx_data,
                           LS_IPC_RX_TASK_PRIORITY, NULL);
#endif
    if (!res)
        return 0;
    rtos_free(slave_rxcfm_ccb);
ERROR4:
    rtos_free(slave_rxcfm_q);
ERROR3:
    rtos_free(master_rxdesc_ccb);
ERROR2:
    rtos_free(master_rxdesc_q);
ERROR1:
    CLOGE("Failed to init rx data ep");
    return -1;
}


static bool ipc_wait_linkup(void)
{
    int32_t i = 0;

    CLOGD("IPC slave startup ...");
    while (!ipc_master_get_link_status() && i++ < IPC_SLAVE_TIMEOUT)
        rtos_delay(1);

    if (ipc_master_get_link_status())
    {
        CLOGD("Done");
        return true;
    }
    else
    {
        CLOGE("Timeout");
        return false;
    }
}

static RTOS_TASK_FCT(ipc_master_wifi_init_task)
{
    uint8_t mac_addr[6];

    if (ipc_wait_linkup())
    {
        if (wifi_get_sta_mac(mac_addr) == LS_OK)
        {
            for (int32_t i = 0; i < WLIF_IDX_MAX; i++)
                wlif_vif_init(i, mac_addr);
            CLOGD("mac: %02x-%02x-%02x-%02x-%02x-%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
    }
    rtos_task_delete(NULL);
}

int32_t ipc_master_wifi_init(void)
{
    wlif_init();

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type ipc_wifi_init_task_stack_buf[IPC_INIT_WIFI_TASK_STACK_SIZE];
    static rtos_static_task_tcb ipc_wifi_init_task_control;
    rtos_task_create_static(ipc_wifi_init_task, "wifi_init", INIT_WIFI_TASK, IPC_INIT_WIFI_TASK_STACK_SIZE, NULL,
                           IPC_INIT_WIFI_TASK_PRIORITY, NULL, ipc_wifi_init_task_stack_buf, &ipc_wifi_init_task_control);
#else
    rtos_task_create(ipc_master_wifi_init_task, "wifi_init", INIT_WIFI_TASK, IPC_INIT_WIFI_TASK_STACK_SIZE, NULL,
                           IPC_INIT_WIFI_TASK_PRIORITY, NULL);
#endif
    return 0;
}

