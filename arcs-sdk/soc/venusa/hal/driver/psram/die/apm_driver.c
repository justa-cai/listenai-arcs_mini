/***************************************************************************
 * @file    PSRAM_APM.c
 * @brief   PSRAM APM Die Configuration and Controller Implementation
 * @details
 * This module implements initialization, configuration, and mode control
 * for APM-compatible PSRAM devices. It provides instruction LUT setup,
 * mode register programming, timing control, and power management
 * sequences tailored for APM die architecture.
 *
 * Key Features:
 *   - APM PSRAM command sequence (read/write/MR access)
 *   - Die identification and density extraction
 *   - Timing calibration (TCEM/TCPH)
 *   - Mode Register programming (MR0–MR6)
 *   - Power management (half-sleep/deep-sleep modes)
 *
 * @note
 * This file is compiled only when `PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM`.
 *
 * @author  Eason
 * @version 2.0
 * @date    2025-10-21
 * @copyright
 * Copyright (C) 2025 ListenAI
 * All rights reserved.
 ***************************************************************************/

#include "PSRAMManager.h"
#include "../PSRAM_common.h"

/* --------------------------------------------------------------------------
 * PSRAM Density Mapping Definitions
 * -------------------------------------------------------------------------- */
/**
 * @brief Density identifiers for APM PSRAM devices.
 */
#define APM_PSRAM_MEM_32Mb_DENSITY_MAP     (0x0)  /**< 32Mb density identifier */
#define APM_PSRAM_MEM_64Mb_DENSITY_MAP     (0x1)  /**< 64Mb density identifier */

/* --------------------------------------------------------------------------
 * PSRAM Command and Sequence Instruction Codes
 * -------------------------------------------------------------------------- */
/**
 * @brief PSRAM controller instruction field definitions.
 */
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
/**
 * @brief Command sequence index identifiers used by controller LUTs.
 */
#define PSRAM_AHB_RD_SEQ_ID           0  /**< AHB read sequence ID */
#define PSRAM_AHB_WR_SEQ_ID           1  /**< AHB write sequence ID */
#define PSRAM_MR_RD_SEQ_ID            2  /**< Mode register read sequence ID */
#define PSRAM_MR_WR_SEQ_ID            3  /**< Mode register write sequence ID */
#define PSRAM_EXE_SLEEP_MODE_SEQ_ID   5  /**< Sleep sequence execution ID */

/* --------------------------------------------------------------------------
 * APM PSRAM Command Definitions
 * -------------------------------------------------------------------------- */
#define PSRAM_APM_SYNC_READ_INS              0x00  /**< Synchronous read command */
#define PSRAM_APM_SYNC_WRITE_INS             0x80  /**< Synchronous write command */
#define PSRAM_APM_LINEAR_BURST_READ_INS      0x20  /**< Linear burst read command */
#define PSRAM_APM_LINEAR_BURST_WRITE_INS     0xA0  /**< Linear burst write command */
#define PSRAM_APM_MR_READ_INS                0x40  /**< Mode register read command */
#define PSRAM_APM_MR_WRITE_INS               0xC0  /**< Mode register write command */
#define PSRAM_APM_GLOBAL_RESET_INS           0xFF  /**< Global reset instruction */

#define PSRAM_TCEM_REFRESH_TIME         20  /**< Refresh interval in nanoseconds */

/* --------------------------------------------------------------------------
 * Mode Register Field Definitions (MR0–MR8)
 * -------------------------------------------------------------------------- */
/**
 * @defgroup PSRAM_MR_Mode_Registers
 * @brief Mode Register bit field definitions for APM PSRAM.
 * @{
 */
// MR0 Drive strength and latency
#define PSRAM_MR0_DRIVE_STR_OFFSET          (0x0)
#define PSRAM_MR0_DRIVE_STR_MASK            (0x3)
#define PSRAM_32Mb_MR0_DRIVE_STR            (0x1)

