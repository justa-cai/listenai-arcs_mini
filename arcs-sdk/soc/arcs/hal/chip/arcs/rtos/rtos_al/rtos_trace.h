/**
 ****************************************************************************************
 *
 * @file rtos_trace.h
 *
 * @brief Declaration of FreeRTOS trace function.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef RTOS_TRACE_H_
#define RTOS_TRACE_H_

//#include "rwnx_config.h"
//#include "dbg.h"

#define NX_FULLY_HOSTED 1
#ifdef SYS_TRACE
#define NX_TRACE 1
#endif
//#define RTOS_TASK_DEF

#if NX_FULLY_HOSTED
enum {
    RTOS_TRACE_SWITCH_IN = 1,
    RTOS_TRACE_SWITCH_OUT,
    RTOS_TRACE_CREATE,
    RTOS_TRACE_DELETE,
    RTOS_TRACE_SUSPEND,
    RTOS_TRACE_RESUME,
    RTOS_TRACE_RESUME_FROM_ISR,
    RTOS_TRACE_ALLOC,
    RTOS_TRACE_FREE,
};

#ifndef CFG_BUILD_FW
#ifdef RTOS_TASK_DEF
#define PROF_RTOS_TASK_SET(taskid)                                      \
    {                                                                   \
        uint32_t prof = nxmac_sw_profiling_get() & ~(0xF << RTOS_TASK); \
        nxmac_sw_profiling_set(prof | (((taskid) & 0xF) << RTOS_TASK)); \
    }
#else
#define PROF_RTOS_TASK_SET(taskid)
#endif
#endif

#if ( configUSE_TRACE_FACILITY == 1 )
#define traceTASK_SWITCHED_IN()                                                          \
{                                                                                        \
    PROF_RTOS_TASK_SET(uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle()));                \
    rtos_trace_task(RTOS_TRACE_SWITCH_IN, xTaskGetCurrentTaskHandle());                  \
}
#else
#define traceTASK_SWITCHED_IN() rtos_trace_task(RTOS_TRACE_SWITCH_IN, xTaskGetCurrentTaskHandle())
#endif // ( configUSE_TRACE_FACILITY == 1 )
#define traceTASK_SWITCHED_OUT() rtos_trace_task(RTOS_TRACE_SWITCH_OUT, xTaskGetCurrentTaskHandle())
#define traceTASK_DELETE(pxTask) rtos_trace_task(RTOS_TRACE_DELETE, pxTask)
#define traceTASK_SUSPEND(pxTask) rtos_trace_task(RTOS_TRACE_SUSPEND, pxTask)
#define traceTASK_RESUME(pxTask) rtos_trace_task(RTOS_TRACE_RESUME, pxTask)
#define traceTASK_RESUME_FROM_ISR(pxTask) rtos_trace_task(RTOS_TRACE_RESUME_FROM_ISR, pxTask)
#define traceTASK_CREATE(pxTask) rtos_trace_task(RTOS_TRACE_CREATE, pxTask)

// Set to 1 to trace all alloc
// Set to 0 to only trace failed allocation
#define RTOS_MALLOC_TRACE_LEVEL 1
#define traceMALLOC(ptr, size) rtos_trace_mem(RTOS_TRACE_ALLOC, ptr, size, xPortGetFreeHeapSize())
#define traceFREE(ptr, size) rtos_trace_mem(RTOS_TRACE_FREE, ptr, size, xPortGetFreeHeapSize())

void rtos_trace_task(int id, void *task);
void rtos_trace_mem(int id, void *ptr, int size, int free_size);
#endif /* NX_FULLY_HOSTED */

#endif /* RTOS_TRACE_H_*/
