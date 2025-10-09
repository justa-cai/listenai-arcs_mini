#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "log_print.h"
#include "chip.h"

#define __HAL_EFUSE_CLK_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SEL_EFUSE_CLK = 0x1; \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1; \
} while(0)

#define __HAL_EFUSE_CLK_DISABLE()    \
do { \
    IIP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x0; \
} while(0)

#define __HAL_EFUSE_POWER_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x1; \
} while(0)

#define __HAL_EFUSE_POWER_DISABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x0; \
} while(0)


void efuse_force_auto_load()
{
	IP_EFUSE_CTRL->REG_AUTO_LOAD_START.all = 0xcafeef01;
	while(IP_EFUSE_CTRL->REG_STA.bit.EFU_AUTO_LD_BUSY)
		;
}

void efuse_program_ctrl(char enable)
{
	if(enable) {  //enable program
		if(!IP_EFUSE_CTRL->REG_PROG_PROTECT.all) {
			IP_EFUSE_CTRL->REG_PROG_PROTECT.all = 0xcafeef02;
		}
	} else {  //disable program
		if(IP_EFUSE_CTRL->REG_PROG_PROTECT.all) {
			IP_EFUSE_CTRL->REG_PROG_PROTECT.all = 0xcafeef02;
		}
	}

}

int8_t efuse_write_bit(uint8_t addr, uint8_t bit)
{
	int8_t ret = -1;
	if(addr < 0x80 && bit < 0x20) {
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = (bit << 7) | addr;
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE = 1;  //program mode
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 1;
		while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START);
		ret = 0;
	}

	return ret;
}

int8_t efuse_read_word(uint8_t addr, uint32_t *val)
{
	int8_t ret = -1;

	if(addr < 0x80) {
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = addr;
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE = 0;  //read mode
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD = 0; //normal read
		IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 1;
		while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START);
		*val = IP_EFUSE_CTRL->REG_RD_DATA.all;
		ret = 0;
	}
	return ret;
}

int8_t efuse_write_word(uint8_t addr, uint32_t val)
{
	int8_t ret = 0, i;

	if(addr < 0x80) {
		for(i = 0; i < 32; i++) {
			if(val & (1UL << i)) {
				ret = efuse_write_bit(addr, i);
				if(ret)
					break;
			}
		}
	} else {
		ret = -1;
	}
	return ret;
}

int8_t efuse_read_word_mr(uint8_t addr, uint32_t *val)
{
	int8_t ret = -1;

	if(addr < 0x80) {
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

/*
 * RF[n] is the flag to record whether the redundancy bit has been used or not.
 * FB[n]_Data is the correct data of nth redundancy bit, which will be stored in RIR.
 * FB[n]_A0~A8 is the address data of nth redundancy bit, which will be stored in RIR.
 * FB[n]_Disable is used to disregard the nth redundancy bits. When set to '1', this repaired bit will be ineffective and
 * disregarded
 *
 * Only A11-A5 are used for mapping, while:
 *     0: [A11...A6A5] = [0...00]
 *     1: [A11...A6A5] = [1...00]
 *     2: [A11...A6A5] = [0...01]
 *     3: [A11...A6A5] = [1...01]
 *     4: [A11...A6A5] = [0...10]
 *     5: [A11...A6A5] = [1...10]
 *     6: [A11...A6A5] = [0...11]
 *     7: [A11...A6A5] = [1...11]
 * RF[n]: [A10 A9 A8 A7] = [0 0 0 0]
 * FB[n]_Data: [A10 A9 A8 A7] = [0 0 0 1]
 * FB[n]_A0~A11: [A10 A9 A8 A7] = [0 0 1 0] ~ [1 1 0 1]
 * FB[n]_Disable: [A10 A9 A8 A7] = [1 1 1 1]
 *
 *
 *  */
int8_t efuse_enable_redundancy_feature(uint8_t rf, uint8_t val, uint8_t addr, uint8_t bit)
{
	int8_t ret = -1;

	if((rf > 7) || (addr > 128) || (bit > 31) || (val > 1))
		return ret;

	uint8_t a11 = (rf % 2);
	uint8_t a6a5 = (rf >> 1);
	uint8_t a10_7;

	efuse_program_ctrl(1);

    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;

    // RF[n]
    a10_7 = 0;
    efuse_write_bit((a6a5 << 5), (a11 << 4) | (a10_7));

    // RB[n]_Data
    a10_7 = 1;
    if(val)
        efuse_write_bit((a6a5 << 5), (a11 << 4) | (a10_7));

    // RB[n]_A0~A11
    uint16_t cmd_addr = (addr | (bit << 7));
    for(int i = 0; i < 12; i++)
    {
        a10_7++;
        if(cmd_addr & (1 << i))
            efuse_write_bit((a6a5 << 5), (a11 << 4) | (a10_7));
    }

//    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
//    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	efuse_program_ctrl(0);

	return ret;
}

int8_t efuse_disable_redundancy_feature(uint8_t rf)
{
	int8_t ret = -1;

	if(rf > 7)
		return ret;

	uint8_t a11 = (rf % 2);
	uint8_t a6a5 = (rf >> 1);
	
	efuse_program_ctrl(1);

    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;

    // RF[n]
	efuse_write_bit((a6a5 << 5), (a11 << 4) | (0b1111));

//    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
//    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	efuse_program_ctrl(0);
	return ret;
}



uint32_t efuse_read_redun0() {
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B   = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD          = 0x1; // 0:normal read; 1:margin read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR  = 0;            // A5A6 = 0b00
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE  = 0x0;			 // 1:program 0:read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 0x1;
    while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START)
        ;

    return IP_EFUSE_CTRL->REG_RD_DATA.all;
}

uint32_t efuse_read_redun1() {
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B   = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD          = 0x1; // 0:normal read; 1:margin read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR  = (0b01 << 5);  // A5A6 = 0b01
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE  = 0x0;			 // 1:program 0:read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 0x1;
    while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START)
        ;

    return IP_EFUSE_CTRL->REG_RD_DATA.all;
}

