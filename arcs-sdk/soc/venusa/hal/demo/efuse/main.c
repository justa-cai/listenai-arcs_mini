/*
 * main.c
 *
 *  Created on: 2025年5月16日
 *      Author: USER
 */

#include <string.h>
#include <stdbool.h>

#include "log_print.h"
#include "venusa_ap.h"
#include "efuse.h"
#include "systick.h"

#define __HAL_EFUSE_CLK_ENABLE()    \
do  {   \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SEL_EFUSE_CLK = 0x1; \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1;  \
} while(0)

#define __HAL_EFUSE_CLK_DISABLE()    \
do  {   \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x0; \
} while(0)

#define TEST_EFUSE_INDEX				(0)
#define TEST_EFUSE_AUTO_LOAD_INDEX 		(1)
#define TEST_EFUSE_DIRECT_INDEX  		(4)

int8_t efuse_read_word_mr(uint8_t addr, uint32_t *val)
{
	int8_t ret = -1;

	if(addr < 0x10) {
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = addr;
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE = 0;  //read mode
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD = 1; //margin read
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 1;
		while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START);
		*val = IP_EFUSE_CTRL->REG_RD_DATA.all;
		ret = 0;
	}
	return ret;
}

static void efuse_write_read(void) {
    uint8_t ret;
    uint32_t val, dat;

    // use pclk for efuse
    // enable efuse clk
    __HAL_EFUSE_CLK_ENABLE();
   
    // Disable redundancy mode
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

    ret = efuse_read_word(TEST_EFUSE_INDEX, &dat);
    if (ret) {
        return;
    }

    val = dat + 1;
    val |= dat;

    efuse_program_ctrl(1);
    ret = efuse_write_word(TEST_EFUSE_INDEX, val);
    if (ret) {
        return;
    }
    dat = 0;
    efuse_program_ctrl(0);

    ret = efuse_read_word(TEST_EFUSE_INDEX, &dat);
    if(ret) {
        return;
    }

    if(dat != val) {
		CLOGD("    error in normal read mode:0x%x against 0x%x", val, dat);
		return;
	}

    ret = efuse_read_word_mr(TEST_EFUSE_INDEX, &dat);
    if(ret) {
		return;
	}

	if(dat != val) {
		CLOGD("    error in margin read mode:0x%x against 0x%x", val, dat);
		return;
	}

    // disable power for efuse program
	__HAL_EFUSE_CLK_DISABLE();

	CLOGD("    success: write and read matched"); 
}

static void efuse_auto_load_write_read(void) {
	int8_t ret;
	uint32_t val, dat0, dat1, dat2;

    // use pclk for efuse
    // enable efuse clk
    __HAL_EFUSE_CLK_ENABLE();

	ret = efuse_read_word(TEST_EFUSE_AUTO_LOAD_INDEX, &dat0);
	if(ret) {
		return;
	}

	val = dat0 + 1;
	val |= dat0;

	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_AUTO_LOAD_INDEX, val);
	if(ret) {
		return;
	}
	efuse_program_ctrl(0);

	ret = efuse_read_word(TEST_EFUSE_AUTO_LOAD_INDEX, &dat1);
	if(ret) {
		return;
	}

	//auto load read
	efuse_force_auto_load();
	dat2 = *((volatile uint32_t*)(EFUSE_CTRL_BASE + 0x80 + TEST_EFUSE_AUTO_LOAD_INDEX * 4));

	if(dat1 != dat2) {
		CLOGD("    error: efuse write is still valid: 0x%x against 0x%x", dat1, dat2);
		return;
	}

	// disable power for efuse program
	__HAL_EFUSE_CLK_DISABLE();

	CLOGD("    success: efuse auto load write read");

}

static void efuse_direct_write_read(void) {
	int8_t ret;
	uint32_t val, dat0, dat1, dat2;

	// use pclk for efuse
	// enable efuse
	__HAL_EFUSE_CLK_ENABLE();

	ret = efuse_read_word(TEST_EFUSE_DIRECT_INDEX, &dat0);
	if(ret) {
		return;
	}

	val = dat0 + 1;
	val |= dat0;

	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_DIRECT_INDEX, val);
	if(ret) {
		return;
	}
	efuse_program_ctrl(0);

	ret = efuse_read_word(TEST_EFUSE_DIRECT_INDEX, &dat1);
	if(ret) {
		return;
	}

	// direct read
	dat2 = *((volatile uint32_t*)(EFUSE_CTRL_BASE + 0x1000 + TEST_EFUSE_DIRECT_INDEX* 4));

	if(dat1 != dat2) {
		CLOGD("    error: efuse write is still valid: 0x%x against 0x%x", dat1, dat2);
		return;
	}

    // disable power for efuse program
	__HAL_EFUSE_CLK_DISABLE();

	CLOGD("    success: efuse direct write read");
}


typedef void (*function)(void);

typedef struct {
	void (*function)(void);
	const char* name;
} test_case_t;

static test_case_t test_array[] = {
//    {efuse_write_read, "efuse_write_read"},
//	  {efuse_auto_load_write_read, "efuse_auto_load_write_read"},
    {efuse_direct_write_read, "efuse_direct_write_read"},
};

int main(void) {
    logInit(0, 115200);
    CLOGD("enter main: efuse test\r\n");

	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 0x1U; //enable LDO_VA

    uint32_t index;

    for(index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++) {
		test_case_t item = test_array[index];
		CLOG("test case : %s", item.name);
		item.function();
	}

    while(1);
}

