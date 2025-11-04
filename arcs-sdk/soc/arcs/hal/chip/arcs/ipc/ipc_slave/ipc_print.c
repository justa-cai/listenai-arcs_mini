/**
 ****************************************************************************************
 *
 * @file ipc_print.c
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdbool.h>
#include "nmsis_compiler.h"
#include "rtos_al.h"
#include "ls_rtos.h"
#include "ipc.h"



extern struct ipc_shared_env_tag ipc_shared_env;
static char dbg_end;
static uint32_t dbg_state;
static uint32_t local_write_pos, last_read_pos = -1;


int32_t ipc_dbg_output(char *string, int32_t len)
{
    char *ptr = string;
    uint32_t state, pos = 0;
    volatile struct ipc_dbg_tag *dbg_buffer = &ipc_shared_env.dbg_buffer;

    if (dbg_buffer->pattern != IPC_PATTERN1)
    {
        state = 0;
    }
    else
    {
        state = 1;
    }

    if ((dbg_state != state) && (dbg_end == 0))
        dbg_state = state;

    if ((string[len - 1] == '\n') || (string[len - 1] == 0))
        dbg_end = 0;
    else
        dbg_end = 1;

    if (dbg_state == 0)
    {
        return -1;
    }

    __ASM volatile ("amoadd.w %0, %2, %1"
                    : "=r"(pos), "+A"(local_write_pos)
                    : "r"(len)
                    : "memory");
    pos %= dbg_buffer->buffer_size;

    if (len + pos <= dbg_buffer->buffer_size)
    {
    	memcpy((uint8_t*)dbg_buffer->buffer_start + pos, string, len);
    }
    else
    {
    	int32_t n;

    	n = dbg_buffer->buffer_size - pos;
    	memcpy((uint8_t*)dbg_buffer->buffer_start + pos, string, n);
    	memcpy((uint8_t*)dbg_buffer->buffer_start, (string + n), len - n);
    }

    __ASM volatile ("amoadd.w %0, %2, %1"
                    : "=r"(pos), "+A"(dbg_buffer->write_pos)
                    : "r"(len)
                    : "memory");

    if ((dbg_buffer->write_pos - dbg_buffer->read_pos) > 128)
    {
        int32_t i = 0, time = 400;

        if (!(dbg_buffer->status == IPC_DBG_STATUS_ACTIVE) && (last_read_pos != dbg_buffer->read_pos))
        {
            ipc_send_notify(IPC_EVT_PRINT);
            last_read_pos = dbg_buffer->read_pos;
        }
        if ((dbg_buffer->write_pos - dbg_buffer->read_pos) > (dbg_buffer->buffer_size >> 1))
            time = 4000;

        while ((i++ < time) && ((dbg_buffer->write_pos - dbg_buffer->read_pos) > 128));
    }

    return 0;
}
