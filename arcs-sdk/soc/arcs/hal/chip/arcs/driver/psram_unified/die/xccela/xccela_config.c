#include "xccela_config.h"

#define XCCELA_PSRAM_MEM_32Mb_DENSITY_MAP       0x1
#define XCCELA_PSRAM_MEM_64Mb_DENSITY_MAP       0x3
#define XCCELA_PSRAM_MEM_128Mb_DENSITY_MAP      0x5
#define XCCELA_PSRAM_MEM_256Mb_DENSITY_MAP      0x7
#define XCCELA_PSRAM_MEM_512Mb_DENSITY_MAP      0x6

#define PSRAM_XCCELA_SYNC_READ_INS              0x00
#define PSRAM_XCCELA_SYNC_WRITE_INS             0x80
#define PSRAM_XCCELA_LINEAR_BURST_READ_INS      0x20
#define PSRAM_XCCELA_LINEAR_BURST_WRITE_INS     0xA0
#define PSRAM_XCCELA_MR_READ_INS                0x40
#define PSRAM_XCCELA_MR_WRITE_INS               0xC0
#define PSRAM_XCCELA_GLOBAL_RESET_INS           0xFF

#define PSRAM_XCCELA_MR0_DRIVE_STR_OFFSET       0x0
#define PSRAM_XCCELA_MR0_DRIVE_STR              0x0
#define PSRAM_XCCELA_MR0_READ_LAT_OFFSET        0x2
#define PSRAM_XCCELA_MR0_READ_LAT_IN66MHZ       0x0
#define PSRAM_XCCELA_MR0_READ_LAT_IN109MHZ      0x1
#define PSRAM_XCCELA_MR0_READ_LAT_IN133MHZ      0x2
#define PSRAM_XCCELA_MR0_READ_LAT_IN166MHZ      0x3
#define PSRAM_XCCELA_MR0_READ_LAT_IN200MHZ      0x4
#define PSRAM_XCCELA_64Mb_MR0_READ_LAT_IN250MHZ 0x5
#define PSRAM_XCCELA_128Mb_MR0_READ_LAT_IN225MHZ 0x5
#define PSRAM_XCCELA_128Mb_MR0_READ_LAT_IN250MHZ 0x6
#define PSRAM_XCCELA_MR0_LT_OFFSET              0x5
#define PSRAM_XCCELA_MR0_LT                     0x1
#define PSRAM_XCCELA_MR0_TSO_OFFSET             0x7
#define PSRAM_XCCELA_MR0_TSO                    0x0

#define PSRAM_XCCELA_MR1_VENDOR_ID_OFFSET       0x0
#define PSRAM_XCCELA_MR1_VENDOR_ID_MASK         0x1F
#define PSRAM_XCCELA_MR1_VENDOR_ID              0xD

#define PSRAM_XCCELA_MR2_DENSITY_MAP_OFFSET     0x0
#define PSRAM_XCCELA_MR2_DENSITY_MAP_MASK       0x7
#define PSRAM_XCCELA_MR2_DEVICE_ID_OFFSET       0x3
#define PSRAM_XCCELA_MR2_DEVICE_ID_MASK         0x3
#define PSRAM_XCCELA_MR2_DEVICE_ID_64Mb         0x2
#define PSRAM_XCCELA_MR2_DEVICE_ID_128Mb        0x3
#define PSRAM_XCCELA_MR2_GOOD_DIE_OFFSET        0x7
#define PSRAM_XCCELA_MR2_GOOD_DIE_MASK          0x1
#define PSRAM_XCCELA_MR2_GOOD_DIE               0x1
#define PSRAM_XCCELA_MR2_GOOD_DIE_128Mb_OFFSET  0x5
#define PSRAM_XCCELA_MR2_GOOD_DIE_128Mb_MASK    0x7
#define PSRAM_XCCELA_MR2_GOOD_DIE_128Mb         0x6

#define PSRAM_XCCELA_MR4_PASR_OFFSET            0x0
#define PSRAM_XCCELA_MR4_RFRATE_OFFSET          0x3
#define PSRAM_XCCELA_MR4_RFRATE_SLOW_REFRESH    0x1
#define PSRAM_XCCELA_MR4_RFRATE_ALWAY_REFRESH   0x0
#define PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET       0x5
#define PSRAM_XCCELA_MR4_WRITE_LAT_IN66MHZ      0x0
#define PSRAM_XCCELA_MR4_WRITE_LAT_IN109MHZ     0x4
#define PSRAM_XCCELA_MR4_WRITE_LAT_IN133MHZ     0x2
#define PSRAM_XCCELA_MR4_WRITE_LAT_IN166MHZ     0x6
#define PSRAM_XCCELA_MR4_WRITE_LAT_IN200MHZ     0x1
#define PSRAM_XCCELA_64Mb_MR4_WRITE_LAT_IN250MHZ 0x5
#define PSRAM_XCCELA_128Mb_MR4_WRITE_LAT_IN225MHZ 0x5
#define PSRAM_XCCELA_128Mb_MR4_WRITE_LAT_IN250MHZ 0x3

#define PSRAM_XCCELA_MR6_HALF_SLEEP_MODE        0xf0
#define PSRAM_XCCELA_MR6_DEEP_SLEEP_MODE        0xc0

#define PSRAM_XCCELA_TCEM_REFRESH_TIME          70

void __psram_xccela_mr_seq_configure(void)
{
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_MR_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR0 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_MR_READ_INS);
    IP_PSRAM_CTRL->REG_S2LUT0.bit.S2_INSTR1 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S2LUT1.bit.S2_INSTR2 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x4);

    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR0 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_MR_WRITE_INS);
    IP_PSRAM_CTRL->REG_S3LUT0.bit.S3_INSTR1 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S3LUT1.bit.S3_INSTR2 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_MRWRDATA, 0x0);

    IP_PSRAM_CTRL->REG_S5LUT0.all =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);

    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR0 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_GLOBAL_RESET_INS);
    IP_PSRAM_CTRL->REG_S6LUT0.bit.S6_INSTR1 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, 2);
}

void __psram_xccela_ahb_seq_configure(uint32_t clock_freq, uint32_t density)
{
    uint8_t write_dummy = 0;

    if (clock_freq <= 66000000UL) {
        write_dummy = 1;
    } else if (clock_freq <= 109000000UL) {
        write_dummy = 2;
    } else if (clock_freq <= 133000000UL) {
        write_dummy = 3;
    } else if (clock_freq <= 166000000UL) {
        write_dummy = 4;
    } else if (clock_freq <= 200000000UL) {
        write_dummy = 5;
    } else if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
        if (clock_freq <= 225000000UL) {
            write_dummy = 6;
        } else if (clock_freq <= 250000000UL) {
            write_dummy = 7;
        }
    } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
        if (clock_freq <= 250000000UL) {
            write_dummy = 6;
        }
    }

    PSRAM_LOG("[Xccela] write dummy: %d", write_dummy);

    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_AHB_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR0 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_LINEAR_BURST_READ_INS);
    IP_PSRAM_CTRL->REG_S0LUT0.bit.S0_INSTR1 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S0LUT1.bit.S0_INSTR2 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_READ, 0x0);

    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_AHB_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR0 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CMD, PSRAM_XCCELA_LINEAR_BURST_WRITE_INS);
    IP_PSRAM_CTRL->REG_S1LUT0.bit.S1_INSTR1 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_ADDR, 0x1);
    IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR2 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_DUMMY, write_dummy);
    IP_PSRAM_CTRL->REG_S1LUT1.bit.S1_INSTR3 =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_WRITE, 0x0);
}

void __psram_xccela_controller_die_type(void)
{
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_XCCELA;
}

void __psram_xccela_controller_die_page_size(uint32_t density)
{
    if ((density == PSRAM_MEM_32Mb_DENSITY_MAP) ||
        (density == PSRAM_MEM_64Mb_DENSITY_MAP)) {
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_1K;
    } else {
        IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_2K;
    }
}

void __psram_xccela_controller_timing_configure(uint32_t clock_freq)
{
    uint32_t tcem_para =
        (uint32_t)(((clock_freq / __INNER_MICROSEC_LOW) *
                    PSRAM_XCCELA_TCEM_REFRESH_TIME) /
                   __INNER_MICROSEC_HIGH);
    uint32_t tcph_para = 4;

    if (clock_freq <= 200000000UL) {
        tcph_para = 4;
    } else if (clock_freq <= 250000000UL) {
        tcph_para = 8;
    }

    IP_PSRAM_CTRL->REG_TIMCFG.bit.TCEM_CFG = tcem_para;
    IP_PSRAM_CTRL->REG_TIMCFG.bit.TCPH_CFG = 8;

    PSRAM_LOG("PSRAM TCEM parameter: %d", tcem_para);
    PSRAM_LOG("PSRAM TCPH parameter: %d", tcph_para);
}

static int32_t xccela_density_to_bytes(uint8_t raw_density, uint32_t *density)
{
    if (raw_density == XCCELA_PSRAM_MEM_32Mb_DENSITY_MAP) {
        PSRAM_LOG("PSRAM Density -> 32M bit");
        *density = PSRAM_MEM_32Mb_DENSITY_MAP;
    } else if (raw_density == XCCELA_PSRAM_MEM_64Mb_DENSITY_MAP) {
        PSRAM_LOG("PSRAM Density -> 64M bit");
        *density = PSRAM_MEM_64Mb_DENSITY_MAP;
    } else if (raw_density == XCCELA_PSRAM_MEM_128Mb_DENSITY_MAP) {
        PSRAM_LOG("PSRAM Density -> 128M bit");
        *density = PSRAM_MEM_128Mb_DENSITY_MAP;
    } else if (raw_density == XCCELA_PSRAM_MEM_256Mb_DENSITY_MAP) {
        PSRAM_LOG("PSRAM Density -> 256M bit");
        *density = PSRAM_MEM_256Mb_DENSITY_MAP;
    } else if (raw_density == XCCELA_PSRAM_MEM_512Mb_DENSITY_MAP) {
        PSRAM_LOG("PSRAM Density -> 512M bit");
        *density = PSRAM_MEM_512Mb_DENSITY_MAP;
    } else {
        PSRAM_LOGE("PSRAM Density -> Unknown 0x%x", raw_density);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

int32_t __psram_xccela_info_extra(uint32_t *density)
{
    if (density == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t mr1 = (uint8_t)(IP_PSRAM_CTRL->REG_MR1.all & 0xff);
    uint8_t mr2 = (uint8_t)(IP_PSRAM_CTRL->REG_MR2.all & 0xff);
    uint8_t vendor_id =
        (mr1 >> PSRAM_XCCELA_MR1_VENDOR_ID_OFFSET) &
        PSRAM_XCCELA_MR1_VENDOR_ID_MASK;
    uint8_t device_id =
        (mr2 >> PSRAM_XCCELA_MR2_DEVICE_ID_OFFSET) &
        PSRAM_XCCELA_MR2_DEVICE_ID_MASK;

    if (vendor_id != PSRAM_XCCELA_MR1_VENDOR_ID) {
        PSRAM_LOGE("Unknown Xccela vendor ID: 0x%x", vendor_id);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    PSRAM_LOG("Vendor ID: 0x%x; Dev ID: 0x%x", vendor_id, device_id);

    if (device_id == PSRAM_XCCELA_MR2_DEVICE_ID_128Mb) {
        uint8_t gdie =
            (mr2 >> PSRAM_XCCELA_MR2_GOOD_DIE_128Mb_OFFSET) &
            PSRAM_XCCELA_MR2_GOOD_DIE_128Mb_MASK;
        if (gdie != PSRAM_XCCELA_MR2_GOOD_DIE_128Mb) {
            PSRAM_LOGE("BAD DIE");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
    } else if (device_id == PSRAM_XCCELA_MR2_DEVICE_ID_64Mb) {
        uint8_t gdie =
            (mr2 >> PSRAM_XCCELA_MR2_GOOD_DIE_OFFSET) &
            PSRAM_XCCELA_MR2_GOOD_DIE_MASK;
        if (gdie != PSRAM_XCCELA_MR2_GOOD_DIE) {
            PSRAM_LOGE("BAD DIE");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
    } else {
        PSRAM_LOGE("PSRAM Device ID Error: 0x%x", device_id);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    PSRAM_LOG("GOOD DIE");

    uint8_t raw_density =
        (mr2 >> PSRAM_XCCELA_MR2_DENSITY_MAP_OFFSET) &
        PSRAM_XCCELA_MR2_DENSITY_MAP_MASK;
    return xccela_density_to_bytes(raw_density, density);
}

void __psram_xccela_die_para_configure(uint32_t clock_freq, uint32_t density)
{
    uint8_t mr_w_value = 0;
    uint8_t mr_r_value = 0;
    uint8_t i = 100;

    if ((density == PSRAM_MEM_128Mb_DENSITY_MAP) ||
        (density == PSRAM_MEM_64Mb_DENSITY_MAP)) {
        mr_w_value =
            (PSRAM_XCCELA_MR0_LT << PSRAM_XCCELA_MR0_LT_OFFSET) |
            (PSRAM_XCCELA_MR0_DRIVE_STR << PSRAM_XCCELA_MR0_DRIVE_STR_OFFSET);
    } else {
        mr_w_value =
            (PSRAM_XCCELA_MR0_TSO << PSRAM_XCCELA_MR0_TSO_OFFSET) |
            (PSRAM_XCCELA_MR0_LT << PSRAM_XCCELA_MR0_LT_OFFSET) |
            (PSRAM_XCCELA_MR0_DRIVE_STR << PSRAM_XCCELA_MR0_DRIVE_STR_OFFSET);
    }

    if (clock_freq <= 66000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR0_READ_LAT_IN66MHZ <<
             PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
    } else if (clock_freq <= 109000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR0_READ_LAT_IN109MHZ <<
             PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
    } else if (clock_freq <= 133000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR0_READ_LAT_IN133MHZ <<
             PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
    } else if (clock_freq <= 166000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR0_READ_LAT_IN166MHZ <<
             PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
    } else if (clock_freq <= 200000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR0_READ_LAT_IN200MHZ <<
             PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
    } else if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
        if (clock_freq <= 225000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_128Mb_MR0_READ_LAT_IN225MHZ <<
                 PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
        } else if (clock_freq <= 250000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_128Mb_MR0_READ_LAT_IN250MHZ <<
                 PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
        }
    } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
        if (clock_freq <= 250000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_64Mb_MR0_READ_LAT_IN250MHZ <<
                 PSRAM_XCCELA_MR0_READ_LAT_OFFSET);
        }
    }

    while (--i) {
        mr_r_value = (uint8_t)(IP_PSRAM_CTRL->REG_MR0.all & 0xff);
        if (mr_r_value == mr_w_value) {
            break;
        }
        IP_PSRAM_CTRL->REG_MR0.all = mr_w_value;
    }

    if (i == 0) {
        PSRAM_LOGE("PSRAM MR0 configure error");
    }

    i = 100;
    mr_w_value =
        (PSRAM_XCCELA_MR4_RFRATE_ALWAY_REFRESH <<
         PSRAM_XCCELA_MR4_RFRATE_OFFSET);

    if (clock_freq <= 66000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR4_WRITE_LAT_IN66MHZ <<
             PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
    } else if (clock_freq <= 109000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR4_WRITE_LAT_IN109MHZ <<
             PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
    } else if (clock_freq <= 133000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR4_WRITE_LAT_IN133MHZ <<
             PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
    } else if (clock_freq <= 166000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR4_WRITE_LAT_IN166MHZ <<
             PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
    } else if (clock_freq <= 200000000UL) {
        mr_w_value |=
            (PSRAM_XCCELA_MR4_WRITE_LAT_IN200MHZ <<
             PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
    } else if (density == PSRAM_MEM_128Mb_DENSITY_MAP) {
        if (clock_freq <= 225000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_128Mb_MR4_WRITE_LAT_IN225MHZ <<
                 PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
        } else if (clock_freq <= 250000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_128Mb_MR4_WRITE_LAT_IN250MHZ <<
                 PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
        }
    } else if (density == PSRAM_MEM_64Mb_DENSITY_MAP) {
        if (clock_freq <= 250000000UL) {
            mr_w_value |=
                (PSRAM_XCCELA_64Mb_MR4_WRITE_LAT_IN250MHZ <<
                 PSRAM_XCCELA_MR4_WRITE_LAT_OFFSET);
        }
    }

    while (--i) {
        mr_r_value = (uint8_t)(IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        if (mr_r_value == mr_w_value) {
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }

    if (i == 0) {
        PSRAM_LOGE("PSRAM MR4 configure error");
    }
}

void __psram_xccela_mr_print(void)
{
    PSRAM_LOG("MR0: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR0.all & 0xff));
    PSRAM_LOG("MR1: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR1.all & 0xff));
    PSRAM_LOG("MR2: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR2.all & 0xff));
    PSRAM_LOG("MR3: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR3.all & 0xff));
    PSRAM_LOG("MR4: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR4.all & 0xff));
    PSRAM_LOG("MR6: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR6.all & 0xff));
    PSRAM_LOG("MR8: 0x%x", (uint8_t)(IP_PSRAM_CTRL->REG_MR8.all & 0xff));
}

int32_t __psram_xccela_enter_sleep_mode(__psram_unified_sleep_mode_t sleep_mode)
{
    if (sleep_mode == __psram_unified_sleep_mode_half_sleep) {
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_XCCELA_MR6_HALF_SLEEP_MODE;
    } else if (sleep_mode == __psram_unified_sleep_mode_deep_sleep) {
        IP_PSRAM_CTRL->REG_MR6.all = PSRAM_XCCELA_MR6_DEEP_SLEEP_MODE;
    } else {
        PSRAM_LOGE("PSRAM sleep mode error");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return CSK_DRIVER_OK;
}

void __psram_xccela_exit_sleep_mode(void)
{
    IP_PSRAM_CTRL->REG_S5LUT0.all =
        PSRAM_INSTR_MARCO(PSRAM_INSTR_CEBLP, 12);

    uint32_t hclk = CRM_GetHclkFreq();
    uint32_t timeout = 30 * (hclk / 24000000UL);
    if (timeout == 0) {
        timeout = 30;
    }

    IP_PSRAM_CTRL->REG_CUSTEXE.all = PSRAM_EXE_SLEEP_MODE_SEQ_ID;
    while (timeout--) {
        __NOP();
    }
}

void __psram_xccela_refresh_rate_normal_set(void)
{
    uint8_t mr_r_value = 0;
    uint8_t mr_w_value = 0;
    uint8_t i = 100;

    while (--i) {
        mr_r_value = (uint8_t)(IP_PSRAM_CTRL->REG_MR4.all & 0xff);
        mr_w_value =
            mr_r_value &
            ((~(PSRAM_XCCELA_MR4_RFRATE_SLOW_REFRESH <<
                PSRAM_XCCELA_MR4_RFRATE_OFFSET)) & 0xff);
        if (mr_r_value == mr_w_value) {
            break;
        }
        IP_PSRAM_CTRL->REG_MR4.all = mr_w_value;
    }
}
