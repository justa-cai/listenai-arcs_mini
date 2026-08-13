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
#include "ipc_core.h"
#include "ipc_rx_task.h"
#include "ipc_master_wifi.h"


static struct ipc_ccb *master_txcfm_ccb;
static struct ipc_ccb *slave_txdesc_ccb;
static struct ipc_ccb *master_rxdesc_ccb;
static struct ipc_ccb *slave_rxcfm_ccb;
static struct ipc_master_wifi_ops ipc_master_wifi_ops;
static uint8_t ipc_wifi_share_ring[IPC_WIFI_SHARE_SIZE] __IPC_WIFI_SHARE;

extern struct ipc_shared_env_tag ipc_shared_env;

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

IPC_FUNC_ATTR static int32_t ipc_wifi_recv_rx_data_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_wifi_rxdesc *rx;

    rx = (struct ipc_wifi_rxdesc*)desc->data;
    ipc_master_wifi_ops.rx_data(rx->data);

    return ret;
}

IPC_FUNC_ATTR static int32_t ipc_wifi_recv_tx_cfm_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_wifi_txcfm *tx_cfm;

    tx_cfm = (struct ipc_wifi_txcfm*)desc->data;
    ipc_master_wifi_ops.tx_data_cfm(tx_cfm->data, tx_cfm->status);

    return ret;
}

static int32_t ipc_master_wifi_init_tx_chan(void)
{
    struct ipc_queue *master_txcfm_q, *slave_txdesc_q;
    struct ipc_rx_task_env *ipc_rx_task_env;

    master_txcfm_q = ipc_get_queue(&ipc_shared_env.wifi.txcfm.ring);
    if (master_txcfm_q == NULL)
        goto ERROR1;

    master_txcfm_ccb = ipc_chan_create(IPC_NAME("m_txcfm"), IPC_CHAN_MASTER_WIFI_TXCFM, master_txcfm_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_txcfm_ccb == NULL)
        goto ERROR2;

    slave_txdesc_q = ipc_get_queue(&ipc_shared_env.wifi.txdesc.ring);
    if (slave_txdesc_q == NULL)
        goto ERROR3;

    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_WIFI_TXDESC, slave_txdesc_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_txdesc_ccb == NULL)
        goto ERROR4;

    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        int32_t res;

        ipc_rx_task_env->ccb = master_txcfm_ccb;
        ipc_rx_task_env->callback = ipc_wifi_recv_tx_cfm_callback;
        ipc_rx_task_env->param = NULL;
#ifdef TASK_CREATE_STATIC
        static rtos_stack_type wifi_tx_task_stack_buf[LS_IPC_TX_CFM_TASK_STACK_SIZE];
        static rtos_static_task_tcb wifi_tx_task_control;
        res = rtos_task_create_static(ipc_rx_task, "wifi_txcfm", IPC_WIFI_TX_TASK, LS_IPC_TX_CFM_TASK_STACK_SIZE, ipc_rx_task_env,
                            LS_IPC_TXCFM_TASK_PRIORITY, NULL, wifi_tx_task_stack_buf, &wifi_tx_task_control);
#else
        res = rtos_task_create(ipc_rx_task, "wifi_txcfm", IPC_WIFI_TX_TASK, LS_IPC_TX_CFM_TASK_STACK_SIZE, ipc_rx_task_env,
                            LS_IPC_TXCFM_TASK_PRIORITY, NULL);
#endif
        if (!res)
            return 0;
    }
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

static int32_t ipc_master_wifi_init_rx_chan(void)
{
    struct ipc_queue *master_rxdesc_q, *slave_rxcfm_q;
    struct ipc_rx_task_env *ipc_rx_task_env;

    master_rxdesc_q = ipc_get_queue(&ipc_shared_env.wifi.rxdesc.ring);
    if (master_rxdesc_q == NULL)
        goto ERROR1;

    master_rxdesc_ccb = ipc_chan_create(IPC_NAME("m_rxdesc"), IPC_CHAN_MASTER_WIFI_RXDESC, master_rxdesc_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_rxdesc_ccb == NULL)
        goto ERROR2;

    slave_rxcfm_q = ipc_get_queue(&ipc_shared_env.wifi.rxcfm.ring);
    if (slave_rxcfm_q == NULL)
        goto ERROR3;

    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_WIFI_RXCFM, slave_rxcfm_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_rxcfm_ccb == NULL)
        goto ERROR4;

    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        int32_t res;

        ipc_rx_task_env->ccb = master_rxdesc_ccb;
        ipc_rx_task_env->callback = ipc_wifi_recv_rx_data_callback;
        ipc_rx_task_env->param = NULL;
#ifdef TASK_CREATE_STATIC
        static rtos_stack_type wifi_rx_task_stack_buf[LS_IPC_RX_DATA_TASK_STACK_SIZE];
        static rtos_static_task_tcb wifi_rx_task_control;
        res = rtos_task_create_static(ipc_rx_task, "wifi_rxdesc", IPC_WIFI_RX_TASK, LS_IPC_RX_DATA_TASK_STACK_SIZE, ipc_rx_task_env,
                            LS_IPC_RX_TASK_PRIORITY, NULL, wifi_rx_task_stack_buf, &wifi_rx_task_control);
#else
        res = rtos_task_create(ipc_rx_task, "wifi_rxdesc", IPC_WIFI_RX_TASK, LS_IPC_RX_DATA_TASK_STACK_SIZE, ipc_rx_task_env,
                            LS_IPC_RX_TASK_PRIORITY, NULL);
#endif
        if (!res)
            return 0;
    }

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

int32_t ipc_master_wifi_init(const struct ipc_master_wifi_ops *ops)
{
    int32_t res = 0;

    if (ops == NULL || ops->tx_data_cfm == NULL || ops->rx_data == NULL)
        return -1;

    ipc_master_wifi_ops = *ops;
    memset(ipc_wifi_share_ring, 0, IPC_WIFI_SHARE_SIZE);

    res  = ipc_master_wifi_init_tx_chan();
    res |= ipc_master_wifi_init_rx_chan();

    return res;
}
