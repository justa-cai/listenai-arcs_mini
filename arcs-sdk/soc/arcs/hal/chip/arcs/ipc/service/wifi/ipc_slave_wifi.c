/**
 ****************************************************************************************
 *
 * @file ipc_slave_wifi.c
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
#include "platform.h"
#include "ipc_slave.h"
#include "ipc_core.h"
#include "ipc_rx_task.h"
#include "ipc_slave_wifi.h"
#include "mrpc.h"
#include "ic_spinlock.h"


static struct ipc_ccb *slave_txdesc_ccb;
static struct ipc_ccb *master_txcfm_ccb;
static struct ipc_ccb *slave_rxcfm_ccb;
static struct ipc_ccb *master_rxdesc_ccb;
static struct ipc_slave_wifi_ops ipc_slave_wifi_ops;

extern struct ipc_shared_env_tag ipc_shared_env;


int32_t ipc_slave_wifi_rxbuf_check(void)
{
    int32_t ret = 0;

    ret = ipc_buf_full(master_rxdesc_ccb);

    return !ret;
}

int32_t ipc_slave_wifi_rxdesc_push(void *data, int32_t size)
{
    ipc_send(master_rxdesc_ccb, data, size, -1);

    return 0;
}

int32_t ipc_slave_wifi_txcfm_push(void *data, int32_t size)
{
    ipc_send(master_txcfm_ccb, data, size, -1);

    return 0;
}
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
IPC_FUNC_ATTR static int32_t ipc_wifi_recv_tx_data_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_wifi_txdesc *tx;

    tx = (struct ipc_wifi_txdesc*)desc->data;
    ipc_slave_wifi_ops.tx(NULL, tx->data);

    return ret;
}

IPC_FUNC_ATTR static int32_t ipc_wifi_recv_rx_cfm_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_wifi_rxcfm *rx_cfm;

    rx_cfm = (struct ipc_wifi_rxcfm*)desc->data;
    ipc_slave_wifi_ops.rx_cfm(NULL, rx_cfm->data);

    return ret;
}
#endif
static int32_t ipc_slave_wifi_init_tx_chan(void)
{
    struct ipc_queue *slave_txdesc_q, *master_txcfm_q;

    slave_txdesc_q = ipc_get_queue(&ipc_shared_env.wifi.txdesc.ring);
    if (slave_txdesc_q == NULL)
        goto ERROR1;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_WIFI_TXDESC, slave_txdesc_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
#else
    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_WIFI_TXDESC, slave_txdesc_q, ipc_slave_wifi_ops.tx, NULL, 0);
#endif
    if (slave_txdesc_ccb == NULL)
        goto ERROR2;

    master_txcfm_q = ipc_get_queue(&ipc_shared_env.wifi.txcfm.ring);
    if (master_txcfm_q == NULL)
        goto ERROR3;

    master_txcfm_ccb = ipc_chan_create(IPC_NAME("m_txcfm"), IPC_CHAN_MASTER_WIFI_TXCFM, master_txcfm_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_txcfm_ccb == NULL)
        goto ERROR4;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    struct ipc_rx_task_env *ipc_rx_task_env;
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        ipc_rx_task_env->ccb = slave_txdesc_ccb;
        ipc_rx_task_env->callback = ipc_wifi_recv_tx_data_callback;
        ipc_rx_task_env->param = NULL;
        if (rtos_task_create(ipc_rx_task, "wifi_tx", IPC_WIFI_TX_TASK, LS_IPC_TX_TASK_STACK_SIZE, ipc_rx_task_env, LS_IPC_TX_TASK_PRIORITY, NULL))
            goto ERROR5;
    }
#else
    ipc_status_set(slave_txdesc_ccb, IPC_QUEUE_STATUS_RX);
#endif
    return 0;

#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
ERROR5:
    rtos_free(master_txcfm_ccb);
#endif
ERROR4:
    rtos_free(master_txcfm_q);
ERROR3:
    rtos_free(slave_txdesc_ccb);
ERROR2:
    rtos_free(slave_txdesc_q);
ERROR1:
    return -1;
}

static int32_t ipc_slave_wifi_init_rx_chan(void)
{
    struct ipc_queue *slave_rxcfm_q, *master_rxdesc_q;

    slave_rxcfm_q = ipc_get_queue(&ipc_shared_env.wifi.rxcfm.ring);
    if (slave_rxcfm_q == NULL)
        goto ERROR1;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_WIFI_RXCFM, slave_rxcfm_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
#else
    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_WIFI_RXCFM, slave_rxcfm_q, ipc_slave_wifi_ops.rx_cfm, NULL, 0);
#endif
    if (slave_rxcfm_ccb == NULL)
        goto ERROR2;

    master_rxdesc_q = ipc_get_queue(&ipc_shared_env.wifi.rxdesc.ring);
    if (master_rxdesc_q == NULL)
        goto ERROR3;

    master_rxdesc_ccb = ipc_chan_create(IPC_NAME("m_rxdesc"), IPC_CHAN_MASTER_WIFI_RXDESC, master_rxdesc_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_rxdesc_ccb == NULL)
        goto ERROR4;

#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    struct ipc_rx_task_env *ipc_rx_task_env;
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        ipc_rx_task_env->ccb = slave_rxcfm_ccb;
        ipc_rx_task_env->callback = ipc_wifi_recv_rx_cfm_callback;
        ipc_rx_task_env->param = NULL;
        if (rtos_task_create(ipc_rx_task, "wifi_rxcfm", IPC_WIFI_RX_TASK, LS_IPC_RXCFM_TASK_STACK_SIZE, ipc_rx_task_env, LS_IPC_RXCFM_TASK_PRIORITY, NULL))
            goto ERROR5;
    }
#else
    ipc_status_set(slave_rxcfm_ccb, IPC_QUEUE_STATUS_RX);
#endif
    return 0;

#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
ERROR5:
    rtos_free(master_rxdesc_ccb);
#endif
ERROR4:
    rtos_free(master_rxdesc_q);
ERROR3:
    rtos_free(slave_rxcfm_ccb);
ERROR2:
    rtos_free(slave_rxcfm_q);
ERROR1:
    return -1;
}

int32_t ipc_slave_wifi_init(const struct ipc_slave_wifi_ops *ops)
{
    int32_t res = 0;

    if (ops == NULL || ops->tx == NULL || ops->rx_cfm == NULL)
        return -1;

    ipc_slave_wifi_ops = *ops;
    res  = ipc_slave_wifi_init_tx_chan();
    res |= ipc_slave_wifi_init_rx_chan();

    return res;
}
