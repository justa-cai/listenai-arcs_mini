//------------------------------------------------
// public configs
#define IC_COMM_SHAREVARIABLE_TIMEGAP   (100) // ms

//------------------------------------------------
// self
#include "ic_comm_share_variable.h"

// family
#include "ic_common.h"

//------------------------------------------------
// module: ic_stream


//------------------------------------------------
// platform
#include "arcs_ap.h"
#include "nmsis_core.h"
// #include "cache.h"

// os
#include "FreeRTOS.h"
#include "queue.h"

// utils: log
//#define LOG_NDEBUG 0
#define LOG_TAG "IC_"
#include "log_print.h"

// utils: ic_assert
//#define ASSERT_NDEBUG 0
#include "ic_common.h"

//------------------------------------------------
// lib: clib

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

//------------------------------------------------
// common defines


//------------------------------------------------
// class implementation

void IC_Comm_ShareVariable_ctor(IC_Comm_ShareVariable* self, uint32_t* shareVar)
{
    self->mShareVar = shareVar;
    *self->mShareVar = 0;

#if IC_HAL_DCACHE_SIZE>0 && !IC_HAL_DCACHE_IS_COHERENT
    // writeback dcache. 单位: bytes
    xthal_dcache_region_writeback((void *) self->mShareVar,
                                  IC_DCACHELINE_ROUNDUP_SIZE(sizeof(uint32_t)));
    // memory barrier: Sequentially-consistent
    __DSB();

    // invalidate cache
    xthal_dcache_region_invalidate((void *) self->mShareVar,
                                    IC_DCACHELINE_ROUNDUP_SIZE(sizeof(uint32_t)));
#endif

    // memory barrier: Sequentially-consistent
    __DSB();

    return;
}

void IC_Comm_ShareVariable_dtor(IC_Comm_ShareVariable *self)
{
    return;
}

int IC_Comm_ShareVariable_signal(IC_Comm_ShareVariable* self, uint32_t value)
{
    // memory barrier: read_acquire
    __DSB();

    *self->mShareVar = value;

    // memory barrier: write_release
    __DSB();

#if IC_HAL_DCACHE_SIZE>0 && !IC_HAL_DCACHE_IS_COHERENT
    // writeback dcache. 单位: bytes
    xthal_dcache_region_writeback((void *) self->mShareVar,
                                  IC_DCACHELINE_ROUNDUP_SIZE(sizeof(uint32_t)));
#endif

    return 0;
}

int IC_Comm_ShareVariable_wait(IC_Comm_ShareVariable* self, uint32_t value)
{
    bool done = false;

    do {
#if IC_HAL_DCACHE_SIZE>0 && !IC_HAL_DCACHE_IS_COHERENT
        // invalidate cache
        xthal_dcache_region_invalidate((void *) self->mShareVar,
                                        IC_DCACHELINE_ROUNDUP_SIZE(sizeof(uint32_t)));
#endif
        // data sync
    __DSB();

        if (*self->mShareVar == value) {
            done = true;
        }
        else {
            // CLOGD("%s: %d\r\n", __FUNCTION__, *self->mShareVar);
            // sleep 100(ms)
            vTaskDelay(pdMS_TO_TICKS(IC_COMM_SHAREVARIABLE_TIMEGAP));
        }
    } while (!done);

    // memory barrier: read_acquire
    __DSB();

    return 0;
}
