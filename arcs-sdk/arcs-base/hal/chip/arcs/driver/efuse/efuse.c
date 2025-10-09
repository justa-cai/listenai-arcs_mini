#include "arcs_ap.h"
#include "Driver_EFUSE.h"
#include "log_print.h"

#define __HAL_EFUSE_CLK_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SEL_EFUSE_CLK = 0x1; \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1; \
} while(0)

#define __HAL_EFUSE_POWER_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x1; \
} while(0)

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

void efuse_force_auto_load()
{
	IP_EFUSE_CTRL->REG_AUTO_LOAD_START.all = 0xcafeef01;
	while(IP_EFUSE_CTRL->REG_STA.bit.EFU_AUTO_LD_BUSY)
		;
}

uint64_t efuse_read_uuid()
{
	uint64_t uuid = 0;
    uint32_t val_word2 = 0;
    uint32_t val_word3 = 0;

    // enable efuse
    __HAL_EFUSE_CLK_ENABLE();
    // enable power for efuse program
    __HAL_EFUSE_POWER_ENABLE();

    // Efuse value word2
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = 0x2;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE = 0;  // Read mode
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD = 0; // Normal read
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 1;
    while (IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START);
    val_word2 = IP_EFUSE_CTRL->REG_RD_DATA.all;

    // Efuse value word3
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = 0x3;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE = 0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_MARGIN_RD = 0;
    IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START = 1;
    while (IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_START);
    val_word3 = IP_EFUSE_CTRL->REG_RD_DATA.all;

    uuid = ((uint64_t)val_word3 << 32) | val_word2;

    return uuid;
}


