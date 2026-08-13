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
#include "ipc_msg.h"
#include "ipc_rx_task.h"
#include "ipc_mrpc_service.h"
#include "ls_event.h"
#include "mrpc.h"
#include "ic_spinlock.h"
#ifdef CFG_AMP_IPC_BUS
#include "ipc_bus.h"
#endif
#ifdef CFG_IPC_TEST_CASE
#include "ipc_test.h"
#endif
#include "vrtc.h"

static struct ipc_master_env_tag ipc_master_env;
struct ipc_shared_env_tag ipc_shared_env __SHAREDRAM_AMP_IPC_ENV;

static struct ipc_ccb *master_msg_ccb;
static struct ipc_ccb *slave_msg_ccb;

static struct ipc_ccb *master_signal_ccb;
static struct ipc_ccb *slave_signal_ccb;

#ifdef  CFG_IPC_PRINT_READER
extern void rtos_ipc_dbg_task_resume(int32_t isr);
extern void ipc_dbg_init(volatile struct ipc_dbg_tag *buffer);
#endif

int32_t ipc_master_send_msg(struct ipc_ep *ep, void *data, uint32_t len, void *resp)
{
    int32_t ret = 0;

    if (ipc_msg_send(ep, data, len, resp, IPC_MSG_TIMEOUT_MS) != IPC_ERR_OK)
        ret = -1;

    return ret;
}

struct ipc_ep* ipc_master_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg)
{
    return ipc_ep_register(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, ep_idx, IPC_EP_ANY, handler, arg);
}

static int32_t ipc_master_signal_handler(void *ccb, void *signal)
{
    uint32_t sig_code;

    ic_spin_lock(IC_SPIN_LOCK_TYPE_IPC);
    sig_code = *((uint32_t*)signal);
    *((uint32_t*)signal) = 0;
    ic_spin_unlock(IC_SPIN_LOCK_TYPE_IPC);

    if (sig_code & IPC_SIG_LINKUP)
    {
        ipc_master_env.link_state = true;
        CLOGD("IPC link up");
    }

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
    if (sig_code & IPC_SIG_HALT)
    {
        ipc_halt_by_peer(true);
    }
#endif

#ifdef  CFG_IPC_PRINT_READER
    if (sig_code & IPC_SIG_PRINT)
    {
        rtos_ipc_dbg_task_resume(1);
    }
#endif

#if CONFIG_PM
#if defined(CFG_VRTC_PROXY) && CFG_VRTC_PROXY
    if (sig_code & IPC_SIG_VRTC_ALERT)
    {
        amp_app_status_set(AMP_APP_STATUS_VRTC_ALERT);
    }
#endif

#if defined(CFG_VRTC) && CFG_VRTC
    if (sig_code & IPC_SIG_VRTC_SET)
    {
        vrtc_set_timer_from_ipc();
    }
#endif

    if (sig_code & IPC_SIG_ENTER_IDLE)
    {
        ;
    }
#endif
    return 0;
}

bool ipc_master_get_link_status(void)
{
    if ((ipc_master_env.link_state == 0) && (ipc_get_signal_state() & IPC_SIG_LINKUP))
    {
        ipc_master_env.link_state = true;
    }
    return ipc_master_env.link_state;
}

bool ipc_master_wait_linkup(uint32_t timeout_ms)
{
    uint32_t i = 0;

    while (!ipc_master_get_link_status() && i++ < timeout_ms)
        rtos_delay(1);

    return ipc_master_get_link_status();
}

