/**
 ****************************************************************************************
 *
 * @file ipc_printf.c
 *
 * @brief IPC module.
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "ls_rtos.h"
#include "ipc.h"



#define IPC_PRINT(fmt, ...)   logDbg(fmt, ##__VA_ARGS__)


static rtos_task_handle ipc_dbg_task_handle = RTOS_TASK_NULL;
static volatile struct ipc_dbg_tag *dbg_buffer;
extern uint8_t _ipc_dbg_start[], _ipc_dbg_end[];


void ipc_dbg_enable(int32_t enable)
{
    if (enable)
        dbg_buffer->pattern = IPC_PATTERN1;
    else
        dbg_buffer->pattern = 0;
}

void rtos_ipc_dbg_task_resume(int32_t isr)
{
    if ((dbg_buffer->read_pos != dbg_buffer->write_pos) && (dbg_buffer->status == IPC_DBG_STATUS_SLEEP))
        rtos_task_notify(ipc_dbg_task_handle, isr);
}

static void rtos_ipc_dbg_task_suspend(void)
{
    rtos_task_wait_notification(-1);
}

static void ipc_dbg_get_width(uint32_t len, char *fmt)
{
    uint32_t number, i = 0;

    do
    {
        number = len % 10;
        fmt[3 - i] = number + '0';
        len = len / 10;
    } while (i++ < 3);
}

static RTOS_TASK_FCT(ipc_dbg_task)
{
    char fmt[8] = "%.0000S";
    char *ptr = &fmt[2];
    uint32_t read_idx, write_idx, read_pos, write_pos, edge, write_pos_raw;

    dbg_buffer->pattern = IPC_PATTERN1;
    while (1)
    {
        if (dbg_buffer->read_pos != dbg_buffer->write_pos)
        {
            read_pos  = dbg_buffer->read_pos;
            write_pos = dbg_buffer->write_pos;
            read_idx  = read_pos % dbg_buffer->buffer_size;
            write_idx = write_pos % dbg_buffer->buffer_size;
            write_pos_raw = write_pos;

            if (write_pos < read_pos)
            {
                read_pos  = read_idx;
                write_pos = write_idx + dbg_buffer->buffer_size;
            }
            edge = (read_pos / dbg_buffer->buffer_size + 1) * dbg_buffer->buffer_size;

            if (write_pos <= edge)
            {
                ipc_dbg_get_width(write_pos - read_pos, ptr);
                IPC_PRINT(fmt, (uint8_t*)dbg_buffer->buffer_start + read_idx);
            }
            else if ((write_pos - read_pos) >= dbg_buffer->buffer_size)
            {
                ipc_dbg_get_width(dbg_buffer->buffer_size - write_idx, ptr);
                IPC_PRINT(fmt, (uint8_t*)dbg_buffer->buffer_start + write_idx);
                ipc_dbg_get_width(write_idx, ptr);
                IPC_PRINT(fmt, (uint8_t*)dbg_buffer->buffer_start);
            }
            else
            {
                ipc_dbg_get_width(dbg_buffer->buffer_size - read_idx, ptr);
                IPC_PRINT(fmt, (uint8_t*)dbg_buffer->buffer_start + read_idx);
                ipc_dbg_get_width(write_idx, ptr);
                IPC_PRINT(fmt, (uint8_t*)dbg_buffer->buffer_start);
            }
            dbg_buffer->read_pos = write_pos_raw;
        }
        else
        {
            dbg_buffer->status = IPC_DBG_STATUS_SLEEP;
            if (dbg_buffer->read_pos == dbg_buffer->write_pos)
                rtos_ipc_dbg_task_suspend();
            dbg_buffer->status = IPC_DBG_STATUS_ACTIVE;
        }
    }
}

void ipc_dbg_init(volatile struct ipc_dbg_tag *buffer)
{
    dbg_buffer = buffer;
    dbg_buffer->buffer_start = (uint32_t)_ipc_dbg_start;
    dbg_buffer->buffer_size  = _ipc_dbg_end - _ipc_dbg_start;
    dbg_buffer->write_pos    = 0;
    dbg_buffer->read_pos     = 0;
    dbg_buffer->status       = IPC_DBG_STATUS_ACTIVE;
    dbg_buffer->pattern      = 0;

    rtos_task_create(ipc_dbg_task, "ipc_dbg", IPC_DBG_TASK, LS_IPC_DBG_TASK_STACK_SIZE, NULL,
    		LS_IPC_DBG_TASK_PRIORITY, &ipc_dbg_task_handle);
}
