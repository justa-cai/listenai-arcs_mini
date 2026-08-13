/**
 ****************************************************************************************
 *
 * @file ipc_utils_stub.c
 *
 * @brief IPC utility stubs for builds without ARCS HAL IPC.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include <stdint.h>

int32_t ipc_halt_peer_core(void)
{
    return 0;
}

int32_t ipc_resume_peer_core(void)
{
    return 0;
}
