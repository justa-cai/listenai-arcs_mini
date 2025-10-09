/*
 * PSRAMManager.c
 *
 *  Created on: Jun 7, 2023
 *      Author: USER
 */

#include "PSRAMManager.h"
#include "ClockManager.h"
#include "log_print.h"
#include "cache.h"

// COMMON FUNCTION ************************************************************* START
#define PSRAM_INSTR_MARCO(opc, oper)        (uint16_t)((opc << 8) | oper)
#define psram_memcpy(dst, src, size)         __builtin_memcpy(dst, src, size)
#define psram_compare(ptr0, ptr1, size)      __builtin_memcmp(ptr0, ptr1, size)
#define psram_memset(dst, c, n)              __builtin_memset(dst, c, n)

#define PSRAM_DELAY_STEP                     10
// COMMON FUNCTION ************************************************************* END

// CONTROLLER CONFIGURE ******************************************************** START
#define PSRAM_PREFETCH_FIFO0_DEPTH           0x40
#define PSRAM_PREFETCH_FIFO1_DEPTH           0x40

#define PSRAM_DEV_TYPE_XCELLA                0x0
#define PSRAM_DEV_TYPE_APM                   0x1

#define PSRAM_DEV_PAGE_SIZE_1K               0xA
#define PSRAM_DEV_PAGE_SIZE_2K               0xB

#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_XCCELA
#define PSRAM_TCEM_REFRESH_TIME              70 // ns
#elif  PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM
#define PSRAM_TCEM_REFRESH_TIME              36 // ns
#endif
#define __INNER_MICROSEC_LOW                 (1000)
#define __INNER_MICROSEC_HIGH                (10000)

// CONTROLLER CONFIGURE ******************************************************** END

// DIE CONFIGURE *************************************************************** START
#define PSRAM_MEM_32Mb_DENSITY_MAP          (0x1)
#define PSRAM_MEM_64Mb_DENSITY_MAP          (0x3)
#define PSRAM_MEM_128Mb_DENSITY_MAP         (0x5)
#define PSRAM_MEM_256Mb_DENSITY_MAP         (0x7)
#define PSRAM_MEM_512Mb_DENSITY_MAP         (0x6)

///******************************XCCELA********************** */
#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_XCCELA
// Driver strength
// DRV_CFG[3:0]
// For SIP package
// 3’b000: 8/1*36Ω
// 3’b001: 8/2*36Ω
// 3’b010: 8/3*36Ω
// 3’b011: 8/4*36Ω
// 3’b100: 8/5*36Ω
// 3’b101: 8/6*36Ω
// 3’b110: 8/7*36Ω
// 3’b111: 8/8*36Ω
#define PSRAM_CFG_IO_DRV_DQ0                 4
#define PSRAM_CFG_IO_DRV_DQ1                 4
#define PSRAM_CFG_IO_DRV_DQ2                 4
#define PSRAM_CFG_IO_DRV_DQ3                 4
#define PSRAM_CFG_IO_DRV_DQ4                 4
#define PSRAM_CFG_IO_DRV_DQ5                 4
#define PSRAM_CFG_IO_DRV_DQ6                 4
#define PSRAM_CFG_IO_DRV_DQ7                 4
#define PSRAM_CFG_IO_DRV_DQS                 4
#define PSRAM_CFG_IO_DRV_CEN                 1
#define PSRAM_CFG_IO_DRV_CLK                 4

// XCCELA
#define PSRAM_XCCELA_SYNC_READ_INS           0x00
#define PSRAM_XCCELA_SYNC_WRITE_INS          0x80
#define PSRAM_XCCELA_LINEAR_BURST_READ_INS   0x20
#define PSRAM_XCCELA_LINEAR_BURST_WRITE_INS  0xA0
#define PSRAM_XCCELA_MR_READ_INS             0x40
#define PSRAM_XCCELA_MR_WRITE_INS            0xC0
#define PSRAM_XCCELA_GLOBAL_RESET_INS        0xFF

// Mode Register Table
//******************MR0 [R/W]
// bit 0-1 MR0[1:0]
// Drive Strength Codes
// 0x0 Full (25Ω) default
// 0x1 Half (50Ω)
// 0x2 1/4  (100Ω)
// 0x3 1/8  (200Ω)
#define PSRAM_MR0_DRIVE_STR_OFFSET          (0x0)
#define PSRAM_MR0_DRIVE_STR_MASK            (0x3)
#define PSRAM_MR0_DRIVE_STR                 (0x0)

// bit 2-4 MR0[5:2]
// Read Latency Codes
#define PSRAM_MR0_READ_LAT_OFFSET           (0x2)
#define PSRAM_MR0_READ_LAT_MASK             (0x7)
#define PSRAM_MR0_READ_LAT_IN66MHZ          (0x0)
#define PSRAM_MR0_READ_LAT_IN109MHZ         (0x1)
#define PSRAM_MR0_READ_LAT_IN133MHZ         (0x2)
#define PSRAM_MR0_READ_LAT_IN166MHZ         (0x3)
#define PSRAM_MR0_READ_LAT_IN200MHZ         (0x4)
#define PSRAM_64Mb_MR0_READ_LAT_IN250MHZ    (0x5)
#define PSRAM_128Mb_MR0_READ_LAT_IN225MHZ   (0x5)
#define PSRAM_128Mb_MR0_READ_LAT_IN250MHZ   (0x6)

// bit 5 MR0[5]
// Read Latency Type
#define PSRAM_MR0_LT_OFFSET                 (0x5)
#define PSRAM_MR0_LT_MASK                   (0x1)
#define PSRAM_MR0_LT                        (0x1)

// bit 7 MR0[7]
// Temperature Sensor Override
#define PSRAM_MR0_TSO_OFFSET                (0x7)
#define PSRAM_MR0_TSO_MASK                  (0x1)
#define PSRAM_MR0_TSO                       (0x0)

