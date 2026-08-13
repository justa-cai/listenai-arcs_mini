#include "xccela_config.h"

#include "log_print.h"

/* --------------------------------------------------------------------------
 * PSRAM UNIFIED DIE ----------------------------XCCELA DRIVER IMPLEMENTATION
 *
 * 
 *                               BEGIN OF XCCELA DRIVER
 * 
 * 
 * -------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------
 * PSRAM Density Mapping Definitions
 * -------------------------------------------------------------------------- */
#define XCCELA_PSRAM_MEM_32Mb_DENSITY_MAP     (0x1)  /**< 32Mb density identifier */
#define XCCELA_PSRAM_MEM_64Mb_DENSITY_MAP     (0x3)  /**< 64Mb density identifier */
#define XCCELA_PSRAM_MEM_128Mb_DENSITY_MAP    (0x5)  /**< 128Mb density identifier */
#define XCCELA_PSRAM_MEM_256Mb_DENSITY_MAP    (0x7)  /**< 256Mb density identifier */
#define XCCELA_PSRAM_MEM_512Mb_DENSITY_MAP    (0x6)  /**< 512Mb density identifier */

/* --------------------------------------------------------------------------
 * Xccela PSRAM Command Definitions
 * -------------------------------------------------------------------------- */
#define PSRAM_XCCELA_SYNC_READ_INS           0x00
#define PSRAM_XCCELA_SYNC_WRITE_INS          0x80
#define PSRAM_XCCELA_LINEAR_BURST_READ_INS   0x20
#define PSRAM_XCCELA_LINEAR_BURST_WRITE_INS  0xA0
#define PSRAM_XCCELA_MR_READ_INS             0x40
#define PSRAM_XCCELA_MR_WRITE_INS            0xC0
#define PSRAM_XCCELA_GLOBAL_RESET_INS        0xFF

/* --------------------------------------------------------------------------
 * Mode Register Field Descriptions (MR0–MR8)
 * -------------------------------------------------------------------------- */
/** @defgroup PSRAM_MR0_Mode_Register
 *  @brief MR0 Field Mapping and Latency Definitions
 *  @{
 */
#define PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR_OFFSET   (0x0)
#define PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR_MASK     (0x3)
#define PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR          (0x2)  /**< Default full drive strength */

#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET    (0x2)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_MASK      (0x7)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN66MHZ   (0x0)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN109MHZ  (0x1)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN133MHZ  (0x2)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN166MHZ  (0x3)
#define PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN200MHZ  (0x4)
#define PSRAM_UNIFIED_XCCELA_64Mb_MR0_READ_LAT_IN250MHZ   (0x5)
#define PSRAM_UNIFIED_XCCELA_128Mb_MR0_READ_LAT_IN225MHZ  (0x5)
#define PSRAM_UNIFIED_XCCELA_128Mb_MR0_READ_LAT_IN250MHZ  (0x6)

// bit 5 MR0[5]
// Read Latency Type
#define PSRAM_UNIFIED_XCCELA_MR0_LT_OFFSET                 (0x5)
#define PSRAM_UNIFIED_XCCELA_MR0_LT_MASK                   (0x1)
#define PSRAM_UNIFIED_XCCELA_MR0_LT                        (0x1)

// bit 7 MR0[7]
// Temperature Sensor Override
#define PSRAM_UNIFIED_XCCELA_MR0_TSO_OFFSET                (0x7)
#define PSRAM_UNIFIED_XCCELA_MR0_TSO_MASK                  (0x1)
#define PSRAM_UNIFIED_XCCELA_MR0_TSO                       (0x0)

//******************MR1 [R]
// bit 0-4 MR1[4:0]
// Vendor ID mapping
#define PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID_OFFSET          (0x0)
#define PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID_MASK            (0x1F)
#define PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID                 (0xD)

// bit 7 MR1[7]
// Ultra Low Power Device mapping
#define PSRAM_UNIFIED_XCCELA_MR1_ULP_OFFSET                (0x7)
#define PSRAM_UNIFIED_XCCELA_MR1_ULP_MASK                  (0x1)

