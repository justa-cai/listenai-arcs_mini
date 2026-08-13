#ifndef _PSRAM_UNIFIED_COMMON_H__
#define _PSRAM_UNIFIED_COMMON_H__
#include "PSRAMUnified.h"
#include "ClockManager.h"
#include <stdint.h>
#include "venusa_ap.h"
#include "log_print.h"

#define PSRAM_UNIFIED_BASE_ADDRESS              0x38000000

#define PSRAM_INNER_SEARCH_LOOP_NUM     64           // Number of internal search iterations
#define PSRAM_SEARCH_DQS_NUM            128          // Data strobe samples per word during search

/**
 * @brief Step size for delay calibration search (in units).
 */
#define PSRAM_DELAY_STEP                1

#define PSRAM_UNIFIED_XCCELA_MAX_FREQ                   (250000000)  /**< Maximum supported clock frequency for Xccela dies */
#define PSRAM_UNIFIED_XCCELA_MAX_FREQ_PARA              CRM_IpSyspllPsram_Div5_240MHz /**< Max frequency parameter for Xccela dies */

#define PSRAM_UNIFIED_APM_MAX_FREQ                      (200000000)  /**< Maximum supported clock frequency for APM dies */
#define PSRAM_UNIFIED_APM_MAX_FREQ_PARA                 CRM_IpSyspllPsram_Div6_200MHz  /**< Max frequency parameter for APM dies */

#define PSRAM_UNIFED_WT_DQS_POS_N                       1
#define PSRAM_UNIFED_WT_DQS_POS_M                       3

#define PSRAM_UNIFED_RD_DQS_POS_N                       1
#define PSRAM_UNIFED_RD_DQS_POS_M                       3

/* --------------------------------------------------------------------------
 * Controller and Timing Configuration
 * -------------------------------------------------------------------------- */

/**
 * @brief Prefetch FIFO buffer depth (words).
 */
#define PSRAM_PREFETCH_FIFO0_DEPTH      0x40
#define PSRAM_PREFETCH_FIFO1_DEPTH      0x20
#define PSRAM_PREFETCH_FIFO2_DEPTH      0x20

/**
 * @brief PSRAM die type identifiers.
 */
#define PSRAM_DEV_TYPE_XCCELA           0x0  /**< Xccela-compatible architecture */
#define PSRAM_DEV_TYPE_APM              0x1  /**< Advanced Peripheral Mode architecture */

/**
 * @brief Device page size configuration.
 */
#define PSRAM_DEV_PAGE_SIZE_512         0x9  /**< 512B page size */
#define PSRAM_DEV_PAGE_SIZE_1K          0xA  /**< 1KB page size */
#define PSRAM_DEV_PAGE_SIZE_2K          0xB  /**< 2KB page size */

/* --------------------------------------------------------------------------
 * Timing Constants (per die type)
 * -------------------------------------------------------------------------- */
/**
 * @brief Internal conversion constants for microsecond timing.
 */
#define __INNER_MICROSEC_LOW            (1000)
#define __INNER_MICROSEC_HIGH           (10000)

/* --------------------------------------------------------------------------
 * Calibration and Eye Diagram Parameters
 * -------------------------------------------------------------------------- */
#define PSRAM_EYE_READ_DIAGRAM_TOP           (0x90)  /**< Maximum scan boundary for DQS delay */
#define PSRAM_EYE_READ_DIAGRAM_BOTTOM        (0x0)  /**< Minimum scan boundary for DQS delay */
#define PSRAM_EYE_WRITE_DIAGRAM_TOP          (0x90)  /**< Maximum scan boundary for DQS delay */
#define PSRAM_EYE_WRITE_DIAGRAM_BOTTOM       (0x28)  /**< Minimum scan boundary for DQS delay */

/* --------------------------------------------------------------------------
 * PSRAM Command and Sequence Instruction Codes
 * -------------------------------------------------------------------------- */
#define PSRAM_INSTR_STOP              0x0  /**< Stop current operation */
#define PSRAM_INSTR_CMD               0x1  /**< Command phase */
#define PSRAM_INSTR_CEBLP             0x3  /**< Chip enable blank pulse */
#define PSRAM_INSTR_ADDR              0x4  /**< Address phase */
#define PSRAM_INSTR_MRWRDATA          0x5  /**< Mode Register write data */
#define PSRAM_INSTR_WRITE             0x8  /**< Write data phase */
#define PSRAM_INSTR_WRITE16           0x9  /**< Write 16-bit data */
#define PSRAM_INSTR_READ              0xA  /**< Read data phase */
#define PSRAM_INSTR_READ16            0xB  /**< Read 16-bit data */
#define PSRAM_INSTR_DUMMY             0xC  /**< Dummy cycle */
#define PSRAM_INSTR_CMDNADDR          0xF  /**< Command/address switch phase */