//******************MR1 [R]
// bit 0-4 MR1[4:0]
// Vendor ID mapping
#define PSRAM_MR1_VENDOR_ID_OFFSET          (0x0)
#define PSRAM_MR1_VENDOR_ID_MASK            (0x1F)
#define PSRAM_MR1_VENDOR_ID                 (0xD)

// bit 7 MR1[7]
// Ultra Low Power Device mapping
#define PSRAM_MR1_ULP_OFFSET                (0x7)
#define PSRAM_MR1_ULP_MASK                  (0x1)

//******************MR2 [R]
// bit 0-2 MR2[2:0]
// Device Density mapping
#define PSRAM_MR2_DENSITY_MAP_OFFSET        (0x0)
#define PSRAM_MR2_DENSITY_MAP_MASK          (0x7)

// bit 3-4 MR2[4:3]
// Device ID
#define PSRAM_MR2_DEVICE_ID_OFFSET          (0x3)
#define PSRAM_MR2_DEVICE_ID_MASK            (0x3)

// TODO In 128Mb the good die use 3bit
// bit 5-7 MR2[7:5]
// Good-Die Bit
#define PSRAM_MR2_GOOD_DIE_OFFSET           (0x7)
#define PSRAM_MR2_GOOD_DIE_MASK             (0x1)
#define PSRAM_MR2_GOOD_DIE                  (0x1)

//******************MR3 [R]
// bit 4-5 MR3[5:4]
// Self Refresh Flag
#define PSRAM_MR3_SELF_REFRESH_OFFSET       (0x4)
#define PSRAM_MR3_SELF_REFRESH_MASK         (0x3)

// bit 7 MR3[7]
// Row Boundary Crossing Enable
#define PSRAM_MR3_RBXEN_OFFSET              (0x7)
#define PSRAM_MR3_RBXEN_MASK                (0x1)

//******************MR4 [R/W]
// bit 0-2 MR4[2:0]
// PASR
#define PSRAM_MR4_PASR_OFFSET               (0x0)
#define PSRAM_MR4_PASR_MASK                 (0x7)

// bit 3-4 MR4[4:3]
// Refresh Frequency setting
#define PSRAM_MR4_RFRATE_OFFSET             (0x3)
#define PSRAM_MR4_RFRATE_MASK               (0x3)
#define PSRAM_MR4_RFRATE_ALWAY_REFRESH      (0x0)

// bit 5-7 MR4[7:5]
// Write Latency
#define PSRAM_MR4_WRITE_LAT_OFFSET          (0x5)
#define PSRAM_MR4_WRITE_LAT_MASK            (0x7)
#define PSRAM_MR4_WRITE_LAT_IN66MHZ         (0x0)
#define PSRAM_MR4_WRITE_LAT_IN109MHZ        (0x4)
#define PSRAM_MR4_WRITE_LAT_IN133MHZ        (0x2)
#define PSRAM_MR4_WRITE_LAT_IN166MHZ        (0x6)
#define PSRAM_MR4_WRITE_LAT_IN200MHZ        (0x1)
#define PSRAM_64Mb_MR4_WRITE_LAT_IN250MHZ   (0x5)
#define PSRAM_128Mb_MR4_WRITE_LAT_IN225MHZ  (0x5)
#define PSRAM_128Mb_MR4_WRITE_LAT_IN250MHZ  (0x3)

//******************MR6 [W]
#define PSRAM_MR6_HALF_SLEEP_OFFSET         (0x4)
#define PSRAM_MR6_HALF_SLEEP_MASK           (0xf)

#define PSRAM_AHB_WR_CMD                    PSRAM_XCCELA_LINEAR_BURST_WRITE_INS
#define PSRAM_AHB_RD_CMD                    PSRAM_XCCELA_LINEAR_BURST_READ_INS
#define PSRAM_MR_WR_CMD                     PSRAM_XCCELA_MR_WRITE_INS
#define PSRAM_MR_RD_CMD                     PSRAM_XCCELA_MR_READ_INS

///******************************APM********************** */
#elif PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM
// Driver strength
// DRV_CFG[3:0]
// For SIP package
// 3’b000: 8/1*36Ω
// 3’b001: 8/2*36Ω
// 3’b010: 8/3*36Ω
// 3’b011: 8/4*36Ω
// 3’b100: 8/5*36Ω
// 3’b101: 8/6*36Ω
// 3’b110: 8/7*36Ω
// 3’b111: 8/8*36Ω
#define PSRAM_CFG_IO_DRV_DQ0               0
#define PSRAM_CFG_IO_DRV_DQ1               0
#define PSRAM_CFG_IO_DRV_DQ2               0
#define PSRAM_CFG_IO_DRV_DQ3               0
#define PSRAM_CFG_IO_DRV_DQ4               0
#define PSRAM_CFG_IO_DRV_DQ5               0
#define PSRAM_CFG_IO_DRV_DQ6               0
#define PSRAM_CFG_IO_DRV_DQ7               0
#define PSRAM_CFG_IO_DRV_DQS               0
#define PSRAM_CFG_IO_DRV_CEN               0
#define PSRAM_CFG_IO_DRV_CLK               0

// APM
#define PSRAM_APM_SYNC_READ_INS              0x00
#define PSRAM_APM_SYNC_WRITE_INS             0x80
#define PSRAM_APM_LINEAR_BURST_READ_INS      0x20
#define PSRAM_APM_LINEAR_BURST_WRITE_INS     0xA0
#define PSRAM_APM_MR_READ_INS                0x40
#define PSRAM_APM_MR_WRITE_INS               0xC0
#define PSRAM_APM_GLOBAL_RESET_INS           0xFF

