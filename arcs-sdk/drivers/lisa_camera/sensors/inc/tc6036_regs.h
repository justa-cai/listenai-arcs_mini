/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * TC6036 register definitions.
 */
#ifndef _TC6036_REGS_H_
#define _TC6036_REGS_H_

/* Page Select Register */
#define REG_PAGE_SELECT             0xFE

/* Page 0 Registers */
#define P0_VTS_HIGH                 0x00    // VTS[15:8]
#define P0_VTS_LOW                  0x01    // VTS[7:0]
#define P0_HTS_HIGH                 0x02    // HTS[15:8]
#define P0_HTS_LOW                  0x03    // HTS[7:0]
#define P0_MIRROR_FLIP              0x1C    // [1:0] mirror flip control

/* Page 1 Registers - Analog & AE/AWB */
#define P1_ANALOG_30                0x30    // Analog control
#define P1_BLC_TARGET               0x08    // BLC target
#define P1_AE_FRONT_MODE            0x30    // AE front mode
#define P1_EXPO_TIME_HSB            0x3E    // Exposure time high byte
#define P1_EXPO_TIME_LSB            0x3F    // Exposure time low byte
#define P1_AE_MAX_STEP              0x3A    // AE max step
#define P1_AE_TARGET                0x54    // AE target
#define P1_DNS_EDGE_EN              0x70    // DNS & Edge enable
#define P1_EDGE_80                  0x80    // Edge control
#define P1_EDGE_81                  0x81    // Edge control
#define P1_DNS_78                   0x78    // DNS control
#define P1_DNS_79                   0x79    // DNS control
#define P1_AWB_90                   0x90    // AWB control
#define P1_AWB_A0                   0xA0    // AWB control
#define P1_RGAIN                    0xAC    // R gain
#define P1_BGAIN                    0xAD    // B gain

/* Page 2 Registers - ISP & Output */
#define P2_BLC_VALUE                0x11    // BLC value
#define P2_LENS_GAIN_R              0x15    // Lens gain R
#define P2_LENS_GAIN_G              0x16    // Lens gain G
#define P2_LENS_GAIN_B              0x17    // Lens gain B
#define P2_GAMMA_30                 0x30    // Gamma start
#define P2_SAT_CTRL                 0x6B    // Saturation control
#define P2_CONTRAST                 0x75    // Contrast
#define P2_SKIN_TH                  0x77    // Skin threshold
#define P2_SAT_79                   0x79    // Saturation
#define P2_CCM_ENABLE               0xD1    // CCM enable
#define P2_CCM_R_60                 0x60    // CCM R
#define P2_CCM_G_63                 0x63    // CCM G
#define P2_CCM_B_66                 0x66    // CCM B

/* Page 2 - Crop Window Registers */
#define P2_CROP_80                  0x80    // Crop X start high
#define P2_CROP_81                  0x81    // Crop X start low
#define P2_CROP_82                  0x82    // Crop Y start high
#define P2_CROP_83                  0x83    // Crop Y start low
#define P2_CROP_84                  0x84    // Crop width high
#define P2_CROP_85                  0x85    // Crop width low

/* Page 2 - Output Format */
#define P2_OUTPUT_FORMAT            0x86    // Output format select
                                            // 0x04: YUV422
                                            // 0x06: RGB565
#define P2_OUTPUT_ENABLE            0xE0    // Output enable

/* Page 2 - Chip ID Registers */
#define REG_CHIP_ID_HIGH            0xD4    // Chip ID high byte (0x03)
#define REG_CHIP_ID_LOW             0xD5    // Chip ID low byte (0x71)

/* Page 3 Registers - System Clock */
#define P3_STREAM_D8                0xD8    // Stream control
#define P3_STREAM_D9                0xD9    // Stream control
#define P3_CLOCK_DB                 0xDB    // Clock control

#endif // _TC6036_REGS_H_
