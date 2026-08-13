#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ipc.h"
#include "ipc_msg.h"
#include "mrpc.h"

struct ipc_ep *mrpc_client_comm_ep;

int32_t mrpc_msg_send_from(struct ipc_ep *ep_client, void *data, int32_t len, void *resp, uint32_t timeout_ms)
{
    if (ep_client == NULL)
        return -1;

    if (ipc_msg_send(ep_client, data, len, resp, timeout_ms) != IPC_ERR_OK)
        return -1;

    return 0;
}

int32_t mrpc_msg_send(void *data, int32_t len, void *resp)
{
#ifdef IPC_DEBUG
    struct mrpc_req_msg *hdr = data;

    ipc_dbg("MRPC client send: type=%d, idx=%d, len=%d\n", hdr->id >> 24, hdr->id & 0xFFFFFF, len);
#endif
    return mrpc_msg_send_from(mrpc_client_comm_ep, data, len, resp, IPC_MSG_TIMEOUT_MS);
}

int32_t mrpc_msg_send_timeout(void *data, int32_t len, void *resp, uint32_t timeout_ms)
{
#ifdef IPC_DEBUG
    struct mrpc_req_msg *hdr = data;

    ipc_dbg("MRPC client send: type=%d, idx=%d, len=%d\n", hdr->id >> 24, hdr->id & 0xFFFFFF, len);
#endif
    return mrpc_msg_send_from(mrpc_client_comm_ep, data, len, resp, timeout_ms);
}

/*
 * mrpc_client_init        - 在一个ipc channel上创建一个mrpc客户端
 *
 * @param ipc_chan_local   - 用户建立mrpc的ipc channel id
 *
 * @return                 - LS_OK 成功， LS_FAIL 失败
 *
 */
struct ipc_ep * mrpc_client_ep_init(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid)
{
    return ipc_ep_register(ipc_chan_local, ipc_chan_remote, local_eid, remote_eid, NULL, NULL);
}

struct ipc_ep * mrpc_client_init(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid)
{
    mrpc_client_comm_ep = mrpc_client_ep_init(ipc_chan_local, ipc_chan_remote, local_eid, remote_eid);

    return mrpc_client_comm_ep;
}