#define PSRAM_MR0_READ_LAT_OFFSET           (0x2)
#define PSRAM_MR0_READ_LAT_MASK             (0x7)
#define PSRAM_MR0_READ_LAT_IN66MHZ          (0x2)
#define PSRAM_MR0_READ_LAT_IN109MHZ         (0x3)
#define PSRAM_MR0_READ_LAT_IN133MHZ         (0x4)
#define PSRAM_MR0_READ_LAT_IN166MHZ         (0x5)
#define PSRAM_MR0_READ_LAT_IN200MHZ         (0x6)
#define PSRAM_MR0_LT_OFFSET                 (0x5)
#define PSRAM_MR0_LT_MASK                   (0x1)
#define PSRAM_MR0_LT                        (0x0)

// MR1–MR2 Device identity
#define PSRAM_MR1_VENDOR_ID_OFFSET          (0x0)
#define PSRAM_MR1_VENDOR_ID_MASK            (0x1F)
#define PSRAM_MR1_VENDOR_ID                 (0xD) /**< APM Manufacturer ID */
#define PSRAM_MR1_DENSITY_MAP_OFFSET        (0x5)
#define PSRAM_MR1_DENSITY_MAP_MASK          (0x1)
#define PSRAM_MR2_DENSITY_MAP_OFFSET        (0x0)
#define PSRAM_MR2_DENSITY_MAP_MASK          (0x7)
#define PSRAM_MR2_DEVICE_ID_OFFSET          (0x3)
#define PSRAM_MR2_DEVICE_ID_MASK            (0x3)
#define PSRAM_MR2_GOOD_DIE_OFFSET           (0x7)
#define PSRAM_MR2_GOOD_DIE_MASK             (0x1)
#define PSRAM_MR2_GOOD_DIE                  (0x1)

// MR4 Refresh and write latency
#define PSRAM_MR4_PASR_OFFSET               (0x0)
#define PSRAM_MR4_PASR_MASK                 (0x7)
#define PSRAM_MR4_RFRATE_OFFSET             (0x3)
#define PSRAM_MR4_RFRATE_MASK               (0x1)
#define PSRAM_MR4_RFRATE_ALWAY_REFRESH      (0x0)
#define PSRAM_MR4_WRITE_LAT_OFFSET          (0x7)
#define PSRAM_MR4_WRITE_LAT_MASK            (0x1)
#define PSRAM_MR4_WRITE_LAT_WL0             (0x0)
#define PSRAM_MR4_WRITE_LAT_WL2             (0x1)

// MR6 Sleep control
#define PSRAM_MR6_HALF_SLEEP_OFFSET         (0x4)
#define PSRAM_MR6_HALF_SLEEP_MASK           (0xf)
#define PSRAM_MR6_HALF_SLEEP_MODE           (0xf)
/** @} */

/* --------------------------------------------------------------------------
 * Function Implementations
 * -------------------------------------------------------------------------- */
#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM

/**
 * @brief Configure Mode Register read/write instruction sequences.
 * @details
 * Sets controller LUT entries for MR read, MR write, and reset operations.
 */
void psram_seq_configure_stage0(void){
    IP_PSRAM_CTRL->REG_S5LUT0.all = PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);

    // MR RESET
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, 0xFF);
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, 2);
    
    // MR RD SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_MR_READ_INS);
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x2);

    // MR WR SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_MR_WRITE_INS);
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);
}

/**
 * @brief Configure AHB access sequences for APM PSRAM.
 * @param[in] clock_freq Current PSRAM controller clock frequency (Hz)
 * @details
 * Initializes controller LUT entries for AHB read/write access based on
 * frequency-dependent latency and dummy cycles.
 */
void psram_seq_configure_stage1(uint32_t clock_freq, uint32_t density){
    uint8_t write_dummy = 0;

    if (clock_freq >= 166000000){ // 200Mhz
        write_dummy = 1;
    }
    else {
        write_dummy = 0;
    }

    // AHB RD SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_AHB_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_SYNC_READ_INS); // CMD
    IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x5);  // READ

    // AHB WR SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_AHB_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_SYNC_WRITE_INS);
    IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
    IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
}