uint32_t efuse_read_redun2() {
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B   = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD          = 0x1; // 0:normal read; 1:margin read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR  = (0b10 << 5);  // A5A6 = 0b10
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE  = 0x0;			 // 1:program 0:read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 0x1;
    while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START)
        ;

    return IP_EFUSE_CTRL->REG_RD_DATA.all;
}

uint32_t efuse_read_redun3() {
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B   = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD          = 0x1; // 0:normal read; 1:margin read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR  = (0b11 << 5);  // A5A6 = 0b11
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE  = 0x0;			 // 1:program 0:read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 0x1;
    while(IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START)
        ;

    return IP_EFUSE_CTRL->REG_RD_DATA.all;
}

/*
 *
 * [Q31~Q0]0 = [FB[1]_Disable, N/A, FB[1]_A11, FB[1]_A10, ...,
 *              FB[1]_A1, FB[1]_A0, FB[1]_data, RF[1],
 *              FB[0]_Disable, N/A, FB[0]_A11, FB[0]_A10, ...,
 *              FB[0]_A1, FB[0]_A0, FB[0]_data, RF[0]]
 * [Q31~Q0]1 = [FB[3]_Disable, N/A, FB[3]_A11, FB[3]_A10, ...,
 *              FB[3]_A1, FB[3]_A0, FB[3]_data, RF[3],
 *              FB[2]_Disable, N/A, FB[2]_A11, FB[2]_A10, ...,
 *              FB[2]_A1, FB[2]_A0, FB[2]_data, RF[2]]
 * [Q31~Q0]2 = [FB[5]_Disable, N/A, FB[5]_A11, FB[5]_A10, ...,
 *              FB[5]_A1, FB[5]_A0, FB[5]_data, RF[5],
 *              FB[4]_Disable, N/A, FB[4]_A11, FB[4]_A10, ...,
 *              FB[4]_A1, FB[4]_A0, FB[4]_data, RF[4]]
 * [Q31~Q0]3 = [FB[7]_Disable, N/A, FB[7]_A11, FB[7]_A10, ...,
 *              FB[7]_A1, FB[7]_A0, FB[7]_data, RF[7],
 *              FB[6]_Disable, N/A, FB[6]_A11, FB[6]_A10, ...,
 *              FB[6]_A1, FB[6]_A0, FB[6]_data, RF[6]]
 *
 */
void efuse_parse_redundancy_bit_info(uint16_t val)
{
	uint8_t reg, bit, data, used, disabled;
	used = (val >> 0) & 0x1;
	data = (val >> 1) & 0x1;
	reg = (val >> 2) & 0x7F;
	bit = (val >> 9) & 0x1F;
	disabled = (val >> 15) & 0x1;
	CLOGD("    disabled: %d, used: %d, data: %d, replacing Reg_%d Bit_%d",\
			disabled, used, data, reg, bit);
}

