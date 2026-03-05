/**
 ****************************************************************************************
 *
 * @file ipc_slave_bt.c
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
#include "mrpc.h"
#include "ic_spinlock.h"


static struct ipc_ccb *slave_txdesc_ccb;
static struct ipc_ccb *master_txcfm_ccb;
static struct ipc_ccb *slave_rxcfm_ccb;
static struct ipc_ccb *master_rxdesc_ccb;


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
static RTOS_TASK_FCT(ipc_slave_wifi_tx_task)
{
    struct ipc_txdesc *tx;
    int32_t (*wifi_tx)(void *ecb, void *param);

    wifi_tx = env;
    while (1)
    {
        if ((tx = (struct ipc_txdesc*)ipc_get_rbuffer(slave_txdesc_ccb, NULL, -1)))
        {
            wifi_tx(NULL, tx->data);
            ipc_free_rbuffer(slave_txdesc_ccb, (uint8_t*)tx, 0);
        }
    }
}

static RTOS_TASK_FCT(ipc_slave_wifi_rx_task)
{
    struct ipc_rxcfm *rx;
    int32_t (*wifi_rx_cfm)(void *ecb, void *param);

    wifi_rx_cfm = env;
    while (1)
    {
        if ((rx = (struct ipc_rxcfm*)ipc_get_rbuffer(slave_rxcfm_ccb, NULL, -1)))
        {
            wifi_rx_cfm(NULL, rx->data);
            ipc_free_rbuffer(slave_rxcfm_ccb, (uint8_t*)rx, 0);
        }
    }
}
#endif
int32_t ipc_slave_wifi_init_tx_data_chan(struct ipc_slave_env_tag *ipc_env)
{
    struct ipc_queue *slave_txdesc_q, *master_txcfm_q;

    slave_txdesc_q = ipc_get_queue(&ipc_env->shared->txdesc.ring);
    if (slave_txdesc_q == NULL)
        goto ERROR1;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_TXDESC, slave_txdesc_q, ipc_platform_task_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
#else
    slave_txdesc_ccb = ipc_chan_create(IPC_NAME("s_txdesc"), IPC_CHAN_SLAVE_TXDESC, slave_txdesc_q, ipc_env->cb.ipc_wifi_tx, NULL, 0);
#endif
    if (slave_txdesc_ccb == NULL)
        goto ERROR2;

    master_txcfm_q = ipc_get_queue(&ipc_env->shared->txcfm.ring);
    if (master_txcfm_q == NULL)
        goto ERROR3;

    master_txcfm_ccb = ipc_chan_create(IPC_NAME("m_txcfm"), IPC_CHAN_MASTER_TXCFM, master_txcfm_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_txcfm_ccb == NULL)
        goto ERROR4;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    if (rtos_task_create(ipc_slave_wifi_tx_task, "wifi_tx", IPC_WIFI_TX_TASK, LS_IPC_TX_TASK_STACK_SIZE, ipc_env->cb.ipc_wifi_tx,LS_IPC_TX_TASK_PRIORITY, NULL))
        goto ERROR5;
#else
    /*½ÓÊÕµÄÊý¾Ý»á±»·¢ËÍµ½rtos queueÖÐ£¬Ã¿´Î½ÓÊÕ¶¼ÐèÒªÖÐ¶ÏÀ´´¥·¢£¬ËùÒÔÒ»Ö±´¦ÓÚRX×´Ì¬*/
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

int32_t ipc_slave_wifi_init_rx_data_chan(struct ipc_slave_env_tag *ipc_env)
{
    struct ipc_queue *slave_rxcfm_q, *master_rxdesc_q;

    slave_rxcfm_q = ipc_get_queue(&ipc_env->shared->rxcfm.ring);
    if (slave_rxcfm_q == NULL)
        goto ERROR1;
#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_RXCFM, slave_rxcfm_q, ipc_platform_task_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
#else
    slave_rxcfm_ccb = ipc_chan_create(IPC_NAME("s_rxcfm"), IPC_CHAN_SLAVE_RXCFM, slave_rxcfm_q, ipc_env->cb.ipc_wifi_rx_cfm, NULL, 0);
#endif
    if (slave_rxcfm_ccb == NULL)
        goto ERROR2;

    master_rxdesc_q = ipc_get_queue(&ipc_env->shared->rxdesc.ring);
    if (master_rxdesc_q == NULL)
        goto ERROR3;

    master_rxdesc_ccb = ipc_chan_create(IPC_NAME("m_rxdesc"), IPC_CHAN_MASTER_RXDESC, master_rxdesc_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_rxdesc_ccb == NULL)
        goto ERROR4;

#ifdef IPC_SLAVE_DATA_CHAN_IN_USER_MODE
    if (rtos_task_create(ipc_slave_wifi_rx_task, "wifi_rxcfm", IPC_WIFI_RX_TASK, LS_IPC_RXCFM_TASK_STACK_SIZE, ipc_env->cb.ipc_wifi_rx_cfm, LS_IPC_RXCFM_TASK_PRIORITY, NULL))
        goto ERROR5;
#else
    /*½ÓÊÕµÄÊý¾Ý»á±»·¢ËÍµ½rtos queueÖÐ£¬Ã¿´Î½ÓÊÕ¶¼ÐèÒªÖÐ¶ÏÀ´´¥·¢£¬ËùÒÔÒ»Ö±´¦ÓÚRX×´Ì¬*/
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