// Mode Register Table
//******************MR0 [R/W]
// bit 0-1 MR0[1:0]
// Drive Strength Codes
#define PSRAM_MR0_DRIVE_STR_OFFSET          (0x0)
#define PSRAM_MR0_DRIVE_STR_MASK            (0x3)
#if PSRAM_MEM_SIZE == PSRAM_MEM_32Mb_DIE
// 0x0 1/16
// 0x1 Half
// 0x2 1/4
// 0x3 1/8 default
#define PSRAM_MR0_DRIVE_STR                 (0x1)
#elif PSRAM_MEM_SIZE == PSRAM_MEM_64Mb_DIE
// 0x0 Full (25Ω)
// 0x1 Half (50Ω)
// 0x2 1/4  (100Ω)
// 0x3 1/8  (200Ω) default
#define PSRAM_MR0_DRIVE_STR                 (0x0)
#else
#error "PSRAM APM PROTOCOL NOT SUPPORT CURRENT SIZE EXCEPT 64Mbit or 32Mbit"
#endif

// bit 2-4 MR0[5:2]
// Read Latency Codes
#define PSRAM_MR0_READ_LAT_OFFSET           (0x2)
#define PSRAM_MR0_READ_LAT_MASK             (0x7)
#define PSRAM_MR0_READ_LAT_IN66MHZ          (0x2)
#define PSRAM_MR0_READ_LAT_IN109MHZ         (0x3)
#define PSRAM_MR0_READ_LAT_IN133MHZ         (0x4)
#define PSRAM_MR0_READ_LAT_IN166MHZ         (0x5)
#define PSRAM_MR0_READ_LAT_IN200MHZ         (0x6)

// bit 5 MR0[5]
// Read Latency Type
#define PSRAM_MR0_LT_OFFSET                 (0x5)
#define PSRAM_MR0_LT_MASK                   (0x1)
#define PSRAM_MR0_LT                        (0x0)

//******************MR1 [R]
// bit 0-4 MR1[4:0]
// Vendor ID mapping
#define PSRAM_MR1_VENDOR_ID_OFFSET          (0x0)
#define PSRAM_MR1_VENDOR_ID_MASK            (0x1F)
#define PSRAM_MR1_VENDOR_ID                 (0xD)

// bit 5 MR1[5]
// Device Density mapping
#define PSRAM_MR1_DENSITY_MAP_OFFSET        (0x5)
#define PSRAM_MR1_DENSITY_MAP_MASK          (0x1)

#if PSRAM_MEM_SIZE == PSRAM_MEM_64Mb_DIE
// bit 7 MR1[7]
// Ultra Low Power Device mapping
#define PSRAM_MR1_ULP_OFFSET                (0x7)
#define PSRAM_MR1_ULP_MASK                  (0x1)
#endif
//******************MR2 [R]
// bit 0-2 MR2[2:0]
// Density2
#define PSRAM_MR2_DENSITY_MAP_OFFSET        (0x0)
#define PSRAM_MR2_DENSITY_MAP_MASK          (0x7)

// bit 3-4 MR2[4:3]
// Device ID
#define PSRAM_MR2_DEVICE_ID_OFFSET          (0x3)
#define PSRAM_MR2_DEVICE_ID_MASK            (0x3)

// bit 7 MR2[7]
// Good-Die Bit
#define PSRAM_MR2_GOOD_DIE_OFFSET           (0x7)
#define PSRAM_MR2_GOOD_DIE_MASK             (0x1)
#define PSRAM_MR2_GOOD_DIE                  (0x1)

//******************MR4 [R/W]
// bit 0-2 MR4[2:0]
// PASR
#define PSRAM_MR4_PASR_OFFSET               (0x0)
#define PSRAM_MR4_PASR_MASK                 (0x7)

// bit 3-4 MR4[4:3]
// Refresh Frequency setting
#define PSRAM_MR4_RFRATE_OFFSET             (0x3)
#define PSRAM_MR4_RFRATE_MASK               (0x1)
#define PSRAM_MR4_RFRATE_ALWAY_REFRESH      (0x0)

// bit 7 MR4[7]
// Write Latency
#define PSRAM_MR4_WRITE_LAT_OFFSET          (0x7)
#define PSRAM_MR4_WRITE_LAT_MASK            (0x1)
#define PSRAM_MR4_WRITE_LAT_WL0             (0x0)
#define PSRAM_MR4_WRITE_LAT_WL2             (0x1)

//******************MR6 [W]
#define PSRAM_MR6_HALF_SLEEP_OFFSET         (0x4)
#define PSRAM_MR6_HALF_SLEEP_MASK           (0xf)

#if PSRAM_MEM_SIZE == PSRAM_MEM_32Mb_DIE
#define PSRAM_AHB_WR_CMD                    PSRAM_APM_SYNC_WRITE_INS
#define PSRAM_AHB_RD_CMD                    PSRAM_APM_SYNC_READ_INS
#elif PSRAM_MEM_SIZE == PSRAM_MEM_64Mb_DIE
#define PSRAM_AHB_WR_CMD                    PSRAM_APM_LINEAR_BURST_WRITE_INS
#define PSRAM_AHB_RD_CMD                    PSRAM_APM_LINEAR_BURST_READ_INS
#else
#error "PSRAM APM PROTOCOL NOT SUPPORT CURRENT SIZE EXCEPT 64Mbit or 32Mbit"
#endif
#define PSRAM_MR_WR_CMD                     PSRAM_APM_MR_WRITE_INS
#define PSRAM_MR_RD_CMD                     PSRAM_APM_MR_READ_INS

#else
#error "PSRAM DIE CONFIGURE ERROR, NOT SET DIE TYPE IS XCCELA OR APM!!!"
#endif
// DIE CONFIGURE ***************************************************************END

