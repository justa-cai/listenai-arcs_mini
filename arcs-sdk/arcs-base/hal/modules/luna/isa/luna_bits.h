/***************************************************************************
 * luna_bits.h                                                  *
 *                                                                         *
 * Copyright (C) 2020 listenai Co.Ltd                   			       *
 * All rights reserved.                                                    *
 ***************************************************************************/
#ifndef __LUNA_LUNA_BITS_H__
#define __LUNA_LUNA_BITS_H__

/*******************  Luna register  ********************/

#define BASE_ADDR_LUNA                  0x49000000

#define ADDR_LUNA_CTRL                  BASE_ADDR_LUNA + 0x00
#define ADDR_LUNA_CODE_BASE             BASE_ADDR_LUNA + 0x04
#define ADDR_LUNA_EVENT_ID              BASE_ADDR_LUNA + 0x08
#define ADDR_LUNA_GLOBAL_CNT_LSB        BASE_ADDR_LUNA + 0x10
#define ADDR_LUNA_GLOBAL_CNT_MSB        BASE_ADDR_LUNA + 0x14
#define ADDR_LUNA_LOCAL_CNT_LSB         BASE_ADDR_LUNA + 0x18
#define ADDR_LUNA_LOCAL_CNT_MSB         BASE_ADDR_LUNA + 0x1c
#define ADDR_LUNA_GPR0                  BASE_ADDR_LUNA + 0x20
#define ADDR_LUNA_GPR1                  BASE_ADDR_LUNA + 0x24
#define ADDR_LUNA_GPR2                  BASE_ADDR_LUNA + 0x28
#define ADDR_LUNA_GPR3                  BASE_ADDR_LUNA + 0x2c
#define ADDR_LUNA_GPR4                  BASE_ADDR_LUNA + 0x30
#define ADDR_LUNA_GPR5                  BASE_ADDR_LUNA + 0x34
#define ADDR_LUNA_GPR6                  BASE_ADDR_LUNA + 0x38
#define ADDR_LUNA_GPR7                  BASE_ADDR_LUNA + 0x3c
#define ADDR_LUNA_GPR8                  BASE_ADDR_LUNA + 0x40
#define ADDR_LUNA_GPR9                  BASE_ADDR_LUNA + 0x44
#define ADDR_LUNA_GPR10                 BASE_ADDR_LUNA + 0x48
#define ADDR_LUNA_GPR11                 BASE_ADDR_LUNA + 0x4c
#define ADDR_LUNA_GPR12                 BASE_ADDR_LUNA + 0x50
#define ADDR_LUNA_GPR13                 BASE_ADDR_LUNA + 0x54
#define ADDR_LUNA_GPR14                 BASE_ADDR_LUNA + 0x58
#define ADDR_LUNA_GPR15                 BASE_ADDR_LUNA + 0x5c
#define ADDR_LUNA_GPR16                 BASE_ADDR_LUNA + 0x60
#define ADDR_LUNA_GPR17                 BASE_ADDR_LUNA + 0x64
#define ADDR_LUNA_GPR18                 BASE_ADDR_LUNA + 0x68
#define ADDR_LUNA_GPR19                 BASE_ADDR_LUNA + 0x6c
#define ADDR_LUNA_GPR20                 BASE_ADDR_LUNA + 0x70
#define ADDR_LUNA_GPR21                 BASE_ADDR_LUNA + 0x74
#define ADDR_LUNA_GPR22                 BASE_ADDR_LUNA + 0x78
#define ADDR_LUNA_GPR23                 BASE_ADDR_LUNA + 0x7c
#define ADDR_LUNA_GPR24                 BASE_ADDR_LUNA + 0x80
#define ADDR_LUNA_GPR25                 BASE_ADDR_LUNA + 0x84
#define ADDR_LUNA_GPR26                 BASE_ADDR_LUNA + 0x88
#define ADDR_LUNA_GPR27                 BASE_ADDR_LUNA + 0x8c
#define ADDR_LUNA_STATUS                BASE_ADDR_LUNA + 0x90
#define ADDR_LUNA_PC                    BASE_ADDR_LUNA + 0x94
#define ADDR_LUNA_LINK                  BASE_ADDR_LUNA + 0x98
#define ADDR_LUNA_SP                    BASE_ADDR_LUNA + 0x9c
#define ADDR_LUNA_MODULE_ST             BASE_ADDR_LUNA + 0xa0
#define ADDR_LUNA_AHB_MST               BASE_ADDR_LUNA + 0xa4
#define ADDR_LUNA_AHB_ST                BASE_ADDR_LUNA + 0xac
#define ADDR_LUNA_INS_IDX               BASE_ADDR_LUNA + 0xb0
#define ADDR_LUNA_PE_LSB                BASE_ADDR_LUNA + 0xb4
#define ADDR_LUNA_PE_MSB                BASE_ADDR_LUNA + 0xb8
#define ADDR_LUNA_CLK_GATE              BASE_ADDR_LUNA + 0xbc
#define ADDR_LUNA_SOFT_RSTN             BASE_ADDR_LUNA + 0xc0
#define ADDR_LUNA_MEM_CFG               BASE_ADDR_LUNA + 0xc4
#define ADDR_LUNA_AHB_BADDR             BASE_ADDR_LUNA + 0xc8
#define ADDR_LUNA_IRQ_CLR               BASE_ADDR_LUNA + 0xd0
#define ADDR_LUNA_IRQ_MASK              BASE_ADDR_LUNA + 0xd4
#define ADDR_LUNA_IRQ_STAT              BASE_ADDR_LUNA + 0xd8

/*******************  Bit definition for Luna Control register  ********************/
#define LUNA_CTRL_START                 (0b1 << 0)
#define LUNA_CTRL_PC_RST                (0b1 << 1)
#define LUNA_CTRL_PC_GO_ON              (0b1 << 2)
#define LUNA_CTRL_AHB_NON_ALGN_RST      (0b1 << 4)
#define LUNA_CTRL_MULTI_THRD_EN         (0b1 << 8)
#define LUNA_CTRL_AUTO_DISC_EN          (0b1 << 9)
#define LUNA_CTRL_AUTO_CLKG_EN          (0b1 << 10)
#define LUNA_CTRL_PE_AUTO_CLKG_EN       (0b1 << 11)

#endif /* __LUNA_LUNA_BITS_H__ */


