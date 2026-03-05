/**
 ****************************************************************************************
 *
 * @file ipc_slave.c
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
#ifdef CFG_AMP_IPC_MRPC_SERVER_WIFI
#include "mrpc_wifi_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT
#include "mrpc_bt_api_server.h"
#endif

#ifdef CFG_AMP_IPC_MRPC_SERVER_OTP
#include "mrpc_otp_api_server.h"
#endif
#ifdef IPC_TEST_CASE
#include "ipc_test.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S
#include "mrpc_utils_m2s_api_server.h"
#endif
#include "pm_impl.h"


static struct ipc_ccb *slave_msg_ccb;
static struct ipc_ccb *master_msg_ccb;

static struct ipc_ccb *slave_fast_ccb;
static struct ipc_ccb *master_fast_ccb;

static struct ipc_slave_env_tag ipc_slave_env;
struct ipc_shared_env_tag ipc_shared_env __SHAREDRAM_AMP_IPC_ENV;


static int32_t ipc_slave_fast_notify_handler(void *ccb, void *fast_notify)
{
    uint32_t notify;

    ic_spin_lock(IC_SPIN_LOCK_TYPE_IPC);
    notify = *((uint32_t*)fast_notify);
    *((uint32_t*)fast_notify) = 0;
    ic_spin_unlock(IC_SPIN_LOCK_TYPE_IPC);

#ifdef CFG_AMP_IPC_HALT_BY_PEER_CORE
    if (notify & IPC_EVT_HALT)
    {
        ipc_halt_by_peer(true);
    }
#endif
#if CONFIG_PM
    if (notify & IPC_EVT_VRTC_SET)
    {
        vrtc_set_timer_from_ipc();
    }

    if (notify & IPC_EVT_ENTER_IDLE)/*The master core will go to sleep*/
    {
        pm_master_idle_request();
    }
#endif
    return 0;
}

int32_t ipc_slave_msg_push(uint32_t chan, uint32_t ep_idx, int32_t len, void *data)
{
    int32_t ret = 0;
    struct ipc_msg_desc desc;
    struct ipc_ccb *ccb;

    ccb = ipc_get_ccb(chan, CHAN_REMOTE);
    if (ccb)
    {
        desc.hdr.dst_id   = ipc_get_eid(chan, ep_idx);
        desc.hdr.src_id   = ipc_get_eid(IPC_CHAN_SLAVE_MSG, IPC_EP_IND);
        desc.hdr.data_len = len;
        desc.data = data;

        if (ipc_sendto(ccb, &desc, IPC_TIMEOUT) != IPC_ERR_OK)
            ret = -1;
    }

    return ret;
}

void ipc_slave_printf(char *string, int32_t len)
{
    uint16_t size = 0;
    struct ipc_msg_hdr *msg;
    struct ipc_msg_desc desc;

    msg = (struct ipc_msg_hdr*)ipc_get_tbuffer(master_msg_ccb, &size, IPC_TIMEOUT);
    if (msg)
    {
        msg->id  = IPC_IND_PRINT;
        msg->len = (len + 1) > size ? size : (len + 1);
        if ((len + 1) > size)
        {
            string[size - 3] = '*';
            string[size - 2] = '\n';
            string[size - 1] = 0;
        }
        memcpy(msg->data, string, msg->len);

        desc.hdr.dst_id = ipc_get_eid(IPC_CHAN_MASTER_MSG, IPC_EP_IND);
        desc.hdr.src_id = ipc_get_eid(IPC_CHAN_SLAVE_MSG, IPC_EP_IND);
        desc.hdr.data_len = sizeof(struct ipc_msg_hdr) + msg->len;
        desc.data = msg;

        ipc_send_tbuffer(master_msg_ccb, &desc);
    }
}

void ipc_slave_putchar(char c)
{
    char string[2];

    string[0] = c;
    string[1] = 0;

    ipc_slave_printf(string, 1);
}

void ipc_slave_vprintf(const char *fmt, ...)
{
    uint16_t remain = 0;
    uint32_t len = 0, offset = 0;
    char *data;
    va_list args;
    struct ipc_msg_hdr *msg;
    struct ipc_msg_desc desc;

    desc.hdr.dst_id = ipc_get_eid(IPC_CHAN_MASTER_MSG, IPC_EP_IND);
    desc.hdr.src_id = ipc_get_eid(IPC_CHAN_SLAVE_MSG, IPC_EP_IND);
    do
    {
        msg = (struct ipc_msg_hdr*)ipc_get_tbuffer(master_msg_ccb, &remain, IPC_TIMEOUT);

        if (msg)
        {
            msg->id  = IPC_IND_PRINT;
            data     = (char*)msg->data;
            va_start(args, fmt);
            len = dbg_vsnprintf_offset((char *)data, remain, offset, fmt, args);
            va_end(args);

            if (len >= offset + remain)
            {
                msg->len = remain - 1;
            }
            else
            {
                msg->len = len - offset;
            }
            desc.data = msg;
            desc.hdr.data_len = sizeof(struct ipc_msg_hdr) + msg->len;

            ipc_send_tbuffer(master_msg_ccb, &desc);
            //Increase offset by remain to write the next chunk in data
            offset += remain - 1;
        }
    } while (len >= offset);
}

