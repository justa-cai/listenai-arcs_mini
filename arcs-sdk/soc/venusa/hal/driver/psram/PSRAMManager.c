/***************************************************************************
 * @file    PSRAMManager.c
 * @brief   PSRAM (Pseudo-Static RAM) Controller Management Implementation
 * @details
 * This file implements the PSRAM controller driver, providing initialization,
 * timing calibration, mode register configuration, and data verification.
 *
 * Main functionalities include:
 *   - PSRAM hardware initialization and clock configuration
 *   - DLL-based timing calibration (eye diagram search)
 *   - Mode Register programming and device parameter setup
 *   - Dynamic read/write delay tuning
 *   - Data integrity and memory density detection
 *
 * @note This module is designed for Xccela-compatible PSRAM devices.
 *
 * @author  Eason
 * @version 2.0
 * @date    2025-10-21
 * @copyright
 * Copyright (C) 2025 ListenAI
 * All rights reserved.
 ***************************************************************************/

#include "PSRAMManager.h"
#include "PSRAM_common.h"


/**
 * @struct DqsDelay_FmtDef
 * @brief  Data Strobe (DQS) delay structure
 * @details
 * Represents minimum, maximum, and optimal delay values obtained from
 * the eye diagram search process. Used for determining optimal read/write
 * timing calibration parameters.
 */
typedef struct {
    uint32_t delay_max;   /**< Maximum working DQS delay value */
    uint32_t delay_min;   /**< Minimum working DQS delay value */
    uint32_t delay_gap;   /**< Optimal delay midpoint between min/max */
} DqsDelay_FmtDef;

/**
 * @brief Initialize PSRAM source data with pseudo-random values.
 * @param[in,out] psram_src_data Pointer to source buffer.
 */
static void psram_data_init(uint32_t *psram_src_data) {
    for (unsigned int i = 0; i < PSRAM_SEARCH_DQS_NUM; i++) {
        psram_src_data[i] = (uint32_t)rand();
    }
}

/**
 * @brief Perform random write/compare test for PSRAM delay calibration.
 * @param[in] psram_src_data Pointer to source data.
 * @param[in,out] psram_dst_data Pointer to destination memory.
 * @param[in] write_delay Write DQS delay to apply.
 * @return 0 if data is valid, nonzero if mismatch detected.
 */