//******************MR2 [R]
// bit 0-2 MR2[2:0]
// Device Density mapping
#define PSRAM_UNIFIED_XCCELA_MR2_DENSITY_MAP_OFFSET        (0x0)
#define PSRAM_UNIFIED_XCCELA_MR2_DENSITY_MAP_MASK          (0x7)

// bit 3-4 MR2[4:3]
// Device ID
#define PSRAM_UNIFIED_XCCELA_MR2_DEVICE_ID_OFFSET          (0x3)
#define PSRAM_UNIFIED_XCCELA_MR2_DEVICE_ID_MASK            (0x3)
#define PSRAM_UNIFIED_XCCELA_MR2_64Mb_DEVICE_ID_VALUE      (0x2)
#define PSRAM_UNIFIED_XCCELA_MR2_128Mb_DEVICE_ID_VALUE     (0x3)

// TODO In 128Mb the good die use 3bit
// bit 5-7 MR2[7:5]
// Good-Die Bit
#define PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE_OFFSET           (0x7)
#define PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE_MASK             (0x1)
#define PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE                  (0x1)

//******************MR3 [R]
// bit 4-5 MR3[5:4]
// Self Refresh Flag
#define PSRAM_UNIFIED_XCCELA_MR3_SELF_REFRESH_OFFSET       (0x4)
#define PSRAM_UNIFIED_XCCELA_MR3_SELF_REFRESH_MASK         (0x3)

// bit 7 MR3[7]
// Row Boundary Crossing Enable
#define PSRAM_UNIFIED_XCCELA_MR3_RBXEN_OFFSET              (0x7)
#define PSRAM_UNIFIED_XCCELA_MR3_RBXEN_MASK                (0x1)

//******************MR4 [R/W]
// bit 0-2 MR4[2:0]
// PASR
#define PSRAM_UNIFIED_XCCELA_MR4_PASR_OFFSET               (0x0)
#define PSRAM_UNIFIED_XCCELA_MR4_PASR_MASK                 (0x7)

// bit 3-4 MR4[4:3]
// Refresh Frequency setting
#define PSRAM_UNIFIED_XCCELA_MR4_RFRATE_OFFSET             (0x3)
#define PSRAM_UNIFIED_XCCELA_MR4_RFRATE_MASK               (0x3)
#define PSRAM_UNIFIED_XCCELA_MR4_RFRATE_ALWAY_REFRESH      (0x0)
#define PSRAM_UNIFIED_XCCELA_MR4_RFRATE_SLOW_REFRESH       (0x1)

// bit 5-7 MR4[7:5]
// Write Latency
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET          (0x5)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_MASK            (0x7)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN66MHZ         (0x0)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN109MHZ        (0x4)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN133MHZ        (0x2)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN166MHZ        (0x6)
#define PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN200MHZ        (0x1)
#define PSRAM_UNIFIED_XCCELA_64Mb_MR4_WRITE_LAT_IN250MHZ   (0x5)
#define PSRAM_UNIFIED_XCCELA_128Mb_MR4_WRITE_LAT_IN225MHZ  (0x5)
#define PSRAM_UNIFIED_XCCELA_128Mb_MR4_WRITE_LAT_IN250MHZ  (0x3)

//******************MR6 [W]
#define PSRAM_UNIFIED_XCCELA_MR6_HALF_SLEEP_OFFSET         (0x4)
#define PSRAM_UNIFIED_XCCELA_MR6_HALF_SLEEP_MASK           (0xf)

#define PSRAM_UNIFIED_XCCELA_MR6_HALF_SLEEP_MODE           (0xf0)
#define PSRAM_UNIFIED_XCCELA_MR6_DEEP_SLEEP_MODE           (0xc0)

#define PSRAM_UNIFIED_XCCELA_TCEM_REFRESH_TIME             70

void __psram_xccela_mr_seq_configure(void){
    // MR RD SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_MR_READ_INS);
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S2LUT1.bit.S2_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x4);

    // MR WR SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_MR_WRITE_INS);
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S3LUT1.bit.S3_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);

    // Exit sleep mode sequence
    IP_PSRAM_CTRL->REG_S5LUT0.all = PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);

    // MR RESET
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, 0xFF);
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, 2);
}

