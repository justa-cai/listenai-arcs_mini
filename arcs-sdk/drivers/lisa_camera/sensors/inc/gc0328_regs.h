/*
 * GC0328 register definitions.
 */
#ifndef __GC0328_REG_REGS_H__
#define __GC0328_REG_REGS_H__


#define RESET_RELATED                   0XFE    // Bit[7]: Software reset
                                                // Bit[6:5]: NA
                                                // Bit[4]: CISCTL_restart_n
                                                // Bit[3:2]: NA
                                                // Bit[1:0]: page select
                                                //  00:page0
                                                //  01:page1

//----page0-----------------------------
// Analog window (sensor pixel array windowing, subsample source)
#define P0_ANA_ROW_START_HIGH           0x09
#define P0_ANA_ROW_START_LOW            0x0A
#define P0_ANA_COL_START_HIGH           0x0B
#define P0_ANA_COL_START_LOW            0x0C
#define P0_ANA_WIN_HEIGHT_HIGH          0x0D
#define P0_ANA_WIN_HEIGHT_LOW           0x0E
#define P0_ANA_WIN_WIDTH_HIGH           0x0F
#define P0_ANA_WIN_WIDTH_LOW            0x10

#define P0_MIRROR_FLIP                  0X17

#define P0_DEBUG_MODE2                  0X4C

// Window mode enable (0x01 = enable windowing/subsample mode)
#define P0_WIN_MODE                     0x50

// Output window (digital output crop, determines final output resolution)
// When analog window > output window, sensor hardware auto-subsamples
#define P0_OUT_WIN_Y1_HIGH              0x51
#define P0_OUT_WIN_Y1_LOW               0x52
#define P0_OUT_WIN_X1_HIGH              0x53
#define P0_OUT_WIN_X1_LOW               0x54
#define P0_OUT_WIN_HEIGHT_HIGH          0x55
#define P0_OUT_WIN_HEIGHT_LOW           0x56
#define P0_OUT_WIN_WIDTH_HIGH           0x57
#define P0_OUT_WIN_WIDTH_LOW            0x58

// Legacy aliases (compatible with old naming)
#define P0_ROW_START_HIGH               P0_OUT_WIN_Y1_HIGH
#define P0_ROW_START_LOW                P0_OUT_WIN_Y1_LOW
#define P0_COLUMN_START_HIGH            P0_OUT_WIN_X1_HIGH
#define P0_COLUMN_START_LOW             P0_OUT_WIN_X1_LOW
#define P0_WINDOW_HEIGHT_HIGH           P0_OUT_WIN_HEIGHT_HIGH
#define P0_WINDOW_HEIGHT_LOW            P0_OUT_WIN_HEIGHT_LOW
#define P0_WINDOW_WIDTH_HIGH            P0_OUT_WIN_WIDTH_HIGH
#define P0_WINDOW_WIDTH_LOW             P0_OUT_WIN_WIDTH_LOW

//----page0 subsample control (GC0328 datasheet)-----
#define P0_SUBSAMPLE_RATIO              0x59    // [7:4] row ratio, [3:0] col ratio
                                                //   0x11=1/1, 0x22=1/2, 0x33=1/3, ...
#define P0_SUBSAMPLE_MODE               0x5A    // [5] row subsample enable
                                                // [4] col subsample enable
                                                // [3] vacancy_zero_mode
                                                // [2] remove_00_mode
                                                // [1] neighbor average mode
                                                // [0] subsample_extend_opclk
#define P0_SUBSAMPLE_MODE_ROW_EN        (1 << 5)
#define P0_SUBSAMPLE_MODE_COL_EN        (1 << 4)
#define P0_SUBSAMPLE_MODE_NEIGHBOR_AVG  (1 << 1)
#define P0_SUBSAMPLE_MODE_EXTEND_PCLK   (1 << 0)

#define REG_CHIP_ID                     0XF0


#endif //__GC0328_REG_REGS_H__
