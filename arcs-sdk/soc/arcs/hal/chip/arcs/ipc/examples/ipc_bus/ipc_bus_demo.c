/**
 ****************************************************************************************
 *
 * @file ipc_bus_demo.c
 *
 * @brief Small demo for IPC Bus.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include <stdint.h>
#include "ipc_bus.h"
#include "ipc_bus_services.h"
#include "log_print.h"
#include "ls_err.h"

/* This demo's service version (the service ID lives in ipc_bus_services.h). */
#define IPC_SVC_DEMO_VERSION                1

/* CALL / POST messages handled by the demo service. */
typedef enum
{
    IPC_DEMO_MSG_PING   = 1,
    IPC_DEMO_MSG_NOTIFY = 2,
} ipc_demo_msg_t;

/* PUBLISH topics emitted by the demo service (separate ID space from MSG). */
typedef enum
{
    IPC_DEMO_EVT_NOTICE = 1,
} ipc_demo_evt_t;

#define IPC_BUS_DEMO_QUEUE_LEN              4
#define IPC_BUS_DEMO_TASK_STACK_SIZE        256
#define IPC_BUS_DEMO_TASK_PRIORITY          RTOS_TASK_PRIORITY(7)

struct ipc_bus_demo_ping_req
{
    uint32_t value;
};

struct ipc_bus_demo_ping_resp
{
    uint32_t value;
    uint32_t result;
};

struct ipc_bus_demo_notice
{
    uint32_t value;
};

static rtos_queue ipc_bus_demo_queue;
static rtos_task_handle ipc_bus_demo_task_handle;

/**
 * @brief Queue-mode worker for IPC Bus messages.
 *
 * Handlers registered with IPC_BUS_CONTEXT_QUEUE receive ipc_bus_msg_t* through
 * the configured queue. The worker must call ipc_bus_msg_handle() so CALL
 * senders can be notified and message storage can be released correctly.
 */
static RTOS_TASK_FCT(ipc_bus_demo_task)
{
    rtos_queue queue = env;
    ipc_bus_msg_t *msg;

    while (1)
    {
        if (rtos_queue_read(queue, &msg, -1, false) == 0)
            ipc_bus_msg_handle(msg);
    }
}

/**
 * @brief Demo CALL handler.
 *
 * This handler expects struct ipc_bus_demo_ping_req and returns
 * struct ipc_bus_demo_ping_resp. The caller provides response capacity through
 * resp_len; the handler writes the actual response size before returning LS_OK.
 */
static int32_t ipc_bus_demo_ping_handler(const void *req_buf,
                                         uint32_t req_len,
                                         void *resp_buf,
                                         uint32_t *resp_len,
                                         void *ctx)
{
    const struct ipc_bus_demo_ping_req *req = req_buf;
    struct ipc_bus_demo_ping_resp *resp = resp_buf;

    (void)ctx;

    if ((req_len != sizeof(*req)) || (resp_len == NULL) || (*resp_len < sizeof(*resp)))
        return LS_FAIL;

    resp->value = req->value;
    resp->result = req->value + 1;
    *resp_len = sizeof(*resp);

    CLOGD("ipc bus demo: recv ping value=%u\n", req->value);

    return LS_OK;
}

/**
 * @brief Demo POST/PUBLISH handler.
 *
 * POST and PUBLISH messages do not use response buffers, so resp_buf and
 * resp_len are ignored. The same handler is reused by a queued POST message and
 * a direct publish subscriber in ipc_bus_demo_server_init().
 */
static int32_t ipc_bus_demo_notice_handler(const void *req_buf,
                                           uint32_t req_len,
                                           void *resp_buf,
                                           uint32_t *resp_len,
                                           void *ctx)
{
    const struct ipc_bus_demo_notice *notice = req_buf;

    (void)ctx;
    (void)resp_buf;
    (void)resp_len;

    if (req_len != sizeof(*notice))
        return LS_FAIL;

    CLOGD("ipc bus demo: notice value=%u\n", notice->value);

    return LS_OK;
}

/**
 * @brief Create the queue and worker task used by the queued demo handler.
 *
 * If this fails, the demo falls back to direct handler mode for the notify
 * message so the rest of the IPC Bus example can still be used.
 */
static int32_t ipc_bus_demo_queue_init(void)
{
    if (ipc_bus_demo_queue != NULL)
        return LS_OK;

    if (rtos_queue_create(sizeof(ipc_bus_msg_t *),
                          IPC_BUS_DEMO_QUEUE_LEN,
                          &ipc_bus_demo_queue))
        return LS_FAIL;

    if (rtos_task_create(ipc_bus_demo_task,
                         "ipc_bus_demo",
                         APPLICATION_TASK,
                         IPC_BUS_DEMO_TASK_STACK_SIZE,
                         ipc_bus_demo_queue,
                         IPC_BUS_DEMO_TASK_PRIORITY,
                         &ipc_bus_demo_task_handle))
    {
        rtos_queue_delete(ipc_bus_demo_queue);
        ipc_bus_demo_queue = NULL;
        return LS_FAIL;
    }

    return LS_OK;
}