static int32_t ipc_master_init_msg_chan(ipc_rx_task_callback_t cb)
{
    int32_t res = -1;
    struct ipc_queue *master_msg_q, *slave_msg_q;

    master_msg_q = ipc_get_queue(&ipc_shared_env.master_msg_buf.ring);
    if (master_msg_q == NULL)
        goto ERROR1;

    master_msg_ccb = ipc_chan_create(IPC_NAME("m_msg"), IPC_CHAN_MASTER_MSG, master_msg_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_msg_ccb == NULL)
        goto ERROR2;

    slave_msg_q = ipc_get_queue(&ipc_shared_env.slave_msg_buf.ring);
    if (slave_msg_q == NULL)
        goto ERROR3;

    slave_msg_ccb = ipc_chan_create(IPC_NAME("s_msg"), IPC_CHAN_SLAVE_MSG, slave_msg_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_msg_ccb == NULL)
        goto ERROR4;

    ipc_msg_mgmt_init((void*)slave_msg_ccb, IPC_MSG_POOL_SIZE);
    struct ipc_rx_task_env *ipc_rx_task_env;
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        ipc_rx_task_env->ccb = master_msg_ccb;
        ipc_rx_task_env->callback = cb;
        ipc_rx_task_env->param = NULL;
#ifdef TASK_CREATE_STATIC
        static rtos_stack_type ipc_msg_task_stack_buf[LS_IPC_MSG_TASK_STACK_SIZE];
        static rtos_static_task_tcb ipc_msg_task_control;
        res = rtos_task_create_static(ipc_rx_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, ipc_rx_task_env,
                    LS_IPC_MSG_TASK_PRIORITY, NULL, ipc_msg_task_stack_buf, &ipc_msg_task_control);
#else
        res = rtos_task_create(ipc_rx_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, ipc_rx_task_env,
                    LS_IPC_MSG_TASK_PRIORITY, NULL);
#endif
        if (!res)
            return 0;
    }

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

static int32_t ipc_master_init_signal_chan(ipc_chan_callback_t cb)
{
    master_signal_ccb = ipc_chan_create(IPC_NAME("m_signal"), IPC_CHAN_MASTER_SIGNAL, NULL, cb, (void*)&ipc_shared_env.master_signal.state, IPC_CHAN_FLAGS_SIGNAL);
    if (master_signal_ccb == NULL)
        goto ERROR1;

    slave_signal_ccb = ipc_chan_create(IPC_NAME("s_signal"), IPC_CHAN_SLAVE_SIGNAL, NULL, NULL, NULL, (IPC_CHAN_FLAGS_SIGNAL| IPC_CHAN_FLAGS_REMOTE));
    if (slave_signal_ccb == NULL)
        goto ERROR2;

    return 0;

ERROR2:
    rtos_free(master_signal_ccb);
ERROR1:
    CLOGE("Failed to init msg chan");
    return -1;
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

int32_t ipc_master_init(void)
{
    int32_t res;
    struct mrpc_server_env *mrpc_server = NULL;

    ipc_master_env.config = (uint32_t*)ipc_shared_env.config;
    ipc_master_env.shared = &ipc_shared_env;
    amp_shared_bind(&ipc_shared_env.amp_shared);
    ipc_init(CORE_ID_MASTER, &ipc_shared_env.master_signal, &ipc_shared_env.slave_signal);
    res  = ipc_master_init_signal_chan(ipc_master_signal_handler);
    res |= ipc_master_init_msg_chan(ipc_recv_msg_callback);
    if (res != 0)
    {
        CLOGE("IPC master init failed, slave may not be ready");
        return res;
    }

#ifdef CFG_AMP_IPC_INDICATION
    ipc_master_ep_register(IPC_EP_IND, ipc_indication_handler, NULL);
#endif

#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_SRV);
    res |= ipc_mrpc_register_master_services(mrpc_server);
#ifdef CFG_AMP_IPC_BUS
    res |= ipc_mrpc_register_bus_service(
        mrpc_server_init(IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_BUS_SRV));
#endif
#endif
#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_CLT, IPC_EP_MRPC_SRV);
#ifdef CFG_AMP_IPC_BUS
    res |= ipc_mrpc_init_bus_client();
#endif
#endif

#if defined(CFG_AMP_IPC_HALT_PEER_CORE) || defined(CFG_AMP_IPC_HALT_BY_PEER_CORE)
    ipc_halt_peer_init();
#endif
#ifdef CFG_IPC_PRINT_READER
    ipc_dbg_init(&ipc_shared_env.dbg_buffer);
#endif
#ifdef CFG_IPC_TEST_CASE
    ipc_test_case_init(mrpc_server);
#endif

    ipc_master_init_config();

    return res;
}