// PSRAM Sequence instruction
#define PSRAM_INSTR_STOP                    0x0
#define PSRAM_INSTR_CMD                     0x1
#define PSRAM_INSTR_CEBLP                   0x3
#define PSRAM_INSTR_ADDR                    0x4
#define PSRAM_INSTR_MRWRDATA                0x5
#define PSRAM_INSTR_WRITE                   0x8
#define PSRAM_INSTR_WRITE16                 0x9
#define PSRAM_INSTR_READ                    0xA
#define PSRAM_INSTR_READ16                  0xB
#define PSRAM_INSTR_DUMMY                   0xC
#define PSRAM_INSTR_CMDNADDR                0xF

#define PSRAM_AHB_RD_SEQ_ID                 0
#define PSRAM_AHB_WR_SEQ_ID                 1
#define PSRAM_MR_RD_SEQ_ID                  2
#define PSRAM_MR_WR_SEQ_ID                  3

// PSRAM Controller inner parameter
#define PSRAM_EYE_DIAGRAM_TOP               (0x90)
#define PSRAM_EYE_DIAGRAM_BOTTOM            (0x3)

#if PSRAM_LOG_CHECK == 1
#define PSRAM_LOG(str, ...)  CLOGD(str, ##__VA_ARGS__)
#define PSRAM_LOGE(str, ...) CLOGE(str, ##__VA_ARGS__)
#else
#define PSRAM_LOG(str, ...)
#define PSRAM_LOGE(str, ...)
#endif

typedef struct {
    uint32_t delay_max;
    uint32_t delay_min;
    uint32_t delay_gap;
}DqsDelay_FmtDef;

static uint32_t* psram_dst_array = NULL;

static void psram_data_init(uint32_t *psram_src_data){
    unsigned int i=0;
    //init src with random data
    for (i = 0; i < PSRAM_SEARCH_DQS_NUM; i++)
    {
        psram_src_data[i] = (uint32_t)rand();
    }
}

