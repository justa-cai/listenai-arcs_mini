/**
 ****************************************************************************************
 *
 * @file ipc_slave_bt.c
 *
 * @brief BT HCI IPC transport - Slave/CP side (BT Host)
 *
 * CP core runs BT Host.
 * - Sends HCI CMD/ACL TX to Controller (AP) via H2C channel
 * - Receives HCI EVT/ACL RX from Controller (AP) via C2H channel
 *
 * Modeled after ipc_slave_wifi.c
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdbool.h>
#include "rtos_al.h"
#include "ls_rtos.h"
#include "log_print.h"
#include "platform.h"
#include "btos_al.h"
#include "ipc_master.h"
#include "ipc_core.h"
#include "ipc_rx_task.h"
#include "ipc_master_bt.h"

#include "bt_ipc_api.h"

extern struct ipc_shared_env_tag ipc_shared_env;

#include "bt_os_task.h"

//#ifdef CFG_AMP_IPC_BT_CHAN

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/** Slave's local C2H channel - receives EVT/ACL from Controller */
static struct ipc_ccb *slave_bt_c2h_ccb;
/** Master's remote H2C channel - sends CMD/ACL to Controller */
static struct ipc_ccb *master_bt_h2c_ccb;

/** Callback to deliver received C2H packets to BT Host stack */
static bt_ipc_hci_recv_cb_t bt_h2c_recv_cb;

/** Transport statistics */
static struct bt_ipc_stats master_bt_stats;

extern void ke_msg_free(void *msg);
/*
 * H2C SEND FUNCTIONS (Host -> Controller)
 ****************************************************************************************
 */

static int32_t ipc_master_bt_post_to_bt_task(const btos_event_t *event)
{
    if (!btos_send_event(OS_TASK_ID_BT, event, BTOS_TASK_NO_DELAY))
    {
        CLOGE("BT IPC: failed to post C2H packet to BT task");
        return -1;
    }

    return 0;
}


/**
 * @brief Send a raw HCI packet from Host to Controller via H2C channel.
 *
 * Implements retry-on-ring-full logic inspired by Zephyr's bt_ipc_send().
 *
 * @return 0 on success, IPC error code on failure
 */
int32_t ipc_master_bt_h2c_send(const uint8_t *data, uint16_t len)
{
    uint16_t send_len;
    int32_t ret;
    int retries;

    uint8_t m_ret;

    btos_event_t event;
#if 0
    m_ret = btos_malloc_api((void**)&event.msg_body, sizeof(btos_msg_t) +  len);

    CLOGI("ipc_master_bt_h2c_send, data:0x%x-%d, m_ret:%d, msg_body:0x%x", data, len, m_ret, event.msg_body);

    if (m_ret == 0)
    {
        event.msg_body->param_len = len;
        event.msg_body->msg_id = BT_OS_IPC_HCI_H2C_SEND_EVT;
        memcpy(event.msg_body->param, data, len);
        //memcpy(event.msg_body->param + len, (uint8_t*)event.msg_body, 4);
        /* Only send actual used bytes for efficiency */
        send_len = sizeof(event.msg_body);
        CLOGI("msg_body data:%02x%02x%02x%02x, %02x%02x%02x%02x, %02x%02x%02x%02x", event.msg_body->param[0], event.msg_body->param[1], event.msg_body->param[2], event.msg_body->param[3],\
                            event.msg_body->param[4], event.msg_body->param[5], event.msg_body->param[6], event.msg_body->param[7],\
                            event.msg_body->param[8], event.msg_body->param[9], event.msg_body->param[10], event.msg_body->param[11]);
    }
    else
    {
        CLOGI("ipc_master_bt_h2c_send failed");
        return IPC_ERR_NO_BUFF;
    }
    #endif
    /* Retry loop on ring-full, inspired by Zephyr ipc_service_send retry */
    for (retries = 0; retries <= BT_IPC_SEND_RETRY_COUNT; retries++)
    {
        ret = ipc_send(slave_bt_c2h_ccb, data, len, IPC_TIMEOUT);
        if (ret == IPC_ERR_OK)
        {
            master_bt_stats.tx_count++;
            return 0;
        }

        /* Only retry on buffer-full; other errors are terminal */
        if (ret != IPC_ERR_NO_BUFF && ret != IPC_ERR_NO_MEM)
            break;

        if (retries < BT_IPC_SEND_RETRY_COUNT)
        {
            master_bt_stats.tx_retry_count++;
            rtos_task_suspend(BT_IPC_SEND_RETRY_DELAY_MS);
        }
    }

    master_bt_stats.tx_err_count++;
    CLOGE("BT IPC: H2C send failed len=%d ret=%d", len, ret);
    return ret;
}


/*
 * C2H RECEIVE TASK (Controller -> Host)
 ****************************************************************************************
 */

/**
 * @brief Task receiving C2H packets from Controller.
 *
 * Blocks on ipc_get_rbuffer waiting for EVT/ACL/SCO/ISO packets
 * from the Controller side, then dispatches via callback.
 */
