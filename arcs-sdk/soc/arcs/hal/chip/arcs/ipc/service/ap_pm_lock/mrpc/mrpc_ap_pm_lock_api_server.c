#include <stdint.h>
#include <string.h>
#include "ipc.h"
#include "mrpc.h"
#include "mrpc_ap_pm_lock_api_msg.h"
#include "mrpc_ap_pm_lock_api_server.h"

static void mrpc_ap_pm_lock_acquire(void *msg, struct mrpc_resp_msg *resp_msg, uint32_t resp_buf_len)
{
    mrpc_ap_pm_lock_acquire_resp_t *resp = (mrpc_ap_pm_lock_acquire_resp_t *)resp_msg;

    (void)msg;
    (void)resp_buf_len;
    memset(resp, 0, sizeof(*resp));
    resp_msg->len = sizeof(*resp);
    resp_msg->status = ap_pm_remote_lock_server_acquire();
}

static void mrpc_ap_pm_lock_release(void *msg, struct mrpc_resp_msg *resp_msg, uint32_t resp_buf_len)
{
    mrpc_ap_pm_lock_release_resp_t *resp = (mrpc_ap_pm_lock_release_resp_t *)resp_msg;

    (void)msg;
    (void)resp_buf_len;
    memset(resp, 0, sizeof(*resp));
    resp_msg->len = sizeof(*resp);
    resp_msg->status = ap_pm_remote_lock_server_release();
}

static void mrpc_ap_pm_lock_get_state(void *msg, struct mrpc_resp_msg *resp_msg, uint32_t resp_buf_len)
{
    mrpc_ap_pm_lock_get_state_resp_t *resp = (mrpc_ap_pm_lock_get_state_resp_t *)resp_msg;

    (void)msg;
    (void)resp_buf_len;
    memset(resp, 0, sizeof(*resp));
    resp_msg->len = sizeof(*resp);
    resp_msg->status = ap_pm_remote_lock_server_get_state(&resp->locked);
}

mrpc_msg_handler_t mrpc_msg_ap_pm_lock_handlers[
    MRPC_MSG_ID_AP_PM_LOCK_MAX - MRPC_MSG_ID_AP_PM_LOCK_START] =
{
    mrpc_ap_pm_lock_acquire,
    mrpc_ap_pm_lock_release,
    mrpc_ap_pm_lock_get_state,
    NULL
};
