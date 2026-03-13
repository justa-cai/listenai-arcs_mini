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
#include "ic_spinlock.h"
#include "ls_event.h"
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
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M
#include "mrpc_utils_s2m_api_server.h"
#endif

static struct ipc_master_env_tag ipc_master_env;
struct ipc_shared_env_tag ipc_shared_env __SHAREDRAM_AMP_IPC_ENV;

static struct ipc_ccb *master_msg_ccb;
static struct ipc_ccb *slave_msg_ccb;

static struct ipc_ccb *master_fast_ccb;
static struct ipc_ccb *slave_fast_ccb;

uint8_t wifi_share_ring[IPC_WIFI_SHARE_SIZE]  __IPC_WIFI_SHARE;

#ifdef  CFG_IPC_PRINT
extern void rtos_ipc_dbg_task_resume(int32_t isr);
extern void ipc_dbg_init(volatile struct ipc_dbg_tag *buffer);
#endif


static void ipc_master_event_handler(struct cfg_ind_event *event)
{
    ls_event_post(event->module_id, event->event_id, event->event_data, event->event_data_size, LS_NEVER_TIMEOUT, false);
}

int32_t ipc_master_indication_handler(struct ipc_msg_desc *desc, void *arg)
{
    char *buf;
    uint16_t id;
    uint32_t res, len;
    struct ipc_msg_hdr *msg = (struct ipc_msg_hdr*)desc->data;

    switch (msg->id)
    {
        case IPC_IND_EVENT:
            ipc_master_event_handler((struct cfg_ind_event*)msg);
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

static int32_t ipc_master_fast_notify_handler(void *ccb, void *fast_notify)
{
    uint32_t notify;

    ic_spin_lock(IC_SPIN_LOCK_TYPE_IPC);
    notify = *((uint32_t*)fast_notify);
    *((uint32_t*)fast_notify) = 0;
    ic_spin_unlock(IC_SPIN_LOCK_TYPE_IPC);

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
    if (notify & IPC_EVT_PRINT)
    {
        rtos_ipc_dbg_task_resume(1);
    }
#endif

#if CONFIG_PM
    if (notify & IPC_EVT_VRTC_ALERT)
    {
        ipc_set_app_status(IPC_APP_STATUS_VRTC_ALERT);
    }
#endif
    return 0;
}

bool ipc_master_get_link_status(void)
{
    if ((ipc_master_env.link_state == 0) && (ipc_get_fast_notify_state() & IPC_EVT_LINKUP))
    {
        ipc_master_env.link_state = true;
    }
    return ipc_master_env.link_state;
}

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

    master_msg_q = ipc_get_queue(&ipc_shared_env.msg_c2a_buf.ring);
    if (master_msg_q == NULL)
        goto ERROR1;

    master_msg_ccb = ipc_chan_create(IPC_NAME("m_msg"), IPC_CHAN_MASTER_MSG, master_msg_q, cb, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_msg_ccb == NULL)
        goto ERROR2;

    slave_msg_q = ipc_get_queue(&ipc_shared_env.msg_a2c_buf.ring);
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
    master_fast_ccb = ipc_chan_create(IPC_NAME("m_fast"), IPC_CHAN_MASTER_FAST, NULL, cb, (void*)&ipc_shared_env.master_notify.state, IPC_CHAN_FLAGS_FAST);
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

volatile struct amp_shared_info* ipc_get_amp_shared_info(void)
{
    return &ipc_shared_env.amp_shared;
}

static int32_t ipc_master_init_config(void)
{
    uint8_t* ptr = (uint8_t*)(ipc_master_env.config);
    struct ipc_config_item *item;

    item      = (struct ipc_config_item*)ptr;
    item->id  = IPC_CFG_END;
    item->len = 0;

    ipc_master_env.shared->state = IPC_READY;

    return 0;
}

int32_t ipc_master_init(struct ipc_master_cb_tag *cb)
{
    int32_t res;
    struct mrpc_server_env *mrpc_server;

    ipc_master_env.config = (uint32_t*)ipc_shared_env.config;
    ipc_master_env.shared = &ipc_shared_env;
    if (cb) {
        ipc_master_env.cb  = *cb;
    }
    ipc_init(CORE_ID_MASTER, &ipc_shared_env.master_notify, &ipc_shared_env.slave_notify);
    /*´´½¨msg, fast, wifi tx, wifi rx IPC channel*/
    res  = ipc_master_init_msg_chan(ipc_platform_task_notify);
    res |= ipc_master_init_fast_chan(ipc_master_fast_notify_handler);
#ifdef CFG_AMP_IPC_WIFI_CHAN
    res |= ipc_master_wifi_init_tx_chan(&ipc_master_env);
    res |= ipc_master_wifi_init_rx_chan(&ipc_master_env);
#endif
    if (res != 0)
    {
        CLOGE("IPC master init failed, slave may not be ready");
        return res;
    }

    /*»ùÓÚmsg channel½¨Á¢indication endpointÓÃÓÚ½ÓÊÕÍ¨Öª*/
    if (cb && cb->indication_handler)
        ipc_master_ep_register(IPC_EP_IND, cb->indication_handler, NULL);

#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_SRV);
#ifdef CFG_AMP_IPC_MRPC_SERVER_LWIP
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_LWIP, mrpc_msg_lwip_handlers, MRPC_MSG_ID_LWIP_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_NVS
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_NVS, mrpc_msg_nvs_handlers, MRPC_MSG_ID_NVS_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_FLASH_IF, mrpc_msg_flash_if_handlers, MRPC_MSG_ID_FLASH_IF_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_UTILS_S2M, mrpc_msg_utils_s2m_handlers, MRPC_MSG_ID_UTILS_S2M_MAX);
#endif
#endif

#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_CLT, IPC_EP_MRPC_SRV);
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

    ipc_master_init_config();

    return res;
}