uint32_t psram_random_write_data_check(uint32_t *psram_src_data,
                                              uint32_t *psram_dst_data,
                                              int32_t write_delay) {
    unsigned int ret = 0;
    unsigned int loop = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = write_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        psram_memcpy(psram_dst_data, psram_src_data,
                     (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        HAL_FlushInvalidateDCache_by_Addr((uint32_t*)psram_dst_data,
                                          (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        psram_dst_data += (PSRAM_SEARCH_DQS_NUM / 2);
    }

    return ret;
}

/**
 * @brief Verify PSRAM read timing accuracy via data comparison.
 * @param[in] psram_src_data Reference source buffer.
 * @param[in,out] psram_dst_data PSRAM destination buffer.
 * @param[in] read_delay DQS read delay value.
 * @return 0 on success, nonzero if mismatch found.
 */
uint32_t psram_random_read_data_check(uint32_t *psram_src_data,
                                             uint32_t *psram_dst_data,
                                             int32_t read_delay) {
    unsigned int ret = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (unsigned int loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        HAL_InvalidateDCache_by_Addr((uint32_t*)psram_dst_data,
                                     (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        if (ret != 0) break;
    }

    return ret;
}

/**
 * @brief Alternate PSRAM read check used during training.
 * @param[in] psram_src_data Source buffer.
 * @param[in,out] psram_dst_data Destination buffer.
 * @param[in] read_delay DQS delay value to test.
 * @return 0 if valid, nonzero on mismatch.
 */
static uint32_t psram_random_read_data_check_1(uint32_t *psram_src_data,
                                               uint32_t *psram_dst_data,
                                               int32_t read_delay) {
    unsigned int ret = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (unsigned int loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        psram_memcpy(psram_dst_data, psram_src_data,
                     (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        HAL_FlushInvalidateDCache_by_Addr((uint32_t*)psram_dst_data,
                                          (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        if (ret != 0) break;
    }

    return ret;
}

/**
 * @brief Perform DQS (Data Strobe) delay window search for PSRAM timing calibration.
 *
 * @details
 * This function dynamically scans and determines the valid DQS delay range
 * (eye diagram) that allows stable PSRAM read/write operations.
 *
 * It adjusts the DQS delay incrementally in both forward and backward directions
 * and tests data reliability at each step using a provided data-check handler.
 * When successful and failed regions are detected, the minimum and maximum
 * delay points are recorded, and the optimal midpoint is derived.
 *
 * Algorithm overview:
 *   1. Initialize or reuse training data.
 *   2. Search from the current delay upward until failure is observed.
 *   3. Reverse search from the current delay downward to detect the lower limit.
 *   4. Log the working delay range and compute the valid window.
 *
 * @param[out] dqs_delay
 *   Pointer to a structure that stores minimum, maximum, and optimal delay values.
 * @param[in] delay_reg
 *   Starting DQS delay register value (current calibration baseline).
 * @param[in] psram_src_data
 *   Pointer to source data buffer for verification.
 *   If `NULL`, the function will internally generate random training data.
 * @param[in] random_data_check_handler
 *   Function pointer used to test PSRAM read/write correctness for each delay step.
 *
 * @return
 *   - `1`: Valid delay window successfully found.
 *   - `0`: Calibration failed (no stable region detected).
 *
 * @note
 * This function is a critical part of PSRAM timing training and
 * should be executed during initialization under stable clock conditions.
 */
static uint8_t quick_psram_search_dqs_delay(
    DqsDelay_FmtDef* dqs_delay,
     int32_t delay_reg,
      uint32_t *psram_src_data,
       uint32_t (*random_data_check_handler)(uint32_t*,  uint32_t*, int32_t))
{
    unsigned int ret = 0;
    uint8_t flag = 0;
    int32_t delay;
    uint8_t finded = 0;  // Indicates if a valid range has been found
    int32_t delay_step = PSRAM_DELAY_STEP;

    // Define search boundaries
    int32_t delay_max = PSRAM_EYE_DIAGRAM_TOP;
    int32_t delay_min = PSRAM_EYE_DIAGRAM_BOTTOM;

    uint32_t *__psram_src_data_ptr;
    uint32_t __psram_src_data[PSRAM_SEARCH_DQS_NUM];
    uint8_t mod_i = 0;

    // Initialize current delay range with starting register value
    dqs_delay->delay_max = delay_reg;
    dqs_delay->delay_min = delay_reg;

    delay = dqs_delay->delay_max;

    search_top:
    /* ==========================================================
     *  Step 1: Search upper edge (incremental direction)
     * ========================================================== */
    while (1)
    {
        /* Initialize training data if not provided */
        if (psram_src_data == NULL) {
            psram_data_init(__psram_src_data);
            __psram_src_data_ptr = __psram_src_data;
        } else {
            __psram_src_data_ptr = psram_src_data;
        }

        /* Alternate data halves to reduce cache overlap effects */
        if (mod_i == 0) {
            ret = random_data_check_handler(__psram_src_data_ptr, (uint32_t*)PSRAM_BASE_ADDRESS, delay);
            mod_i = 1;
        } else {
            ret = random_data_check_handler(__psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2),
                                            (uint32_t*)PSRAM_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2),
                                            delay);
            mod_i = 0;
        }

        if (ret != 0) {
            /* Data verification failed at current delay */
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (finded == 1) {
                /* Previous delay was successful; record upper limit */
                delay_max = delay - 1;

                delay = dqs_delay->delay_max + 1;
                if (delay > delay_max)
                    break;

                /* Reduce step size for finer adjustment if needed */
                if (delay_step == 1)
                    break;

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            /* Data valid at current delay */
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (finded == 0) {
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            } else {
                dqs_delay->delay_max = delay;
            }
        }

        /* Stop if maximum limit reached */
        if (delay >= delay_max)
            break;

        delay += delay_step;
        if (delay > delay_max)
            delay = delay_max;
    }

    PSRAM_LOG("DQS Delay search (UP): %d--%d", dqs_delay->delay_min, dqs_delay->delay_max);

    /* Exit if search completed */
    if (flag == 2)
        return finded;

    /* ==========================================================
     *  Step 2: Reverse search (lower edge)
     * ========================================================== */
    if (finded == 0) {
        delay_max = dqs_delay->delay_max - 1;
        flag = 1;  // Indicates need for downward search
    }

    if ((dqs_delay->delay_min > delay_reg) && (finded == 1))
        delay_min = delay_reg;

    delay_step = PSRAM_DELAY_STEP;
    delay = dqs_delay->delay_min;

    while (1)
    {
        /* Data preparation (same logic as before) */
        if (psram_src_data == NULL) {
            psram_data_init(__psram_src_data);
            __psram_src_data_ptr = __psram_src_data;
        } else {
            __psram_src_data_ptr = psram_src_data;
        }

        if (mod_i == 0) {
            ret = random_data_check_handler(__psram_src_data_ptr, (uint32_t*)PSRAM_BASE_ADDRESS, delay);
            mod_i = 1;
        } else {
            ret = random_data_check_handler(__psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2),
                                            (uint32_t*)PSRAM_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2),
                                            delay);
            mod_i = 0;
        }

        if (ret != 0) {
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (finded == 1) {
                /* Record lower limit once error appears after success */
                delay_min = delay + 1;

                delay = dqs_delay->delay_min - 1;
                if (delay < delay_min)
                    break;

                if (delay_step == 1)
                    break;

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (finded == 0) {
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            } else {
                dqs_delay->delay_min = delay;
            }
        }

        if (delay <= delay_min)
            break;

        delay -= delay_step;
        if (delay < delay_min)
            delay = delay_min;
    }

    PSRAM_LOG("DQS Delay search (DOWN): %d--%d", dqs_delay->delay_min, dqs_delay->delay_max);

    /* ==========================================================
     *  Step 3: Bidirectional refinement
     * ========================================================== */
    if (flag == 1) {
        /* Switch to upward scan for confirmation */
        delay = dqs_delay->delay_max + 1;
        flag = 2;
        goto search_top;
    }

    /* Return status: 1 if valid delay window found, 0 otherwise */
    return finded;
}


/**
 * @brief Conduct write-side DQS eye search and update delay registers.
 * @param[out] g_wt_delay Pointer to variable to store calculated write delay.
 * @return 0 on success, -1 if calibration fails.
 */
static int32_t search_write_dqs_delay(uint32_t* g_wt_delay){
    unsigned int ret=0;
    DqsDelay_FmtDef dqs_delay = {0};
    PSRAM_LOG("[INFO][START]search_write_delay, check write delay start...\n");

    if (0 == quick_psram_search_dqs_delay(&dqs_delay, IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY, NULL, psram_random_write_data_check)){
        PSRAM_LOGE("[INFO][WRITE] Search finished, Failed!!! Psram initialize program can't find EYE DIAGRAM!!!");
        ret = -1;
    }

    dqs_delay.delay_gap = (dqs_delay.delay_max - dqs_delay.delay_min)/3 + dqs_delay.delay_min;
    PSRAM_LOG("[INFO][WRITE] Search finished, 0x%x - 0x%x range, average write delay_gap=0x%x", dqs_delay.delay_min, dqs_delay.delay_max, dqs_delay.delay_gap);

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = dqs_delay.delay_gap;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    if (g_wt_delay){
        *g_wt_delay = dqs_delay.delay_gap;
    }

    return ret;
}

/**
 * @brief Conduct read-side DQS eye search and update delay registers.
 * @param[in] psram_src_data Pointer to training data buffer.
 * @param[out] g_rd_delay Output read delay value.
 * @return 0 on success, -1 on failure.
 */
static int32_t search_read_dqs_delay(uint32_t *psram_src_data, uint32_t* g_rd_delay){
    unsigned int ret=0;
    DqsDelay_FmtDef dqs_delay = {0};
    PSRAM_LOG("[INFO][START]search_read_delay, check read delay start...\n");

    if (psram_src_data == NULL){
        if (0 == quick_psram_search_dqs_delay(&dqs_delay, IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY, psram_src_data, psram_random_read_data_check_1)){
            PSRAM_LOGE("[INFO][READ] Search finished, Failed!!! Psram initialize program can't find EYE DIAGRAM!!!");
            ret = -1;
        }
    } else {
        if (0 == quick_psram_search_dqs_delay(&dqs_delay, IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY, psram_src_data, psram_random_read_data_check)){
            PSRAM_LOGE("[INFO][READ] Search finished, Failed!!! Psram initialize program can't find EYE DIAGRAM!!!");
            ret = -1;
        }
    }

    dqs_delay.delay_gap = (dqs_delay.delay_max - dqs_delay.delay_min)/3 + dqs_delay.delay_min;
    PSRAM_LOG("[INFO][READ] Search finished, 0x%x - 0x%x range, average read delay_gap=0x%x", dqs_delay.delay_min, dqs_delay.delay_max, dqs_delay.delay_gap);

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = dqs_delay.delay_gap;
    IP_PSRAM_CTRL->REG_DLLRESYNC.all = 1;

    if (g_rd_delay != NULL){
        *g_rd_delay = dqs_delay.delay_gap;
    }

    return ret;
}

static int32_t __dll_lock(void)
{
#define PSRAM_DLL_LOCK_TIMEOUT       1000000
#define PSRAM_DETECT_PHASE_MAX       8
    // DLL lock program start ####################################################
    uint8_t rdata = 0;
    uint32_t i = 0;
    uint8_t phase_detect_sel = 0;

    IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_BYPASS = phase_detect_sel;
    IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN =1;
    IP_PSRAM_CTRL->REG_DLLRST.all = 1;
    PSRAM_LOG("Wait PSRAM DLL done\n\n\n");
    do{
        rdata = IP_PSRAM_CTRL->REG_LOCKDONE.all;
        i++;
        if (i > PSRAM_DLL_LOCK_TIMEOUT){
            IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN =0;
            phase_detect_sel++;
            if (phase_detect_sel >= PSRAM_DETECT_PHASE_MAX){
                PSRAM_LOGE("DLL lock failed, all phase detect sel tried");
                return -1;
            }
            i = 0;
            IP_PSRAM_CTRL->REG_DLLEN.bit.PHASE_DETECT_SEL = phase_detect_sel;
            IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN =1;
            IP_PSRAM_CTRL->REG_DLLRST.all = 1;
            continue;
        }
    }while(rdata != 1);
    PSRAM_LOG("DLL lock!!! %d", phase_detect_sel);
    // The PSRAM DLL measures how many internal fixed delay cells are required for one clock cycle of a PSRAM.
    // If a whole clock cycle is measured, 1/4 can be obtained by dividing the delay cell by 4.
    // If the clock cycle is too large, only half a cycle can be measured, therefore the HALF_CLOCK_MODE is set, adopting a 1/2 bit coefficient.
    uint32_t div = IP_PSRAM_CTRL->REG_DLLOBSV0.bit.HALF_CLOCK_MODE;
    if (div){
        div = 2;
    } else{
        div = 4;
    }
    PSRAM_LOG("DLL lock value: %d, lock div: %d", IP_PSRAM_CTRL->REG_DLLOBSV0.bit.DLL_LOCK_VALUE, div);

    // resync DLL (0x28)
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;

#undef PSRAM_DLL_LOCK_TIMEOUT
#undef PSRAM_DETECT_PHASE_MAX

    return 0;
}

/**
 * @brief Perform complete PSRAM initialization sequence.
 * @param[out] read_delay Calculated read delay value.
 * @param[out] write_delay Calculated write delay value.
 * @param[in] search If nonzero, performs eye diagram search.
 * @return 0 on success, negative value on failure.
 *
 * @details
 * This function executes the following steps:
 *   1. Reset and enable PSRAM controller.
 *   2. Configure timing parameters and IO driver strength.
 *   3. Initialize DLL and mode registers.
 *   4. Optionally perform eye diagram search for read/write delays.
 *   5. Apply final timing configuration.
 */
int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search){
    uint32_t psram_clock = 0;
    int32_t ret = 0; // Success

    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_PRESET = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_RESET = 1; // Reset PSRAM controller

    // Enable PSRAM controller clock
    __HAL_CRM_PSRAM_CLK_ENABLE();
    // Get PSRAM controller clock
    psram_clock = CRM_GetPsramFreq();
    PSRAM_LOG("PSRAM CLOCK: %d", psram_clock);

#if PSRAM_DRV_STR_EN
	// Driver strength
    IP_CMN_SYS->REG_PSRAMIO_CFG0.all = (PSRAM_CFG_IO_DRV_DQ0 | 
                                        (PSRAM_CFG_IO_DRV_DQ1 << 3) |
                                        (PSRAM_CFG_IO_DRV_DQ2 << 6) |
                                        (PSRAM_CFG_IO_DRV_DQ3 << 9) |
                                        (PSRAM_CFG_IO_DRV_DQ4 << 12) |
                                        (PSRAM_CFG_IO_DRV_DQ5 << 15) |
                                        (PSRAM_CFG_IO_DRV_DQ6 << 18) |
                                        (PSRAM_CFG_IO_DRV_DQ7 << 21));
    IP_CMN_SYS->REG_PSRAMIO_CFG1.all = (PSRAM_CFG_IO_DRV_DQS |
                                        (PSRAM_CFG_IO_DRV_CEN << 6) |
                                        (PSRAM_CFG_IO_DRV_CLK << 9));
#endif
    // MASTER configure
    {
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF0_MSTRID = PSRAM_MASTER_FIFO0_HM;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF1_MSTRID = PSRAM_MASTER_FIFO1_HM;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF2_MSTRID = PSRAM_MASTER_FIFO2_HM;
    }

    // Prefetch
    {
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF0_PREFETCH_EN = PSRAM_PREFETCH_FIFO0_HM_EN;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF1_PREFETCH_EN = PSRAM_PREFETCH_FIFO1_HM_EN;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF2_PREFETCH_EN = PSRAM_PREFETCH_FIFO2_HM_EN;

        // TODO Enable prefetch till full <Not recommended>
        // IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF0_PREFETCH_TILL_FULL = 0x1;
        // IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF1_PREFETCH_TILL_FULL = 0x1;
        // IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF2_PREFETCH_TILL_FULL = 0x1;
    }

    // Priority configure
    {
        IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all = (PSRAM_BUS_PRIORITY_MASTER0 |
                                               (PSRAM_BUS_PRIORITY_MASTER1 << 2) |
                                               (PSRAM_BUS_PRIORITY_MASTER2 << 4) |
                                               (PSRAM_BUS_PRIORITY_MASTER3 << 6) |
                                               (PSRAM_BUS_PRIORITY_MASTER4 << 8) |
                                               (PSRAM_BUS_PRIORITY_MASTER5 << 10) |
                                               (PSRAM_BUS_PRIORITY_MASTER6 << 12) |
                                               (PSRAM_BUS_PRIORITY_MASTER7 << 14) |
                                               (PSRAM_BUS_PRIORITY_MASTER8 << 16) |
                                               (PSRAM_BUS_PRIORITY_MASTER9 << 18));

        PSRAM_LOG("PSRAM priority parameter: 0x%x", IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all);
    }
    // Controller sequence register configure start #########################################
    psram_seq_configure_stage0();

    controller_timing_configure(psram_clock);

    clock_src_name_t src;
    {
		// Set low psram clock
		uint32_t div_m;
		HAL_CRM_GetPsramClkConfig(&src, &div_m);
		if (src == CRM_IpSrcSyspllPsram){
			// Change clock source to xtal 24m
			HAL_CRM_SetPsramClkSrc(CRM_IpSrcCORE24M);

			PSRAM_LOG("PSRAM REDUCE TO 24Mhz");
		} else {
			// Keep the clock source
			PSRAM_LOG("PSRAM clock is always 24Mhz");
		}
	}

    // DLL lock program start ####################################################
    {
        ret = __dll_lock();
        if (ret < 0){
            return ret;
        }
    }
    // DLL lock program end ####################################################

    // controller configure start ######################################################
	controller_para_cfg_type();
	// controller configure end ######################################################

    // Before MR operation, PSRAM need exit sleep mode
    exit_sleep_mode_sequence(psram_clock);
    // Controller sequence register configure end #########################################

    uint32_t density = 0;

    //// Extra PSRAM information from read MR register
    {
        int32_t ret = 0;
        ret = information_extraction_sequence(&density);
        if (ret < 0){
            PSRAM_LOGE("PSRAM information extraction failed!!!");
            return -1;
        }
    }

    controller_para_cfg_page_size(density);

    psram_seq_configure_stage1(psram_clock, density);

    // Controller sequence register configure end #########################################


    // MR configure start ######################################################
    die_para_configure(psram_clock, density);
    // MR configure end ########################################################

    uint32_t psram_src_data[PSRAM_SEARCH_DQS_NUM];

    // Write Golden
    if (search){ // Search read delay and write delay

        // generate training data
        // psram_data_init(psram_src_data); // NOTE: Replace with FIX value 0x5aa55aa5
        for (int i = 0; i < PSRAM_SEARCH_DQS_NUM; i++) {
            psram_src_data[i] = 0x5aa55aa5;
        }

        psram_memcpy((uint32_t *)PSRAM_BASE_ADDRESS, psram_src_data,         PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));

        // Write back from cache
        HAL_FlushInvalidateDCache_by_Addr((uint32_t *)PSRAM_BASE_ADDRESS, PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));

        //wait for psram write data done
        for (int j = 0; j < 3000; j++) // maybe can use fence.i
            __NOP();
    }

    // Restore clock
    HAL_CRM_SetPsramClkSrc(src);

    PSRAM_LOG("PSRAM RECOVER TO %d", CRM_GetPsramFreq());

    // DLL lock program start ####################################################
    {
        ret = __dll_lock();
        if (ret < 0){
            return ret;
        }
    }
    // DLL lock program end ####################################################

    // EYE DIAGRAM search start ######################################################
    if (search){ // Search read delay and write delay
        ret |= search_read_dqs_delay(psram_src_data, read_delay);

        ret |= search_write_dqs_delay(write_delay);

    } else { // Configure read delay and write delay immediately

        IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = *read_delay;
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = *write_delay;

        // resync DLL (0x28)
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;

    }

    // EYE DIAGRAM search end ########################################################
    return ret;
}

/**
 * @brief Read current PSRAM device density.
 * @return PSRAM memory density in bits.
 */
uint32_t PSRAM_GetDensity(void){
	uint32_t density = 0;
    int32_t ret = 0;
    ret = information_extraction_sequence(&density);
    if (ret < 0){
        PSRAM_LOGE("PSRAM information extraction failed!!!");
        return 0;
    }
    return density;
}

/**
 * @brief Enter PSRAM low-power mode.
 * @param[in] sleep_mode Sleep mode selection (half/deep).
 */
void PSRAM_EnterSleepMode(_psram_sleep_mode_t sleep_mode){
    enter_sleep_mode_sequence(sleep_mode);
}