void __psram_xccela_ahb_seq_configure(uint32_t clock_freq, uint32_t density){
    // XCCELA DIE AHB READ, AHB WRITE, MR READ AND MR WRITE LOGIC PARAMETER
    {
        uint8_t write_dummy = 0;

        if (clock_freq <= 66000000) // 66Mhz
        {
            write_dummy = 1;
        }

        else if (clock_freq <= 109000000) // 109Mhz
        {
            write_dummy = 2;
        }

        else if (clock_freq <= 133000000) // 133Mhz
        {
            write_dummy = 3;
        }

        else if (clock_freq <= 166000000) // 166Mhz
        {
            write_dummy = 4;
        }

        else if (clock_freq <= 200000000){ // 200Mhz
            write_dummy = 5;
        }

        else{
        	if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
        		if (clock_freq <= 225000000){ // 225Mhz
					write_dummy = 6;
				}

				else if (clock_freq <= 250000000){ // 250Mhz
					write_dummy = 7;
				}
        	} else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
        		if (clock_freq <= 250000000){ // 250Mhz
					write_dummy = 6;
				}
        	}
        }

        PSRAM_LOG("[Xccela]--->Write dummy: %d", write_dummy);

        // AHB RD SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_AHB_RD_SEQ_ID;
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_LINEAR_BURST_READ_INS); // CMD
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);  // ADDR
        IP_PSRAM_CTRL->REG_S0LUT1.bit.S0_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x0);  // READ

        // AHB WR SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_AHB_WR_SEQ_ID;
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_LINEAR_BURST_WRITE_INS);
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR3 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
    }
}

inline void __psram_xccela_controller_die_type(void){
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_XCCELA; // Set type to xcella
}

inline void __psram_xccela_controller_die_page_size(uint32_t density){
    // Set device page size
    if ((density == PSRAM_MEM_32Mb_DENSITY_MAP) || (density == PSRAM_MEM_64Mb_DENSITY_MAP)){
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_512;
    } else {
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_512;
    }
}

void __psram_xccela_controller_timing_configure(uint32_t clock_freq){
    // TCEM configure
    {
        uint32_t tcem_para = 0;
        tcem_para = (uint32_t)(((clock_freq / __INNER_MICROSEC_LOW) * PSRAM_UNIFIED_XCCELA_TCEM_REFRESH_TIME) / __INNER_MICROSEC_HIGH);
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCEM_CFG = tcem_para;
        PSRAM_LOG("PSRAM TCEM parameter: %d", tcem_para);
    }
    // TCPH configure
    {
        uint32_t tcph_para = 4;
        if (clock_freq <= 200000000){
            tcph_para = 4;
        } else if (clock_freq <= 250000000){
            tcph_para = 8;
        }
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCPH_CFG = tcph_para;
        PSRAM_LOG("PSRAM TCPH parameter: %d", tcph_para);
    }
}

