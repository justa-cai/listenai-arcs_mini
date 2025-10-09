/**
 ****************************************************************************************
 *
 * @file ipc_master.c
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdbool.h>
#include "ls_rtos.h"
#include "ipc_master.h"
#include "ls_event.h"
#include "mrpc.h"
#ifdef CFG_AMP_IPC_MRPC_SERVER_LWIP
#include "mrpc_lwip_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_NVS
#include "mrpc_nvs_api_server.h"
#endif
#ifdef IPC_TEST_CASE
#include "ipc_test.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
#include "mrpc_flash_if_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS
#include "mrpc_utils_api_server.h"
#endif

static struct ipc_master_env_tag ipc_master_env;
struct ipc_shared_env_tag ipc_shared_env __SHAREDRAM_AMP_IPC_ENV;

static struct ipc_ccb *master_msg_ccb;
static struct ipc_ccb *slave_msg_ccb;
#ifdef CFG_AMP_IPC_WIFI_CHAN
static struct ipc_ccb *master_txcfm_ccb;
static struct ipc_ccb *slave_txdesc_ccb;
static struct ipc_ccb *master_rxdesc_ccb;
static struct ipc_ccb *slave_rxcfm_ccb;
#endif
static struct ipc_ccb *master_fast_ccb;
static struct ipc_ccb *slave_fast_ccb;

uint8_t wifi_share_ring[IPC_WIFI_SHARE_SIZE]  __IPC_WIFI_SHARE;

#ifdef  CFG_IPC_PRINT
extern void rtos_ipc_dbg_task_resume(int32_t isr);
extern void ipc_dbg_init(volatile struct ipc_dbg_tag *buffer);
#endif

int32_t ipc_master_send_msg(struct ipc_ep *ep, void *data, uint32_t len, void *resp)
{
    int32_t ret = 0;

    if (ipc_msg_send(ep, data, len, resp) != IPC_ERR_OK)
        ret = -1;

    return ret;
}

struct ipc_ep* ipc_master_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg)
{
    return ipc_ep_register(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, ep_idx, IPC_EP_ANY, handler, arg);
}

static int32_t ipc_master_fast_notify_handler(void *ccb, void *fast_notify_status)
{
    uint32_t notify;

    notify = *((uint32_t*)fast_notify_status);
    *((uint32_t*)fast_notify_status) = 0;

    if (notify & IPC_EVT_LINKUP)
    {
        ipc_master_env.link_state = true;
        CLOGD("IPC link up");
    }

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
    if (notify & IPC_EVT_HALT)
    {
        ipc_halt_by_peer(true);
    }
#endif

#ifdef  CFG_IPC_PRINT
    #error "not support CFG_IPC_PRINT"
    if (notify & IPC_EVT_PRINT)
    {
        rtos_ipc_dbg_task_resume(1);
    }
#endif

    return 0;
}

bool ipc_master_get_link_status(void)
{
    if ((ipc_master_env.link_state == 0) && (ipc_get_fast_notify_status() & IPC_EVT_LINKUP))
    {
        ipc_master_env.link_state = true;
    }
    return ipc_master_env.link_state;
}
#ifdef CFG_AMP_IPC_WIFI_CHAN
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

    while (1)
    {
        if ((rx = (struct ipc_rxdesc*)ipc_get_rbuffer(master_rxdesc_ccb, NULL, -1)))
        {
            ipc_master_env.cb.wifi_rx_data_ind(rx->data);
            ipc_free_rbuffer(master_rxdesc_ccb, (uint8_t*)rx, 0);
        }
    }
}

IPC_FUNC_ATTR static RTOS_TASK_FCT(ipc_master_wifi_tx_cfm_task)
{
    struct ipc_txcfm *tx_cfm;

    while (1)
    {
        if ((tx_cfm = (struct ipc_txcfm*)ipc_get_rbuffer(master_txcfm_ccb, NULL, -1)))
        {
            ipc_master_env.cb.wifi_tx_data_cfm(tx_cfm->data, tx_cfm->status);
            ipc_free_rbuffer(master_txcfm_ccb, (uint8_t*)tx_cfm, 0);
        }
    }
}
#endif
IPC_FUNC_ATTR static RTOS_TASK_FCT(ipc_master_msg_task)
{
    uint8_t *msg;
    int32_t ret;
    struct ipc_msg_desc desc;

    while (1)
    {
        if ((msg = ipc_get_rbuffer(master_msg_ccb, &desc, -1)))
        {
            ret = IPC_MSG_RELEASE;
            if (!ipc_msg_process(&desc))
                ret = ipc_ep_process(master_msg_ccb, &desc);
            if (ret == IPC_MSG_RELEASE)
                ipc_free_rbuffer(master_msg_ccb, msg, 0);
        }
    }
}

static int32_t ipc_master_init_msg_chan(ipc_chan_callback_t cb)
{
    int32_t res;
    struct ipc_queue *master_msg_q, *slave_msg_q;

    master_msg_q = ipc_shared_queue_init(true, &ipc_shared_env.msg_c2a_buf.ring, ipc_shared_env.msg_c2a_buf.items, sizeof(struct ipc_epmsg_c2a_msg), IPC_MSGC2A_BUF_CNT);
    if (master_msg_q == NULL)
        goto ERROR1;

    master_msg_ccb = ipc_chan_create(IPC_NAME("m_msg"), IPC_CHAN_MASTER_MSG, master_msg_q, cb, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_msg_ccb == NULL)
        goto ERROR2;

    slave_msg_q = ipc_shared_queue_init(true, &ipc_shared_env.msg_a2c_buf.ring, ipc_shared_env.msg_a2c_buf.items, sizeof(struct ipc_epmsg_a2c_msg), IPC_MSGA2C_BUF_CNT);
    if (slave_msg_q == NULL)
        goto ERROR3;

    slave_msg_ccb = ipc_chan_create(IPC_NAME("s_msg"), IPC_CHAN_SLAVE_MSG, slave_msg_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_msg_ccb == NULL)
        goto ERROR4;

    ipc_msg_mgmt_init((void*)slave_msg_ccb, IPC_MSG_POOL_SIZE);
#ifdef TASK_CREATE_STATIC
    static rtos_stack_type ipc_msg_task_stack_buf[LS_IPC_MSG_TASK_STACK_SIZE];
    static rtos_static_task_tcb ipc_msg_task_control;
    res = rtos_task_create_static(ipc_master_msg_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, NULL,
                   LS_IPC_MSG_TASK_PRIORITY, NULL, ipc_msg_task_stack_buf, &ipc_msg_task_control);
#else
    res = rtos_task_create(ipc_master_msg_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, NULL,
                   LS_IPC_MSG_TASK_PRIORITY, NULL);
#endif
    if (!res)
        return 0;

ERROR5:
    rtos_free(slave_msg_ccb);
ERROR4:
    rtos_free(slave_msg_q);
ERROR3:
    rtos_free(master_msg_ccb);
ERROR2:
    rtos_free(master_msg_q);
ERROR1:
    CLOGE("Failed to init msg ep");
    return -1;
}

static int32_t ipc_master_init_fast_chan(ipc_chan_callback_t cb)
{
    master_fast_ccb = ipc_chan_create(IPC_NAME("m_fast"), IPC_CHAN_MASTER_FAST, NULL, cb, (void*)&ipc_shared_env.master_status.fast_notify_status, IPC_CHAN_FLAGS_FAST);
    if (master_fast_ccb == NULL)
        goto ERROR1;

    slave_fast_ccb = ipc_chan_create(IPC_NAME("s_fast"), IPC_CHAN_SLAVE_FAST, NULL, NULL, NULL, (IPC_CHAN_FLAGS_FAST | IPC_CHAN_FLAGS_REMOTE));
    if (slave_fast_ccb == NULL)
        goto ERROR2;

    return 0;

ERROR2:
    rtos_free(master_fast_ccb);
ERROR1:
    CLOGE("Failed to init msg chan");
    return -1;
}
#ifdef CFG_AMP_IPC_WIFI_CHAN
static int32_t ipc_master_init_wifi_tx_chan(ipc_chan_callback_t cb)
{
    int32_t res;
    struct ipc_queue *master_txcfm_q, *slave_txdesc_q;

    master_txcfm_q = ipc_shared_queue_init(true, &ipc_shared_env.txcfm.ring, ipc_shared_env.txcfm.items, sizeof(struct ipc_epmsg_txcfm), IPC_TXCFM_CNT);
    if (master_txcfm_q == NULL)
        goto ERROR1;

    master_txcfm_ccb = ipc_chan_create(IPC_NAME("m_txcfm"), IPC_CHAN_MASTER_TXCFM, master_txcfm_q, cb, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_txcfm_ccb == NULL)
        goto ERROR2;

    slave_txdesc_q = ipc_shared_queue_init(true, &ipc_shared_env.txdesc.ring, ipc_shared_env.txdesc.items, sizeof(struct ipc_epmsg_txdesc), IPC_TXDESC_CNT);
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
    res = rtos_task_create(ipc_master_wifi_tx_cfm_task, "wifi_txcfm", IPC_WIFI_TX_TASK, LS_IPC_TX_CFM_TASK_STACK_SIZE, NULL,
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

static int32_t ipc_master_init_wifi_rx_chan(ipc_chan_callback_t cb)
{
    int32_t res;
    struct ipc_queue *master_rxdesc_q, *slave_rxcfm_q;

    master_rxdesc_q = ipc_shared_queue_init(true, &ipc_shared_env.rxdesc.ring, ipc_shared_env.rxdesc.items, sizeof(struct ipc_epmsg_rxdesc), IPC_RXDESC_CNT);
    if (master_rxdesc_q == NULL)
        goto ERROR1;

    master_rxdesc_ccb = ipc_chan_create(IPC_NAME("m_rxdesc"), IPC_CHAN_MASTER_RXDESC, master_rxdesc_q, cb, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_rxdesc_ccb == NULL)
        goto ERROR2;

    slave_rxcfm_q = ipc_shared_queue_init(true, &ipc_shared_env.rxcfm.ring, ipc_shared_env.rxcfm.items, sizeof(struct ipc_epmsg_rxcfm), IPC_RXCFM_CNT);
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
    res = rtos_task_create(ipc_master_wifi_rx_task, "wifi_rxdesc", IPC_WIFI_RX_TASK, LS_IPC_RX_DATA_TASK_STACK_SIZE, NULL,
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
#endif

int32_t ipc_master_init(struct ipc_master_cb_tag *cb)
{
    int32_t res;
    struct mrpc_server_env *mrpc_server;

    ipc_init(CORE_ID_MASTER, &ipc_shared_env.master_status, &ipc_shared_env.slave_status);
    /*创建msg, fast, wifi tx, wifi rx IPC channel*/
    res  = ipc_master_init_msg_chan(ipc_platform_task_notify);
    res |= ipc_master_init_fast_chan(ipc_master_fast_notify_handler);
