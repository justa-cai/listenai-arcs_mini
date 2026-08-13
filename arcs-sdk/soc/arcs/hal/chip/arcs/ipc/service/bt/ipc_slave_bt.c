/**
 ****************************************************************************************
 *
 * @file ipc_slave_bt.c
 *
 * @brief BT HCI IPC transport - Master/AP side (BT Controller)
 *
 * AP core runs BT Controller.
 * - Receives HCI CMD/ACL TX from Host (CP) via H2C channel
 * - Sends HCI EVT/ACL RX to Host (CP) via C2H channel
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
#include "bt_os_task.h"
#include "ipc_slave.h"
#include "ipc_core.h"
#include "ipc_rx_task.h"
#include "ipc_slave_bt.h"

#include "bt_ipc_api.h"

extern struct ipc_shared_env_tag ipc_shared_env;
#include "bt_os_task.h"

//#ifdef CFG_AMP_IPC_BT_CHAN

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

extern void ke_msg_free(void *msg);


/** Master's local H2C channel - receives CMD/ACL from Host */
static struct ipc_ccb *slave_bt_c2h_ccb;
/** Slave's remote C2H channel - sends EVT/ACL to Host */
static struct ipc_ccb *master_bt_h2c_ccb;

/** Callback to deliver received H2C packets to BT Controller stack */
static bt_ipc_hci_recv_cb_t bt_c2h_recv_cb;

/** H2C receive task handle (for deinit) */
//static rtos_task_handle bt_c2h_task_handle;

static rtos_task_handle ipc_slave_msg_task_handle = RTOS_TASK_NULL;

/** Transport statistics */
static struct bt_ipc_stats slave_bt_stats;

static int32_t ipc_slave_bt_post_to_bt_task(const btos_event_t *event)
{
    if (!btos_send_event(OS_TASK_ID_BT, event, BTOS_TASK_NO_DELAY))
    {
        CLOGE("BT IPC: failed to post C2H packet to BT task");
        return -1;
    }

    return 0;
}

/*
 * C2H SEND FUNCTIONS (Controller -> Host)
 ****************************************************************************************
 */

/**
 * @brief Send a raw HCI packet from Controller to Host via C2H channel.
 *
 * Implements retry-on-ring-full logic inspired by Zephyr's bt_ipc_send():
 * when ipc_send returns IPC_ERR_NO_BUFF (ring full), retries up to
 * BT_IPC_SEND_RETRY_COUNT times with BT_IPC_SEND_RETRY_DELAY_MS between attempts.
 *
 * @return 0 on success, IPC error code on failure
 */

extern void print_msg(uint8_t *param);
int32_t ipc_slave_bt_c2h_send(const uint8_t *data, uint16_t len)
{
    uint16_t send_len;
    int32_t ret;
    int retries;
    uint8_t m_ret;
#if 0
    btos_event_t event;

    m_ret = cp_btos_malloc_api((void**)&event.msg_body, sizeof(btos_msg_t) +  len);

    CLOGI("ipc_slave_bt_c2h_send, data:0x%x-%d, m_ret:%d, msg_body:0x%x", data, len, m_ret, event.msg_body);

    if(m_ret == 0)
    {
        event.msg_body->param_len = len;
        event.msg_body->msg_id = BT_OS_IPC_HCI_C2H_SEND_EVT;
        memcpy(event.msg_body->param, data, len);
        /* Only send actual used bytes for efficiency */
        send_len = sizeof(event.msg_body);
    }
    else
    {
        CLOGI("cp_btos_malloc_api failed");
        return - 1;
    }
#endif
    /* Retry loop on ring-full, inspired by Zephyr ipc_service_send retry */
    for (retries = 0; retries <= BT_IPC_SEND_RETRY_COUNT; retries++)
    {
        ret = ipc_send(master_bt_h2c_ccb, data, len, IPC_TIMEOUT);
        if (ret == IPC_ERR_OK)
        {
            //ke_msg_free((struct ke_msg const *)data);
            slave_bt_stats.tx_count++;
            return 0;
        }

        /* Only retry on buffer-full; other errors are terminal */
        if (ret != IPC_ERR_NO_BUFF && ret != IPC_ERR_NO_MEM)
            break;

        if (retries < BT_IPC_SEND_RETRY_COUNT)
        {
            slave_bt_stats.tx_retry_count++;
            rtos_task_suspend(BT_IPC_SEND_RETRY_DELAY_MS);
        }
    }

    slave_bt_stats.tx_err_count++;
    CLOGE("BT IPC: C2H send failed  len=%d ret=%d", len, ret);
    return ret;
}




/*
 * H2C RECEIVE TASK (Host -> Controller)
 ****************************************************************************************
 */

/**
 * @brief Task receiving H2C packets from Host.
 *
 * Blocks on ipc_get_rbuffer waiting for CMD/ACL/SCO/ISO packets
 * from the Host side, then dispatches via callback.
 */