int32_t __psram_xccela_info_extra(uint32_t* density){
    uint8_t mr_r_value_m1 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);

    uint8_t vendor_id = ((mr_r_value_m1 >> PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID_OFFSET) & PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID_MASK);
    if (vendor_id != PSRAM_UNIFIED_XCCELA_MR1_VENDOR_ID){
        PSRAM_LOG("\n\n\nUnknown Vendor ID: 0x%x\n\n\n", vendor_id);
        return -1;
    }

    uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);

    if (((mr_r_value_m2 >> PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE_OFFSET) & PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE_MASK) == PSRAM_UNIFIED_XCCELA_MR2_GOOD_DIE){
        PSRAM_LOG("\n\n\nGOOD DIE\n\n\n");
    } else {
        PSRAM_LOG("\n\n\nBAD DIE\n\n\n");
        return -1;
    }

    uint8_t device_id = ((mr_r_value_m2 >> PSRAM_UNIFIED_XCCELA_MR2_DEVICE_ID_OFFSET) & PSRAM_UNIFIED_XCCELA_MR2_DEVICE_ID_MASK);
    PSRAM_LOG("Vendor ID: 0x%x;     Dev ID: 0x%x", vendor_id, device_id);

    uint8_t _density = 0;
    _density = (mr_r_value_m2 >> PSRAM_UNIFIED_XCCELA_MR2_DENSITY_MAP_OFFSET) & PSRAM_UNIFIED_XCCELA_MR2_DENSITY_MAP_MASK;
    if (_density == XCCELA_PSRAM_MEM_32Mb_DENSITY_MAP){
        PSRAM_LOG("PSRAM Density -> 32M bit");
        *density = PSRAM_MEM_32Mb_DENSITY_MAP;
    } else if (_density == XCCELA_PSRAM_MEM_64Mb_DENSITY_MAP){
        if (device_id != PSRAM_UNIFIED_XCCELA_MR2_64Mb_DEVICE_ID_VALUE) {
            PSRAM_LOG("Device ID Mismatch for 64Mb Density: 0x%x", device_id);
            return -1;
        }
        PSRAM_LOG("PSRAM Density -> 64M bit");
        *density = PSRAM_MEM_64Mb_DENSITY_MAP;
    } else if (_density == XCCELA_PSRAM_MEM_128Mb_DENSITY_MAP){
        if (device_id != PSRAM_UNIFIED_XCCELA_MR2_128Mb_DEVICE_ID_VALUE) {
            PSRAM_LOG("Device ID Mismatch for 128Mb Density: 0x%x", device_id);
            return -1;
        }
        PSRAM_LOG("PSRAM Density -> 128M bit");
        *density = PSRAM_MEM_128Mb_DENSITY_MAP;
    } else if (_density == XCCELA_PSRAM_MEM_256Mb_DENSITY_MAP){
        PSRAM_LOG("PSRAM Density -> 256M bit");
        *density = PSRAM_MEM_256Mb_DENSITY_MAP;
    } else if (_density == XCCELA_PSRAM_MEM_512Mb_DENSITY_MAP){
        PSRAM_LOG("PSRAM Density -> 512M bit");
        *density = PSRAM_MEM_512Mb_DENSITY_MAP;
    } else {
        PSRAM_LOG("PSRAM Density -> Unknown");
    }

    return 0;
}