#ifdef IPC_MSG_MGMT
static RTOS_TASK_FCT(ipc_slave_msg_task)
{
    uint8_t *msg;
    int32_t ret;
    struct ipc_msg_desc desc;

    while (1)
    {
        if ((msg = ipc_get_rbuffer(slave_msg_ccb, &desc, -1)))
        {
            ret = IPC_MSG_RELEASE;
            if (!ipc_msg_process(&desc))
                ret = ipc_ep_process(slave_msg_ccb, &desc);
            if (ret == IPC_MSG_RELEASE)
                ipc_free_rbuffer(slave_msg_ccb, msg, 0);
        }
    }
}
#else
static int32_t ipc_slave_msg_input(void *ccb, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_msg_desc *desc = param;

    if (!ipc_msg_process(&desc))
        ret = ipc_ep_process((struct ipc_ccb*)ccb, desc);

    return ret;
}
#endif

static int32_t ipc_slave_init_msg_chan(ipc_chan_callback_t cb)
{
#ifdef IPC_MSG_MGMT
    struct ipc_msg_mgmt *mgmt;
#endif
    struct ipc_queue *slave_msg_q, *master_msg_q;

    slave_msg_q = ipc_get_queue(&ipc_shared_env.msg_a2c_buf.ring);
    if (slave_msg_q == NULL)
        goto ERROR1;

    slave_msg_ccb = ipc_chan_create(IPC_NAME("s_msg"), IPC_CHAN_SLAVE_MSG, slave_msg_q, cb, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (slave_msg_ccb == NULL)
        goto ERROR2;

    master_msg_q = ipc_get_queue(&ipc_shared_env.msg_c2a_buf.ring);
    if (master_msg_q == NULL)
        goto ERROR3;

    master_msg_ccb = ipc_chan_create(IPC_NAME("m_msg"), IPC_CHAN_MASTER_MSG, master_msg_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_msg_ccb == NULL)
        goto ERROR4;
#ifdef IPC_MSG_MGMT
    ipc_msg_mgmt_init((void*)master_msg_ccb, IPC_MSG_POOL_SIZE);
#ifdef TASK_CREATE_STATIC
    static rtos_stack_type ipc_msg_task_stack_buf[LS_IPC_MSG_TASK_STACK_SIZE];
    static rtos_static_task_tcb ipc_msg_task_control;
    rtos_task_create_static(ipc_slave_msg_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, NULL,
                   LS_IPC_MSG_TASK_PRIORITY, NULL, ipc_msg_task_stack_buf, &ipc_msg_task_control);
#else
    rtos_task_create(ipc_slave_msg_task, "ipc_msg", IPC_MSG_TASK, LS_IPC_MSG_TASK_STACK_SIZE, NULL,
                   LS_IPC_MSG_TASK_PRIORITY, NULL);
#endif
#else
    ipc_status_set(slave_msg_ccb, IPC_QUEUE_STATUS_RX);
#endif
    return 0;

ERROR5:
    rtos_free(master_msg_ccb);
ERROR4:
    rtos_free(master_msg_q);
ERROR3:
    rtos_free(slave_msg_ccb);
ERROR2:
    rtos_free(slave_msg_q);
ERROR1:
    return -1;
}

static int32_t ipc_slave_init_fast_chan(ipc_chan_callback_t cb)
{
    slave_fast_ccb  = ipc_chan_create(IPC_NAME("s_fast"), IPC_CHAN_SLAVE_FAST, NULL, cb, (void*)&ipc_shared_env.slave_notify.state, IPC_CHAN_FLAGS_FAST);
    if (slave_fast_ccb == NULL)
        goto ERROR1;

    master_fast_ccb = ipc_chan_create(IPC_NAME("m_fast"), IPC_CHAN_MASTER_FAST, NULL, NULL, NULL, (IPC_CHAN_FLAGS_FAST | IPC_CHAN_FLAGS_REMOTE));
    if (master_fast_ccb == NULL)
        goto ERROR2;

    return 0;
ERROR2:
    rtos_free(slave_fast_ccb);
ERROR1:
    return -1;
}

volatile struct amp_shared_info* ipc_get_amp_shared_info(void)
{
    return &ipc_shared_env.amp_shared;
}

int32_t ipc_slave_init(struct ipc_slave_cb_tag *cb)
{
    int32_t res;
    struct mrpc_server_env *mrpc_server;

    ipc_slave_env.shared = &ipc_shared_env;
    ipc_slave_env.cb     = *cb;
    ipc_init(CORE_ID_SLAVE, &ipc_shared_env.slave_notify, &ipc_shared_env.master_notify);
    res  = ipc_slave_init_fast_chan(ipc_slave_fast_notify_handler);
    res |= ipc_slave_init_msg_chan(ipc_platform_task_notify);

#ifdef CFG_AMP_IPC_WIFI_CHAN
    res |= ipc_slave_wifi_init_tx_data_chan(&ipc_slave_env);
    res |= ipc_slave_wifi_init_rx_data_chan(&ipc_slave_env);
#endif
    ipc_slave_env.link_state = IPC_LINK_STATE_INIT;
    IPC_ASSERT(res == 0);
#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_SRV);
#ifdef CFG_AMP_IPC_MRPC_SERVER_WIFI
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_WIFI, mrpc_msg_wifi_handlers, MRPC_MSG_ID_WIFI_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_BT, mrpc_msg_bt_handlers, MRPC_MSG_ID_BT_MAX);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S
    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_UTILS_M2S, mrpc_msg_utils_m2s_handlers, MRPC_MSG_ID_UTILS_M2S_MAX);
#endif
#endif
#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_CLT, IPC_EP_MRPC_SRV);
#endif
#ifdef IPC_TEST_CASE
    ipc_test_case_init(mrpc_server);
#endif
#if defined(CFG_AMP_IPC_HALT_PEER_CORE) || defined(CFG_AMP_IPC_HALT_BY_PEER_CORE)
    ipc_halt_peer_init();
#endif

    return 0;
}

