#include <stdint.h>
#include "ls_err.h"
#include "ipc.h"
#include "mrpc.h"
#include "mrpc_ap_pm_lock_api_msg.h"
#include "mrpc_ap_pm_lock_api_server.h"

int32_t ap_pm_remote_lock_acquire(void)
{
    mrpc_ap_pm_lock_acquire_req_t req;
    mrpc_ap_pm_lock_acquire_resp_t resp;

    if (sizeof(req) > IPC_MSG_BUFFER_SIZE) {
        return LS_FAIL;
    }

    req.hdr.id = MRPC_MSG_ID_AP_PM_LOCK_ACQUIRE;

    if (mrpc_msg_send(&req, sizeof(req), &resp)) {
        return LS_FAIL;
    }

    return resp.hdr.status;
}

int32_t ap_pm_remote_lock_release(void)
{
    mrpc_ap_pm_lock_release_req_t req;
    mrpc_ap_pm_lock_release_resp_t resp;

    if (sizeof(req) > IPC_MSG_BUFFER_SIZE) {
        return LS_FAIL;
    }

    req.hdr.id = MRPC_MSG_ID_AP_PM_LOCK_RELEASE;

    if (mrpc_msg_send(&req, sizeof(req), &resp)) {
        return LS_FAIL;
    }

    return resp.hdr.status;
}

int32_t ap_pm_remote_lock_get_state(uint32_t *locked)
{
    mrpc_ap_pm_lock_get_state_req_t req;
    mrpc_ap_pm_lock_get_state_resp_t resp;

    if (locked == 0) {
        return LS_ERR_PARAM;
    }

    if (sizeof(req) > IPC_MSG_BUFFER_SIZE) {
        return LS_FAIL;
    }

    req.hdr.id = MRPC_MSG_ID_AP_PM_LOCK_GET_STATE;

    if (mrpc_msg_send(&req, sizeof(req), &resp)) {
        return LS_FAIL;
    }

    if (resp.hdr.status == LS_OK) {
        *locked = resp.locked;
    }

    return resp.hdr.status;
}
