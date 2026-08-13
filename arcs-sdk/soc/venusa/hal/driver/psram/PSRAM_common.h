/***************************************************************************
 * @file    PSRAM_Common.h
 * @brief   Common Macros and Definitions for PSRAM Driver
 * @details
 * This header provides shared constants, macros, and low-level utilities
 * used across PSRAM driver modules. It defines basic timing parameters,
 * memory density mappings, logging control, and optimized memory operations.
 *
 * Key features:
 *   - PSRAM density map constants
 *   - Instruction, memory, and utility macros
 *   - Timing and eye diagram boundary definitions
 *   - Logging and cache management utilities
 *
 * @author  Eason
 * @version 2.0
 * @date    2025-10-21
 * @copyright
 * Copyright (C) 2025 ListenAI
 * All rights reserved.
 ***************************************************************************/

#ifndef _PSRAM_COMMON_H__
#define _PSRAM_COMMON_H__

#include "ClockManager.h"
#include "log_print.h"
#include "cache.h"

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

/**
 * @brief Step size for delay calibration search (in units).
 */
#define PSRAM_DELAY_STEP                10

/* --------------------------------------------------------------------------
 * Controller and Timing Configuration
 * -------------------------------------------------------------------------- */

/**
 * @brief Prefetch FIFO buffer depth (words).
 */
#define PSRAM_PREFETCH_FIFO0_DEPTH      0x40
#define PSRAM_PREFETCH_FIFO1_DEPTH      0x40

/**
 * @brief PSRAM die type identifiers.
 */
#define PSRAM_DEV_TYPE_XCELLA           0x0  /**< Xccela-compatible architecture */
#define PSRAM_DEV_TYPE_APM              0x1  /**< Advanced Peripheral Mode architecture */

/**
 * @brief Device page size configuration.
 */
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
#define PSRAM_EYE_DIAGRAM_TOP           (0x90)  /**< Maximum scan boundary for DQS delay */
#define PSRAM_EYE_DIAGRAM_BOTTOM        (0x03)  /**< Minimum scan boundary for DQS delay */

/* --------------------------------------------------------------------------
 * Logging Control
 * -------------------------------------------------------------------------- */

/**
 * @brief Debug logging macro.
 * @note Controlled by compile-time macro `PSRAM_LOG_CHECK`.
 */
#if PSRAM_LOG_CHECK == 1
#define PSRAM_LOG(str, ...)  CLOGD(str, ##__VA_ARGS__)  /**< Enable debug logging */
#else
#define PSRAM_LOG(str, ...)  /**< Logging disabled */
#endif

/**
 * @brief Error logging macro (always enabled).
 */
#define PSRAM_LOGE(str, ...) CLOGE(str, ##__VA_ARGS__)

/**
 * @brief Configure the PSRAM controller command LUT for MR read/write.
 * @details
 * Initializes LUT entries used by the controller to perform
 * Mode Register read/write sequences and reset command operations.
 */
void psram_seq_configure_stage0(void);

/**
 * @brief Configure AHB access sequences for Xccela PSRAM.
 * @param[in] clock_freq Current PSRAM controller clock frequency (Hz)
 * @param[in] density    Device density value.
 * @details
 * Sets the AHB read/write sequence lookup table (LUT) based on
 * operating frequency and memory density to ensure correct latency.
 */
void psram_seq_configure_stage1(uint32_t clock_freq, uint32_t density);

/**
 * @brief Extract PSRAM vendor, density, and die information.
 * @param[out] density Pointer to variable to store density mapping.
 * @return 0 on success, -1 if die check fails.
 * @details
 * Reads MR1–MR3 registers to validate die status and decode
 * density and vendor information.
 */
int32_t information_extraction_sequence(uint32_t* density);
/**
 * @brief Configure PSRAM timing parameters based on clock frequency.
 * @param[in] clock_freq Current PSRAM operating frequency.
 * @details
 * Computes timing constants such as TCEM and TCPH and writes
 * them into the timing control register.
 */
void controller_timing_configure(uint32_t clock_freq);

/**
 * @brief Configure PSRAM controller parameters.
 * @param[in] density PSRAM density identifier.
 * @details
 * Sets device type and page size according to the detected PSRAM density.
 */
void controller_para_cfg_type(void);

void controller_para_cfg_page_size(uint32_t density);

/**
 * @brief Program Mode Registers (MR0, MR4) based on frequency and density.
 * @param[in] clock_freq PSRAM operating frequency.
 * @param[in] density    PSRAM density identifier.
 * @details
 * Configures read/write latency and refresh settings according to the
 * operational frequency range and memory die characteristics.
 */
void die_para_configure(uint32_t clock_freq, uint32_t density);

/**
 * @brief Exit PSRAM sleep mode.
 * @param[in] clock_freq PSRAM operating clock frequency.
 * @details
 * Executes the wakeup command sequence to restore PSRAM from
 * deep or half-sleep state.
 */
void exit_sleep_mode_sequence(uint32_t clock_freq);
/**
 * @brief Enter PSRAM low-power mode.
 * @param[in] sleep_mode Target sleep mode (half/deep).
 * @retval 0 Success.
 * @retval -1 Unsupported mode.
 */
int32_t enter_sleep_mode_sequence(_psram_sleep_mode_t sleep_mode);

#endif /* _PSRAM_COMMON_H__ */
