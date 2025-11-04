#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ls_err.h"
#include "rtos_al.h"
#include "ls_rtos.h"
#include "ipc.h"
#include "mrpc.h"


#ifdef CFG_AMP_IPC_MASTER
#ifdef ipc_dbg
#undef ipc_dbg
#define ipc_dbg(...)
#endif
#endif



#ifdef CFG_AMP_IPC_MASTER

#ifdef IPC_MSG_SEGMENT
#define MAX_MRCP_REQ_MSG_SIZE          (IPC_C2A_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX)
#define MAX_MRCP_RESP_MSG_SIZE         (IPC_A2C_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX)
#else
#define MAX_MRCP_REQ_MSG_SIZE          IPC_C2A_MSG_BUF_SIZE
#define MAX_MRCP_RESP_MSG_SIZE         IPC_A2C_MSG_BUF_SIZE
#endif

#else

#ifdef IPC_MSG_SEGMENT
#define MAX_MRCP_REQ_MSG_SIZE          (IPC_A2C_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX)
#define MAX_MRCP_RESP_MSG_SIZE         (IPC_C2A_MSG_BUF_SIZE * IPC_MSG_SEGMENT_MAX)
#else
#define MAX_MRCP_REQ_MSG_SIZE          (IPC_A2C_MSG_BUF_SIZE)
#define MAX_MRCP_RESP_MSG_SIZE         (IPC_C2A_MSG_BUF_SIZE)
#endif

#endif


static RTOS_TASK_FCT(mrpc_server_task)
{
    uint8_t service_type;
    uint32_t hdl_id;
    struct ipc_msg_desc desc;
    struct mrpc_server_env *mrpc_env = env;
    struct mrpc_service_entry *service;
    struct mrpc_req_msg *msg;
    struct mrpc_resp_msg *resp_msg;

    ipc_ep_register(mrpc_env->ipc_chan_local, IPC_CHAN_ANY, mrpc_env->server_eid, IPC_EP_ANY, ipc_ep_queue_rx_cb, mrpc_env->queue);
    resp_msg = mrpc_env->resp_buffer;

    while(1)
    {
        rtos_queue_read(mrpc_env->queue, &desc, -1, false);
        resp_msg->status = LS_FAIL;
        resp_msg->len    = sizeof(struct mrpc_resp_msg);
        msg              = (struct mrpc_req_msg*)desc.data;

#ifdef IPC_MSG_SEGMENT
        if (desc.seg[0])
        {
            if (desc.hdr.total_len <= MAX_MRCP_REQ_MSG_SIZE)
            {
                int32_t i = 0;
                uint16_t len = 0;
                void **ptr = &desc.data;

                msg = mrpc_env->req_buffer;
                do
                {
                    memcpy(((uint8_t*)msg + len), ptr[i], IPC_GET_EPMSG_LEN(ptr[i]));
                    len += IPC_GET_EPMSG_LEN(ptr[i]);
                    ipc_dbg("seg[%d]: %d\n", i, len);
                    i++;
                } while ((i < (IPC_MSG_SEGMENT_MAX)) && (len < desc.hdr.total_len));
                ipc_dbg("seg: %d\n", len);
            }
            else
            {
                ipc_dbg("MRPC: Too long %d\n", desc.hdr.total_len);
                goto END;
            }
        }
#endif
        service      = mrpc_env->services;
        service_type = msg->id >> 24;
        hdl_id       = msg->id & 0xFFF;
        ipc_dbg("MRPC server rev: type=%d api_idx=(%d-1)\n", service_type, hdl_id);
        if (hdl_id)
        {
            hdl_id -= 1;
            while (service)
            {
                if (service_type == service->type)
                {
                    if (hdl_id < service->total)
                    {
                        if (service->handler[hdl_id])
                        {
                            ipc_dbg("Hdl: %p\n", service->handler[hdl_id]);
                            service->handler[hdl_id](msg, resp_msg);
                        }
                    }
                    break;
                }
                service = service->next;
            }
        }

END:
        ipc_msg_release(mrpc_env->ipc_chan_local, desc.data);
        ipc_msg_reply((uint16_t)desc.hdr.src_id.val, desc.hdr.dst_id.val, resp_msg->len, (void*)resp_msg);
        ipc_dbg("End len %d status %d type %d id %d\n", resp_msg->len, resp_msg->status, service_type, hdl_id);
    }
}

struct mrpc_server_env* mrpc_server_init(uint32_t ipc_chan_local, uint32_t server_eid)
{
    struct mrpc_server_env *env;
    struct mrpc_req_msg *req_buffer;
    struct mrpc_resp_msg *resp_buffer;

    if (!(env = rtos_malloc(sizeof(struct mrpc_server_env))))
        return NULL;

    if (!(req_buffer = rtos_malloc(MAX_MRCP_REQ_MSG_SIZE)))
        return NULL;

    if (!(resp_buffer = rtos_malloc(MAX_MRCP_RESP_MSG_SIZE)))
        return NULL;

    env->ipc_chan_local = ipc_chan_local;
    env->server_eid     = server_eid;
    env->req_buffer     = req_buffer;
    env->resp_buffer    = resp_buffer;
    env->services       = NULL;

    CLOGD("MRPC server: %x  %x\n", ipc_chan_local, server_eid);
    rtos_queue_create(sizeof(struct ipc_msg_desc), MRPC_SERV_QUEUE_SIZE, &env->queue);
    rtos_task_create(mrpc_server_task, "mrpc_server", MRPC_SER_TASK, LS_MRPC_SERV_TASK_STACK_SIZE, env,
                   LS_MRPC_SERV_TASK_PRIORITY, NULL);

    return env;
}

int32_t mrpc_service_register(struct mrpc_server_env *env, int32_t service_type, mrpc_msg_handler_t *mrpc_services, int32_t total)
{
    struct mrpc_service_entry *ser, *tmp;

    if (!(ser = rtos_malloc(sizeof(struct mrpc_service_entry))))
        return -1;

    ser->type    = service_type;
    ser->total   = total;
    ser->handler = mrpc_services;
    ser->next    = NULL;

    if (env->services == NULL)
    {
        env->services = ser;
    }
    else
    {
        tmp = env->services;
        while(tmp->next)
            tmp = tmp->next;

        tmp->next = ser;
    }

    return 0;
}
