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
#include "ipc_msg.h"
#include "ipc_rx_task.h"
#include "ipc_mrpc_service.h"
#include "mrpc.h"
#include "ic_spinlock.h"
#ifdef CFG_AMP_IPC_BUS
#include "ipc_bus.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_OTP
#include "mrpc_otp_api_server.h"
#endif
#ifdef CFG_IPC_TEST_CASE
#include "ipc_test.h"
#endif
#include "vrtc.h"

static struct ipc_ccb *slave_msg_ccb;
static struct ipc_ccb *master_msg_ccb;

static struct ipc_ccb *slave_signal_ccb;
static struct ipc_ccb *master_signal_ccb;

static struct ipc_slave_env_tag ipc_slave_env;
struct ipc_shared_env_tag ipc_shared_env __SHAREDRAM_AMP_IPC_ENV;

#ifdef  CFG_IPC_PRINT_READER
extern void rtos_ipc_dbg_task_resume(int32_t isr);
extern void ipc_dbg_init(volatile struct ipc_dbg_tag *buffer);
#endif


static int32_t ipc_slave_signal_handler(void *ccb, void *signal)
{
    uint32_t sig_code;

    ic_spin_lock(IC_SPIN_LOCK_TYPE_IPC);
    sig_code = *((uint32_t*)signal);
    *((uint32_t*)signal) = 0;
    ic_spin_unlock(IC_SPIN_LOCK_TYPE_IPC);

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

struct ipc_ep* ipc_slave_ep_register(uint32_t ep_idx, ipc_ep_handler_t handler, void *arg)
{
    return ipc_ep_register(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, ep_idx, IPC_EP_ANY, handler, arg);
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
        desc.hdr.flags    = 0;
        desc.hdr.seq      = 0;
        desc.hdr.data_len = len;
        desc.data = data;

        if (ipc_sendto(ccb, &desc, IPC_TIMEOUT) != IPC_ERR_OK)
            ret = -1;
    }

    return ret;
}

int32_t ipc_slave_printf(char *string, int32_t len)
{
    uint16_t size = 0;
    int32_t ret = -1;
    struct ipc_msg_hdr *msg;

    if ((string == NULL) || (len <= 0))
        return -1;

    msg = (struct ipc_msg_hdr*)ipc_get_tbuffer(master_msg_ccb, &size, IPC_TIMEOUT);
    if (msg)
    {
        char *dst = (char *)msg->data;
        uint32_t copy_len;
        struct ipc_msg_desc desc;

        size -= sizeof(struct ipc_msg_hdr);
        msg->id  = IPC_IND_PRINT;
        msg->len = (len + 1) >= size ? size : (len + 1);
        copy_len = msg->len - 1;

        memcpy(dst, string, copy_len);
        if ((len + 1) > size)
        {
            dst[size - 3] = '*';
            dst[size - 2] = '\n';
            dst[size - 1] = '\0';
        }
        else
        {
            dst[copy_len] = '\0';
        }

        desc.hdr.dst_id = ipc_get_eid(IPC_CHAN_MASTER_MSG, IPC_EP_IND);
        desc.hdr.src_id = ipc_get_eid(IPC_CHAN_SLAVE_MSG, IPC_EP_IND);
        desc.hdr.flags = 0;
        desc.hdr.seq = 0;
        desc.hdr.data_len = sizeof(struct ipc_msg_hdr) + msg->len;
        desc.data = msg;

        ipc_send_tbuffer(master_msg_ccb, &desc);
        ret = 0;
    }

    return ret;
}

