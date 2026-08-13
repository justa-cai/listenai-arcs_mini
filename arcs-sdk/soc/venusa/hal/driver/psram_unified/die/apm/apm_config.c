#include "apm_config.h"

#include "log_print.h"

/* --------------------------------------------------------------------------
 * PSRAM UNIFIED DIE ----------------------------APM DRIVER IMPLEMENTATION
 *
 * 
 *                               BEGIN OF APM DRIVER
 * 
 * 
 * -------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------
 * PSRAM Density Mapping Definitions
 * -------------------------------------------------------------------------- */
/**
 * @brief Density identifiers for APM PSRAM devices.
 */
#define APM_PSRAM_MEM_32Mb_DENSITY_MAP     (0x0)  /**< 32Mb density identifier */
#define APM_PSRAM_MEM_64Mb_DENSITY_MAP     (0x1)  /**< 64Mb density identifier */
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

/* --------------------------------------------------------------------------
 * Mode Register Field Definitions (MR0–MR8)
 * -------------------------------------------------------------------------- */
/**
 * @defgroup PSRAM_MR_Mode_Registers
 * @brief Mode Register bit field definitions for APM PSRAM.
 * @{
 */
//******************MR0 [R/W]
// bit 0-1 MR0[1:0]
// MR0 Drive strength and latency
#define PSRAM_UNIFIED_APM_MR0_DRIVE_STR_OFFSET          (0x0)
#define PSRAM_UNIFIED_APM_MR0_DRIVE_STR_MASK            (0x3)
#define PSRAM_UNIFIED_APM_32Mb_MR0_DRIVE_STR            (0x1)

// bit 2-4 MR0[4:2]
// MR0 Read Latency
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET           (0x2)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_MASK             (0x7)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_IN66MHZ          (0x2)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_IN109MHZ         (0x3)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_IN133MHZ         (0x4)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_IN166MHZ         (0x5)
#define PSRAM_UNIFIED_APM_MR0_READ_LAT_IN200MHZ         (0x6)

// bit 5 MR0[5]
// MR0 Read Latency Type
#define PSRAM_UNIFIED_APM_MR0_LT_OFFSET                 (0x5)
#define PSRAM_UNIFIED_APM_MR0_LT_MASK                   (0x1)
#define PSRAM_UNIFIED_APM_MR0_LT                        (0x0)

//******************MR1 [R]
// bit 0-4 MR1[4:0]
// Vendor ID
#define PSRAM_UNIFIED_APM_MR1_VENDOR_ID_OFFSET          (0x0)
#define PSRAM_UNIFIED_APM_MR1_VENDOR_ID_MASK            (0x1F)
#define PSRAM_UNIFIED_APM_MR1_VENDOR_ID                 (0xD) /**< APM Manufacturer ID */

// bit 5 MR1[5]
// Density mapping
#define PSRAM_UNIFIED_APM_MR1_DENSITY_MAP_OFFSET        (0x5)
#define PSRAM_UNIFIED_APM_MR1_DENSITY_MAP_MASK          (0x1)

//******************MR2 [R]
// bit 0-2 MR2[2:0]
// Device Density mapping
#define PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_OFFSET        (0x0)
#define PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_MASK          (0x7)
#define PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_VALUE         (0x1)

// bit 3-4 MR2[4:3]
// Device ID
#define PSRAM_UNIFIED_APM_MR2_DEVICE_ID_OFFSET          (0x3)
#define PSRAM_UNIFIED_APM_MR2_DEVICE_ID_MASK            (0x3)
#define PSRAM_UNIFIED_APM_MR2_32Mb_DEVICE_ID_VALUE      (0x1)
#define PSRAM_UNIFIED_APM_MR2_64Mb_DEVICE_ID_VALUE      (0x2)

// bit 7 MR2[7]
// Good-Die Bit
#define PSRAM_UNIFIED_APM_MR2_GOOD_DIE_OFFSET           (0x7)
#define PSRAM_UNIFIED_APM_MR2_GOOD_DIE_MASK             (0x1)
#define PSRAM_UNIFIED_APM_MR2_GOOD_DIE                  (0x1)

//******************MR4 [R/W]
// bit 0-2 MR4[2:0]
// PASR
#define PSRAM_UNIFIED_APM_MR4_PASR_OFFSET               (0x0)
#define PSRAM_UNIFIED_APM_MR4_PASR_MASK                 (0x7)

// bit 3 MR4[3]
// Refresh Frequency setting
#define PSRAM_UNIFIED_APM_MR4_RFRATE_OFFSET             (0x3)
#define PSRAM_UNIFIED_APM_MR4_RFRATE_MASK               (0x1)
#define PSRAM_UNIFIED_APM_MR4_RFRATE_ALWAY_REFRESH      (0x0)
#define PSRAM_UNIFIED_APM_MR4_RFRATE_SLOW_REFRESH       (0x1)

