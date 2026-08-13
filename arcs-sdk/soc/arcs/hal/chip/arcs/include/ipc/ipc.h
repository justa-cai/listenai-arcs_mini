/**
 ****************************************************************************************
 *
 * @file ipc.h
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */
#ifndef _IPC_H_
#define _IPC_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include <stdint.h>

#if CFG_AMP_IPC_MASTER == 1
#include "ipc_master.h"
#else
#include "ipc_slave.h"
#endif

void ipc_send_signal(uint32_t event);
void ipc_stats_dump(void);

#endif
