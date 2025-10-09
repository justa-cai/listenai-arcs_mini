/*
 * GC032A register definitions.
 */
#ifndef __OV9655_REG_REGS_H__
#define __OV9655_REG_REGS_H__


/* OV9655 Registers definition */
#define OV9655_REG_GAIN       0x00
#define OV9655_REG_BLUE       0x01
#define OV9655_REG_RED        0x02
#define OV9655_REG_VREF       0x03
#define OV9655_REG_COM1       0x04
#define OV9655_REG_BAVE       0x05
#define OV9655_REG_GbAVE      0x06
#define OV9655_REG_GrAVE      0x07
#define OV9655_REG_RAVE       0x08
#define OV9655_REG_COM2       0x09
#define OV9655_REG_PID        0x0A
#define OV9655_REG_VER        0x0B
#define OV9655_REG_COM3       0x0C
#define OV9655_REG_COM4       0x0D
#define OV9655_REG_COM5       0x0E
#define OV9655_REG_COM6       0x0F
#define OV9655_REG_AEC        0x10
#define OV9655_REG_CLKRC      0x11
#define OV9655_REG_COM7       0x12
#define OV9655_REG_COM8       0x13
#define OV9655_REG_COM9       0x14
#define OV9655_REG_COM10      0x15
#define OV9655_REG_REG16      0x16
#define OV9655_REG_HSTART     0x17
#define OV9655_REG_HSTOP      0x18
#define OV9655_REG_VSTART     0x19
#define OV9655_REG_VSTOP      0x1A
#define OV9655_REG_PSHFT      0x1B
#define OV9655_REG_MIDH       0x1C
#define OV9655_REG_MIDL       0x1D
#define OV9655_REG_MVFP       0x1E
#define OV9655_REG_BOS        0x20
#define OV9655_REG_GBOS       0x21
#define OV9655_REG_GROS       0x22
#define OV9655_REG_ROS        0x23
#define OV9655_REG_AEW        0x24
#define OV9655_REG_AEB        0x25
#define OV9655_REG_VPT        0x26
#define OV9655_REG_BBIAS      0x27
#define OV9655_REG_GbBIAS     0x28
#define OV9655_REG_PREGAIN    0x29
#define OV9655_REG_EXHCH      0x2A
#define OV9655_REG_EXHCL      0x2B
#define OV9655_REG_RBIAS      0x2C
#define OV9655_REG_ADVFL      0x2D
#define OV9655_REG_ADVFH      0x2E
#define OV9655_REG_YAVE       0x2F
#define OV9655_REG_HSYST      0x30
#define OV9655_REG_HSYEN      0x31
#define OV9655_REG_HREF       0x32
#define OV9655_REG_CHLF       0x33
#define OV9655_REG_AREF1      0x34
#define OV9655_REG_AREF2      0x35
#define OV9655_REG_AREF3      0x36
#define OV9655_REG_ADC1       0x37
#define OV9655_REG_ADC2       0x38
#define OV9655_REG_AREF4      0x39
#define OV9655_REG_TSLB       0x3A
#define OV9655_REG_COM11      0x3B
#define OV9655_REG_COM12      0x3C
#define OV9655_REG_COM13      0x3D
#define OV9655_REG_COM14      0x3E
#define OV9655_REG_EDGE       0x3F
#define OV9655_REG_COM15      0x40
#define OV9655_REG_COM16      0x41
#define OV9655_REG_COM17      0x42
#define OV9655_REG_MTX1       0x4F
#define OV9655_REG_MTX2       0x50
#define OV9655_REG_MTX3       0x51
#define OV9655_REG_MTX4       0x52
#define OV9655_REG_MTX5       0x53
#define OV9655_REG_MTX6       0x54
#define OV9655_REG_BRTN       0x55
#define OV9655_REG_CNST1      0x56
#define OV9655_REG_CNST2      0x57
#define OV9655_REG_MTXS       0x58
#define OV9655_REG_AWBOP1     0x59
#define OV9655_REG_AWBOP2     0x5A
#define OV9655_REG_AWBOP3     0x5B
#define OV9655_REG_AWBOP4     0x5C
#define OV9655_REG_AWBOP5     0x5D
#define OV9655_REG_AWBOP6     0x5E
#define OV9655_REG_BLMT       0x5F
#define OV9655_REG_RLMT       0x60
#define OV9655_REG_GLMT       0x61
#define OV9655_REG_LCC1       0x62
#define OV9655_REG_LCC2       0x63
#define OV9655_REG_LCC3       0x64
#define OV9655_REG_LCC4       0x65
#define OV9655_REG_MANU       0x66
#define OV9655_REG_MANV       0x67
#define OV9655_REG_MANY       0x68
#define OV9655_REG_VARO       0x69
#define OV9655_REG_BD50MAX    0x6A
#define OV9655_REG_DBLV       0x6B
#define OV9655_REG_DNSTH      0x70
#define OV9655_REG_POIDX      0x72
#define OV9655_REG_PCKDV      0x73
#define OV9655_REG_XINDX      0x74
#define OV9655_REG_YINDX      0x75
#define OV9655_REG_SLOP       0x7A
#define OV9655_REG_GAM1       0x7B
#define OV9655_REG_GAM2       0x7C
#define OV9655_REG_GAM3       0x7D
#define OV9655_REG_GAM4       0x7E
#define OV9655_REG_GAM5       0x7F
#define OV9655_REG_GAM6       0x80
#define OV9655_REG_GAM7       0x81
#define OV9655_REG_GAM8       0x82
#define OV9655_REG_GAM9       0x83
#define OV9655_REG_GAM10      0x84
#define OV9655_REG_GAM11      0x85
#define OV9655_REG_GAM12      0x86
#define OV9655_REG_GAM13      0x87
#define OV9655_REG_GAM14      0x88
#define OV9655_REG_GAM15      0x89
#define OV9655_REG_COM18      0x8B
#define OV9655_REG_COM19      0x8C
#define OV9655_REG_COM20      0x8D
#define OV9655_REG_DMLNL      0x92
#define OV9655_REG_DMLNH      0x93
#define OV9655_REG_LCC6       0x9D
#define OV9655_REG_LCC7       0x9E
#define OV9655_REG_AECH       0xA1
#define OV9655_REG_BD50       0xA2
#define OV9655_REG_BD60       0xA3
#define OV9655_REG_COM21      0xA4
#define OV9655_REG_GREEN      0xA6
#define OV9655_REG_VZST       0xA7
#define OV9655_REG_REFA8      0xA8
#define OV9655_REG_REFA9      0xA9
#define OV9655_REG_BLC1       0xAC
#define OV9655_REG_BLC2       0xAD
#define OV9655_REG_BLC3       0xAE
#define OV9655_REG_BLC4       0xAF
#define OV9655_REG_BLC5       0xB0
#define OV9655_REG_BLC6       0xB1
#define OV9655_REG_BLC7       0xB2
#define OV9655_REG_BLC8       0xB3
#define OV9655_REG_CTRLB4     0xB4
#define OV9655_REG_FRSTL      0xB7
#define OV9655_REG_FRSTH      0xB8
#define OV9655_REG_ADBOFF     0xBC
#define OV9655_REG_ADROFF     0xBD
#define OV9655_REG_ADGbOFF    0xBE
#define OV9655_REG_ADGrOFF    0xBF
#define OV9655_REG_COM23      0xC4
#define OV9655_REG_BD60MAX    0xC5
#define OV9655_REG_COM24      0xC7

