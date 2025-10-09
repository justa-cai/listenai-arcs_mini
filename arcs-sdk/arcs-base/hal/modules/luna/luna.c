/*
 * luna.c
 *
 *  Created on: Sep 7, 2017
 *      Author: dwwang
 */

#include "luna/luna.h"
#include "luna/isa/luna_isa.h"
#include "luna/isa/luna_bits.h"
#include "luna/cmd/luna_excute_cmd.h"
#include "luna_privates.h"

//------------------------------------------------------------------------------------------------------

unsigned int reg_read(unsigned int addr)
{
    return *((volatile unsigned int *)(addr));
}


void reg_write(unsigned int addr, unsigned int data)
{
    *((volatile unsigned int *)(addr))= data;
    return;
}


#define luna_print_single_reg_dec(rn)	LUNA_LOG("%s : %d\r\n", #rn, *((volatile uint32_t *)(ADDR_LUNA_##rn)));
#define luna_print_single_reg_hex(rn)	LUNA_LOG("%s : 0x%08x\r\n", #rn, *((volatile uint32_t *)(ADDR_LUNA_##rn)));
#define luna_print_single_reg(rn)		luna_print_single_reg_hex(rn)

void luna_print_regs()
{
	uint32_t tmp_addr = BASE_ADDR_LUNA;

	luna_print_single_reg(GPR0);
	luna_print_single_reg(GPR1);
	luna_print_single_reg(GPR2);
	luna_print_single_reg(GPR3);
	luna_print_single_reg(GPR4);
	luna_print_single_reg(GPR5);
	luna_print_single_reg(GPR6);
	luna_print_single_reg(GPR7);
	luna_print_single_reg(GPR8);
	luna_print_single_reg(GPR9);
	luna_print_single_reg(GPR10);
	luna_print_single_reg(GPR11);
	luna_print_single_reg(GPR12);
	luna_print_single_reg(GPR13);
	luna_print_single_reg(GPR14);
	luna_print_single_reg(GPR15);
	luna_print_single_reg(GPR16);
	luna_print_single_reg(GPR17);
	luna_print_single_reg(GPR18);
	luna_print_single_reg(GPR19);
	luna_print_single_reg(GPR20);
	luna_print_single_reg(GPR21);
	luna_print_single_reg(GPR22);
	luna_print_single_reg(GPR23);
	luna_print_single_reg(GPR24);
	luna_print_single_reg(GPR25);
	luna_print_single_reg(GPR26);
	luna_print_single_reg(GPR27);
	luna_print_single_reg_hex(STATUS);
	luna_print_single_reg_hex(PC);
	luna_print_single_reg_hex(LINK);
	luna_print_single_reg_hex(SP);
	luna_print_single_reg_hex(INS_IDX);
	luna_print_single_reg_hex(MODULE_ST);
}

uint32_t luna_return_reg0()
{
	return *((volatile unsigned int *)(ADDR_LUNA_GPR0));
}


//------------------------------------------------------------------------------------------------------
void luna_start(uint32_t addr)
{
    reg_write(ADDR_LUNA_CODE_BASE,  addr);
    reg_write(ADDR_LUNA_AHB_MST,    0x07010700);
    reg_write(ADDR_LUNA_CTRL, LUNA_CTRL_START | LUNA_CTRL_AUTO_DISC_EN | LUNA_CTRL_AUTO_CLKG_EN );
}

__attribute__((optimize("O0"))) int luna_wait()
{
	volatile uint32_t loop_cnt = 0;
	unsigned int int_vector;
	while(1)
	{
		int_vector = reg_read(ADDR_LUNA_IRQ_STAT);
		if (int_vector & 0x01)
		{
			 reg_write(ADDR_LUNA_IRQ_CLR, int_vector);
			 break;
		}
		if (loop_cnt++ >= 10000000)
		{
			return -1;
		}
	}
	return 0;
}

__attribute__((optimize("O0"))) void luna_init() 
{
	reg_write(ADDR_LUNA_SOFT_RSTN, 0xffffffff);
//	reg_write(ADDR_LUNA_IRQ_MASK,   0xfffffffe);
	reg_write(ADDR_LUNA_IRQ_MASK,   0x0);

}

_FAST_DATA_ZI static volatile uint32_t luna_cmd[8] = {};

__attribute__((optimize("O0"))) int32_t luna_execute(const uint32_t *api, void* param)
{
	int32_t ret = 0;
	int32_t index = 0;
    unsigned int val;

	luna_cmd[index++] = setrl_word(r7, (uint32_t)api);
	luna_cmd[index++] = setrh_word(r7, (uint32_t)api);
	luna_cmd[index++] = setrl_word(r0, (uint32_t)param);
	luna_cmd[index++] = setrh_word(r0, (uint32_t)param);
	luna_cmd[index++] = bl(r7);
	luna_cmd[index++] = wait(interrupt);

	luna_start(luna_cmd);
	if (0 != luna_wait())
	{
		LUNA_LOG("    time out while waiting for interrupt\n");
		return -1;
	}
	ret = reg_read(ADDR_LUNA_GPR0);
	return ret;
}

int32_t luna_execute_cmd(const uint32_t *api, void* param, uint32_t param_size) 
{
    return luna_execute(api, param);
}

uint32_t luna_version()
{
	return LUNA_VERSION;
}

void start_counter()
{
	reg_write(ADDR_LUNA_GLOBAL_CNT_LSB, 1);
}

uint32_t get_counter()
{
	uint32_t cnt_l, cnt_h;
	uint64_t cnt;
	reg_write(ADDR_LUNA_GLOBAL_CNT_LSB, 2);
	cnt_l = reg_read(ADDR_LUNA_GLOBAL_CNT_LSB);
	//cnt_h = reg_read(ADDR_LUNA_GLOBAL_CNT_MSB);   
	//cnt = (((uint64_t)cnt_h) << 16|cnt_l);
	return cnt_l;
}