/**
 * @brief Extract PSRAM manufacturer and density information.
 * @param[out] density Pointer to variable for storing density mapping.
 * @return 0 on success, -1 if die check fails.
 * @details
 * Reads MR1–MR2 registers to confirm device health and determine vendor/density.
 */
int32_t information_extraction_sequence(uint32_t* density){
    uint8_t mr_r_value_m1 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);
    uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);

    if (((mr_r_value_m2 >> PSRAM_MR2_GOOD_DIE_OFFSET) & PSRAM_MR2_GOOD_DIE_MASK) == PSRAM_MR2_GOOD_DIE){
        PSRAM_LOG("\n\n\nGOOD DIE\n\n\n");
    } else {
        PSRAM_LOGE("\n\n\nBAD DIE\n\n\n");
        return -1;
    }

    PSRAM_LOG("Vendor ID: 0x%x;     Dev ID: 0x%x", ((mr_r_value_m1 >> PSRAM_MR1_VENDOR_ID_OFFSET) & PSRAM_MR1_VENDOR_ID_MASK),\
            ((mr_r_value_m2 >> PSRAM_MR2_DEVICE_ID_OFFSET) & PSRAM_MR2_DEVICE_ID_MASK));

    uint8_t _density = 0;
    _density = (mr_r_value_m2 >> PSRAM_MR1_DENSITY_MAP_OFFSET) & PSRAM_MR1_DENSITY_MAP_MASK;
    if (_density == APM_PSRAM_MEM_32Mb_DENSITY_MAP){
        PSRAM_LOG("PSRAM Density -> 32M bit");
        *density = PSRAM_MEM_32Mb_DENSITY_MAP;
    } else if (_density == APM_PSRAM_MEM_64Mb_DENSITY_MAP){
        PSRAM_LOG("PSRAM Density -> 64M bit");
        *density = PSRAM_MEM_64Mb_DENSITY_MAP;
    } else {
        PSRAM_LOGE("PSRAM Density -> Unknown");
    }

    return 0;
}

/**
 * @brief Configure PSRAM timing parameters (TCEM, TCPH).
 * @param[in] clock_freq Current PSRAM operating frequency.
 * @details
 * Computes timing constants and updates corresponding timing registers.
 */
void controller_timing_configure(uint32_t clock_freq){
    // TCEM configure
    {
        uint32_t tcem_para = 0;
        tcem_para = (uint32_t)(((clock_freq / __INNER_MICROSEC_LOW) * PSRAM_TCEM_REFRESH_TIME) / __INNER_MICROSEC_HIGH);
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCEM_CFG = tcem_para;
        PSRAM_LOG("PSRAM TCEM parameter: %d", tcem_para);
    }
    // TCPH configure
    {
        uint32_t tcph_para = 4;
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCPH_CFG = tcph_para;
        PSRAM_LOG("PSRAM TCPH parameter: %d", tcph_para);
    }
}

/**
 * @brief Configure PSRAM controller device parameters.
 * @param[in] density PSRAM density identifier.
 * @details
 * Sets device type and page size configuration.
 */
void controller_para_cfg_type(void){
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_APM; // Set type to xcella
}

void controller_para_cfg_page_size(uint32_t density){
    // Set device page size
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_1K;
}

/**
 * @brief Program Mode Registers MR0–MR4 according to clock and density.
 * @param[in] clock_freq Operating frequency in Hz.
 * @param[in] density Device density mapping value.
 * @details
 * Adjusts read/write latency and refresh parameters for stable operation.
 */