static uint32_t psram_random_write_data_check(uint32_t *psram_src_data, uint32_t *psram_dst_data, int32_t write_delay){
    unsigned int ret = 0;
    unsigned int loop = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = write_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++){

        psram_memcpy(psram_dst_data, psram_src_data,       (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        // write back + invalidate
        HAL_FlushInvalidateDCache_by_Addr((uint32_t*)psram_dst_data, (uint32_t)((PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t)));

        ret |= psram_compare(psram_src_data, psram_dst_data, (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        psram_dst_data += (PSRAM_SEARCH_DQS_NUM / 2);
    }

    // 0 means equal
    return ret;
}

static uint32_t psram_random_read_data_check(uint32_t *psram_src_data, uint32_t *psram_dst_data, int32_t read_delay){
    unsigned int ret = 0;
    unsigned int loop = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++){
        // invalidate
        HAL_InvalidateDCache_by_Addr((uint32_t*)psram_dst_data, (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data, (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        if (ret != 0){
            break;
        }
    }

    // 0 means equal
    return ret;
}

static uint32_t psram_random_read_data_check_1(uint32_t *psram_src_data, uint32_t *psram_dst_data, int32_t read_delay){
    unsigned int ret = 0;
    unsigned int loop = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++){        

        psram_memcpy(psram_dst_data, psram_src_data,       (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        // invalidate
        HAL_FlushInvalidateDCache_by_Addr((uint32_t*)psram_dst_data, (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data, (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        if (ret != 0){
            break;
        }
    }

    // 0 means equal
    return ret;
}

static uint8_t quick_psram_search_dqs_delay(DqsDelay_FmtDef* dqs_delay, int32_t delay_reg, uint32_t *psram_src_data, uint32_t (*random_data_check_handler)(uint32_t*,  uint32_t*, int32_t)){
    unsigned int ret=0;

    uint8_t flag = 0;
    int32_t delay;
    uint8_t finded = 0;
    int32_t delay_step = PSRAM_DELAY_STEP;
    // The max of the dynamic DQS search delay
    int32_t delay_max = PSRAM_EYE_DIAGRAM_TOP;
    // The min of the dynamic DQS search delay
    int32_t delay_min = PSRAM_EYE_DIAGRAM_BOTTOM;
    uint32_t *__psram_src_data_ptr;
    uint32_t __psram_src_data[PSRAM_SEARCH_DQS_NUM];
    uint8_t mod_i = 0;

    // The max and min DQS search delay in running time
    dqs_delay->delay_max = delay_reg;
    dqs_delay->delay_min = delay_reg;

    delay = dqs_delay->delay_max;

    search_top:

    while(1){
        {
            if (psram_src_data == NULL){
                psram_data_init(__psram_src_data);

                __psram_src_data_ptr = __psram_src_data;
            } else {
                __psram_src_data_ptr = psram_src_data;
            }

            if (mod_i == 0){
                ret = random_data_check_handler(__psram_src_data_ptr, (uint32_t*)PSRAM_BASE_ADDRESS, delay);
                mod_i = 1;
            } else {
                ret = random_data_check_handler(__psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2), (uint32_t*)PSRAM_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2), delay);
                mod_i = 0;
            }
        }

        if (0 != ret){
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (1 == finded){
                delay_max = (delay - 1);

                delay = dqs_delay->delay_max + 1;
                if (delay > delay_max){
                    break;
                }

                if (1 == delay_step){
                    break;
                }

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (0 == finded){
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            }

            else {
                dqs_delay->delay_max = delay;
            }
        }

        // delay equal to delay max
        if (delay >= delay_max){
            break;
        }

        delay += delay_step;
        if (delay > delay_max){
            delay = delay_max;
        }
    }

    PSRAM_LOG("DQS Delay search: %d--%d", dqs_delay->delay_min, dqs_delay->delay_max);

    if (flag == 2){
        return finded;
    }

    if (finded == 0){
        delay_max = dqs_delay->delay_max - 1;
        flag = 1; // Need search top delay line
    }

    if ((dqs_delay->delay_min > delay_reg) && (finded == 1)){
      delay_min = delay_reg;
    }

    delay_step = PSRAM_DELAY_STEP;

    // Init delay line
    delay = dqs_delay->delay_min;

    while(1){
        {
            if (psram_src_data == NULL){
                psram_data_init(__psram_src_data);

                __psram_src_data_ptr = __psram_src_data;
            } else {
                __psram_src_data_ptr = psram_src_data;
            }

            if (mod_i == 0){
                ret = random_data_check_handler(__psram_src_data_ptr, (uint32_t*)PSRAM_BASE_ADDRESS, delay);
                mod_i = 1;
            } else {
                ret = random_data_check_handler(__psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2), (uint32_t*)PSRAM_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2), delay);
                mod_i = 0;
            }
        }

        if (0 != ret){
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (1 == finded){
                delay_min = (delay + 1);

                delay = dqs_delay->delay_min - 1;
                if (delay < delay_min){
                    break;
                }

                if (1 == delay_step){
                    break;
                }

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (0 == finded){
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            }

            else {
                dqs_delay->delay_min = delay;
            }
        }

        // delay equal to delay max
        if (delay <= delay_min){
            break;
        }

        delay -= delay_step;
        if (delay < delay_min){
            delay = delay_min;
        }
    }

    PSRAM_LOG("DQS Delay search: %d--%d", dqs_delay->delay_min, dqs_delay->delay_max);

    if (flag == 1){
        delay = dqs_delay->delay_max + 1;
        flag = 2;
        goto search_top;
    }

    return finded;
}

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

    if (g_rd_delay){
        *g_rd_delay = dqs_delay.delay_gap;
    }

    return ret;
}

int32_t PSRAM_Initialize(uint32_t* read_delay, uint32_t* write_delay, uint8_t search){
    uint32_t psram_clock = 0;
    uint32_t pre_psram_clock = 0;
    int32_t ret = 0; // Success
    uint32_t psram_src_data[PSRAM_SEARCH_DQS_NUM];
    
    IP_SYSCTRL->REG_SW_RESET_CP2.bit.PSRAM_CTRL_RESET = 0x1;

    // Enable PSRAM controller clock
    __HAL_CRM_PSRAM_CLK_ENABLE();
    // Get PSRAM controller clock
    psram_clock = CRM_GetPsramFreq();
    pre_psram_clock = psram_clock;
    PSRAM_LOG("PSRAM CLOCK: %d", psram_clock);

 #if PSRAM_DRV_STR_EN
    // Driver strength
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ0_DRV_CFG = PSRAM_CFG_IO_DRV_DQ0;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ1_DRV_CFG = PSRAM_CFG_IO_DRV_DQ1;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ2_DRV_CFG = PSRAM_CFG_IO_DRV_DQ2;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ3_DRV_CFG = PSRAM_CFG_IO_DRV_DQ3;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ4_DRV_CFG = PSRAM_CFG_IO_DRV_DQ4;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ5_DRV_CFG = PSRAM_CFG_IO_DRV_DQ5;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ6_DRV_CFG = PSRAM_CFG_IO_DRV_DQ6;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ7_DRV_CFG = PSRAM_CFG_IO_DRV_DQ7;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_DQSDM_DRV_CFG = PSRAM_CFG_IO_DRV_DQS;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_CEN_DRV_CFG = PSRAM_CFG_IO_DRV_CEN;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_CLK_DRV_CFG = PSRAM_CFG_IO_DRV_CLK;
 #endif

 #if PSRAM_RX_DIFF_EN
    PSRAM_LOG("PSRAM DIFF MODE IO");
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_DIFF_EN = 1; // DQS
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_DIFF_EN = 1;  // DM
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_DIFF_EN = 1; // CEN

    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_DIFF_EN = 1; // DQ0
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_DIFF_EN = 1; // DQ1
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_DIFF_EN = 1; // DQ2
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_DIFF_EN = 1; // DQ3
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_DIFF_EN = 1; // DQ4
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_DIFF_EN = 1; // DQ5
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_DIFF_EN = 1; // DQ6
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_DIFF_EN = 1; // DQ7

    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_COMP_EN = 0; // DQS
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_COMP_EN = 0;  // DM
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_COMP_EN = 0; // CEN

    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_COMP_EN = 0; // DQ0
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_COMP_EN = 0; // DQ1
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_COMP_EN = 0; // DQ2
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_COMP_EN = 0; // DQ3
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_COMP_EN = 0; // DQ4
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_COMP_EN = 0; // DQ5
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_COMP_EN = 0; // DQ6
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_COMP_EN = 0; // DQ7
 #else
    PSRAM_LOG("PSRAM COMP MODE IO");
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_DIFF_EN = 0; // DQS
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_DIFF_EN = 0;  // DM
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_DIFF_EN = 0; // CEN

    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_DIFF_EN = 0; // DQ0
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_DIFF_EN = 0; // DQ1
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_DIFF_EN = 0; // DQ2
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_DIFF_EN = 0; // DQ3
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_DIFF_EN = 0; // DQ4
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_DIFF_EN = 0; // DQ5
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_DIFF_EN = 0; // DQ6
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_DIFF_EN = 0; // DQ7

    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_COMP_EN = 1; // DQS
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_COMP_EN = 1;  // DM
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_COMP_EN = 1; // CEN

    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_COMP_EN = 1; // DQ0
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_COMP_EN = 1; // DQ1
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_COMP_EN = 1; // DQ2
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_COMP_EN = 1; // DQ3
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_COMP_EN = 1; // DQ4
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_COMP_EN = 1; // DQ5
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_COMP_EN = 1; // DQ6
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_COMP_EN = 1; // DQ7
 #endif


    // MR RD SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_MR_RD_CMD);
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S2LUT1.bit.S2_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x4);

    // MR WR SEQ ID
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_MR_WR_CMD);
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S3LUT1.bit.S3_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);

    // MR RESET
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, 0xFF);
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, 2);

    // TCEM configure
    {
        uint32_t tcem_para = 0;
        tcem_para = (uint32_t)(((pre_psram_clock / __INNER_MICROSEC_LOW) * PSRAM_TCEM_REFRESH_TIME) / __INNER_MICROSEC_HIGH);
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCEM_CFG = tcem_para;
        PSRAM_LOG("PSRAM TCEM parameter: %d", tcem_para);
    }
    // TCPH configure
    {
        uint32_t tcph_para = 4;
        if (pre_psram_clock <= 200000000){
            tcph_para = 4;
        } else if (pre_psram_clock <= 250000000){
            tcph_para = 8;
        }
        IP_PSRAM_CTRL->REG_TIMCFG.bit.TCPH_CFG = 8;
        PSRAM_LOG("PSRAM TCPH parameter: %d", tcph_para);
    }

    // Prefetch
    {
#if PSRAM_PREFETCH_EN
        // IP_PSRAM_CTRL->REG_RDWRCTRL.bit.RD_LOOKUP_CMD_FOR_CONTD_RD_ONLY = 0;

        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF0_MSTRID = PSRAM_PREFETCH_FIFO0_HM;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF1_MSTRID = PSRAM_PREFETCH_FIFO1_HM;

        IP_PSRAM_CTRL->REG_RXBUFPFEN.bit.RXBUF0_PREFETCH_EN = PSRAM_PREFETCH_FIFO0_HM_EN;
        IP_PSRAM_CTRL->REG_RXBUFPFEN.bit.RXBUF1_PREFETCH_EN = PSRAM_PREFETCH_FIFO1_HM_EN;
#endif
    }


    // Controller register configure end #########################################

    // Set low psram clock
    uint32_t div_m, p_div_m;
    HAL_CRM_GetPsramClkConfig(&p_div_m);
    div_m = p_div_m;
    while(psram_clock > 24000000){
        div_m *= 2;
        HAL_CRM_SetPsramClkDiv(div_m);
        psram_clock = CRM_GetPsramFreq();
    }

    PSRAM_LOG("PSRAM REDUCE TO %d", psram_clock);

    // DLL lock program start ####################################################
    {
        uint8_t rdata = 0;

        IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_BYPASS = 0x0;
        IP_PSRAM_CTRL->REG_DLLEN.bit.PHASE_DETECT_SEL = 0x1;
        IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN =1;
        IP_PSRAM_CTRL->REG_DLLRST.all = 1;
        PSRAM_LOG("Wait PSRAM DLL done\n\n\n");
        do{
          rdata = IP_PSRAM_CTRL->REG_LOCKDONE.all;
        }while(rdata != 1);
        PSRAM_LOG("DLL lock!!!");
        // The PSRAM DLL measures how many internal fixed delay cells are required for one clock cycle of a PSRAM.
        // If a whole clock cycle is measured, 1/4 can be obtained by dividing the delay cell by 4.
        // If the clock cycle is too large, only half a cycle can be measured, therefore the HALF_CLOCK_MODE is set, adopting a 1/2 bit coefficient.
        uint32_t div = IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.HALF_CLOCK_MODE;
        if (div){
            div = 2;
        } else{
            div = 4;
        }
        PSRAM_LOG("DLL lock value: %d, lock div: %d", IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.DLL_LOCK_VALUE, div);

        // resync DLL (0x28)
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;
    }
    // DLL lock program end ####################################################

    uint8_t density = 0;

    // MR1 & MR2 & MR3
    {
        uint8_t mr_r_value_m1 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);
        uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);
        uint8_t mr_r_value_m3 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR3.all & 0xff);

        if (((mr_r_value_m2 >> PSRAM_MR2_GOOD_DIE_OFFSET) & PSRAM_MR2_GOOD_DIE_MASK) == PSRAM_MR2_GOOD_DIE){
            PSRAM_LOG("\n\n\nGOOD DIE\n\n\n");
        } else {
            PSRAM_LOGE("\n\n\nBAD DIE\n\n\n");
            return -1;
        }

        PSRAM_LOG("Vendor ID: 0x%x;     Dev ID: 0x%x", ((mr_r_value_m1 >> PSRAM_MR1_VENDOR_ID_OFFSET) & PSRAM_MR1_VENDOR_ID_MASK),\
                ((mr_r_value_m2 >> PSRAM_MR2_DEVICE_ID_OFFSET) & PSRAM_MR2_DEVICE_ID_MASK));

        density = (mr_r_value_m2 >> PSRAM_MR2_DENSITY_MAP_OFFSET) & PSRAM_MR2_DENSITY_MAP_MASK;
        if (density == PSRAM_MEM_32Mb_DENSITY_MAP){
            PSRAM_LOG("PSRAM Density -> 32M bit");
        } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP){
            PSRAM_LOG("PSRAM Density -> 64M bit");
        } else if (density == PSRAM_MEM_128Mb_DENSITY_MAP){
            PSRAM_LOG("PSRAM Density -> 128M bit");
        } else if (density == PSRAM_MEM_256Mb_DENSITY_MAP){
            PSRAM_LOG("PSRAM Density -> 256M bit");
        } else if (density == PSRAM_MEM_512Mb_DENSITY_MAP){
            PSRAM_LOG("PSRAM Density -> 512M bit");
        }
    }