/* Registers bit definition */
/* COM1 Register */
#define CCIR656_FORMAT          0x40
#define HREF_SKIP_0             0x00
#define HREF_SKIP_1             0x04
#define HREF_SKIP_3             0x08

/* COM2 Register */
#define SOFT_SLEEP_MODE         0x10
#define ODCAP_1x                0x00
#define ODCAP_2x                0x01
#define ODCAP_3x                0x02
#define ODCAP_4x                0x03

/* COM3 Register */
#define COLOR_BAR_OUTPUT        0x80
#define OUTPUT_MSB_LAS_SWAP     0x40
#define PIN_REMAP_RESETB_EXPST  0x08
#define RGB565_FORMAT           0x00
#define RGB_OUTPUT_AVERAGE      0x04
#define SINGLE_FRAME            0x01

/* COM5 Register */
#define SLAM_MODE_ENABLE        0x40
#define EXPOSURE_NORMAL_MODE    0x01

/* COM7 Register */
#define SCCB_REG_RESET                       0x80
#define FORMAT_CTRL_15fpsVGA                 0x00
#define FORMAT_CTRL_30fpsVGA_NoVArioPixel    0x50
#define FORMAT_CTRL_30fpsVGA_VArioPixel      0x60
#define OUTPUT_FORMAT_RAWRGB                 0x00
#define OUTPUT_FORMAT_RAWRGB_DATA            0x00
#define OUTPUT_FORMAT_RAWRGB_INTERP          0x01
#define OUTPUT_FORMAT_YUV                    0x02
#define OUTPUT_FORMAT_RGB                    0x03

/* COM9 Register */
#define GAIN_2x                 0x00
#define GAIN_4x                 0x10
#define GAIN_8x                 0x20
#define GAIN_16x                0x30
#define GAIN_32x                0x40
#define GAIN_64x                0x50
#define GAIN_128x               0x60
#define DROP_VSYNC              0x04
#define DROP_HREF               0x02

/* COM10 Register */
#define RESETb_REMAP_SLHS       0x80
#define HREF_CHANGE_HSYNC       0x40
#define PCLK_ON                 0x00
#define PCLK_OFF                0x20
#define PCLK_POLARITY_REV       0x10
#define HREF_POLARITY_REV       0x08
#define RESET_ENDPOINT          0x04
#define VSYNC_NEG               0x02
#define HSYNC_NEG               0x01

/* TSLB Register */
#define PCLK_DELAY_0            0x00
#define PCLK_DELAY_2            0x40
#define PCLK_DELAY_4            0x80
#define PCLK_DELAY_6            0xC0
#define OUTPUT_BITWISE_REV      0x20
#define UV_NORMAL               0x00
#define UV_FIXED                0x10
#define YUV_SEQ_YUYV            0x00
#define YUV_SEQ_YVYU            0x02
#define YUV_SEQ_VYUY            0x04
#define YUV_SEQ_UYVY            0x06
#define BANDING_FREQ_50         0x02

#define RGB_NORMAL              0x00
#define RGB_565                 0x10
#define RGB_555                 0x30



#endif //__GC032A_REG_REGS_H__
