/**
 ****************************************************************************************
 *
 * @file ipc_bus.c
 *
 * @brief Upper IPC service bus built on a fixed MRPC service.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ipc.h"
#include "ipc_bus.h"
#include "ipc_core.h"
#include "ls_err.h"
#include "mrpc.h"
#include "rtos_al.h"

#define IPC_BUS_ENTRY_MSG_HANDLER           1U
#define IPC_BUS_ENTRY_PUBLISH_HANDLER       2U

struct ipc_bus_req_hdr
{
    uint32_t op;
    uint16_t service_id;
    uint16_t msg_id;
    uint16_t version;
    uint16_t resp_capacity;
};

struct ipc_bus_resp_hdr
{
    int32_t status;
};

struct ipc_bus_req
{
    uint16_t op;
    uint16_t service_id;
    uint16_t msg_id;
    uint16_t version;
    const void *payload;
    uint32_t payload_len;
};

/* service_id == 0 marks an unused slot (0 is rejected by the public API) */
struct ipc_bus_service_entry
{
    uint16_t service_id;
    uint16_t version;
};

struct ipc_bus_handler_entry
{
    uint16_t service_id;
    uint16_t msg_id;
    uint16_t entry_type;
    ipc_bus_handler_t handler;
    ipc_bus_handler_cfg_t cfg;
    void *ctx;
};

static struct ipc_bus_service_entry ipc_bus_services[IPC_BUS_MAX_SERVICES];
static struct ipc_bus_handler_entry ipc_bus_msg_handlers[IPC_BUS_MAX_MSG_HANDLERS];
static struct ipc_bus_handler_entry ipc_bus_pub_handlers[IPC_BUS_MAX_PUB_HANDLERS];
static struct ipc_ep *ipc_bus_mrpc_ep;


static uint32_t ipc_bus_effective_timeout(uint32_t timeout_ms)
{
    return timeout_ms ? timeout_ms : IPC_BUS_DEFAULT_TIMEOUT_MS;
}

static uint8_t ipc_bus_op_valid(uint32_t op)
{
    return ((op == IPC_BUS_OP_CALL) ||
            (op == IPC_BUS_OP_POST) ||
            (op == IPC_BUS_OP_PUBLISH));
}

static uint32_t ipc_bus_max_payload(void)
{
    uint32_t mrpc_overhead = sizeof(struct mrpc_req_msg);
    uint32_t bus_overhead = sizeof(struct ipc_bus_req_hdr);

    if (IPC_MSG_BUFFER_SIZE <= (mrpc_overhead + bus_overhead))
        return 0;

    return IPC_MSG_BUFFER_SIZE - mrpc_overhead - bus_overhead;
}

/**
 * @brief Allocate and fill the MRPC request frame used by IPC Bus.
 *
 * The bus header is stored in mrpc_req_msg::data first, followed by the
 * optional user payload. The caller owns the returned buffer and must free it
 * with rtos_free().
 */
static struct mrpc_req_msg *ipc_bus_mrpc_req_fill(uint16_t op,
                                                  uint16_t service_id,
                                                  uint16_t msg_id,
                                                  uint16_t version,
                                                  uint16_t resp_capacity,
                                                  const void *payload,
                                                  uint32_t payload_len)
{
    uint32_t req_size = sizeof(struct mrpc_req_msg) +
                        sizeof(struct ipc_bus_req_hdr) + payload_len;
    struct mrpc_req_msg *req_msg;
    struct ipc_bus_req_hdr *req;

    req_msg = rtos_malloc(req_size);
    if (req_msg == NULL)
        return NULL;

    memset(req_msg, 0, req_size);
    req_msg->id = MRPC_MSG_ID_IPC_BUS_DISPATCH;
    req_msg->len = req_size;

    req = (struct ipc_bus_req_hdr *)req_msg->data;
    req->op = op;
    req->service_id = service_id;
    req->msg_id = msg_id;
    req->version = version;
    req->resp_capacity = resp_capacity;

    if (payload_len)
        memcpy((uint8_t *)req + sizeof(*req), payload, payload_len);

    return req_msg;
}

static struct ipc_bus_service_entry *ipc_bus_find_service_locked(uint16_t service_id)
{
    uint32_t i;

    if (service_id == 0)
        return NULL;

    for (i = 0; i < IPC_BUS_MAX_SERVICES; i++)
    {
        if (ipc_bus_services[i].service_id == service_id)
            return &ipc_bus_services[i];
    }

    return NULL;
}

static struct ipc_bus_handler_entry *ipc_bus_find_msg_handler_locked(uint16_t service_id,
                                                                      uint16_t msg_id)
{
    uint32_t i;

    if (service_id == 0)
        return NULL;

    for (i = 0; i < IPC_BUS_MAX_MSG_HANDLERS; i++)
    {
        if ((ipc_bus_msg_handlers[i].service_id == service_id) &&
            (ipc_bus_msg_handlers[i].msg_id == msg_id))
            return &ipc_bus_msg_handlers[i];
    }

    return NULL;
}