#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_XCCELA
    // XCCELA DIE AHB READ, AHB WRITE, MR READ AND MR WRITE LOGIC PARAMETER
    {
        uint8_t write_dummy = 0;

        if (pre_psram_clock <= 66000000) // 66Mhz
        {
            write_dummy = 1;
        }

        else if (pre_psram_clock <= 109000000) // 109Mhz
        {
            write_dummy = 2;
        }

        else if (pre_psram_clock <= 133000000) // 133Mhz
        {
            write_dummy = 3;
        }

        else if (pre_psram_clock <= 166000000) // 166Mhz
        {
            write_dummy = 4;
        }

        else if (pre_psram_clock <= 200000000){ // 200Mhz
            write_dummy = 5;
        }

        else{
            if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
                if (pre_psram_clock <= 225000000){ // 225Mhz
                    write_dummy = 6;
                }

                else if (pre_psram_clock <= 250000000){ // 250Mhz
                    write_dummy = 7;
                }
            } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
                if (pre_psram_clock <= 250000000){ // 250Mhz
                    write_dummy = 6;
                }
            }
        }

        PSRAM_LOG("Write dummy: %d", write_dummy);

        // AHB RD SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_AHB_RD_SEQ_ID;
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_AHB_RD_CMD); // CMD
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);  // ADDR
        IP_PSRAM_CTRL->REG_S0LUT1.bit.S0_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x0);  // READ

        // AHB WR SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_AHB_WR_SEQ_ID;
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_AHB_WR_CMD);
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR3 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
    }

    // APM DIE AHB READ, AHB WRITE, MR READ AND MR WRITE LOGIC PARAMETER