/* --------------------------------------------------------------------------
 * Sequence ID Assignments
 * -------------------------------------------------------------------------- */
#define PSRAM_AHB_RD_SEQ_ID           0  /**< AHB read sequence ID */
#define PSRAM_AHB_WR_SEQ_ID           1  /**< AHB write sequence ID */
#define PSRAM_MR_RD_SEQ_ID            2  /**< Mode register read sequence ID */
#define PSRAM_MR_WR_SEQ_ID            3  /**< Mode register write sequence ID */
#define PSRAM_EXE_SLEEP_MODE_SEQ_ID   5  /**< Sleep sequence execution ID */

/* --------------------------------------------------------------------------
 * PSRAM Density Mapping (in bytes)
 * -------------------------------------------------------------------------- */
/**
 * @brief PSRAM density mapping constants (bit-to-byte conversion).
 */
#define PSRAM_MEM_32Mb_DENSITY_MAP     (4*1024*1024)   /**< 32Mb = 4MB  */
#define PSRAM_MEM_64Mb_DENSITY_MAP     (8*1024*1024)   /**< 64Mb = 8MB  */
#define PSRAM_MEM_128Mb_DENSITY_MAP    (16*1024*1024)  /**< 128Mb = 16MB */
#define PSRAM_MEM_256Mb_DENSITY_MAP    (32*1024*1024)  /**< 256Mb = 32MB */
#define PSRAM_MEM_512Mb_DENSITY_MAP    (64*1024*1024)  /**< 512Mb = 64MB */

#define PSRAM_UNIFIED_CFG_IO_DRV_DQ0               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ1               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ2               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ3               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ4               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ5               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ6               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQ7               0
#define PSRAM_UNIFIED_CFG_IO_DRV_DQS               0
#define PSRAM_UNIFIED_CFG_IO_DRV_CEN               0
#define PSRAM_UNIFIED_CFG_IO_DRV_CLK               0

/* --------------------------------------------------------------------------
 * Logging Control
 * -------------------------------------------------------------------------- */

/**
 * @brief Debug logging macro.
 * @note Controlled by compile-time macro `PSRAM_UNIFIED_LOG_CHECK`.
 */
#if PSRAM_UNIFIED_LOG_CHECK == 1
#define PSRAM_LOG(str, ...)  CLOGD(str, ##__VA_ARGS__)  /**< Enable debug logging */
#else
#define PSRAM_LOG(str, ...)  /**< Logging disabled */
#endif

/**
 * @brief Error logging macro (always enabled).
 */
#define PSRAM_LOGE(str, ...) CLOGE(str, ##__VA_ARGS__)

/* --------------------------------------------------------------------------
 * Memory Operation Macros
 * -------------------------------------------------------------------------- */

/**
 * @brief Construct a 16-bit instruction word from opcode and operand.
 * @param opc Operation code value.
 * @param oper Operand value.
 * @return Combined 16-bit instruction.
 */
#define PSRAM_INSTR_MARCO(opc, oper)   (uint16_t)((opc << 8) | oper)

/**
 * @brief Allocate memory from system heap for PSRAM use.
 * @param size Allocation size in bytes.
 * @return Pointer to allocated memory.
 */
#define psram_malloc(size)              malloc(size)

/**
 * @brief Free PSRAM-allocated memory.
 * @param addr Pointer to previously allocated block.
 */
#define psram_free(addr)                free(addr)

/**
 * @brief Optimized memcpy for PSRAM access.
 * @param dst Destination pointer.
 * @param src Source pointer.
 * @param size Number of bytes to copy.
 */
#define psram_memcpy(dst, src, size)    __builtin_memcpy(dst, src, size)

/**
 * @brief Optimized memcmp for PSRAM data comparison.
 * @param ptr0 First buffer pointer.
 * @param ptr1 Second buffer pointer.
 * @param size Number of bytes to compare.
 * @return Zero if equal, nonzero otherwise.
 */
#define psram_compare(ptr0, ptr1, size) __builtin_memcmp(ptr0, ptr1, size)

/**
 * @brief Optimized memset for PSRAM memory initialization.
 * @param dst Destination address.
 * @param c Fill byte value.
 * @param n Number of bytes to set.
 */
#define psram_memset(dst, c, n)         __builtin_memset(dst, c, n)

/**
 * @brief Memory barrier ensuring write completion.
 * @details
 * Forces CPU to complete all preceding memory writes before executing
 * subsequent instructions. This ensures synchronization between PSRAM
 * and system cache.
 */
#define psram_barrier() do { \
    __RWMB(); \
    __FENCE_I(); \
} while(0)

extern int32_t __psram_unified_dll_lock(void);

extern int32_t __psram_unified_search_read_dqs_delay(uint32_t *psram_src_data, uint32_t* g_rd_delay);

extern int32_t __psram_unified_search_write_dqs_delay(uint32_t* g_wt_delay);

#endif  // _PSRAM_UNIFIED_COMMON_H__