static int32_t ipc_bt_host_task_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    btos_event_t event;
    uint8_t *msg = desc->data;

    //memcpy(&event.msg_body, msg, sizeof(event.msg_body));
    //event.msg_body = (btos_msg_t *)(*(btos_msg_t *)msg);

    if (msg)
    {
        event.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(msg));
        event.msg_body->msg_id = BT_OS_IPC_HCI_C2H_SEND_EVT;
        event.msg_body->param_len = sizeof(msg);
        memcpy(event.msg_body->param, &msg, sizeof(msg));

        //CLOGI("ipc_master_bt_host_task, msg:0x%x,event.msg_body:0x%x, msg_id:0x%x", msg, event.msg_body, event.msg_body->msg_id);

        if (ipc_master_bt_post_to_bt_task(&event) == 0)
        {
            master_bt_stats.rx_count++;
            ret = IPC_MSG_HOLD;
        }
        else
        {
            master_bt_stats.rx_err_count++;
        }
        //if (bt_h2c_recv_cb)
            //bt_h2c_recv_cb(pkt->type, pkt->data, pkt->len);
    }

    return ret;
}

/*
 * INITIALIZATION
 ****************************************************************************************
 */

/**
 * @brief Initialize BT HCI IPC channels on slave/CP side.
 *
 * Creates:
 *   - C2H channel (local, USER_MODE): Slave reads EVT/ACL from Controller
 *   - H2C channel (remote): Slave writes CMD/ACL to Controller
 *   - C2H receive task
 *
 * @param shared   IPC shared memory environment
 * @param c2h_cb   Callback for C2H packet dispatch to Host stack
 */
int32_t ipc_master_bt_init(void)
{
    struct ipc_queue *slave_c2h_q, *master_h2c_q;
    struct ipc_rx_task_env *ipc_rx_task_env;

    memset(&master_bt_stats, 0, sizeof(master_bt_stats));

    /* C2H channel: Slave reads from this (local, user-mode) */
    slave_c2h_q = ipc_get_queue(&ipc_shared_env.bt.c2h.ring);
    if (slave_c2h_q == NULL)
        goto ERROR1;

    slave_bt_c2h_ccb = ipc_chan_create(IPC_NAME("s_bt_c2h"),
        IPC_CHAN_SLAVE_BT_C2H, slave_c2h_q,
        ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (slave_bt_c2h_ccb == NULL)
        goto ERROR2;

    /* H2C channel: Slave writes to this (remote) */
    master_h2c_q = ipc_get_queue(&ipc_shared_env.bt.h2c.ring);
    if (master_h2c_q == NULL)
        goto ERROR3;

    master_bt_h2c_ccb = ipc_chan_create(IPC_NAME("m_bt_h2c"),
        IPC_CHAN_MASTER_BT_H2C, master_h2c_q,
        ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (master_bt_h2c_ccb == NULL)
        goto ERROR4;

    /* Create C2H receive task */
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        int32_t res;

        ipc_rx_task_env->ccb = master_bt_h2c_ccb;
        ipc_rx_task_env->callback = ipc_bt_host_task_callback;
        ipc_rx_task_env->param = NULL;
#ifdef TASK_CREATE_STATIC
        static rtos_stack_type bt_c2h_task_stack[BT_IPC_TASK_STACK_SIZE];
        static rtos_static_task_tcb bt_c2h_task_tcb;
        res = rtos_task_create_static(ipc_rx_task, "bt_host", IPC_BT_HOST_TASK, BT_IPC_TASK_STACK_SIZE, ipc_rx_task_env,
            BT_IPC_TASK_PRIORITY, NULL, bt_c2h_task_stack, &bt_c2h_task_tcb);
#else
        res = rtos_task_create(ipc_rx_task, "bt_host", IPC_BT_HOST_TASK, BT_IPC_TASK_STACK_SIZE, ipc_rx_task_env, BT_IPC_TASK_PRIORITY, NULL);
#endif

        if (!res)
        {
            CLOGD("BT IPC: Master BT channels initialized");
            return 0;
        }
    }

    rtos_free(master_bt_h2c_ccb);
ERROR4:
    rtos_free(master_h2c_q);
ERROR3:
    rtos_free(slave_bt_c2h_ccb);
ERROR2:
    rtos_free(slave_c2h_q);
ERROR1:
    CLOGE("BT IPC: Failed to init master bt channels");
    return -1;
}

uint8_t ipc_master_bt_free_buf(uint8_t *msg)
{
    //CLOGI("ipc_master_bt_free_buf, msg:0x%x", msg);
    ipc_free_rbuffer(master_bt_h2c_ccb, msg, 0);
    return 0;
}

/**
 * @brief Get transport statistics for slave/CP side.
 */
const struct bt_ipc_stats *ipc_master_bt_get_stats(void)
{
    return &master_bt_stats;
}

//#endif /* CFG_AMP_IPC_BT_CHAN */