#elif PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM
    {
        uint8_t write_dummy = 0;

        if (pre_psram_clock >= 166000000){ // 200Mhz
            write_dummy = 1;
        }
        else {
            write_dummy = 0;
        }

        // AHB RD SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_AHB_RD_SEQ_ID;
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_AHB_RD_CMD); // CMD
        IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x5);  // READ

        // AHB WR SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_AHB_WR_SEQ_ID;
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_AHB_WR_CMD);
        IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
        IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 = PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);

        // MR RD SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
        IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_MR_RD_CMD);
        IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x2);

        // MR WR SEQ ID
        IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
        IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 = PSRAM_INSTR_MARCO(PSRAM_INSTR_CMDNADDR, PSRAM_MR_WR_CMD);
        IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 = PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);
    }
#else
// APM
#error "PSRAM DIE CONFIGURE ERROR, NOT SET DIE TYPE IS XCCELA OR APM!!!"
#endif

    IP_PSRAM_CTRL->REG_RDWRCTRL.bit.RD_LOOKUP_TX_BUF = 0;

    // Controller register configure start #######################################
    {
        // XCCELA Configure
#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_XCCELA
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_XCELLA; // Set type to xcella
        // Set device page size
        if ((density == PSRAM_MEM_32Mb_DENSITY_MAP) || (density == PSRAM_MEM_64Mb_DENSITY_MAP)){
            IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_1K;
        } else {
            IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_2K;
        }

        // APM Configure
#elif PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_APM; // set type to apm
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_1K;
#else
// APM
#error "PSRAM DIE CONFIGURE ERROR, NOT SET DIE TYPE IS XCCELA OR APM!!!"
#endif

    }

    // MR configure start ######################################################