void ipc_slave_putchar(char c)
{
    char string[2];

    string[0] = c;
    string[1] = 0;

    ipc_slave_printf(string, 1);
}
#if 0
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
#endif
static int32_t ipc_slave_init_msg_chan(ipc_rx_task_callback_t cb)
{
    struct ipc_queue *slave_msg_q, *master_msg_q;

    slave_msg_q = ipc_get_queue(&ipc_shared_env.slave_msg_buf.ring);
    if (slave_msg_q == NULL)
        goto ERROR1;

    slave_msg_ccb = ipc_chan_create(IPC_NAME("s_msg"), IPC_CHAN_SLAVE_MSG, slave_msg_q, ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);

    if (slave_msg_ccb == NULL)
        goto ERROR2;

    master_msg_q = ipc_get_queue(&ipc_shared_env.master_msg_buf.ring);
    if (master_msg_q == NULL)
        goto ERROR3;

    master_msg_ccb = ipc_chan_create(IPC_NAME("m_msg"), IPC_CHAN_MASTER_MSG, master_msg_q, NULL, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_msg_ccb == NULL)
        goto ERROR4;
#ifdef IPC_MSG_MGMT
    ipc_msg_mgmt_init((void*)master_msg_ccb, IPC_MSG_POOL_SIZE);
    struct ipc_rx_task_env *ipc_rx_task_env;
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        int32_t res = -1;

        ipc_rx_task_env->ccb = slave_msg_ccb;
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

static int32_t ipc_slave_init_signal_chan(ipc_chan_callback_t cb)
{
    slave_signal_ccb  = ipc_chan_create(IPC_NAME("s_signal"), IPC_CHAN_SLAVE_SIGNAL, NULL, cb, (void*)&ipc_shared_env.slave_signal.state, IPC_CHAN_FLAGS_SIGNAL);
    if (slave_signal_ccb == NULL)
        goto ERROR1;

    master_signal_ccb = ipc_chan_create(IPC_NAME("m_signal"), IPC_CHAN_MASTER_SIGNAL, NULL, NULL, NULL, (IPC_CHAN_FLAGS_SIGNAL| IPC_CHAN_FLAGS_REMOTE));
    if (master_signal_ccb == NULL)
        goto ERROR2;

    return 0;
ERROR2:
    rtos_free(slave_signal_ccb);
ERROR1:
    return -1;
}

int32_t ipc_slave_init(void)
{
    int32_t res;
    struct mrpc_server_env *mrpc_server = NULL;

    ipc_slave_env.shared = &ipc_shared_env;
    amp_shared_bind(&ipc_shared_env.amp_shared);
    ipc_init(CORE_ID_SLAVE, &ipc_shared_env.slave_signal, &ipc_shared_env.master_signal);
    res  = ipc_slave_init_signal_chan(ipc_slave_signal_handler);
    res |= ipc_slave_init_msg_chan(ipc_recv_msg_callback);

    ipc_slave_env.link_state = IPC_LINK_STATE_INIT;
    IPC_ASSERT(res == 0);
#ifdef CFG_AMP_IPC_INDICATION
    ipc_slave_ep_register(IPC_EP_IND, ipc_indication_handler, NULL);
#endif

#ifdef CFG_AMP_IPC_MRPC_SERVER
    mrpc_server = mrpc_server_init(IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_SRV);
    res |= ipc_mrpc_register_slave_services(mrpc_server);
#ifdef CFG_AMP_IPC_BUS
    res |= ipc_mrpc_register_bus_service(
        mrpc_server_init(IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_BUS_SRV));
#endif
#endif
#ifdef CFG_AMP_IPC_MRPC_CLIENT
    mrpc_client_init(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_CLT, IPC_EP_MRPC_SRV);
#ifdef CFG_AMP_IPC_BUS
    res |= ipc_mrpc_init_bus_client();
#endif
#endif

#ifdef CFG_IPC_TEST_CASE
    ipc_test_case_init(mrpc_server);
#endif
#if defined(CFG_AMP_IPC_HALT_PEER_CORE) || defined(CFG_AMP_IPC_HALT_BY_PEER_CORE)
    ipc_halt_peer_init();
#endif
#ifdef CFG_IPC_PRINT_READER
    ipc_dbg_init(&ipc_shared_env.dbg_buffer);
#endif

    return res;
}