// bit 7 MR4[7]
// Write Latency
#define PSRAM_UNIFIED_APM_MR4_WRITE_LAT_OFFSET          (0x7)
#define PSRAM_UNIFIED_APM_MR4_WRITE_LAT_MASK            (0x1)
#define PSRAM_UNIFIED_APM_MR4_WRITE_LAT_WL0             (0x0)
#define PSRAM_UNIFIED_APM_MR4_WRITE_LAT_WL2             (0x1)

//******************MR6 [W]
// bit 4-7 MR6[7:4]
// Sleep control
#define PSRAM_UNIFIED_APM_MR6_HALF_SLEEP_OFFSET         (0x4)
#define PSRAM_UNIFIED_APM_MR6_HALF_SLEEP_MASK           (0xf)
#define PSRAM_UNIFIED_APM_MR6_HALF_SLEEP_MODE           (0xf)
/** @} */

#define PSRAM_UNIFIED_APM_TCEM_REFRESH_TIME             30
#define PSRAM_UNIFIED_APM_TCPH_TIME                     8

void __psram_apm_mr_seq_configure(void){
    // MR RD SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_MR_READ_INS);
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x2);

    // MR WR SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_APM_MR_WRITE_INS);
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);

    // MR RESET
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, 0xFF);
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, 2);

    IP_PSRAM_CTRL->REG_S5LUT0.all = PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);
}

void __psram_apm_ahb_seq_configure(uint32_t clock_freq, uint32_t density){
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
    if (write_dummy == 0){
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
    } else {
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
    }
}

inline void __psram_apm_controller_die_type(void){
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_APM; // Set type to apm
}

inline void __psram_apm_controller_die_page_size(uint32_t density){
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_512;
}

void __psram_apm_controller_timing_configure(uint32_t clock_freq){
    // TCEM configure
    {
        uint32_t tcem_para = 0;
        tcem_para = (uint32_t)(((clock_freq / __INNER_MICROSEC_LOW) * PSRAM_UNIFIED_APM_TCEM_REFRESH_TIME) / __INNER_MICROSEC_HIGH);
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCEM_CFG = tcem_para;
        PSRAM_LOG("PSRAM TCEM parameter: %d", tcem_para);
    }
    // TCPH configure
    {
        uint32_t tcph_para = PSRAM_UNIFIED_APM_TCPH_TIME;
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCPH_CFG = tcph_para;
        PSRAM_LOG("PSRAM TCPH parameter: %d", tcph_para);
    }
}

int32_t __psram_apm_info_extra(uint32_t* density){
    uint8_t mr_r_value_m1 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);

    uint8_t vendor_id = ((mr_r_value_m1 >> PSRAM_UNIFIED_APM_MR1_VENDOR_ID_OFFSET) & PSRAM_UNIFIED_APM_MR1_VENDOR_ID_MASK);
    if (vendor_id != PSRAM_UNIFIED_APM_MR1_VENDOR_ID){
        PSRAM_LOG("\n\n\nUnknown Vendor ID: 0x%x\n\n\n", vendor_id);
        return -1;
    }

    uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);

    if (((mr_r_value_m2 >> PSRAM_UNIFIED_APM_MR2_GOOD_DIE_OFFSET) & PSRAM_UNIFIED_APM_MR2_GOOD_DIE_MASK) == PSRAM_UNIFIED_APM_MR2_GOOD_DIE){
        PSRAM_LOG("\n\n\nGOOD DIE\n\n\n");
    } else {
        PSRAM_LOG("\n\n\nBAD DIE\n\n\n");
        return -1;
    }

    if (((mr_r_value_m2 >> PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_OFFSET) & PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_MASK) != PSRAM_UNIFIED_APM_MR2_DENSITY_MAP_VALUE){
        PSRAM_LOGE("Density Mapping Mismatch in MR2: 0x%x", mr_r_value_m2);
        return -1;
    }

    uint8_t device_id = ((mr_r_value_m2 >> PSRAM_UNIFIED_APM_MR2_DEVICE_ID_OFFSET) & PSRAM_UNIFIED_APM_MR2_DEVICE_ID_MASK);

    PSRAM_LOG("Vendor ID: 0x%x;     Dev ID: 0x%x", vendor_id, device_id);

    uint8_t _density = 0;
    _density = (mr_r_value_m2 >> PSRAM_UNIFIED_APM_MR1_DENSITY_MAP_OFFSET) & PSRAM_UNIFIED_APM_MR1_DENSITY_MAP_MASK;
    if (_density == APM_PSRAM_MEM_32Mb_DENSITY_MAP){
        if (device_id != PSRAM_UNIFIED_APM_MR2_32Mb_DEVICE_ID_VALUE) {
            PSRAM_LOG("Device ID Mismatch for 32Mb Density: 0x%x", device_id);
            return -1;
        }
        PSRAM_LOG("PSRAM Density -> 32M bit");
        *density = PSRAM_MEM_32Mb_DENSITY_MAP;
    } else if (_density == APM_PSRAM_MEM_64Mb_DENSITY_MAP){
        if (device_id != PSRAM_UNIFIED_APM_MR2_64Mb_DEVICE_ID_VALUE) {
            PSRAM_LOG("Device ID Mismatch for 64Mb Density: 0x%x", device_id);
            return -1;
        }
        PSRAM_LOG("PSRAM Density -> 64M bit");
        *density = PSRAM_MEM_64Mb_DENSITY_MAP;
    } else {
        PSRAM_LOG("PSRAM Density -> Unknown");
    }

    return 0;
}