#if PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_XCCELA
    {
        uint8_t mr_w_value = 0, mr_r_value = 0;
        uint8_t i = 100;

        // MR0
        {
            if ((density == PSRAM_MEM_128Mb_DENSITY_MAP) || (density == PSRAM_MEM_64Mb_DENSITY_MAP)){
                mr_w_value = ((PSRAM_MR0_LT << PSRAM_MR0_LT_OFFSET) | (PSRAM_MR0_DRIVE_STR << PSRAM_MR0_DRIVE_STR_OFFSET));
            }
            else{
                mr_w_value = ((PSRAM_MR0_TSO << PSRAM_MR0_TSO_OFFSET) | (PSRAM_MR0_LT << PSRAM_MR0_LT_OFFSET) | (PSRAM_MR0_DRIVE_STR << PSRAM_MR0_DRIVE_STR_OFFSET));
            }            

            if (pre_psram_clock <= 66000000) // 66Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN66MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 109000000) // 109Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN109MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 133000000) // 133Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN133MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 166000000) // 166Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN166MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 200000000){ // 200Mhz
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN200MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else {
                if (density == PSRAM_MEM_128Mb_DENSITY_MAP){
                    if (pre_psram_clock <= 225000000){ // 225Mhz
                        mr_w_value |= (PSRAM_128Mb_MR0_READ_LAT_IN225MHZ << PSRAM_MR0_READ_LAT_OFFSET);
                    }

                    else if (pre_psram_clock <= 250000000){ // 250Mhz
                        mr_w_value |= (PSRAM_128Mb_MR0_READ_LAT_IN250MHZ << PSRAM_MR0_READ_LAT_OFFSET);
                    }
                } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP){
                    if (pre_psram_clock <= 250000000){ // 250Mhz
                        mr_w_value |= (PSRAM_64Mb_MR0_READ_LAT_IN250MHZ << PSRAM_MR0_READ_LAT_OFFSET);
                    }
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

            mr_w_value = (PSRAM_MR4_RFRATE_ALWAY_REFRESH << PSRAM_MR4_RFRATE_OFFSET);

            if (pre_psram_clock <= 66000000) // 66Mhz
            {
                mr_w_value |= (PSRAM_MR4_WRITE_LAT_IN66MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 109000000) // 109Mhz
            {
                mr_w_value |= (PSRAM_MR4_WRITE_LAT_IN109MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 133000000) // 133Mhz
            {
                mr_w_value |= (PSRAM_MR4_WRITE_LAT_IN133MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 166000000) // 166Mhz
            {
                mr_w_value |= (PSRAM_MR4_WRITE_LAT_IN166MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 200000000){ // 200Mhz
                mr_w_value |= (PSRAM_MR4_WRITE_LAT_IN200MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
            }

            else {
                if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
                    if (pre_psram_clock <= 225000000){ // 225Mhz
                        mr_w_value |= (PSRAM_128Mb_MR4_WRITE_LAT_IN225MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
                    } else if (pre_psram_clock <= 250000000){ // 250Mhz
                        mr_w_value |= (PSRAM_128Mb_MR4_WRITE_LAT_IN250MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
                    }
                } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
                    if (pre_psram_clock <= 250000000){ // 250Mhz
                        mr_w_value |= (PSRAM_64Mb_MR4_WRITE_LAT_IN250MHZ << PSRAM_MR4_WRITE_LAT_OFFSET);
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
#elif PSRAM_DIE_TYPE == PSRAM_DIE_TYPE_APM
    {
        uint8_t mr_w_value = 0, mr_r_value = 0;
        uint8_t i = 100;

        // MR0
        {
            mr_w_value = ((PSRAM_MR0_LT << PSRAM_MR0_LT_OFFSET) | (PSRAM_MR0_DRIVE_STR << PSRAM_MR0_DRIVE_STR_OFFSET));

            if (pre_psram_clock >= 200000000){ // 200Mhz
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN200MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 66000000) // 66Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN66MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 109000000) // 109Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN109MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 133000000) // 133Mhz
            {
                mr_w_value |= (PSRAM_MR0_READ_LAT_IN133MHZ << PSRAM_MR0_READ_LAT_OFFSET);
            }

            else if (pre_psram_clock <= 166000000) // 166Mhz
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

        // MR1 & MR2 & MR3
        {
            uint8_t mr_r_value_m1 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR1.all & 0xff);
            uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);
            uint8_t mr_r_value_m3 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR3.all & 0xff);

            if (((mr_r_value_m2 >> PSRAM_MR2_GOOD_DIE_OFFSET) & PSRAM_MR2_GOOD_DIE_MASK) == PSRAM_MR2_GOOD_DIE){
                PSRAM_LOG("\n\n\nGOOD DIE\n\n\n");
            } else {
                PSRAM_LOGE("\n\n\nBAD DIE\n\n\n");
                return -1;
            }

            PSRAM_LOG("Vendor ID: 0x%x;     Dev ID: 0x%x", ((mr_r_value_m1 >> PSRAM_MR1_VENDOR_ID_OFFSET) & PSRAM_MR1_VENDOR_ID_MASK),\
                    ((mr_r_value_m2 >> PSRAM_MR2_DEVICE_ID_OFFSET) & PSRAM_MR2_DEVICE_ID_MASK));

            uint8_t density = 0;
            density = ((mr_r_value_m1 >> PSRAM_MR1_DENSITY_MAP_OFFSET) & PSRAM_MR1_DENSITY_MAP_MASK) | \
                    ((mr_r_value_m2 >> PSRAM_MR2_DENSITY_MAP_OFFSET) & PSRAM_MR2_DENSITY_MAP_MASK);
            if (density == PSRAM_MEM_32Mb_DENSITY_MAP){
                PSRAM_LOG("PSRAM Density -> 32M bit");
            } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP){
                PSRAM_LOG("PSRAM Density -> 64M bit");
            }
        }

        // MR4
        {
            i = 100;

            mr_w_value = (PSRAM_MR4_RFRATE_ALWAY_REFRESH << PSRAM_MR4_RFRATE_OFFSET);

            if (pre_psram_clock >= 166000000){ // 166Mhz
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
#else
// APM
#error "PSRAM DIE CONFIGURE ERROR, NOT SET DIE TYPE IS XCCELA OR APM!!!"
#endif

    
    // MR configure end ########################################################

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
        for (int j = 0; j < 300; j++)   
            __NOP();    
    }

    // Restore clock
    HAL_CRM_SetPsramClkDiv(p_div_m); // TODO comment only for FPGA

    PSRAM_LOG("PSRAM RECOVER TO %d", CRM_GetPsramFreq());

    {
        uint8_t rdata = 0;

        IP_PSRAM_CTRL->REG_DLLRST.all = 1;
        PSRAM_LOG("Wait PSRAM DLL done\n\n\n");
        do{
          rdata = IP_PSRAM_CTRL->REG_LOCKDONE.all;
        }while(rdata != 1);
        PSRAM_LOG("DLL lock!!!");
        // The PSRAM DLL measures how many internal fixed delay cells are required for one clock cycle of a PSRAM.
        // If a whole clock cycle is measured, 1/4 can be obtained by dividing the delay cell by 4.
        // If the clock cycle is too large, only half a cycle can be measured, therefore the HALF_CLOCK_MODE is set, adopting a 1/2 bit coefficient.
        uint32_t div = IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.HALF_CLOCK_MODE;
        if (div){
            div = 2;
        } else{
            div = 4;
        }
        PSRAM_LOG("DLL lock value: %d, lock div: %d", IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.DLL_LOCK_VALUE, div);

        // resync DLL (0x28)
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;
    }

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

uint32_t PSRAM_GetDensity(void){
    uint8_t mr_r_value_m2 = (uint8_t)((uint32_t)IP_PSRAM_CTRL->REG_MR2.all & 0xff);
    
    uint8_t density = (mr_r_value_m2 >> PSRAM_MR2_DENSITY_MAP_OFFSET) & PSRAM_MR2_DENSITY_MAP_MASK;

    if (density == PSRAM_MEM_32Mb_DENSITY_MAP){
        return PSRAM_MEM_32Mb_DIE;
    } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP){
        return PSRAM_MEM_64Mb_DIE;
    } else if (density == PSRAM_MEM_128Mb_DENSITY_MAP){
        return PSRAM_MEM_128Mb_DIE;
    } else if (density == PSRAM_MEM_256Mb_DENSITY_MAP){
        return PSRAM_MEM_256Mb_DIE;
    } else if (density == PSRAM_MEM_512Mb_DENSITY_MAP){
        return PSRAM_MEM_512Mb_DIE;
    }

    return 0;
}
