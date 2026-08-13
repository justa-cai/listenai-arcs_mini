/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 *
 ****************************************************************************************
*/
#ifndef _IPC_MEM_H_
#define _IPC_MEM_H_

#include <stdint.h>

struct ipc_shared_env_tag;

void ipc_mem_init(void);
int32_t ipc_mem_validate(void);


#endif