void __psram_apm_die_para_configure(uint32_t clock_freq, uint32_t density){
    uint8_t mr_w_value = 0, mr_r_value = 0;
    uint8_t i = 100;

    // MR0
    {
        if (density == PSRAM_MEM_32Mb_DENSITY_MAP){
            mr_w_value = ((PSRAM_UNIFIED_APM_MR0_LT << PSRAM_UNIFIED_APM_MR0_LT_OFFSET) | (PSRAM_UNIFIED_APM_32Mb_MR0_DRIVE_STR << PSRAM_UNIFIED_APM_MR0_DRIVE_STR_OFFSET));
        }

        if (clock_freq <= 66000000) // 66Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_APM_MR0_READ_LAT_IN66MHZ << PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 109000000) // 109Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_APM_MR0_READ_LAT_IN109MHZ << PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 133000000) // 133Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_APM_MR0_READ_LAT_IN133MHZ << PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 166000000) // 166Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_APM_MR0_READ_LAT_IN166MHZ << PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET);
        }

        else { /// <= 200Mhz
            mr_w_value |= (PSRAM_UNIFIED_APM_MR0_READ_LAT_IN200MHZ << PSRAM_UNIFIED_APM_MR0_READ_LAT_OFFSET);
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

        mr_w_value = (PSRAM_UNIFIED_APM_MR4_RFRATE_ALWAY_REFRESH << PSRAM_UNIFIED_APM_MR4_RFRATE_OFFSET);

        if (clock_freq >= 166000000){ // 166Mhz
            mr_w_value |= (PSRAM_UNIFIED_APM_MR4_WRITE_LAT_WL2 << PSRAM_UNIFIED_APM_MR4_WRITE_LAT_OFFSET);
        } else {
            mr_w_value |= (PSRAM_UNIFIED_APM_MR4_WRITE_LAT_WL0 << PSRAM_UNIFIED_APM_MR4_WRITE_LAT_OFFSET);
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
}

void __psram_apm_mr_print(void){
    uint8_t mr_r_value = 0;

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

void __psram_apm_enter_sleep_mode(__psram_unified_sleep_mode_t sleep_mode){
    uint8_t mr_r_value = 0, mr_w_value = 0;
    uint8_t i = 100;

    while(--i){
        mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        mr_w_value = mr_r_value | (PSRAM_UNIFIED_APM_MR4_RFRATE_SLOW_REFRESH << PSRAM_UNIFIED_APM_MR4_RFRATE_OFFSET);
        if (mr_r_value == mr_w_value){
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }

    if (sleep_mode == __psram_unified_sleep_mode_half_sleep){
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_UNIFIED_APM_MR6_HALF_SLEEP_MODE;
    }
}

void __psram_apm_exit_sleep_mode(void){
    // Exit sleep mode sequence
    IP_PSRAM_CTRL->REG_S5LUT0.all = PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);

    uint32_t hclk = CRM_GetHclkFreq();
    IP_PSRAM_CTRL->REG_CUSTEXE.all = PSRAM_EXE_SLEEP_MODE_SEQ_ID;
    uint32_t timeout = 30 * (hclk / 24000000);
    while(timeout--);
}

void __psram_apm_refresh_rate_normal_set(void){
    uint8_t mr_r_value = 0, mr_w_value = 0;
    uint8_t i = 100;

    while(--i){
        mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        mr_w_value = mr_r_value & ((~(PSRAM_UNIFIED_APM_MR4_RFRATE_SLOW_REFRESH << PSRAM_UNIFIED_APM_MR4_RFRATE_OFFSET)) & 0xFF);
        if (mr_r_value == mr_w_value){
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }
}
/* --------------------------------------------------------------------------
 *
 *
 *                               END OF APM DRIVER
 * 
 * 
 * PSRAM UNIFIED DIE ----------------------------APM DRIVER IMPLEMENTATION
 * -------------------------------------------------------------------------- */

