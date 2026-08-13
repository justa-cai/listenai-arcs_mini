#ifndef _PSRAM_UNIFIED_COMMON_H__
#define _PSRAM_UNIFIED_COMMON_H__

#include <stdint.h>

#include "ClockManager.h"
#include "PSRAMUnified.h"
#include "arcs_ap.h"
#include "log_print.h"

#define PSRAM_UNIFIED_BASE_ADDRESS       CMN_PSRAM_REGION

#define PSRAM_INNER_SEARCH_LOOP_NUM      64
#define PSRAM_SEARCH_DQS_NUM             128
#define PSRAM_DELAY_STEP                 10

#define PSRAM_UNIFIED_XCCELA_MAX_FREQ    250000000UL

#define PSRAM_UNIFED_WT_DQS_POS_N        1
#define PSRAM_UNIFED_WT_DQS_POS_M        3
#define PSRAM_UNIFED_RD_DQS_POS_N        1
#define PSRAM_UNIFED_RD_DQS_POS_M        3

#define PSRAM_PREFETCH_FIFO0_DEPTH        0x40
#define PSRAM_PREFETCH_FIFO1_DEPTH        0x40
#define PSRAM_PREFETCH_FIFO2_DEPTH        0x20

#define PSRAM_DEV_TYPE_XCCELA            0x0
#define PSRAM_DEV_TYPE_APM               0x1
#define PSRAM_DEV_TYPE_WINBOND           0x2

#define PSRAM_DEV_PAGE_SIZE_1K           0xA
#define PSRAM_DEV_PAGE_SIZE_2K           0xB

#define __INNER_MICROSEC_LOW             1000
#define __INNER_MICROSEC_HIGH            10000

#define PSRAM_EYE_READ_DIAGRAM_TOP       0x90
#define PSRAM_EYE_READ_DIAGRAM_BOTTOM    0x3
#define PSRAM_EYE_WRITE_DIAGRAM_TOP      0x90
#define PSRAM_EYE_WRITE_DIAGRAM_BOTTOM   0x3

#define PSRAM_INSTR_STOP                 0x0
#define PSRAM_INSTR_CMD                  0x1
#define PSRAM_INSTR_CEBLP                0x3
#define PSRAM_INSTR_ADDR                 0x4
#define PSRAM_INSTR_MRWRDATA             0x5
#define PSRAM_INSTR_WRITE                0x8
#define PSRAM_INSTR_WRITE16              0x9
#define PSRAM_INSTR_READ                 0xA
#define PSRAM_INSTR_READ16               0xB
#define PSRAM_INSTR_DUMMY                0xC
#define PSRAM_INSTR_CMDNADDR             0xF

#define PSRAM_AHB_RD_SEQ_ID              0
#define PSRAM_AHB_WR_SEQ_ID              1
#define PSRAM_MR_RD_SEQ_ID               2
#define PSRAM_MR_WR_SEQ_ID               3
#define PSRAM_EXE_SLEEP_MODE_SEQ_ID      5

#define PSRAM_MEM_32Mb_DENSITY_MAP       (4 * 1024 * 1024)
#define PSRAM_MEM_64Mb_DENSITY_MAP       (8 * 1024 * 1024)
#define PSRAM_MEM_128Mb_DENSITY_MAP      (16 * 1024 * 1024)
#define PSRAM_MEM_256Mb_DENSITY_MAP      (32 * 1024 * 1024)
#define PSRAM_MEM_512Mb_DENSITY_MAP      (64 * 1024 * 1024)

#define PSRAM_UNIFIED_CFG_IO_DRV_DQ0     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ1     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ2     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ3     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ4     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ5     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ6     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ7     4
#define PSRAM_UNIFIED_CFG_IO_DRV_DQS     4
#define PSRAM_UNIFIED_CFG_IO_DRV_CEN     1
#define PSRAM_UNIFIED_CFG_IO_DRV_CLK     4

#if PSRAM_UNIFIED_LOG_CHECK == 1
#define PSRAM_LOG(str, ...)              CLOGD(str, ##__VA_ARGS__)
#else
#define PSRAM_LOG(str, ...)
#endif

#define PSRAM_LOGE(str, ...)             CLOGE(str, ##__VA_ARGS__)

#define PSRAM_INSTR_MARCO(opc, oper)     (uint16_t)(((opc) << 8) | (oper))
#define psram_memcpy(dst, src, size)     __builtin_memcpy((dst), (src), (size))
#define psram_compare(ptr0, ptr1, size)  __builtin_memcmp((ptr0), (ptr1), (size))
#define psram_memset(dst, c, n)          __builtin_memset((dst), (c), (n))

#define psram_barrier() do { \
    __RWMB(); \
    __FENCE_I(); \
} while (0)

int32_t __psram_unified_dll_lock(void);
int32_t __psram_unified_search_read_dqs_delay(uint32_t *psram_src_data,
                                              uint32_t *g_rd_delay);
int32_t __psram_unified_search_write_dqs_delay(uint32_t *g_wt_delay);

#endif /* _PSRAM_UNIFIED_COMMON_H__ */
