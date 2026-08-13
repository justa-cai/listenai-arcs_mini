/**
 ****************************************************************************************
 *
 * @file ipc_mrpc_service.h
 *
 * @brief IPC MRPC service registration helpers.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#ifndef _IPC_MRPC_SERVICE_H_
#define _IPC_MRPC_SERVICE_H_

#include <stdint.h>
#include "mrpc_server.h"

int32_t ipc_mrpc_register_master_services(struct mrpc_server_env *server);
int32_t ipc_mrpc_register_slave_services(struct mrpc_server_env *server);
int32_t ipc_mrpc_register_bus_service(struct mrpc_server_env *server);
int32_t ipc_mrpc_init_bus_client(void);

#endif