void test_efuse_redundancy_feature()
{
	uint32_t rdata;

	// read out the redundancy bit 0 & 1 info
	rdata = efuse_read_redun0();
	CLOGD("Redundancy bit 0 info:");
	efuse_parse_redundancy_bit_info(rdata & 0x0000FFFF);
	CLOGD("Redundancy bit 1 info:");
	efuse_parse_redundancy_bit_info((rdata >> 16) & 0x0000FFFF);

	// read out the redundancy bit 2 & 3 info
	rdata = efuse_read_redun1();
	CLOGD("Redundancy bit 2 info:");
	efuse_parse_redundancy_bit_info(rdata & 0x0000FFFF);
	CLOGD("Redundancy bit 3 info:");
	efuse_parse_redundancy_bit_info((rdata >> 16) & 0x0000FFFF);

	// read out the redundancy bit 4 & 5 info
	rdata = efuse_read_redun2();
	CLOGD("Redundancy bit 4 info:");
	efuse_parse_redundancy_bit_info(rdata & 0x0000FFFF);
	CLOGD("Redundancy bit 5 info:");
	efuse_parse_redundancy_bit_info((rdata >> 16) & 0x0000FFFF);

	// read out the redundancy bit 6 & 7 info
	rdata = efuse_read_redun3();
	CLOGD("Redundancy bit 6 info:");
	efuse_parse_redundancy_bit_info(rdata & 0x0000FFFF);
	CLOGD("Redundancy bit 7 info:");
	efuse_parse_redundancy_bit_info((rdata >> 16) & 0x0000FFFF);


	// RIR is valid after 'efuse_read_redunX' called
	rdata = IP_EFUSE_CTRL->REG_PROG_PROTECT1.all;
	CLOGD("Redundancy bits used: 0x%x\n\n", rdata);


	// disable redundancy mode(EFU_REDUNDANCY_ENA_B is LOW active)
//	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
//	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

//	efuse_disable_redundancy_feature(0);
//	efuse_enable_redundancy_feature(0, 0, 1, 0);  // reg_1, bit_0 -> rb_0 = 0
//	efuse_enable_redundancy_feature(1, 1, 1, 31); // reg_1, bit_31 -> rb_1 = 1
//	efuse_enable_redundancy_feature(2, 1, 1, 30); // reg_1, bit_30 -> rb_2 = 1
//	efuse_enable_redundancy_feature(3, 1, 1, 15); // reg_1, bit_15 -> rb_3 = 1
}

// choose one from 64 to 127
#define TEST_EFUSE_INDEX (64)

#define TEST_EFUSE_WRITE_LOCK_POS (16)
#define TEST_EFUSE_READ_LOCK_POS  (17)


static void efuse_write_read(void){
	int8_t ret;
	uint32_t val, dat;

	// use pclk for efuse
	// enable efuse
	__HAL_EFUSE_CLK_ENABLE();

	// enable power for efuse program
	__HAL_EFUSE_POWER_ENABLE();

	// disable redundancy mode
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	ret = efuse_read_word(TEST_EFUSE_INDEX, &dat);
	if(ret) {
		return;
	}

	val = dat + 1;
	val |= dat;

	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_INDEX, val);
	if(ret) {
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
	__HAL_EFUSE_POWER_DISABLE();

	CLOGD("    success: write and read matched");
}

void crypto_efuse_write_word(int index, uint32_t val){
    int8_t ret;
    uint32_t dat;

    // use pclk for efuse
    // enable efuse
    __HAL_EFUSE_CLK_ENABLE();

    // enable power for efuse program
    __HAL_EFUSE_POWER_ENABLE();

    // disable redundancy mode
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

    efuse_program_ctrl(1);
    ret = efuse_write_word(index, val);
    if(ret) {
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
    __HAL_EFUSE_POWER_DISABLE();

    CLOGD("    success: write and read matched");
}

static void efuse_disable_write(void){
	int8_t ret;
	uint32_t val, dat0, dat1;

	// use pclk for efuse
	// enable efuse
	__HAL_EFUSE_CLK_ENABLE();

	// enable power for efuse program
	__HAL_EFUSE_POWER_ENABLE();

	// disable redundancy mode
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	ret = efuse_read_word(TEST_EFUSE_INDEX, &dat0);
	if(ret) {
		return;
	}

	// enable the relevant lock bit for write operation
	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_WRITE_LOCK_POS, (1 << (TEST_EFUSE_INDEX >> 2)));
	if(ret) {
		return;
	}
	efuse_program_ctrl(0);

	efuse_force_auto_load();

	val = dat0 + 1;
	val |= dat0;

	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_INDEX, val);
	if(ret) {
		return;
	}
	efuse_program_ctrl(0);

	ret = efuse_read_word(TEST_EFUSE_INDEX, &dat1);
	if(ret) {
		return;
	}
	if(dat1 != dat0) {
		CLOGD("    error: efuse write is still valid: 0x%x against 0x%x", dat0, dat1);
		return;
	}

	// disable power for efuse program
	__HAL_EFUSE_POWER_DISABLE();

	CLOGD("    success: write is disabled");
}

