/**
 ****************************************************************************************
 *
 * @file mrpc_server.h
 *
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#ifndef _MRPC_SERVER_H_
#define _MRPC_SERVER_H_

#include "ls_err.h"
#include "mrpc.h"


#define MRPC_SERV_QUEUE_SIZE  5


struct mrpc_resp_msg;

typedef void (*mrpc_msg_handler_t)(void *msg, struct mrpc_resp_msg *resp_msg);

struct mrpc_service_entry
{
    int32_t type;
    int32_t total;
    mrpc_msg_handler_t *handler;
    struct mrpc_service_entry *next;
};

struct mrpc_server_env
{
    uint32_t ipc_chan_local;
    uint32_t server_eid;
    rtos_queue queue;
    struct mrpc_req_msg *req_buffer;
    struct mrpc_resp_msg *resp_buffer;
    struct mrpc_service_entry *services;
};



struct mrpc_server_env* mrpc_server_init(uint32_t ipc_chan_local, uint32_t server_eid);
int32_t mrpc_service_register(struct mrpc_server_env *env, int32_t service_id, mrpc_msg_handler_t *mrpc_services, int32_t total);

#endif