static struct ipc_bus_handler_entry *ipc_bus_find_publish_handler_locked(uint16_t service_id,
                                                                          uint16_t msg_id,
                                                                          ipc_bus_handler_t handler,
                                                                          void *ctx)
{
    uint32_t i;

    if (service_id == 0)
        return NULL;

    for (i = 0; i < IPC_BUS_MAX_PUB_HANDLERS; i++)
    {
        if ((ipc_bus_pub_handlers[i].service_id == service_id) &&
            (ipc_bus_pub_handlers[i].msg_id == msg_id) &&
            (ipc_bus_pub_handlers[i].handler == handler) &&
            (ipc_bus_pub_handlers[i].ctx == ctx))
            return &ipc_bus_pub_handlers[i];
    }

    return NULL;
}

static uint16_t ipc_bus_local_service_version(uint16_t service_id)
{
    struct ipc_bus_service_entry *service;
    uint16_t version = IPC_BUS_SERVICE_VERSION_ANY;

    GLOBAL_INT_DISABLE();
    service = ipc_bus_find_service_locked(service_id);
    if (service != NULL)
        version = service->version;
    GLOBAL_INT_RESTORE();

    return version;
}

/**
 * @brief Check whether a received request is compatible with local service.
 *
 * A version value of IPC_BUS_SERVICE_VERSION_ANY skips strict version matching.
 * This allows simple services to ignore version negotiation while still letting
 * versioned services reject mismatched peers.
 */
