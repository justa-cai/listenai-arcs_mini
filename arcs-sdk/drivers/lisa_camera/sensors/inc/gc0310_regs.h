/*
 * GC032A register definitions.
 */
#ifndef __GC0310_REG_REGS_H__
#define __GC0310_REG_REGS_H__


#define RESET_RELATED                   0XFE    // Bit[7]: Software reset
                                                // Bit[6:5]: NA
                                                // Bit[4]: CISCTL_restart_n
                                                // Bit[3:2]: NA
                                                // Bit[1:0]: page select
                                                //  00:page0
                                                //  01:page1

//----page0-----------------------------
#define P0_ROW_START_HIGH               0x51
#define P0_ROW_START_LOW                0x52
#define P0_COLUMN_START_HIGH            0x53
#define P0_COLUMN_START_LOW             0x54
#define P0_WINDOW_HEIGHT_HIGH           0x55
#define P0_WINDOW_HEIGHT_LOW            0x56
#define P0_WINDOW_WIDTH_HIGH            0x57
#define P0_WINDOW_WIDTH_LOW             0x58

#define P0_MIRROR_FLIP                  0X17

#define P0_DEBUG_MODE2                  0X4C

#define REG_CHIP_ID_H                   0XF0
#define REG_CHIP_ID_L                   0XF1


#endif //__GC032A_REG_REGS_H__
