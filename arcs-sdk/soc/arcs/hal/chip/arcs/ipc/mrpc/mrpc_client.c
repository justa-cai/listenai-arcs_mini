#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ipc.h"
#include "mrpc.h"

struct ipc_ep *mrpc_client_wl_ep;

int32_t mrpc_msg_send_from(struct ipc_ep *ep_client, void *data, int32_t len, void *resp)
{
    if (ep_client == NULL)
        return -1;

    rtos_mutex_lock(ep_client->lock);
    if (ipc_msg_send(ep_client, data, len, resp) != IPC_ERR_OK)
    {
        rtos_mutex_unlock(ep_client->lock);
        return -1;
    }
    rtos_mutex_unlock(ep_client->lock);

    return 0;
}

struct ipc_ep * mrpc_client_create(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid)
{
    struct ipc_ep *client_ep;

    if ((client_ep = ipc_ep_register(ipc_chan_local, ipc_chan_remote, local_eid, remote_eid, NULL, NULL)))
        rtos_mutex_create(&client_ep->lock);

    return client_ep;
}

int32_t mrpc_msg_send(void *data, int32_t len, void *resp)
{
#ifdef IPC_DEBUG
    struct mrpc_req_msg *hdr = data;

    ipc_dbg("MRPC client send: type=%d, idx=%d, len=%d\n", hdr->id >> 24, hdr->id & 0xFFFFFF, len);
#endif
    return mrpc_msg_send_from(mrpc_client_wl_ep, data, len, resp);
}

/*
 * mrpc_client_init        - 在一个ipc channel上创建一个mrpc客户端
 *
 * @param ipc_chan_local   - 用户建立mrpc的ipc channel id
 *
 * @return                 - LS_OK 成功， LS_FAIL 失败
 *
 */
struct ipc_ep * mrpc_client_init(uint32_t ipc_chan_local, uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid)
{
    mrpc_client_wl_ep = mrpc_client_create(ipc_chan_local, ipc_chan_remote, local_eid, remote_eid);

    return mrpc_client_wl_ep;
}