/**
 * @brief Register the demo service and its handlers.
 *
 * Call this on the core that provides IPC_SVC_DEMO before the peer
 * sends demo requests. It registers:
 * - IPC_DEMO_MSG_PING as a direct CALL handler.
 * - IPC_DEMO_MSG_NOTIFY as a queued POST handler.
 * - IPC_DEMO_EVT_NOTICE as a direct publish subscriber.
 */
void ipc_bus_demo_server_init(void)
{
    ipc_bus_handler_cfg_t direct_cfg = {0};
    ipc_bus_handler_cfg_t queue_cfg = {0};

    direct_cfg.context = IPC_BUS_CONTEXT_DIRECT;
    direct_cfg.call_timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;

    queue_cfg.context = IPC_BUS_CONTEXT_QUEUE;
    queue_cfg.call_timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;
    queue_cfg.enqueue_timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;
    if (ipc_bus_demo_queue_init() == LS_OK)
    {
        queue_cfg.queue = ipc_bus_demo_queue;
    }
    else
    {
        CLOGD("ipc bus demo: queue init failed, fallback to direct handler\n");
        queue_cfg = direct_cfg;
    }

    ipc_bus_register_service(IPC_SVC_DEMO, IPC_SVC_DEMO_VERSION);
    ipc_bus_register_msg_handler(IPC_SVC_DEMO,
                                 IPC_DEMO_MSG_PING,
                                 ipc_bus_demo_ping_handler,
                                 &direct_cfg,
                                 NULL);
    ipc_bus_register_msg_handler(IPC_SVC_DEMO,
                                 IPC_DEMO_MSG_NOTIFY,
                                 ipc_bus_demo_notice_handler,
                                 &queue_cfg,
                                 NULL);
    ipc_bus_register_publish_handler(IPC_SVC_DEMO,
                                     IPC_DEMO_EVT_NOTICE,
                                     ipc_bus_demo_notice_handler,
                                     &direct_cfg,
                                     NULL);
}

/**
 * @brief Send a synchronous ping request to the demo service.
 *
 * The function wraps ipc_bus_call(): it fills a ping request, passes a response
 * buffer, and prints the returned value/result pair when the peer handler
 * succeeds.
 *
 * @return LS_OK on successful CALL response, otherwise the IPC Bus error code.
 */
int32_t ipc_bus_demo_ping(uint32_t value)
{
    struct ipc_bus_demo_ping_req req;
    struct ipc_bus_demo_ping_resp resp;
    uint32_t resp_len = sizeof(resp);
    int32_t ret;

    req.value = value;

    ret = ipc_bus_call(IPC_SVC_DEMO,
                       IPC_DEMO_MSG_PING,
                       &req,
                       sizeof(req),
                       &resp,
                       &resp_len,
                       IPC_BUS_DEFAULT_TIMEOUT_MS);
    if (ret == LS_OK)
    {
        CLOGD("ipc bus demo: ping resp value=%u result=%u\n",
              resp.value, resp.result);
    }
    else
    {
        CLOGD("ipc bus demo: ping failed ret=%d resp_len=%u\n", ret, resp_len);
    }

    return ret;
}

/**
 * @brief Send an asynchronous notify message to the demo service.
 *
 * The peer service dispatches this through IPC_DEMO_MSG_NOTIFY. In this
 * demo, that message is configured for queue mode, so the worker task processes
 * it outside the MRPC dispatch context.
 */
int32_t ipc_bus_demo_post(uint32_t value)
{
    struct ipc_bus_demo_notice notice;

    notice.value = value;

    return ipc_bus_post(IPC_SVC_DEMO,
                        IPC_DEMO_MSG_NOTIFY,
                        &notice,
                        sizeof(notice));
}

/**
 * @brief Publish a notice to local and peer demo subscribers.
 *
 * This wraps ipc_bus_publish(), so every matching local publish handler and peer
 * publish handler can receive the same notice payload.
 */
int32_t ipc_bus_demo_publish(uint32_t value)
{
    struct ipc_bus_demo_notice notice;

    notice.value = value;

    return ipc_bus_publish(IPC_SVC_DEMO,
                           IPC_DEMO_EVT_NOTICE,
                           &notice,
                           sizeof(notice));
}
