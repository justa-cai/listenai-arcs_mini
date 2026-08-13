#include "efuse.h"

int8_t efuse_read_word(uint8_t addr, uint32_t *val)
{
    int8_t ret = -1;

    if (addr < 0x10) {
        IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR  = addr;
        IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_TYPE  = 0; //read mode
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
    if(enable) {    // enable program
        if(!IP_EFUSE_CTRL->REG_PROG_PROTECT.all) {
            IP_EFUSE_CTRL->REG_PROG_PROTECT.all = 0xcafeef02;
        }
    } else {    // disable program
        if(IP_EFUSE_CTRL->REG_PROG_PROTECT.all) {
            IP_EFUSE_CTRL->REG_PROG_PROTECT.all = 0xcafeef02;
        }
    }
}

int8_t efuse_write_word(uint8_t addr, uint32_t val) {
    int8_t ret = 0, i;
    if (addr < 0x10) {
        for(i = 0; i < 32; i++) {
            if(val & (1UL << i)) {
                ret = efuse_write_bit(addr, i);
                if (ret) {
                   break;
                }               
            }
        }
    } else {
        ret = -1;
    }
    return ret;
}

int8_t efuse_write_bit(uint8_t addr, uint8_t bit) {
    int8_t ret = -1;
    if (addr < 0x10 && bit < 0x20) {
        IP_EFUSE_CTRL->REG_CMD_CTL.bit.EFU_CMD_ADDR = (bit << 4) | addr;
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
