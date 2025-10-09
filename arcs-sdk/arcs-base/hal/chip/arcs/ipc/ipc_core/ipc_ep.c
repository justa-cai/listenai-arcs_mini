
#include <string.h>
#include <stdbool.h>
#include <rtos_al.h>
#include "ipc_msg.h"
#include "ipc_ep.h"

struct ipc_ep* ipc_ep_register(uint32_t ipc_chan_local,  uint32_t ipc_chan_remote, uint32_t local_eid, uint32_t remote_eid, ipc_ep_handler_t handler, void *arg)
{
    int32_t ret = -1;
    struct ipc_ep *entry = NULL;
    struct ipc_ccb *ccb;

    ipc_dbg("%s:local 0x%02x-0x%02x remote 0x%02x-0x%02x\n", __func__, ipc_chan_local, local_eid, ipc_chan_remote, remote_eid);
    if ((ccb = ipc_get_ccb(ipc_chan_local, CHAN_LOCAL)))
    {
        entry = rtos_malloc(sizeof(struct ipc_ep));
        if (entry)
        {
            entry->local_eid  = ipc_get_eid(ipc_chan_local, local_eid);
            entry->remote_eid = ipc_get_eid(ipc_chan_remote, remote_eid);;
            entry->handler    = handler;
            entry->arg        = arg;
            GLOBAL_INT_DISABLE();
            dl_list_add(&ccb->priv->epts, &entry->list);
            GLOBAL_INT_RESTORE();
        }
    }

    return entry;
}

void ipc_ep_unregister(struct ipc_ep *ep)
{
    if (ep)
    {
        dl_list_del(&ep->list);
        rtos_free(ep);
    }
}

int32_t ipc_ep_process(void *ccb, struct ipc_msg_desc *desc)
{
    int32_t ret = IPC_MSG_RELEASE;
    struct ipc_ep *cur;
    struct ipc_ccb *ccb_ptr = (struct ipc_ccb*)ccb;
#ifdef IPC_DEBUG
    int32_t got = 0;
#endif

    dl_list_for_each(cur, &ccb_ptr->priv->epts, struct ipc_ep, list)
    {
        ipc_dbg("%s:local 0x%x dst 0x%x\n", __func__, cur->local_eid.val, desc->hdr.dst_id.val);
        if (cur->local_eid.val == desc->hdr.dst_id.val)
        {
            if (cur->handler)
            {
#ifdef IPC_DEBUG
                ipc_dbg("EP handler: %p\n", cur->handler);
                got = 1;
#endif
                ret = cur->handler(desc, cur->arg);
            }
#ifdef IPC_DEBUG
            else
            {
                ipc_dbg("EP handler: NULL\n");
            }
#endif
            break;
        }
    }
#ifdef IPC_DEBUG
    if (!got)
        ipc_dbg("Failed to get ep handler\n");
#endif
    return ret;
}

int32_t ipc_ep_queue_rx_cb(struct ipc_msg_desc *desc, void *arg)
{
    rtos_queue queue = (rtos_queue)arg;

    rtos_queue_write(queue, desc, 0, true);

    return IPC_MSG_HOLD;
}
