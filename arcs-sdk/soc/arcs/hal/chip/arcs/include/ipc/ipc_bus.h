/**
 ****************************************************************************************
 *
 * @file ipc_bus.h
 *
 * @brief Upper IPC service bus built on a dedicated MRPC service.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */
#ifndef _IPC_BUS_H_
#define _IPC_BUS_H_

#include <stdint.h>
#include "mrpc_types.h"
#include "rtos_al.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_BUS_DEFAULT_TIMEOUT_MS          5000
#define IPC_BUS_SERVICE_VERSION_ANY         0

#ifndef IPC_BUS_MAX_SERVICES
#define IPC_BUS_MAX_SERVICES                2
#endif
#ifndef IPC_BUS_MAX_MSG_HANDLERS
#define IPC_BUS_MAX_MSG_HANDLERS            4
#endif
#ifndef IPC_BUS_MAX_PUB_HANDLERS
#define IPC_BUS_MAX_PUB_HANDLERS            4
#endif

enum
{
    MRPC_MSG_ID_IPC_BUS_START = MRPC_SERVICE_TYPE_IPC_BUS << 24,
    MRPC_MSG_ID_IPC_BUS_DISPATCH,
    MRPC_MSG_ID_IPC_BUS_MAX
};

#define IPC_BUS_MRPC_HANDLER_NUM            (MRPC_MSG_ID_IPC_BUS_MAX - MRPC_MSG_ID_IPC_BUS_START)

typedef enum
{
    IPC_BUS_OP_CALL = 1,
    IPC_BUS_OP_POST,
    IPC_BUS_OP_PUBLISH,
} ipc_bus_op_t;

/*
 * Execution context for a registered handler.
 *
 *   IPC_BUS_CONTEXT_DIRECT  The handler runs immediately, in the IPC dispatch
 *                           context. Lowest latency, but the handler must be
 *                           short and must not block.
 *
 *   IPC_BUS_CONTEXT_QUEUE   The bus marshals the request into a message and
 *                           posts it to a worker queue; the handler runs later
 *                           in that worker task. Use this when the handler may
 *                           block or do heavy work. A CALL sender stays blocked
 *                           until the worker finishes (up to call_timeout_ms).
 */
typedef enum
{
    IPC_BUS_CONTEXT_DIRECT = 0,
    IPC_BUS_CONTEXT_QUEUE,
} ipc_bus_context_type_t;

typedef int32_t (*ipc_bus_handler_t)(const void *req,
                                     uint32_t req_len,
                                     void *resp,
                                     uint32_t *resp_len,
                                     void *ctx);

/*
 * Handler execution configuration.
 *
 * Pass NULL to the register functions to accept direct-mode defaults. The
 * queue fields below are used only when context is IPC_BUS_CONTEXT_QUEUE;
 * IPC_BUS_CONTEXT_DIRECT ignores them.
 */
typedef struct
{
    ipc_bus_context_type_t context;

    rtos_queue queue;              /* worker queue that receives ipc_bus_msg_t* */
    uint32_t   enqueue_timeout_ms; /* max wait when posting the message to queue */
    uint32_t   call_timeout_ms;    /* max wait for a CALL worker (0 = default)   */
} ipc_bus_handler_cfg_t;

/*
 * Opaque queued-message handle.
 *
 * The bus creates one per IPC_BUS_CONTEXT_QUEUE delivery and posts the pointer
 * to the handler's queue. The worker task hands it back to the bus through
 * ipc_bus_msg_handle() (run the handler) or ipc_bus_msg_release() (drop it),
 * after which the bus frees it. Worker code never dereferences this type.
 */
typedef struct ipc_bus_msg ipc_bus_msg_t;

int32_t ipc_bus_init(void);
int32_t ipc_bus_mrpc_client_init(void);
uint32_t ipc_bus_get_max_payload(void);
int32_t ipc_bus_register_service(uint16_t service_id, uint16_t version);
int32_t ipc_bus_unregister_service(uint16_t service_id);
int32_t ipc_bus_register_msg_handler(uint16_t service_id,
                                     uint16_t msg_id,
                                     ipc_bus_handler_t handler,
                                     const ipc_bus_handler_cfg_t *cfg,
                                     void *ctx);
int32_t ipc_bus_unregister_msg_handler(uint16_t service_id, uint16_t msg_id);
int32_t ipc_bus_register_publish_handler(uint16_t service_id,
                                         uint16_t msg_id,
                                         ipc_bus_handler_t handler,
                                         const ipc_bus_handler_cfg_t *cfg,
                                         void *ctx);
int32_t ipc_bus_unregister_publish_handler(uint16_t service_id,
                                           uint16_t msg_id,
                                           ipc_bus_handler_t handler,
                                           void *ctx);

int32_t ipc_bus_call(uint16_t service_id,
                     uint16_t msg_id,
                     const void *req,
                     uint32_t req_len,
                     void *resp,
                     uint32_t *resp_len,
                     uint32_t timeout_ms);
int32_t ipc_bus_post(uint16_t service_id,
                     uint16_t msg_id,
                     const void *data,
                     uint32_t len);
int32_t ipc_bus_publish(uint16_t service_id,
                        uint16_t msg_id,
                        const void *data,
                        uint32_t len);

int32_t ipc_bus_msg_handle(ipc_bus_msg_t *msg);
void ipc_bus_msg_release(ipc_bus_msg_t *msg);

extern mrpc_msg_handler_t ipc_bus_mrpc_handlers[IPC_BUS_MRPC_HANDLER_NUM];

#ifdef __cplusplus
}
#endif

#endif