static int32_t ipc_bus_check_service_version(uint16_t service_id, uint16_t version)
{
    struct ipc_bus_service_entry *service;
    int32_t ret = LS_OK;

    GLOBAL_INT_DISABLE();
    service = ipc_bus_find_service_locked(service_id);
    if (service == NULL)
    {
        ret = LS_ERR_NOT_FOUND;
    }
    else if ((service->version != IPC_BUS_SERVICE_VERSION_ANY) &&
             (version != IPC_BUS_SERVICE_VERSION_ANY) &&
             (service->version != version))
    {
        ret = LS_ERR_VERSION;
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/* Snapshot a msg handler under lock so it can run after the lock is released. */
static int32_t ipc_bus_copy_handler(uint16_t service_id,
                                    uint16_t msg_id,
                                    struct ipc_bus_handler_entry *entry)
{
    struct ipc_bus_handler_entry *found;
    int32_t ret = LS_FAIL;

    GLOBAL_INT_DISABLE();
    found = ipc_bus_find_msg_handler_locked(service_id, msg_id);
    if (found != NULL)
    {
        *entry = *found;
        ret = LS_OK;
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/* Snapshot the skip-th matching publish subscriber under lock. */
static int32_t ipc_bus_copy_publish_handler(uint16_t service_id,
                                            uint16_t msg_id,
                                            uint32_t skip,
                                            struct ipc_bus_handler_entry *entry)
{
    uint32_t matched = 0;
    int32_t ret = LS_FAIL;
    uint32_t i;

    GLOBAL_INT_DISABLE();
    for (i = 0; i < IPC_BUS_MAX_PUB_HANDLERS; i++)
    {
        if ((ipc_bus_pub_handlers[i].service_id != service_id) ||
            (ipc_bus_pub_handlers[i].msg_id != msg_id))
            continue;

        if (matched++ == skip)
        {
            *entry = ipc_bus_pub_handlers[i];
            ret = LS_OK;
            break;
        }
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/**
 * @brief Run a registered handler and validate its response length.
 *
 * For CALL requests, resp_len is both input capacity and output length. If the
 * handler writes more than the advertised capacity, the bus reports failure and
 * clears the response length.
 */
static int32_t ipc_bus_run_handler(const struct ipc_bus_handler_entry *entry,
                                   const void *req,
                                   uint32_t req_len,
                                   void *resp,
                                   uint32_t *resp_len)
{
    uint32_t resp_capacity = resp_len ? *resp_len : 0;
    int32_t status;

    if ((entry == NULL) || (entry->handler == NULL))
    {
        if (resp_len != NULL)
            *resp_len = 0;
        return LS_FAIL;
    }

    status = entry->handler(req, req_len, resp, resp_len, entry->ctx);
    if ((status != LS_OK) || ((resp_len != NULL) && (*resp_len > resp_capacity)))
    {
        if (resp_len != NULL)
            *resp_len = 0;
        return LS_FAIL;
    }

    return LS_OK;
}

/*
 * ===========================================================================
 *  Queue-mode execution (IPC_BUS_CONTEXT_QUEUE)
 *
 *  A queued handler does not run in the IPC dispatch context. The producer side
 *  (ipc_bus_dispatch_queue) copies the request into a heap ipc_bus_msg and posts
 *  the pointer to the handler's worker queue. The consumer side runs in that
 *  worker task and calls ipc_bus_msg_handle() (run the handler) or
 *  ipc_bus_msg_release() (drop it).
 *
 *  The heap message has exactly one owner, handed over through msg->waiter under
 *  a short interrupt-locked section so producer and consumer never double-free:
 *
 *    waiter != NULL : a CALL sender is blocked on the message. The consumer
 *                     wakes it and the SENDER frees the message.
 *    waiter == NULL : nobody is waiting (POST/PUBLISH, or the sender already
 *                     timed out and abandoned it). The CONSUMER frees it.
 * ===========================================================================
 */
struct ipc_bus_msg
{
    uint16_t op;
    uint16_t service_id;
    uint16_t msg_id;

    const void *req;
    uint32_t req_len;
    void *resp;
    uint32_t resp_capacity;
    uint32_t resp_len;

    int32_t status;
    void *ctx;

    ipc_bus_handler_t handler;
    rtos_task_handle waiter;
};

/*
 * Marshal a request into a heap message. The payload is copied in so the worker
 * can process it after dispatch returns; for CALL, response storage is reserved
 * in the same allocation.
 */
static ipc_bus_msg_t *ipc_bus_msg_alloc(const struct ipc_bus_handler_entry *entry,
                                        const struct ipc_bus_req *req,
                                        uint32_t resp_capacity,
                                        uint8_t sync)
{
    ipc_bus_msg_t *msg;
    uint8_t *cursor;
    uint32_t alloc_size;

    alloc_size = sizeof(ipc_bus_msg_t) + req->payload_len;
    if (sync)
        alloc_size += resp_capacity;

    msg = rtos_malloc(alloc_size);
    if (msg == NULL)
        return NULL;

    memset(msg, 0, sizeof(*msg));
    msg->op = req->op;
    msg->service_id = req->service_id;
    msg->msg_id = req->msg_id;
    msg->req_len = req->payload_len;
    msg->resp_capacity = sync ? resp_capacity : 0;
    msg->ctx = entry->ctx;
    msg->handler = entry->handler;

    cursor = (uint8_t *)msg + sizeof(*msg);
    if (req->payload_len)
    {
        memcpy(cursor, req->payload, req->payload_len);
        msg->req = cursor;
        cursor += req->payload_len;
    }

    if (sync && resp_capacity)
        msg->resp = cursor;

    return msg;
}

/*
 * Producer side of queue mode: marshal the request, post it to the worker queue,
 * and for CALL block until the worker completes it (up to call_timeout_ms). POST
 * and PUBLISH return as soon as the message is enqueued. See the section banner
 * above for how message ownership is transferred.
 */
static int32_t ipc_bus_dispatch_queue(const struct ipc_bus_handler_entry *entry,
                                      const struct ipc_bus_req *req,
                                      void *resp,
                                      uint32_t *resp_len)
{
    ipc_bus_msg_t *msg;
    rtos_task_handle waiter = RTOS_TASK_NULL;
    uint32_t resp_capacity = resp_len ? *resp_len : 0;
    uint32_t call_timeout;
    uint8_t sync = (req->op == IPC_BUS_OP_CALL);
    uint8_t completed = 0;
    int32_t status = LS_FAIL;

    if (entry->cfg.queue == NULL)
        return LS_FAIL;

    msg = ipc_bus_msg_alloc(entry, req, resp_capacity, sync);
    if (msg == NULL)
        return LS_FAIL;

    if (sync)
    {
        waiter = rtos_get_task_handle();
        if (waiter == RTOS_TASK_NULL)
            goto FAIL;
        (void)rtos_task_init_notification(waiter);
        (void)rtos_task_wait_notification(0);
        msg->waiter = waiter;
    }

    if (rtos_queue_write(entry->cfg.queue,
                         &msg,
                         (int)entry->cfg.enqueue_timeout_ms,
                         false))
    {
        goto FAIL;
    }

    if (!sync)
    {
        if (resp_len != NULL)
            *resp_len = 0;
        return LS_OK;
    }

    call_timeout = ipc_bus_effective_timeout(entry->cfg.call_timeout_ms);
    if (!rtos_task_wait_notification((int)call_timeout))
    {
        GLOBAL_INT_DISABLE();
        if (msg->waiter != RTOS_TASK_NULL)
            msg->waiter = RTOS_TASK_NULL;
        else
            completed = 1;
        GLOBAL_INT_RESTORE();

        if (!completed)
        {
            if (resp_len != NULL)
                *resp_len = 0;
            return LS_FAIL;
        }
    }

    status = msg->status;
    if ((status == LS_OK) && (resp_len != NULL))
    {
        if (msg->resp_len > resp_capacity)
        {
            status = LS_FAIL;
            *resp_len = 0;
        }
        else
        {
            if (msg->resp_len)
                memcpy(resp, msg->resp, msg->resp_len);
            *resp_len = msg->resp_len;
        }
    }

    rtos_free(msg);

    return status;

FAIL:
    rtos_free(msg);
    return LS_FAIL;
}

/*
 * Run a handler snapshot in its configured execution context. This is the only
 * place that distinguishes DIRECT from QUEUE, so every dispatch path can stay
 * context-agnostic and just call ipc_bus_invoke().
 */
static int32_t ipc_bus_invoke(const struct ipc_bus_handler_entry *entry,
                              const struct ipc_bus_req *req,
                              void *resp,
                              uint32_t *resp_len)
{
    if (entry->cfg.context == IPC_BUS_CONTEXT_QUEUE)
        return ipc_bus_dispatch_queue(entry, req, resp, resp_len);

    return ipc_bus_run_handler(entry, req->payload, req->payload_len, resp, resp_len);
}

/**
 * @brief Fan out a PUBLISH request to every matching local subscriber.
 *
 * Unlike normal message handlers, publish handlers are not unique by service and
 * message ID: multiple callbacks can subscribe to the same published message.
 */
static int32_t ipc_bus_dispatch_publish(const struct ipc_bus_req *req)
{
    struct ipc_bus_handler_entry entry;
    uint32_t skip = 0;
    uint16_t delivered = 0;
    int32_t status = LS_OK;

    while (ipc_bus_copy_publish_handler(req->service_id,
                                        req->msg_id,
                                        skip,
                                        &entry) == LS_OK)
    {
        int32_t ret;

        skip++;
        ret = ipc_bus_invoke(&entry, req, NULL, NULL);

        delivered++;
        if (ret != LS_OK)
            status = LS_FAIL;
    }

    return delivered ? status : LS_ERR_NOT_FOUND;
}

/**
 * @brief Dispatch a request to local service handlers.
 *
 * The function first verifies service registration and version compatibility,
 * then routes CALL/POST to a single message handler or PUBLISH to all matching
 * publish handlers.
 */
static int32_t ipc_bus_dispatch_local(const struct ipc_bus_req *req, void *resp, uint32_t *resp_len)
{
    struct ipc_bus_handler_entry copy;
    int32_t status;

    status = ipc_bus_check_service_version(req->service_id, req->version);
    if (status != LS_OK)
        goto fail;

    if (req->op == IPC_BUS_OP_PUBLISH)
        return ipc_bus_dispatch_publish(req);

    if (ipc_bus_copy_handler(req->service_id, req->msg_id, &copy) != LS_OK)
    {
        status = LS_ERR_NOT_FOUND;
        goto fail;
    }

    /* DIRECT or QUEUE: ipc_bus_invoke() handles both and owns resp_len. */
    return ipc_bus_invoke(&copy, req, resp, resp_len);

fail:
    if (resp_len != NULL)
        *resp_len = 0;
    return status;
}

/**
 * @brief MRPC server entry point for IPC Bus requests from the peer core.
 *
 * It parses the bus header, limits the response buffer to both local capacity
 * and caller-advertised capacity, then sends back an ipc_bus_resp_hdr followed
 * by optional CALL response payload.
 */
static void ipc_bus_mrpc_dispatch(void *msg, struct mrpc_resp_msg *resp_msg, uint32_t resp_buf_len)
{
    struct mrpc_req_msg *req_msg = msg;
    const struct ipc_bus_req_hdr *hdr;
    struct ipc_bus_req req;
    struct ipc_bus_resp_hdr *rsp;
    uint32_t resp_capacity;
    uint32_t out_len = 0;
    uint32_t req_len;
    int32_t status = LS_FAIL;

    rsp = (struct ipc_bus_resp_hdr *)resp_msg->data;
    resp_msg->status = LS_FAIL;
    resp_msg->len = sizeof(*resp_msg);

    if ((req_msg->len < (int32_t)(sizeof(*req_msg) + sizeof(*hdr))) ||
        (resp_buf_len < (sizeof(*resp_msg) + sizeof(*rsp))))
        return;

    req_len = (uint32_t)req_msg->len - sizeof(*req_msg);
    hdr = (const struct ipc_bus_req_hdr *)req_msg->data;

    if (!ipc_bus_op_valid(hdr->op))
        return;

    req.op = (uint16_t)hdr->op;
    req.service_id = hdr->service_id;
    req.msg_id = hdr->msg_id;
    req.version = hdr->version;
    req.payload = (const uint8_t *)req_msg->data + sizeof(*hdr);
    req.payload_len = req_len - sizeof(*hdr);

    resp_capacity = resp_buf_len - sizeof(*resp_msg) - sizeof(*rsp);
    if (resp_capacity > hdr->resp_capacity)
        resp_capacity = hdr->resp_capacity;
    if (req.op != IPC_BUS_OP_CALL)
        resp_capacity = 0;

    memset(rsp, 0, sizeof(*rsp));
    rsp->status = LS_FAIL;

    if (req.op == IPC_BUS_OP_CALL)
    {
        out_len = resp_capacity;
        status = ipc_bus_dispatch_local(&req, (uint8_t *)rsp + sizeof(*rsp), &out_len);
    }
    else
    {
        status = ipc_bus_dispatch_local(&req, NULL, NULL);
    }

    rsp->status = status;
    resp_msg->status = LS_OK;
    resp_msg->len = sizeof(*resp_msg) + sizeof(*rsp) + out_len;
}

mrpc_msg_handler_t ipc_bus_mrpc_handlers[IPC_BUS_MRPC_HANDLER_NUM] =
{
    ipc_bus_mrpc_dispatch,
};

/**
 * @brief Initialize the MRPC client endpoint used by IPC Bus send APIs.
 *
 * The function is idempotent. It selects channel direction according to whether
 * this build is the IPC master or slave side.
 *
 * @return LS_OK on success, LS_FAIL if client support is disabled or init fails.
 */
int32_t ipc_bus_mrpc_client_init(void)
{
#ifdef CFG_AMP_IPC_MRPC_CLIENT
    if (ipc_bus_mrpc_ep != NULL)
        return LS_OK;

#ifdef CFG_AMP_IPC_MASTER
    ipc_bus_mrpc_ep = mrpc_client_ep_init(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_BUS_CLT, IPC_EP_MRPC_BUS_SRV);
#else
    ipc_bus_mrpc_ep = mrpc_client_ep_init(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_BUS_CLT, IPC_EP_MRPC_BUS_SRV);
#endif

    return (ipc_bus_mrpc_ep != NULL) ? LS_OK : LS_FAIL;
#else
    return LS_FAIL;
#endif
}

/**
 * @brief Initialize IPC Bus for upper-layer users.
 *
 * Call this before ipc_bus_call(), ipc_bus_post(), or ipc_bus_publish() on a
 * side that sends requests to the peer core.
 *
 * @return LS_OK on success, LS_FAIL on initialization failure.
 */
int32_t ipc_bus_init(void)
{
    return ipc_bus_mrpc_client_init();
}

/**
 * @brief Get the maximum user payload size accepted by IPC Bus send APIs.
 *
 * The returned value excludes the MRPC and IPC Bus protocol headers.
 */
uint32_t ipc_bus_get_max_payload(void)
{
    return ipc_bus_max_payload();
}

static void ipc_bus_clear_service_handlers_locked(struct ipc_bus_handler_entry *arr,
                                                  uint32_t count,
                                                  uint16_t service_id)
{
    uint32_t i;

    for (i = 0; i < count; i++)
    {
        if (arr[i].service_id == service_id)
            memset(&arr[i], 0, sizeof(arr[i]));
    }
}

/**
 * @brief Register or update a local IPC Bus service.
 *
 * A service must be registered before its message or publish handlers are
 * registered. Registering the same service_id again updates its version.
 * Use IPC_BUS_SERVICE_VERSION_ANY when the service does not require version
 * matching.
 *
 * @param service_id Non-zero service identifier.
 * @param version Service protocol version or IPC_BUS_SERVICE_VERSION_ANY.
 * @return LS_OK, LS_ERR_PARAM for service_id 0, or LS_ERR_NO_MEM.
 */
int32_t ipc_bus_register_service(uint16_t service_id, uint16_t version)
{
    struct ipc_bus_service_entry *service;
    uint32_t i;
    int32_t ret = LS_OK;

    if (service_id == 0)
        return LS_ERR_PARAM;

    GLOBAL_INT_DISABLE();
    service = ipc_bus_find_service_locked(service_id);
    if (service != NULL)
    {
        service->version = version;
    }
    else
    {
        ret = LS_ERR_NO_MEM;
        for (i = 0; i < IPC_BUS_MAX_SERVICES; i++)
        {
            if (ipc_bus_services[i].service_id == 0)
            {
                ipc_bus_services[i].service_id = service_id;
                ipc_bus_services[i].version = version;
                ret = LS_OK;
                break;
            }
        }
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/**
 * @brief Unregister a local IPC Bus service and all handlers under it.
 *
 * After this call, incoming requests for the service return LS_ERR_NOT_FOUND
 * until the service and handlers are registered again.
 *
 * @param service_id Non-zero service identifier.
 * @return LS_OK on removal, LS_ERR_NOT_FOUND if absent, or LS_ERR_PARAM.
 */
int32_t ipc_bus_unregister_service(uint16_t service_id)
{
    struct ipc_bus_service_entry *service;
    int32_t ret = LS_ERR_NOT_FOUND;

    if (service_id == 0)
        return LS_ERR_PARAM;

    GLOBAL_INT_DISABLE();
    service = ipc_bus_find_service_locked(service_id);
    if (service != NULL)
    {
        memset(service, 0, sizeof(*service));
        ipc_bus_clear_service_handlers_locked(ipc_bus_msg_handlers, IPC_BUS_MAX_MSG_HANDLERS, service_id);
        ipc_bus_clear_service_handlers_locked(ipc_bus_pub_handlers, IPC_BUS_MAX_PUB_HANDLERS, service_id);
        ret = LS_OK;
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

static void ipc_bus_entry_set(struct ipc_bus_handler_entry *entry,
                              uint16_t service_id,
                              uint16_t msg_id,
                              uint16_t entry_type,
                              ipc_bus_handler_t handler,
                              const ipc_bus_handler_cfg_t *cfg,
                              void *ctx)
{
    entry->service_id = service_id;
    entry->msg_id = msg_id;
    entry->entry_type = entry_type;
    entry->handler = handler;
    if (cfg != NULL)
        entry->cfg = *cfg;
    else
        memset(&entry->cfg, 0, sizeof(entry->cfg));
    entry->ctx = ctx;
}

static int32_t ipc_bus_register_entry(uint16_t service_id,
                                      uint16_t msg_id,
                                      uint16_t entry_type,
                                      ipc_bus_handler_t handler,
                                      const ipc_bus_handler_cfg_t *cfg,
                                      void *ctx)
{
    struct ipc_bus_handler_entry *arr;
    struct ipc_bus_handler_entry *entry;
    uint32_t count;
    uint32_t i;
    int32_t ret = LS_OK;

    if ((service_id == 0) || (handler == NULL))
        return LS_ERR_PARAM;

    if (entry_type == IPC_BUS_ENTRY_MSG_HANDLER)
    {
        arr = ipc_bus_msg_handlers;
        count = IPC_BUS_MAX_MSG_HANDLERS;
    }
    else if (entry_type == IPC_BUS_ENTRY_PUBLISH_HANDLER)
    {
        arr = ipc_bus_pub_handlers;
        count = IPC_BUS_MAX_PUB_HANDLERS;
    }
    else
    {
        return LS_ERR_PARAM;
    }

    GLOBAL_INT_DISABLE();
    if (ipc_bus_find_service_locked(service_id) == NULL)
    {
        ret = LS_ERR_NOT_FOUND;
    }
    else
    {
        if (entry_type == IPC_BUS_ENTRY_MSG_HANDLER)
            entry = ipc_bus_find_msg_handler_locked(service_id, msg_id);
        else
            entry = ipc_bus_find_publish_handler_locked(service_id, msg_id, handler, ctx);

        if (entry != NULL)
        {
            ipc_bus_entry_set(entry, service_id, msg_id, entry_type, handler, cfg, ctx);
        }
        else
        {
            ret = LS_ERR_NO_MEM;
            for (i = 0; i < count; i++)
            {
                if (arr[i].service_id == 0)
                {
                    ipc_bus_entry_set(&arr[i], service_id, msg_id, entry_type, handler, cfg, ctx);
                    ret = LS_OK;
                    break;
                }
            }
        }
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/**
 * @brief Register the single CALL/POST handler for a service message.
 *
 * The service must already exist. Registering the same service_id and msg_id
 * replaces the previous handler configuration. With IPC_BUS_CONTEXT_DIRECT the
 * handler runs in the dispatch context; with IPC_BUS_CONTEXT_QUEUE the bus posts
 * ipc_bus_msg_t* to cfg->queue and a user task must call ipc_bus_msg_handle().
 *
 * @param service_id Registered service identifier.
 * @param msg_id Message identifier within the service.
 * @param handler User callback that processes request and optional response.
 * @param cfg Handler execution configuration; NULL means direct defaults.
 * @param ctx User context passed to handler.
 * @return LS_OK, LS_ERR_NOT_FOUND if service is absent, LS_ERR_PARAM, or LS_ERR_NO_MEM.
 */
int32_t ipc_bus_register_msg_handler(uint16_t service_id,
                                     uint16_t msg_id,
                                     ipc_bus_handler_t handler,
                                     const ipc_bus_handler_cfg_t *cfg,
                                     void *ctx)
{
    return ipc_bus_register_entry(service_id, msg_id, IPC_BUS_ENTRY_MSG_HANDLER, handler, cfg, ctx);
}

/**
 * @brief Remove the CALL/POST handler for a service message.
 *
 * @return LS_OK when removed or LS_ERR_NOT_FOUND when no handler matches.
 */
int32_t ipc_bus_unregister_msg_handler(uint16_t service_id, uint16_t msg_id)
{
    struct ipc_bus_handler_entry *entry;
    int32_t ret = LS_ERR_NOT_FOUND;

    GLOBAL_INT_DISABLE();
    entry = ipc_bus_find_msg_handler_locked(service_id, msg_id);
    if (entry != NULL)
    {
        memset(entry, 0, sizeof(*entry));
        ret = LS_OK;
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/**
 * @brief Subscribe a handler to a published service message.
 *
 * Multiple publish handlers can share the same service_id and msg_id. A handler
 * is considered duplicate only when service_id, msg_id, handler, and ctx all
 * match; registering that tuple again updates its configuration.
 *
 * @return LS_OK, LS_ERR_NOT_FOUND if service is absent, LS_ERR_PARAM, or LS_ERR_NO_MEM.
 */
int32_t ipc_bus_register_publish_handler(uint16_t service_id,
                                         uint16_t msg_id,
                                         ipc_bus_handler_t handler,
                                         const ipc_bus_handler_cfg_t *cfg,
                                         void *ctx)
{
    return ipc_bus_register_entry(service_id,
                                  msg_id,
                                  IPC_BUS_ENTRY_PUBLISH_HANDLER,
                                  handler,
                                  cfg,
                                  ctx);
}

/**
 * @brief Remove one publish subscription.
 *
 * The same handler and ctx values used during registration must be provided.
 *
 * @return LS_OK when removed or LS_ERR_NOT_FOUND when no subscription matches.
 */
int32_t ipc_bus_unregister_publish_handler(uint16_t service_id,
                                           uint16_t msg_id,
                                           ipc_bus_handler_t handler,
                                           void *ctx)
{
    struct ipc_bus_handler_entry *entry;
    int32_t ret = LS_ERR_NOT_FOUND;

    GLOBAL_INT_DISABLE();
    entry = ipc_bus_find_publish_handler_locked(service_id, msg_id, handler, ctx);
    if (entry != NULL)
    {
        memset(entry, 0, sizeof(*entry));
        ret = LS_OK;
    }
    GLOBAL_INT_RESTORE();

    return ret;
}

/**
 * @brief Common sender for CALL, POST, and PUBLISH messages.
 *
 * This helper validates payload and response sizes, builds the MRPC frame, and
 * waits for a peer response only for IPC_BUS_OP_CALL.
 */
static int32_t ipc_bus_send(uint16_t op,
                            uint16_t service_id,
                            uint16_t msg_id,
                            const void *req_data,
                            uint32_t req_len,
                            void *resp_data,
                            uint32_t *resp_len,
                            uint32_t timeout_ms)
{
#ifdef CFG_AMP_IPC_MRPC_CLIENT
    struct mrpc_req_msg *req_msg = NULL;
    struct mrpc_resp_msg *resp_msg = NULL;
    struct ipc_bus_resp_hdr *resp = NULL;
    uint32_t resp_payload_len;
    uint8_t wait_resp = (op == IPC_BUS_OP_CALL);
    uint32_t resp_user_capacity = (wait_resp && resp_len) ? *resp_len : 0;
    uint32_t resp_msg_size = 0;
    uint16_t version;
    int32_t status = LS_FAIL;

    if (!ipc_bus_op_valid(op) ||
        (req_len && (req_data == NULL)) ||
        (req_len > ipc_bus_max_payload()) ||
        (ipc_bus_mrpc_ep == NULL))
        goto out;

    if (wait_resp)
    {
        resp_msg_size = sizeof(*resp_msg) + sizeof(*resp) + resp_user_capacity;
        if ((resp_len && (*resp_len != 0) && (resp_data == NULL)) ||
            (resp_user_capacity > UINT16_MAX) ||
            (resp_msg_size > IPC_MSG_BUFFER_SIZE))
            goto out;
    }

    version = ipc_bus_local_service_version(service_id);
    req_msg = ipc_bus_mrpc_req_fill(op,
                                    service_id,
                                    msg_id,
                                    version,
                                    (uint16_t)resp_user_capacity,
                                    req_data,
                                    req_len);
    if (req_msg == NULL)
        goto out;

    if (wait_resp)
    {
        resp_msg = rtos_malloc(resp_msg_size);
        if (resp_msg == NULL)
            goto out;
        memset(resp_msg, 0, resp_msg_size);
        resp = (struct ipc_bus_resp_hdr *)resp_msg->data;
    }

    if (mrpc_msg_send_from(ipc_bus_mrpc_ep,
                                   req_msg,
                                   (uint32_t)req_msg->len,
                                   resp_msg,
                                   ipc_bus_effective_timeout(timeout_ms)) == 0)
    {
        if (!wait_resp)
        {
            status = LS_OK;
        }
        else if ((resp_msg->status == LS_OK) &&
            (resp_msg->len >= (int32_t)(sizeof(*resp_msg) + sizeof(*resp))) &&
            (resp_msg->len <= (int32_t)resp_msg_size))
        {
            resp_payload_len = (uint32_t)resp_msg->len - sizeof(*resp_msg) - sizeof(*resp);
            status = resp->status;
            if (resp_len != NULL)
            {
                if ((status == LS_OK) && (resp_payload_len <= *resp_len))
                {
                    if (resp_payload_len)
                        memcpy(resp_data, (uint8_t *)resp + sizeof(*resp), resp_payload_len);
                    *resp_len = resp_payload_len;
                }
                else
                {
                    *resp_len = 0;
                    if (status == LS_OK)
                        status = LS_FAIL;
                }
            }
        }
    }

out:
    if ((status != LS_OK) && (resp_len != NULL))
        *resp_len = 0;
    if (resp_msg != NULL)
        rtos_free(resp_msg);
    if (req_msg != NULL)
        rtos_free(req_msg);

    return status;
#else
    (void)op;
    (void)service_id;
    (void)msg_id;
    (void)req_data;
    (void)req_len;
    (void)resp_data;
    (void)timeout_ms;

    if (resp_len != NULL)
        *resp_len = 0;

    return LS_FAIL;
#endif
}

/**
 * @brief Send a synchronous request to a peer service and wait for response.
 *
 * Before calling, set *resp_len to the capacity of resp. On success, *resp_len
 * is replaced with the actual response length. On failure, *resp_len is cleared
 * to 0. timeout_ms of 0 uses IPC_BUS_DEFAULT_TIMEOUT_MS.
 *
 * @param service_id Target service identifier.
 * @param msg_id Target message identifier.
 * @param req Request payload, or NULL when req_len is 0.
 * @param req_len Request payload length, up to ipc_bus_get_max_payload().
 * @param resp Response buffer, required when *resp_len is non-zero.
 * @param resp_len In/out response capacity and actual length.
 * @param timeout_ms Wait timeout in milliseconds, or 0 for default.
 * @return LS_OK when the peer handler succeeds; otherwise an LS_ERR_* or LS_FAIL.
 */
int32_t ipc_bus_call(uint16_t service_id,
                     uint16_t msg_id,
                     const void *req,
                     uint32_t req_len,
                     void *resp,
                     uint32_t *resp_len,
                     uint32_t timeout_ms)
{
    return ipc_bus_send(IPC_BUS_OP_CALL, service_id, msg_id,
                        req, req_len, resp, resp_len, timeout_ms);
}

/**
 * @brief Send an asynchronous one-to-one message to a peer service.
 *
 * POST does not carry a response payload and returns after the request has been
 * sent through MRPC. The remote side dispatches it to the registered message
 * handler for service_id/msg_id.
 *
 * @return LS_OK on successful send, otherwise LS_FAIL.
 */
int32_t ipc_bus_post(uint16_t service_id,
                     uint16_t msg_id,
                     const void *data,
                     uint32_t len)
{
    return ipc_bus_send(IPC_BUS_OP_POST, service_id, msg_id,
                        data, len, NULL, NULL, 0);
}

/**
 * @brief Publish a message to local subscribers and peer subscribers.
 *
 * PUBLISH fans out to all local publish handlers first, then sends the same
 * payload to the peer core. The call succeeds if local dispatch or remote send
 * succeeds.
 *
 * @return LS_OK if delivered locally or remotely; otherwise the remote send status.
 */
int32_t ipc_bus_publish(uint16_t service_id,
                        uint16_t msg_id,
                        const void *data,
                        uint32_t len)
{
    struct ipc_bus_req req;
    int32_t local_status;
    int32_t remote_status;

    if (len && (data == NULL))
        return LS_FAIL;

    req.op = IPC_BUS_OP_PUBLISH;
    req.service_id = service_id;
    req.msg_id = msg_id;
    req.version = ipc_bus_local_service_version(service_id);
    req.payload = data;
    req.payload_len = len;

    local_status = ipc_bus_dispatch_local(&req, NULL, NULL);
    remote_status = ipc_bus_send(IPC_BUS_OP_PUBLISH, service_id, msg_id,
                                 data, len, NULL, NULL, 0);

    if ((remote_status == LS_OK) || (local_status == LS_OK))
        return LS_OK;

    return remote_status;
}

/*
 * Consumer side: finish a queued message and resolve its ownership. If a CALL
 * sender is still waiting, store the result and wake it (the sender then frees
 * the message); otherwise free it here. See the queue-mode section banner above.
 */
static void ipc_bus_msg_complete(ipc_bus_msg_t *msg, int32_t status, uint32_t resp_len)
{
    rtos_task_handle waiter;

    msg->status = status;
    msg->resp_len = resp_len;

    GLOBAL_INT_DISABLE();
    waiter = msg->waiter;
    if (waiter != RTOS_TASK_NULL)
        msg->waiter = RTOS_TASK_NULL;
    GLOBAL_INT_RESTORE();

    if (waiter != RTOS_TASK_NULL)
    {
        rtos_task_notify(waiter, false);
    }
    else
    {
        rtos_free(msg);
    }
}

/**
 * @brief Execute a queued IPC Bus message and complete it.
 *
 * Use this in the task that reads ipc_bus_msg_t* from a queue configured with
 * IPC_BUS_CONTEXT_QUEUE. For CALL messages, the waiting sender is notified with
 * the handler status and response length. For POST/PUBLISH messages, the message
 * storage is released after the handler returns.
 *
 * @return LS_OK if the queued message object was accepted, otherwise LS_FAIL.
 */
int32_t ipc_bus_msg_handle(ipc_bus_msg_t *msg)
{
    struct ipc_bus_handler_entry entry;
    uint32_t resp_len;
    int32_t status;

    if ((msg == NULL) || (msg->handler == NULL))
        return LS_FAIL;

    memset(&entry, 0, sizeof(entry));
    entry.service_id = msg->service_id;
    entry.msg_id = msg->msg_id;
    entry.entry_type = IPC_BUS_ENTRY_MSG_HANDLER;
    entry.handler = msg->handler;
    entry.ctx = msg->ctx;

    resp_len = msg->resp_capacity;
    status = ipc_bus_run_handler(&entry,
                                 msg->req,
                                 msg->req_len,
                                 msg->resp,
                                 (msg->op == IPC_BUS_OP_CALL) ? &resp_len : NULL);
    ipc_bus_msg_complete(msg,
                         status,
                         (status == LS_OK && msg->op == IPC_BUS_OP_CALL) ? resp_len : 0);

    return LS_OK;
}

/**
 * @brief Drop a queued IPC Bus message without running its handler.
 *
 * This is useful when a queue consumer decides it cannot process the message.
 * A waiting CALL sender is completed with LS_FAIL; unowned async messages are
 * freed immediately.
 */
void ipc_bus_msg_release(ipc_bus_msg_t *msg)
{
    if (msg == NULL)
        return;

    ipc_bus_msg_complete(msg, LS_FAIL, 0);
}