static int32_t ipc_bt_ctrl_task_callback(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, void *param)
{
    int32_t ret = IPC_MSG_RELEASE;
    btos_event_t event;
    uint8_t *msg = desc->data;

    //memcpy(&event.msg_body, msg, sizeof(event.msg_body));
    //event.msg_body = (btos_msg_t *)(*(btos_msg_t *)msg);

    if (msg)
    {
        event.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(msg));
        event.msg_body->msg_id = BT_OS_IPC_HCI_H2C_SEND_EVT;
        event.msg_body->param_len = sizeof(msg);
        memcpy(event.msg_body->param, &msg, sizeof(msg));

        //CLOGI("ipc_slave_bt_ctrl_task, msg:0x%x,event.msg_body:0x%x, msg_id:0x%x", msg, event.msg_body, event.msg_body->msg_id);

        if (ipc_slave_bt_post_to_bt_task(&event) == 0)
        {
            ret = IPC_MSG_HOLD;
            slave_bt_stats.rx_count++;
        }
        else
        {
            slave_bt_stats.rx_err_count++;
        }
    }

    return ret;
}

uint8_t ipc_slave_bt_free_buf(uint8_t *msg)
{
    //CLOGI("ipc_slave_bt_free_buf, msg:0x%x", msg);
    ipc_free_rbuffer(slave_bt_c2h_ccb, msg, 0);
    return 0;
}

/*
 * INITIALIZATION
 ****************************************************************************************
 */

/**
 * @brief Initialize BT HCI IPC channels on slave/AP side.
 *
 * Creates:
 *   - H2C channel (local, USER_MODE): Master reads CMD/ACL from Host
 *   - C2H channel (remote): Master writes EVT/ACL to Host
 *   - H2C receive task
 *
 * @param shared   IPC shared memory environment
 * @param h2c_cb   Callback for H2C packet dispatch to Controller stack
 */
int32_t ipc_slave_bt_init(void)
{
    struct ipc_queue *master_h2c_q, *slave_c2h_q;
    struct ipc_rx_task_env *ipc_rx_task_env;

    //bt_c2h_recv_cb = c2h_cb;
    memset(&slave_bt_stats, 0, sizeof(slave_bt_stats));

    /* H2C channel: Master reads from this (local, user-mode) */
    master_h2c_q = ipc_get_queue(&ipc_shared_env.bt.h2c.ring);
    if (master_h2c_q == NULL)
        goto ERROR1;

    master_bt_h2c_ccb = ipc_chan_create(IPC_NAME("m_bt_h2c"),
        IPC_CHAN_MASTER_BT_H2C, master_h2c_q,
        ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_REMOTE);
    if (master_bt_h2c_ccb == NULL)
        goto ERROR2;

    /* C2H channel: Master writes to this (remote) */
    slave_c2h_q = ipc_get_queue(&ipc_shared_env.bt.c2h.ring);
    if (slave_c2h_q == NULL)
        goto ERROR3;

    slave_bt_c2h_ccb = ipc_chan_create(IPC_NAME("s_bt_c2h"),
        IPC_CHAN_SLAVE_BT_C2H, slave_c2h_q,
        ipc_platform_event_notify, NULL, IPC_CHAN_FLAGS_USER_MODE);
    if (slave_bt_c2h_ccb == NULL)
        goto ERROR4;

    /* Create H2C receive task */
    ipc_rx_task_env = rtos_malloc(sizeof(struct ipc_rx_task_env));
    if (ipc_rx_task_env != NULL)
    {
        int32_t res;

        ipc_rx_task_env->ccb = slave_bt_c2h_ccb;
        ipc_rx_task_env->callback = ipc_bt_ctrl_task_callback;
        ipc_rx_task_env->param = NULL;

#ifdef TASK_CREATE_STATIC
    static rtos_stack_type bt_h2c_task_stack[BT_IPC_TASK_STACK_SIZE];
    static rtos_static_task_tcb bt_h2c_task_tcb;
    res = rtos_task_create_static(ipc_rx_task, "bt_ctrl", IPC_BT_CTRL_TASK, BT_IPC_TASK_STACK_SIZE, ipc_rx_task_env,
                                    BT_IPC_TASK_PRIORITY, NULL, bt_h2c_task_stack, &bt_h2c_task_tcb);
#else
    res = rtos_task_create(ipc_rx_task, "bt_ctrl", IPC_BT_CTRL_TASK, BT_IPC_TASK_STACK_SIZE, ipc_rx_task_env, BT_IPC_TASK_PRIORITY, NULL);
#endif

        if (!res)
        {
            CLOGD("BT IPC: Slave BT channels initialized");
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
    CLOGE("BT IPC: Failed to init slave bt channels");
    return -1;
}

/**
 * @brief Get transport statistics for slave/AP side.
 */
const struct bt_ipc_stats *ipc_slave_bt_get_stats(void)
{
    return &slave_bt_stats;
}

//#endif /* CFG_AMP_IPC_BT_CHAN */