void __psram_xccela_die_para_configure(uint32_t clock_freq, uint32_t density){
    uint8_t mr_w_value = 0, mr_r_value = 0;
    uint8_t i = 100;

    // MR0
    {
        if ((density == PSRAM_MEM_128Mb_DENSITY_MAP) || (density == PSRAM_MEM_64Mb_DENSITY_MAP)){
            mr_w_value = ((PSRAM_UNIFIED_XCCELA_MR0_LT << PSRAM_UNIFIED_XCCELA_MR0_LT_OFFSET) | (PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR << PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR_OFFSET));
        }
        else{
            mr_w_value = ((PSRAM_UNIFIED_XCCELA_MR0_TSO << PSRAM_UNIFIED_XCCELA_MR0_TSO_OFFSET) | (PSRAM_UNIFIED_XCCELA_MR0_LT << PSRAM_UNIFIED_XCCELA_MR0_LT_OFFSET) | (PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR << PSRAM_UNIFIED_XCCELA_MR0_DRIVE_STR_OFFSET));
        }            

        if (clock_freq <= 66000000) // 66Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN66MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 109000000) // 109Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN109MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 133000000) // 133Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN133MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 166000000) // 166Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN166MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
        }

        else if (clock_freq <= 200000000){ // 200Mhz
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_IN200MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
        }

        else {
            if (density == PSRAM_MEM_128Mb_DENSITY_MAP){
                if (clock_freq <= 225000000){ // 225Mhz
                    mr_w_value |= (PSRAM_UNIFIED_XCCELA_128Mb_MR0_READ_LAT_IN225MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
                }

                else { // 250Mhz
                    mr_w_value |= (PSRAM_UNIFIED_XCCELA_128Mb_MR0_READ_LAT_IN250MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
                }
            } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP){
                mr_w_value |= (PSRAM_UNIFIED_XCCELA_64Mb_MR0_READ_LAT_IN250MHZ << PSRAM_UNIFIED_XCCELA_MR0_READ_LAT_OFFSET);
            }
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

        mr_w_value = (PSRAM_UNIFIED_XCCELA_MR4_RFRATE_ALWAY_REFRESH << PSRAM_UNIFIED_XCCELA_MR4_RFRATE_OFFSET);

        if (clock_freq <= 66000000) // 66Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN66MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
        }

        else if (clock_freq <= 109000000) // 109Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN109MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
        }

        else if (clock_freq <= 133000000) // 133Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN133MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
        }

        else if (clock_freq <= 166000000) // 166Mhz
        {
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN166MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
        }

        else if (clock_freq <= 200000000){ // 200Mhz
            mr_w_value |= (PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_IN200MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
        }

        else {
            if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
                if (clock_freq <= 225000000){ // 225Mhz
                    mr_w_value |= (PSRAM_UNIFIED_XCCELA_128Mb_MR4_WRITE_LAT_IN225MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
                } else if (clock_freq <= 250000000){ // 250Mhz
                    mr_w_value |= (PSRAM_UNIFIED_XCCELA_128Mb_MR4_WRITE_LAT_IN250MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
                }
            } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
                if (clock_freq <= 250000000){ // 250Mhz
                    mr_w_value |= (PSRAM_UNIFIED_XCCELA_64Mb_MR4_WRITE_LAT_IN250MHZ << PSRAM_UNIFIED_XCCELA_MR4_WRITE_LAT_OFFSET);
                }
            }
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

void __psram_xccela_mr_print(void){
    uint8_t mr_r_value = 0;

    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR0.all & 0xff);
    PSRAM_LOG("MR0: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);
    PSRAM_LOG("MR1: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);
    PSRAM_LOG("MR2: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR3.all & 0xff);
    PSRAM_LOG("MR3: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
    PSRAM_LOG("MR4: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR6.all & 0xff);
    PSRAM_LOG("MR6: 0x%x", mr_r_value);
    mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR8.all & 0xff);
    PSRAM_LOG("MR8: 0x%x", mr_r_value);
}

void __psram_xccela_enter_sleep_mode(__psram_unified_sleep_mode_t sleep_mode){
    uint8_t mr_r_value = 0, mr_w_value = 0;
    uint8_t i = 100;

    while(--i){
        mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        mr_w_value = mr_r_value | (PSRAM_UNIFIED_XCCELA_MR4_RFRATE_SLOW_REFRESH << PSRAM_UNIFIED_XCCELA_MR4_RFRATE_OFFSET);
        if (mr_r_value == mr_w_value){
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }

    if (sleep_mode == __psram_unified_sleep_mode_half_sleep){
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_UNIFIED_XCCELA_MR6_HALF_SLEEP_MODE;
    } else if (sleep_mode == __psram_unified_sleep_mode_deep_sleep){
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_UNIFIED_XCCELA_MR6_DEEP_SLEEP_MODE;
    }
}

void __psram_xccela_exit_sleep_mode(void){
    // Exit sleep mode sequence
    IP_PSRAM_CTRL->REG_S5LUT0.all = PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);
    
    uint32_t hclk = CRM_GetHclkFreq();
    IP_PSRAM_CTRL->REG_CUSTEXE.all = PSRAM_EXE_SLEEP_MODE_SEQ_ID;
    uint32_t timeout = 30 * (hclk / 24000000);
    while(timeout--);
}

void __psram_xccela_refresh_rate_normal_set(void){
    uint8_t mr_r_value = 0, mr_w_value = 0;
    uint8_t i = 100;

    while(--i){
        mr_r_value = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        mr_w_value = mr_r_value & ((~(PSRAM_UNIFIED_XCCELA_MR4_RFRATE_SLOW_REFRESH << PSRAM_UNIFIED_XCCELA_MR4_RFRATE_OFFSET)) & 0xFF);
        if (mr_r_value == mr_w_value){
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }
}
/* --------------------------------------------------------------------------
 *
 *
 *                               END OF XCCELA DRIVER
 * 
 * 
 * PSRAM UNIFIED DIE ----------------------------XCCELA DRIVER IMPLEMENTATION
 * -------------------------------------------------------------------------- */