void die_para_configure(uint32_t clock_freq, uint32_t density){
    uint8_t mr_w_value = 0, mr_r_value = 0;
    uint8_t i = 100;

    // MR0
    {
        if (density == PSRAM_MEM_32Mb_DENSITY_MAP){
            mr_w_value = ((PSRAM_MR0_LT << PSRAM_MR0_LT_OFFSET) | (PSRAM_32Mb_MR0_DRIVE_STR << PSRAM_MR0_DRIVE_STR_OFFSET));
        }

        if (clock_freq >= 200000000){ // 200Mhz
            mr_w_value |= (PSRAM_MR0_READ_LAT_IN200MHZ << PSRAM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 66000000) // 66Mhz
        {
            mr_w_value |= (PSRAM_MR0_READ_LAT_IN66MHZ << PSRAM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 109000000) // 109Mhz
        {
            mr_w_value |= (PSRAM_MR0_READ_LAT_IN109MHZ << PSRAM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 133000000) // 133Mhz
        {
            mr_w_value |= (PSRAM_MR0_READ_LAT_IN133MHZ << PSRAM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 166000000) // 166Mhz
        {
            mr_w_value |= (PSRAM_MR0_READ_LAT_IN166MHZ << PSRAM_MR0_READ_LAT_OFFSET);
        }

        while(--i){
            mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR0.all & 0xff);
            if (mr_r_value == mr_w_value){
                // TODO change read latancy
                break;
            }
            IP_PSRAM_CTRL->REG_MR0.all = mr_w_value;
        }

        if (i == 0){
            PSRAM_LOGE("PSRAM MR0 configure error");
        }
    }

    // MR4
    {
        i = 100;

        mr_w_value = (PSRAM_MR4_RFRATE_ALWAY_REFRESH << PSRAM_MR4_RFRATE_OFFSET);

        if (clock_freq >= 166000000){ // 166Mhz
            mr_w_value |= (PSRAM_MR4_WRITE_LAT_WL2 << PSRAM_MR4_WRITE_LAT_OFFSET);
        } else {
            mr_w_value |= (PSRAM_MR4_WRITE_LAT_WL0 << PSRAM_MR4_WRITE_LAT_OFFSET);
        }

        while(--i){
            mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
            if (mr_r_value == mr_w_value){
                // TODO change write latancy
                break;
            }
            IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
        }

        if (i == 0){
            PSRAM_LOGE("PSRAM MR4 configure error");
        }
    }

    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR0.all & 0xff);
    PSRAM_LOG("MR0: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);
    PSRAM_LOG("MR1: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);
    PSRAM_LOG("MR2: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
    PSRAM_LOG("MR4: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR6.all & 0xff);
    PSRAM_LOG("MR6: 0x%x", mr_r_value);
}

/**
 * @brief Exit PSRAM from sleep mode.
 * @param[in] clock_freq PSRAM operating clock frequency.
 * @details
 * Executes wakeup sequence to restore PSRAM to normal operation state.
 */
void exit_sleep_mode_sequence(uint32_t clock_freq){
    uint32_t hclk = CRM_GetHclkFreq();
    IP_PSRAM_CTRL->REG_CUSTEXE.all = PSRAM_EXE_SLEEP_MODE_SEQ_ID;
    uint32_t timeout = 30 * (hclk / clock_freq);
    while(timeout--);
}

/**
 * @brief Enter PSRAM low-power (sleep) mode.
 * @param[in] sleep_mode Desired sleep mode (half/deep).
 * @retval 0 Success.
 * @retval -1 Unsupported or invalid mode.
 * @details
 * Configures MR6 to enter low-power state; deep sleep currently unsupported.
 */
int32_t enter_sleep_mode_sequence(_psram_sleep_mode_t sleep_mode){
    if (sleep_mode == PSRAM_SLEEP_MODE_HALF_SLEEP){
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_MR6_HALF_SLEEP_MODE;
    } else if (sleep_mode == PSRAM_SLEEP_MODE_DEEP_SLEEP){
        PSRAM_LOGE("PSRAM deep sleep mode unsupported!!!");
        return -1;
    } else {
        PSRAM_LOGE("PSRAM sleep mode unsupported!!!");
        return -1;
    }
    return 0;
}

#endif /* PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM */
