/**
 ****************************************************************************************
 *
 * @file ls_efuse.c
 *
 * @brief temperature functions implement.
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 **/
#include <stdbool.h>
#include "log_print.h"
#include "arcs_ap.h"
#include "rf_drv.h"
#include "Driver_EFUSE.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

#define __HAL_EFUSE_CLK_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SEL_EFUSE_CLK = 0x1; \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1; \
} while(0)

#define __HAL_EFUSE_POWER_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x1; \
} while(0)

static void efuse_ctrl_init(void)
{
    static bool efuse_inited = false;

    if (!efuse_inited) {
        // use pclk for efuse
        // enable efuse
        __HAL_EFUSE_CLK_ENABLE();
        
        // enable power for efuse program
        __HAL_EFUSE_POWER_ENABLE();

        // disable redundancy mode
        IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
        IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

        efuse_inited = true;
    }
}

int8_t ls_efuse_read_word(uint8_t addr, uint32_t *val)
{
    int8_t ret = -1;

    efuse_ctrl_init();
    ret = efuse_read_word(addr, val);

    return ret;
}

int ls_efuse_write_word(uint32_t addr, uint32_t val)
{
    int8_t ret;
    uint32_t dat, buf;

    efuse_read_word(0, &buf);

    efuse_program_ctrl(1);
    ret = efuse_write_word(addr, val);
    if(ret) {
        return -1;
    }
    efuse_program_ctrl(0);

    ret = efuse_read_word(addr, &dat);
    if(ret) {
        return -1;
    }
    if(dat != val) {
        CLOGE("error in normal read mode:0x%x against 0x%x\n", val, dat);
        return -1;
    }
    CLOGI(": Efuse %d word written with 0x%08x\n", addr, val);

    CLOGI(": success: write and read matched\n");
    return 0;
}