static void efuse_disable_read(void){
	int8_t ret;
	uint32_t val, dat0, dat1;

	// use pclk for efuse
	// enable efuse
	__HAL_EFUSE_CLK_ENABLE();

	// enable power for efuse program
	__HAL_EFUSE_POWER_ENABLE();

	// disable redundancy mode
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	ret = efuse_read_word(TEST_EFUSE_INDEX, &dat0);
	if(ret) {
		return;
	}

	// enable the relevant lock bit for read operation
	efuse_program_ctrl(1);
	ret = efuse_write_word(TEST_EFUSE_READ_LOCK_POS, (1 << (TEST_EFUSE_INDEX >> 2)));
	if(ret) {
		return;
	}
	efuse_program_ctrl(0);

	efuse_force_auto_load();

	ret = efuse_read_word(TEST_EFUSE_INDEX, &dat1);
	if(ret) {
		return;
	}

	if(dat1 != 0x0 || dat1 == dat0) {
		CLOGD("    error: efuse read before and after: 0x%x against 0x%x", dat0, dat1);
		return;
	}

	CLOGD("    efuse read before and after: 0x%x against 0x%x", dat0, dat1);

	// disable power for efuse program
	__HAL_EFUSE_POWER_DISABLE();

	CLOGD("    success: read is disabled");
}

#define TEST_EFUSE_RED_POS 0
static void efuse_redundancy_test(void){
	int8_t ret;
	uint32_t val, dat;

	// use pclk for efuse
	// enable efuse
	__HAL_EFUSE_CLK_ENABLE();

	// enable power for efuse program
	__HAL_EFUSE_POWER_ENABLE();


	test_efuse_redundancy_feature();


	// disable redundancy mode
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x1;
	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

	ret = efuse_read_word(TEST_EFUSE_RED_POS, &dat);
	if(ret) {
		return;
	}
	CLOGD("    efuse read %d:0x%x", TEST_EFUSE_RED_POS, dat);

    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;
	
	CLOGD("    enable redundancy");
	efuse_enable_redundancy_feature(2, 0, 17, 16);  // reg_0, bit_0 -> rb_0 = 0

//	efuse_enable_redundancy_feature(1, 0, 0, 0);  // reg_0, bit_0 -> rb_0 = 0
//	efuse_enable_redundancy_feature(0, 1, 0, 31);  // reg_0, bit_31 -> rb_0 = 1
//	efuse_enable_redundancy_feature(1, 0, 1, 0);  // reg_1, bit_0 -> rb_1 = 0
//
	// enable redundancy mode
//	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
//	IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x1;

	test_efuse_redundancy_feature();

//	efuse_force_auto_load();

    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;

    ret = efuse_read_word(TEST_EFUSE_RED_POS, &dat);
	if(ret) {
		return;
	}
	CLOGD("    efuse read %d:0x%x", TEST_EFUSE_RED_POS, dat);

	test_efuse_redundancy_feature();

    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ENA_B = 0x0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_REDUNDANCY_ROW_SEL = 0x0;
//
//	efuse_disable_redundancy_feature(0);
//	CLOGD("    disablee redundancy\n");
//	ret = efuse_read_word(TEST_EFUSE_RED_POS, &dat);
//	if(ret) {
//		return;
//	}
//	CLOGD("    efuse read %d:0x%x\n", TEST_EFUSE_RED_POS, dat);
//
//	test_efuse_redundancy_feature();
//
	// disable power for efuse program
    __HAL_EFUSE_POWER_DISABLE();
}


typedef void (*function)(void);

typedef struct {
    void (*function)(void);
    const char* name;
} test_case_t;

static test_case_t test_array[] = {
    {efuse_write_read, "efuse_write_read"},
	{efuse_disable_write, "efuse_disable_write"},
	{efuse_disable_read, "efuse_disable_read"},
//	{efuse_redundancy_test, "efuse_redundancy_test"}
};


int efuse_main()
{
	logInit(0, 115200);
	CLOGD("enter main: efuse test\n");
    uint32_t index;
    for(index = 0; index < sizeof(test_array)/sizeof(test_array[0]); index++){
    	test_case_t item = test_array[index];
    	CLOGD("test case: %s", item.name);
    	item.function();
    }
    while(1);

    return 0;
}