#ifdef CFG_AMP_IPC_WIFI_CHAN
    res |= ipc_master_init_wifi_tx_chan(ipc_platform_task_notify);
    res |= ipc_master_init_wifi_rx_chan(ipc_platform_task_notify);
#endif
    IPC_ASSERT(res == 0);

    ipc_master_env.config  = (uint32_t*)ipc_shared_env.config;
    ipc_master_env.ipc_env = &ipc_shared_env;
    /*基于msg channel建立indication endpoint用于接收通知*/
    if (cb)
    {
        ipc_master_env.cb = *cb;
        ipc_master_ep_register(IPC_EP_IND, cb->indication_handler, NULL);
    }

#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_WL_SRV);
#ifdef CFG_AMP_IPC_MRPC_SERVER_LWIP
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_LWIP, mrpc_msg_lwip_handlers, MRPC_MSG_ID_LWIP_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_NVS
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_NVS, mrpc_msg_nvs_handlers, MRPC_MSG_ID_NVS_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_FLASH_IF, mrpc_msg_flash_if_handlers, MRPC_MSG_ID_FLASH_IF_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_UTILS, mrpc_msg_utils_handlers, MRPC_MSG_ID_UTILS_MAX);
#endif
#endif

#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_WL_CLT, IPC_EP_MRPC_WL_SRV);
#endif
    memset(wifi_share_ring, 0, IPC_WIFI_SHARE_SIZE);
#if defined(CFG_AMP_IPC_HALT_PEER_CORE) || defined(CFG_AMP_IPC_HALT_BY_PEER_CORE)
    ipc_halt_peer_init();
#endif
#ifdef CFG_IPC_PRINT
    ipc_dbg_init(&ipc_shared_env.dbg_buffer);
#endif
#ifdef IPC_TEST_CASE
    ipc_test_case_init(mrpc_server);
#endif

    return res;
}

int32_t ipc_master_init_config(struct ipc_config *config)
{
    uint8_t* ptr = (uint8_t*)(ipc_master_env.config);
    struct ipc_config_item *item;

    item      = (struct ipc_config_item*)ptr;
    item->id  = IPC_CFG_END;
    item->len = 0;

    ipc_master_env.ipc_env->state = IPC_READY;

    return 0;
}
