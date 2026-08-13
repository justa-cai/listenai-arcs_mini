#ifndef FTSDC021_H
#define FTSDC021_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "venusa_ap.h"
#include "lib_sdc.h"
#include "drv_sdc.h"

#include "ftsdc021_mmc.h"
#include "ftsdc021_sdio.h"

#include "log_print.h"

#ifdef CFG_RTOS
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#endif

#define FTSDC021_VERSION    "001.20131112"

//#define TIMER_HZ        1000
/*SDC021*/

#define SDC_FTSDC021_0_PA_BASE      SDIOH_BASE
#define SDC_FTSDC021_0_PA_SIZE      0x00100000
#define SDC_FTSDC021_0_IRQ          IRQ_SDIOH_VECTOR

#define sdc_dbg_print(format, arg...) CLOGD(format, ## arg)
#define sdc_dbg_print_level(level, format, arg...) //CLOGD(format, ## arg);
/**
 * Descriptor Table used for ADMA2 transfer.
 * One descriptor line consumes 8 bytes and can transfer
 * maximum 65536 bytes. Default ADMA2_NUM_OF_LINES is
 * 1536.
 */
#define ADMA2_NUM_OF_LINES  64

#define FTSDC021_BUFFER_LENGTH 4096
#define FTSDC021_DEF_BLK_LEN    0x200

#define SDHCI_HOST_0    0
#define SDHCI_HOST_1    1

#define SDHCI_MODULE_STS_INVERT  0x00000001
#define SDHCI_MODULE_STS_FIXED   0x00000002
#define SDHCI_MODULE_STS_SDIO_STD_FUNC  0x00000004
#define SDHCI_MODULE_STS_SDIO_FORCE_3_3_V  0x00000008

#define Standard_Capacity   0
#define Normal_Capacity     1
#define High_Capacity       2

#define FIFO_depth_4_words  0
#define FIFO_depth_8_words  1
#define FIFO_depth_16_words 2
#define FIFO_depth_128_words    3
#define FIFO_depth_256_words    4
#define FIFO_depth_512_words    5

#define SDHCI_SUPPORT_eMMC 0

/* For SD Memory Register (unit of byte) */
#define SCR_LENGTH              8
#define SD_WRITTEN_NUM_LENGTH   4
#define SD_STATUS_LENGTH        64

/* For MMC Memory Register */
#define EXT_CSD_LENGTH      512

#define SDC_PS_FUNC_NORMAL      0
#define SDC_PS_FUNC_RESTORE     1
/* For Initial E-MMC*/
//#define EMBEDDED_MMC
//#pragma pack(1)
typedef struct
{
    u32 SdmaAddr;   // 0x00-0x03
    u16 BlkSize;        // 0x04-0x05
    u16 BlkCnt;     // 0x06-0x07
    u32 CmdArgu;        // 0x08-0x0B
    u16 TxMode;     // 0x0C-0x0D
    u16 CmdReg;     // 0x0E-0x0F
    u64 CmdRespLo;  // 0x10-0x17
    u64 CmdRespHi;  // 0x18-0x1F
    u32 BufData;        // 0x20-0x23
    u32 PresentState;   // 0x24-0x27
    u8 HCReg;       // 0x28
    u8 PowerCtl;        // 0x29
    u8 BlkGapCtl;   // 0x2A
    u8 WakeUpCtl;   // 0x2B
    u16 ClkCtl;     // 0x2C-0x2D
    u8 TimeOutCtl;  // 0x2E
    u8 SoftRst;     // 0x2F
    u16 IntrSts;        // 0x30-0x31
    u16 ErrSts;     // 0x32-0x33
    u16 IntrEn;     // 0x34-0x35
    u16 ErrEn;      // 0x36-0x37
    u16 IntrSigEn;  // 0x38-0x39
    u16 ErrSigEn;   // 0x3A-0x3B
    u16 AutoCMDErr; // 0x3C-0x3D
    u16 HostCtrl2;  // 0x3E-0x3F
    u32 CapReg;     // 0x40-0x43
    u32 CapReg2;        // 0x44-0x47
    u64 MaxCurr;        // 0x48-0x4F
    u16 CMD12ForceEvt;  // 0x50-0x51
    u16 ForceEvt;   // 0x52-0x53
    u32 ADMAErrSts; // 0x54-0x57
    u64 ADMAAddr;   // 0x58-0x5F
    /*
     *  Register offset 0x60 - 0x6F change at FTSDC021
     */
    u16 PresetValInit;  // 0x60-0x61
    u16 PresetValDS;    // 0x62-0x63
    u16 PresetValHS;    // 0x64-0x65
    u16 PresetValSDR12; // 0x66-0x67
    u16 PresetValSDR25; // 0x68-0x69
    u16 PresetValSDR50; // 0x6A-0x6B
    u16 PresetValSDR104;    // 0x6C-0x6D
    u16 PresetValDDR50; // 0x6E-0x6F

    u32 Reserved[28];   // 0x70-0xDF
    u32 ShareBusCtrl;   // 0xE0-0xE3
    u32 Reserved2[6];   // 0xE4-0xFB
    u16 SltIntrSts; // 0xFC-0xFD
    u16 HCVer;      // 0xFE-0xFF
    u32 VendorReg0; // 0x100-0x103
    u32 VendorReg1; // 0x104-0x107
    u32 VendorReg2; // 0x108-0x10B
    u32 VendorReg3; // 0x10C-0x10F
    u32 VendorReg4; // 0x110-0x113
    u32 VendorReg5; // 0x114-0x117
    u32 VendorReg6; // 0x118-0x11B
    u32 AhbErrSts;      // 0x11C-0x11F
    u32 AhbErrEn;       // 0x120-0x124
    u32 AhbErrSigEn;    // 0x124-0x127
    u32 DmaHndshk;  // 0x128-0x12C
    u32 Reserved4[19];  // 0x12C-0x177
    u32 VendorReg7; // 0x178-0x17B
    u32 IpVersion;  // 0x17C-0x17F
    u32 Ciph_M_Ctl; // 0x180-0x183
    u32 Ciph_M_Sts; // 0x184-0x187
    u16 Ciph_M_Sts_En;  // 0x188-0x189
    u16 Ciph_M_Sig_En;  // 0x18A-0x18B
    u32 In_Data_Lo; // 0x18C-0x18F
    u32 In_Data_Hi; // 0x190-0x193
    u32 In_Key_Lo;  // 0x194-0x197
    u32 In_Key_Hi;  // 0x198-0x19B
    u32 Out_Data_Lo;    // 0x19C-0x19F
    u32 Out_Data_Hi;    // 0x1A0-0x1A3
    u32 Secr_Table_Port;    // 0x1A4-0x1A7
} __attribute__ ((__packed__)) ftsdc021_reg;
//#pragma pack()

typedef enum
{
    WRITE = 0,
    READ
} Transfer_Act;

typedef enum
{
    ADMA = 0,
    SDMA,
    PIO,
    EDMA,
    TRANS_UNKNOWN
} Transfer_Type;

typedef struct
{
    Transfer_Type UseDMA; /* 0: PIO 1: SDMA 2: ADMA */
    u64 adma_buffer;
    u32 sdma_bound_mask;
    u32 timeout_ms;
    u16 lineBound;
    u16 adma2Rand;
    u8 Erasing; /* 0: No earsing in buruin 1:Include erase testing */
    u8 autoCmd;
    u8 adma2_insert_nop;
    u8 adma2_use_interrupt;
    //u8 Reserved;
} FlowInfo;

typedef struct
{
    u32 Reserved1 :5; /* 508:502 */
    u32 SECURED_MODE :1; /* 509 */
    u32 DAT_BUS_WIDTH :2; /* 511:510 */
    u32 SD_CARD_TYPE_HI :8; /* 495:488 */
    u32 Reserved2 :8; /* 501:496 */
    u32 SD_CARD_TYPE_LO :8; /* 487:480 */
    u32 SIZE_OF_PROTECTED_AREA; /* 479:448 */
    u8 SPEED_CLASS;
    u8 PERFORMANCE_MOVE;
    u32 Reserved3 :4; /* 427:424 */
    u32 AU_SIZE :4; /* 431:428 */
    u8 ERASE_SIZE[2]; /* 423:408 */
    u32 ERASE_OFFSET :2; /* 401:400 */
    u32 ERASE_TIMEOUT :6; /* 407:402 */
    u8 Reserved4[11];
    u8 Reserved5[39];
} SDStatus;

typedef struct
{
    u32 CSD_STRUCTURE :2;
    u32 Reserved1 :6;
    u8 TAAC;
    u8 NSAC;
    u8 TRAN_SPEED;
    u32 CCC :12;
    u32 READ_BL_LEN :4;
    u32 READ_BL_PARTIAL :1;
    u32 WRITE_BLK_MISALIGN :1;
    u32 READ_BLK_MISALIGN :1;
    u32 DSR_IMP :1;
    u32 Reserved2 :2;
    u32 C_SIZE :12;
    u32 VDD_R_CURR_MIN :3;
    u32 VDD_R_CURR_MAX :3;
    u32 VDD_W_CURR_MIN :3;
    u32 VDD_W_CURR_MAX :3;
    u32 C_SIZE_MULT :3;
    u32 ERASE_BLK_EN :1;
    u32 SECTOR_SIZE :7;
    u32 WP_GRP_SIZE :7;
    u32 WP_GRP_ENABLE :1;
    u32 Reserved3 :2;
    u32 R2W_FACTOR :3;
    u32 WRITE_BL_LEN :4;
    u32 WRITE_BL_PARTIAL :1;
    u32 Reserved4 :5;
    u32 FILE_FORMAT_GRP :1;
    u32 COPY :1;
    u32 PERM_WRITE_PROTECT :1;
    u32 TMP_WRITE_PROTECT :1;
    u32 FILE_FORMAT :2;
    u32 Reserver5 :2;

} CSD_v1;

typedef struct
{
    u32 CSD_STRUCTURE :2;
    u32 Reserved1 :6;
    u8 TAAC;
    u8 NSAC;
    u8 TRAN_SPEED;
    u32 CCC :12;
    u32 READ_BL_LEN :4;
    u32 READ_BL_PARTIAL :1;
    u32 WRITE_BLK_MISALIGN :1;
    u32 READ_BLK_MISALIGN :1;
    u32 DSR_IMP :1;
    u32 Reserved2 :6;
    u32 C_SIZE :22;
    u32 Reserved3 :1;
    u32 ERASE_BLK_EN :1;
    u32 SECTOR_SIZE :7;
    u32 WP_GRP_SIZE :7;
    u32 WP_GRP_ENABLE :1;
    u32 Reserved4 :2;
    u32 R2W_FACTOR :3;
    u32 WRITE_BL_LEN :4;
    u32 WRITE_BL_PARTIAL :1;
    u32 Reserved5 :5;
    u32 FILE_FORMAT_GRP :1;
    u32 COPY :1;
    u32 PERM_WRITE_PROTECT :1;
    u32 TMP_WRITE_PROTECT :1;
    u32 FILE_FORMAT :2;
    u32 Reserver6 :2;
} CSD_v2;

// The sequence of variable in SD_SCR structure is constrained.
typedef struct
{
    u32 SD_SPEC :4; /* [59:56] */
    u32 SCR_STRUCTURE :4; /* [60:63] */

    u32 SD_BUS_WIDTHS :4; /* [51:48] */
    u32 SD_SECURITY :3; /* [52:54] */
    u32 DATA_STAT_AFTER_ERASE :1; /* [55:55] */

    u32 Reserved1 :7; /* [46:40] */
    u32 SD_SPEC3 :1; /* [47:47] */

    u32 CMD20_SUPPORT :1; /* [32:32] */
    u32 CMD23_SUPPORT :1; /* [33:33] */
    u32 Reserverd2 :6; /* [34:39] */
    u32 Reserverd3; /* [31:0] */

    u32 Reserverd4[6]; /*extent sizeof(SD_SCR) to cache line size: 32 bytes */
} __attribute__ ((__packed__)) SD_SCR;

/* for SCR */
#define SDHCI_SCR_SUPPORT_4BIT_BUS  0x4
#define SDHCI_SCR_SUPPORT_1BIT_BUS  0x1

typedef struct
{
    u32 hs_max_dtr;
    u32 uhs_max_dtr;
    #define UHS_SDR104_MAX_DTR  208000000
#define UHS_SDR50_MAX_DTR   100000000
#define UHS_DDR50_MAX_DTR   50000000
#define UHS_SDR25_MAX_DTR   UHS_DDR50_MAX_DTR
#define UHS_SDR12_MAX_DTR   25000000
    u32 sd3_bus_mode;
    #define UHS_SDR12_BUS_SPEED 0
#define UHS_SDR25_BUS_SPEED 1
#define UHS_SDR50_BUS_SPEED 2
#define UHS_SDR104_BUS_SPEED    3
#define UHS_DDR50_BUS_SPEED 4
#define SD_MODE_UHS_SDR12   (1 << UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25   (1 << UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50   (1 << UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104  (1 << UHS_SDR104_BUS_SPEED)
#define SD_MODE_UHS_DDR50   (1 << UHS_DDR50_BUS_SPEED)
    u32 sd3_drv_type;
    #define SD_DRIVER_TYPE_B    0x01
#define SD_DRIVER_TYPE_A    0x02
#define SD_DRIVER_TYPE_C    0x04
#define SD_DRIVER_TYPE_D    0x08
    u32 sd3_curr_limit;
#define SD_SET_CURRENT_LIMIT_200    0
#define SD_SET_CURRENT_LIMIT_400    1
#define SD_SET_CURRENT_LIMIT_600    2
#define SD_SET_CURRENT_LIMIT_800    3
#define SD_MAX_CURRENT_200  (1 << SD_SET_CURRENT_LIMIT_200)
#define SD_MAX_CURRENT_400  (1 << SD_SET_CURRENT_LIMIT_400)
#define SD_MAX_CURRENT_600  (1 << SD_SET_CURRENT_LIMIT_600)
#define SD_MAX_CURRENT_800  (1 << SD_SET_CURRENT_LIMIT_800)
} SD_SWITCH_CAPS;

typedef enum
{
    SPEED_DEFAULT = 0, /* or SDR12 in 1.8v IO signalling level */
    SPEED_HIGH, /* or SDR25 in 1.8v IO signalling level */
    SPEED_SDR50,
    SPEED_SDR104,
    SPEED_DDR50,
    SPEED_RSRV
} Bus_Speed;

/* To indicate which complete we want to wait */
#define WAIT_CMD_COMPLETE   (1 << 0)
#define WAIT_TRANS_COMPLETE (1 << 1)
#define WAIT_DMA_INTR       (1 << 2)
#define WAIT_BLOCK_GAP      (1 << 3)

typedef struct
{
    u32 CardInsert;
    FlowInfo FlowSet;
    u16 RCA;
    u16 DSR;
    SD_SCR SCR;
    u32 OCR;
    u64 CSD_LO;
    u64 CSD_HI;
    CSD_v1 CSD_ver_1;
    CSD_v2 CSD_ver_2;
    u64 CID_LO;
    u64 CID_HI;
    u64 respLo;
    u64 respHi;
    u64 capacity;
    u32 numOfBlocks;
    u32 read_block_len;
    u32 write_block_len;
    u32 erase_sector_size;
    u8 SwitchSts[64];
    volatile u16 ErrorSts;
    volatile u16 autoErr;
    volatile u8 cmplMask;
    SDStatus sd_status;
    Bus_Speed speed;
    u16 bs_mode; /* Bus Speed Mode */
    u8 bus_width;
    u8 already_init;
    u8 block_addr;
    u32 max_dtr;
    u16 fifo_depth;
    u32 rblk_sz_2exp;
    u32 wblk_sz_2exp;

    /* MMC */
    MMC_CSD CSD_MMC;
    MMC_EXT_CSD EXT_CSD_MMC;
    u32 numOfBootBlocks;

    SD_SWITCH_CAPS sw_caps;
    u32 CardType;

    /* SDIO */
    u8 sdio_funcs;
    u8 Memory_Present;
    u32 num_info;
    s8** info;
    SDIO_CCCR cccr;
    SDIO_CIS cis;
    SDIO_FUNC func[7];
#ifdef FTSDC021_READ_CIS
    struct sdio_func_tuple* tuples;
#endif

} SDCardInfo;

typedef struct
{
    u32 max_clk;
    u32 min_clk;
    u32 clock;
    u32 ocr_avail;
    u32 cmd_index;
    u8 power;
    u8 ErrRecover;
    u8 sdcard_init_complete;
    u8 module_sts;
    u8 reset_flag;
    u8 reserved[3];
#ifdef CFG_RTOS
    SemaphoreHandle_t sd_mutex;
    SemaphoreHandle_t sdio_dma_semaphore;
    u8 dma_transfer_data;
#else
    void* sd_mutex;
#endif    
    SDIO_ISR_handler sdio_card_int;
    SDC_device_handler sdcard_app_init;
    SDC_device_handler sdio_app_init;
    SDC_driver_func sd_init[2];
    SDC_driver_func sd_detect[2];
    SDCardInfo* Card;
} SDHostInfo;

// Value return from Send Command function
enum
{
    ERR_CMD_TIMEOUT = 1,
    ERR_NON_RECOVERABLE,
    ERR_RECOVERABLE
};

/* 0x0C: TxMode */
#define SDHCI_TXMODE_DMA_EN     (1 << 0)
#define SDHCI_TXMODE_BLKCNT_EN      (1 << 1)
#define SDHCI_TXMODE_AUTOCMD12_EN   (1 << 2)
#define SDHCI_TXMODE_AUTOCMD23_EN   (2 << 2)
#define SDHCI_TXMODE_READ_DIRECTION (1 << 4)
#define SDHCI_TXMODE_WRITE_DIRECTION    (0 << 4)
#define SDHCI_TXMODE_MULTI_SEL      (1 << 5)

/* 0x0E: CmdReg */
/* response type: bit 0 - 4 */
#define SDHCI_CMD_NO_RESPONSE       0x0 // For no response command
#define SDHCI_CMD_RTYPE_R2      0x9 // For R2
#define SDHCI_CMD_RTYPE_R3R4        0x2 // For R3,R4
#define SDHCI_CMD_RTYPE_R1R5R6R7    0x1A    // For R1,R5,R6,R7
#define SDHCI_CMD_RTYPE_R1BR5B      0x1B    // For R1b, R5b

#define SDHCI_CMD_TYPE_NORMAL       0x0
#define SDHCI_CMD_TYPE_SUSPEND      0x1
#define SDHCI_CMD_TYPE_RESUME       0x2
#define SDHCI_CMD_TYPE_ABORT        0x3

#define SDHCI_CMD_DATA_PRESENT      0x1

/* 0x20: Buf data port*/
#define SDHCI_DATA_PORT         0x20

/* 0x24: Present State Register */
#define SDHCI_Pre_State         0x24
#define SDHCI_STS_CMD_INHIBIT       (1 << 0)
#define SDHCI_STS_CMD_DAT_INHIBIT   (1 << 1)
#define SDHCI_STS_DAT_LINE_ACT      (1 << 2)
#define SDHCI_STS_WRITE_TRAN_ACT    (1 << 8)
#define SDHCI_STS_READ_TRAN_ACT     (1 << 9)
#define SDHCI_STS_BUFF_WRITE        (1 << 10)
#define SDHCI_STS_BUFF_READ     (1 << 11)
#define SDHCI_STS_CARD_INSERT       (1 << 16)
#define SDHCI_STS_CARD_STABLE       (1 << 17)
#define SDHCI_STS_CARD_DETECT       (1 << 18)
#define SDHCI_STS_CARD_WRITABLE     (1 << 19)
#define SDHCI_STS_DAT_LINE_LEVEL    (0xF << 20)
#define SDHCI_STS_CMD_LINE_LEVEL    (1 << 24)

/* 0x28: HCReg */
#define SDHCI_HC_LED_ON         (1 << 0)
#define SDHCI_HC_BUS_WIDTH_4BIT     (1 << 1)
#define SDHCI_HC_HI_SPEED       (1 << 2)
#define SDHCI_HC_USE_ADMA2      (2 << 3)
#define SDHCI_HC_BUS_WIDTH_8BIT     (1 << 5)
#define SDHCI_HC_CARD_DETECT_TEST   (1 << 6)
#define SDHCI_HC_CARD_DETECT_SIGNAL (1 << 7)

/* 0x29: */
#define SDHCI_POWER_ON          (1 << 0)
#define SDHCI_POWER_180         (5 << 1)
#define SDHCI_POWER_300         (6 << 1)
#define SDHCI_POWER_330         (7 << 1)

/* 0x2A: BlkGapCtl*/
#define SDHCI_STOP_AT_BLOCK_GAP_REQ     (1 << 0)
#define SDHCI_CONTINUE_REQ      (1 << 1)
#define SDHCI_READ_WAIT_CTL     (1 << 2)
#define SDHCI_INT_AT_BLOCK_GAP      (1 << 3)

/* 0x2C: ClkCntl */
#define SDHCI_CLKCNTL_INTERNALCLK_EN    (1 << 0)
#define SDHCI_CLKCNTL_INTERNALCLK_STABLE    (1 << 1)
#define SDHCI_CLKCNTL_SDCLK_EN      (1 << 2)

/* 0x2F: SoftRst */
#define SDHCI_SOFTRST_ALL       (1 << 0)
#define SDHCI_SOFTRST_CMD       (1 << 1)
#define SDHCI_SOFTRST_DAT       (1 << 2)

/* 0x30: IntrSts */
#define SDHCI_INTR_State        0x30
#define SDHCI_INTR_STS_ERR      (1 << 15)
#define SDHCI_INTR_STS_CARD_INTR    (1 << 8)
#define SDHCI_INTR_STS_CARD_REMOVE  (1 << 7)
#define SDHCI_INTR_STS_CARD_INSERT  (1 << 6)
#define SDHCI_INTR_STS_BUFF_READ_READY  (1 << 5)
#define SDHCI_INTR_STS_BUFF_WRITE_READY (1 << 4)
#define SDHCI_INTR_STS_DMA      (1 << 3)
#define SDHCI_INTR_STS_BLKGAP       (1 << 2)
#define SDHCI_INTR_STS_TXR_COMPLETE (1 << 1)
#define SDHCI_INTR_STS_CMD_COMPLETE (1 << 0)

/* 0x32: ErrSts */
#define SDHCI_INTR_ERR_TUNING       (1 << 10)
#define SDHCI_INTR_ERR_ADMA     (1 << 9)
#define SDHCI_INTR_ERR_AutoCMD      (1 << 8)
#define SDHCI_INTR_ERR_CURR_LIMIT   (1 << 7)
#define SDHCI_INTR_ERR_DATA_ENDBIT  (1 << 6)
#define SDHCI_INTR_ERR_DATA_CRC     (1 << 5)
#define SDHCI_INTR_ERR_DATA_TIMEOUT (1 << 4)
#define SDHCI_INTR_ERR_CMD_INDEX    (1 << 3)
#define SDHCI_INTR_ERR_CMD_ENDBIT   (1 << 2)
#define SDHCI_INTR_ERR_CMD_CRC      (1 << 1)
#define SDHCI_INTR_ERR_CMD_TIMEOUT  (1 << 0)
#define SDHCI_INTR_ERR_CMD_LINE     (SDHCI_INTR_ERR_CMD_INDEX | SDHCI_INTR_ERR_CMD_ENDBIT | SDHCI_INTR_ERR_CMD_CRC | SDHCI_INTR_ERR_CMD_TIMEOUT)
#define SDHCI_INTR_ERR_DAT_LINE     (SDHCI_INTR_ERR_DATA_ENDBIT | SDHCI_INTR_ERR_DATA_CRC | SDHCI_INTR_ERR_DATA_TIMEOUT)

/* 0x34: IntrEn */
#define SDHCI_INTR_EN_ALL       (0x008B)//(0x10FF)
#define SDHCI_INTR_EN_INIT      (0x004B)

/* 0x36: ErrEn */
#define SDHCI_ERR_EN_ALL        (0x01ff)//(0xF7FF)
#define SDHCI_ERR_EN_INIT       (0x00ff)

/* 0x38: IntrSigEn */
//#define SDHCI_INTR_SIG_EN_ALL           (0x018b)
//#define SDHCI_INTR_SIG_EN             (0x10CC)
//#define SDHCI_INTR_SIG_EN (SDHCI_INTR_STS_CARD_REMOVE | SDHCI_INTR_STS_CARD_INSERT | SDHCI_INTR_STS_CMD_COMPLETE | SDHCI_INTR_STS_TXR_COMPLETE)
#define SDHCI_INTR_SIG_EN (SDHCI_INTR_STS_CARD_REMOVE | SDHCI_INTR_STS_CARD_INSERT | SDHCI_INTR_STS_CMD_COMPLETE | SDHCI_INTR_STS_TXR_COMPLETE)
#define SDHCI_INTR_SIGN_EN_SDMA (SDHCI_INTR_SIG_EN | SDHCI_INTR_STS_DMA | SDHCI_INTR_STS_BLKGAP)
#define SDHCI_INTR_SIGN_EN_ADMA (SDHCI_INTR_SIG_EN | SDHCI_INTR_STS_DMA)
#define SDHCI_INTR_SIGN_EN_PIO (SDHCI_INTR_SIG_EN | SDHCI_INTR_STS_BLKGAP)

/* 0x3A: ErrSigEn */
//#define SDHCI_ERR_SIG_EN_ALL            (0xF2FF)
//#define SDHCI_ERR_SIG_EN_ALL      (0x02FF)

/* 0x3C: AutoCMD12 Err */
#define SDHCI_AUTOCMD12_ERR_NOT_EXECUTED    (1 << 0)
#define SDHCI_AUTOCMD12_ERR_TIMEOUT     (1 << 1)
#define SDHCI_AUTOCMD12_ERR_CRC         (1 << 2)
#define SDHCI_AUTOCMD12_ERR_END_BIT     (1 << 3)
#define SDHCI_AUTOCMD12_ERR_INDEX       (1 << 4)
#define SDHCI_AUTOCMD12_ERR_CMD_NOT_ISSUE   (1 << 7)

/* 0x3E: Host Control 2 */
#define SDHCI_HOST_CONTROL2     0x3E
#define  SDHCI_PRESET_VAL_EN    (1 << 15)
#define  SDHCI_ASYNC_INT_EN     (1 << 14)
#define  SDHCI_SMPL_CLCK_SELECT (1 << 7)
#define  SDHCI_EXECUTE_TUNING   (1 << 6)    /* Write 1 Auto clear */
#define  SDHCI_DRV_TYPE_MASK    (3 << 4)
#define  SDHCI_DRV_TYPE_SHIFT   4
#define   SDHCI_DRV_TYPEB   0
#define   SDHCI_DRV_TYPEA   1
#define   SDHCI_DRV_TYPEC       2
#define   SDHCI_DRV_TYPED   3
#define  SDHCI_18V_SIGNAL       (1 << 3)
#define  SDHCI_UHS_MODE_MASK    (7 << 0)
#define   SDHCI_SDR12           0
#define   SDHCI_SDR25           1
#define   SDHCI_SDR50           2
#define   SDHCI_SDR104          3
#define   SDHCI_DDR50           4

/* 0x40: Capabilities */
#define SDHCI_CAP_VOLTAGE_33V       (1 << 24)
#define SDHCI_CAP_VOLTAGE_30V       (1 << 25)
#define SDHCI_CAP_VOLTAGE_18V       (1 << 26)
#define SDHCI_CAP_FIFO_DEPTH_16BYTE (0 << 29)
#define SDHCI_CAP_FIFO_DEPTH_32BYTE (1 << 29)
#define SDHCI_CAP_FIFO_DEPTH_64BYTE (2 << 29)
#define SDHCI_CAP_FIFO_DEPTH_512BYTE    (3 << 29)
#define SDHCI_CAP_FIFO_DEPTH_1024BYTE   (4 << 29)
#define SDHCI_CAP_FIFO_DEPTH_2048BYTE   (5 << 29)

/*BingJiun: 0x44 - 0x47 */
#define  SDHCI_SUPPORT_SDR50        (1 << 0)
#define  SDHCI_SUPPORT_SDR104       (1 << 1)
#define  SDHCI_SUPPORT_DDR50        (1 << 2)
#define  SDHCI_SUPPORT_DRV_TYPEA        (1 << 4)
#define  SDHCI_SUPPORT_DRV_TYPEC        (1 << 5)
#define  SDHCI_SUPPORT_DRV_TYPED        (1 << 6)
#define  SDHCI_RETUNING_TIME_MASK       0xF
#define  SDHCI_RETUNING_TIME_SHIFT      8
#define  SDHCI_SDR50_TUNING         (1 << 13)
#define  SDCHI_RETUNING_MODE_MASK       0x3
#define  SDHCI_RETUNING_MODE_SHIFT      14

/* Vendor Defined0(0x100) */
/* Vendor Defined1(0x104) */
#define MMC_BOOT_ACK            (1 << 2)
#define MMC_BUS_TEST_MODE       0x3
#define MMC_ALTERNATIVE_BOOT_MODE   0x2
#define MMC_BOOT_MODE           0x1
#define NORMAL_MODE         0x0
/* Vendor Defined2(0x108) */
/* Vendor Defined3(0x10C) */

#define SDHCI_GO_IDLE_STATE     0
#define MMC_SEND_OP_COND        1
#define SDHCI_SEND_ALL_CID      2
#define SDHCI_SEND_RELATIVE_ADDR    3
#define SDHCI_IO_SEND_OP_COND       5
#define SDHCI_SWITCH_FUNC       6
#define SDHCI_SET_BUS_WIDTH     6
#define SDHCI_SELECT_CARD       7
#define SDHCI_SEND_IF_COND      8
#define SDHCI_SEND_EXT_CSD      8
#define SDHCI_SEND_CSD          9
#define SDHCI_SEND_CID          10
#define SDHCI_VOLTAGE_SWITCH        11
#define SDHCI_STOP_TRANS        12
#define SDHCI_SEND_STATUS       13
#define SDHCI_SD_STATUS         13
#define SDHCI_SET_BLOCKLEN      16
#define SDHCI_READ_SINGLE_BLOCK     17
#define SDHCI_READ_MULTI_BLOCK      18
#define SDHCI_SEND_TUNE_BLOCK       19
#define SDHCI_SEND_NUM_WR_BLKS      22
#define SDHCI_SET_WR_BLOCK_CNT      23
#define SDHCI_WRITE_BLOCK       24
#define SDHCI_WRITE_MULTI_BLOCK     25

#define SDHCI_ERASE_WR_BLK_START    32
#define SDHCI_ERASE_WR_BLK_END      33
#define SDHCI_ERASE_GROUP_START     35
#define SDHCI_ERASE_GROUP_END       36
#define SDHCI_ERASE         38
#define SDHCI_SD_SEND_OP_COND       41
#define SDHCI_GET_MKB           43
#define SDHCI_GET_MID           44
#define SDHCI_CER_RN1           45
#define SDHCI_CER_RN2           46
#define SDHCI_CER_RES2          47
#define SDHCI_CER_RES1          48
#define SDHCI_SEND_SCR          51
#define SDHCI_IO_RW_DIRECT      52
#define SDHCI_IO_RW_EXTENDED        53
#define SDHCI_APP_CMD           55
#define SDHCI_GEN_CMD           56

#define SDHCI_SEND_IF_COND_ARGU         0x1AA
#define SDHCI_SD_SEND_OP_COND_HCS_ARGU      0xC0FF8000
#define SDHCI_SD_SEND_OP_COND_ARGU      0x00FF8000
#define SDHCI_MMC_SEND_OP_COND_BYTE_MODE    0x80FF8000
#define SDHCI_MMC_SEND_OP_COND_SECTOR_MODE  0xC0FF8000

#define CMD_RETRY_CNT       5
#define SDHCI_TIMEOUT       0xFFF

/* For CMD52*/
#define SD_CMD52_RW_in_W        0x80000000
#define SD_CMD52_RW_in_R        0x00000000
#define SD_CMD52_RAW            0x08000000
#define SD_CMD52_no_RAW         0x00000000
#define SD_CMD52_FUNC(Num)      (Num  << 28)
#define SD_CMD52_Reg_Addr(Addr) (Addr << 9)
/* For CMD53*/
#define SD_CMD53_RW_in_W        0x80000000
#define SD_CMD53_RW_in_R        0x00000000
#define SD_CMD53_FUNC(Num)      (Num  << 28)
#define SD_CMD53_Block_Mode     0x08000000
#define SD_CMD53_Byte_Mode      0x00000000
#define SD_CMD53_OP_inc         0x04000000
#define SD_CMD53_OP_fix         0x00000000
#define SD_CMD53_Reg_Addr(Addr) (Addr << 9)
//************************************

/**
 * Card status return from R1 response format.
 * Or use CMD13 to get this status
 */
#define SD_STATUS_OUT_OF_RANGE        0x80000000
#define SD_STATUS_ADDRESS_ERROR       (1 << 30)
#define SD_STATUS_BLOCK_LEN_ERROR     (1 << 29)
#define SD_STATUS_ERASE_SEQ_ERROR     (1 << 28)
#define SD_STATUS_ERASE_PARAM         (1 << 27)
#define SD_STATUS_WP_VIOLATION        (1 << 26)
#define SD_STATUS_CARD_IS_LOCK        (1 << 25)
#define SD_STATUS_LOCK_UNLOCK_FAILED  (1 << 24)
#define SD_STATUS_COM_CRC_ERROR       (1 << 23)
#define SD_STATUS_ILLEGAL_COMMAND     (1 << 22)
#define SD_STATUS_CARD_ECC_FAILED     (1 << 21)
#define SD_STATUS_CC_ERROR            (1 << 20)
#define SD_STATUS_ERROR               (1 << 19)
#define SD_STATUS_UNDERRUN            (1 << 18)
#define SD_STATUS_OVERRUN             (1 << 17)
#define SD_STATUS_CSD_OVERWRITE       (1 << 16)
#define SD_STATUS_WP_ERASE_SKIP       (1 << 15)
#define SD_STATUS_CARD_ECC_DISABLE    (1 << 14)
#define SD_STATUS_ERASE_RESET         (1 << 13)
#define SD_STATUS_CURRENT_STATE       (0xF << 9)
typedef enum
{
    CUR_STATE_IDLE = 0,
    CUR_STATE_READY,
    CUR_STATE_IDENT,
    CUR_STATE_STBY,
    CUR_STATE_TRAN,
    CUR_STATE_DATA,
    CUR_STATE_RCV,
    CUR_STATE_PRG,
    CUR_STATE_DIS,
    CUR_STATE_RSV
} Card_State;
#define SD_STATUS_READY_FOR_DATA      (1 << 8)
#define MMC_STATUS_SWITCH_ERROR       (1 << 7)
#define SD_STATUS_APP_CMD             (1 << 5)
#define SD_STATUS_AKE_SEQ_ERROR       (1 << 3)

#define SD_STATUS_ERROR_BITS          (SD_STATUS_OUT_OF_RANGE | SD_STATUS_ADDRESS_ERROR | \
                                       SD_STATUS_BLOCK_LEN_ERROR | SD_STATUS_ERASE_SEQ_ERROR | \
                                       SD_STATUS_ERASE_PARAM | SD_STATUS_WP_VIOLATION | \
                                       SD_STATUS_LOCK_UNLOCK_FAILED | SD_STATUS_CARD_ECC_FAILED | \
                                       SD_STATUS_CC_ERROR | SD_STATUS_ERROR | \
                                       SD_STATUS_UNDERRUN | SD_STATUS_OVERRUN | \
                                       SD_STATUS_CSD_OVERWRITE | SD_STATUS_WP_ERASE_SKIP | \
                                       SD_STATUS_AKE_SEQ_ERROR | MMC_STATUS_SWITCH_ERROR)

#define SDHCI_1BIT_BUS_WIDTH    0x0
#define SDHCI_4BIT_BUS_WIDTH    0x2

/* ADMA Descriptor Table Generator */
#define ADMA2_ENTRY_VALID   (1 << 0)
#define ADMA2_ENTRY_END     (1 << 1)
#define ADMA2_ENTRY_INT     (1 << 2)

#define ADMA2_NOP       (0 << 4)
#define ADMA2_SET       (1 << 4)
#define ADMA2_TRAN      (2 << 4)
#define ADMA2_LINK      (3 << 4)

typedef struct
{
    u16 attr;
    u16 lgth;
    u32 addr;
} Adma2DescTable;

typedef void
(*sdio_irq_handler_t)(void*);
/* ftsdc021_main.c */
u8*
SDC_ShowCardState(Card_State state);

/* ftsdc021_ops.c */
/**
 * SDIO card related operations
 */
u32
ftsdc021_ops_send_io_op_cond(u8 ip_idx, u32 ocr, u32* rocr);
u32
ftsdc021_sdio_enable_wide(u8 ip_idx);
u32
ftsdc021_sdio_enable_4bit_bus(u8 ip_idx);
u32
ftsdc021_sdio_set_bus_width(u8 ip_idx, u32 width);
u32
ftsdc021_sdio_set_bus_speed(u8 ip_idx, u8 speed);
u32
ftsdc021_sdio_set_func_block_size(u8 ip_idx, SDIO_FUNC* func, u32 blksize);
u32
ftsdc021_sdio_init_func(u8 ip_idx, SDIO_FUNC* func);
u32
ftsdc021_sdio_enable_func(u8 ip_idx, u32 fn);
u32
ftsdc021_sdio_disable_func(u8 ip_idx, u32 fn);
u32
ftsdc021_sdio_claim_irq_func(u8 ip_idx, u32 fn);
u32
ftsdc021_sdio_release_irq_func(u8 ip_idx, u32 fn);
u32
ftsdc021_sdio_enable_hs(u8 ip_idx);
u32
ftsdc021_ops_Card_Info(u8 ip_idx);
u32
ftsdc021_sdio_CMD53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr,
        u32* buf,
        u32 blocks, u32 blksz);
u32
ftsdc021_sdio_CMD52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out);
u32
ftsdc021_sdio_read_cccr(u8 ip_idx, u32 ocr);
u32
ftsdc021_sdio_read_func_cis(u8 ip_idx, SDIO_FUNC* func);
u32
ftsdc021_sdio_cis_init(void);
u32
ftsdc021_sdio_read_common_cis(u8 ip_idx);
#define ftsdc021_io_rw_direct  ftsdc021_sdio_CMD52
#define ftsdc021_io_rw_extended  ftsdc021_sdio_CMD53

/**
 * MMC card related operations
 */
u32
ftsdc021_ops_send_op_cond(u8 ip_idx, u32 ocr, u32* rocr);
u32
ftsdc021_ops_mmc_switch(u8 ip_idx, u8 set, u8 index, u8 value);
u32
ftsdc021_ops_send_ext_csd(u8 ip_idx);
/**
 * SD card related operations(Some applies to MMC)
 */
u32
ftsdc021_ops_go_idle_state(u8 ip_idx, u32 arg);
u32
ftsdc021_ops_send_if_cond(u8 ip_idx, u32 arg);
u32
ftsdc021_ops_send_app_op_cond(u8 ip_idx, u32 ocr, u32* rocr);
u32
ftsdc021_ops_send_voltage_switch(u8 ip_idx);
u32
ftsdc021_ops_all_send_cid(u8 ip_idx);
u32
ftsdc021_ops_send_rca(u8 ip_idx);
u32
ftsdc021_ops_send_csd(u8 ip_idx);
u32
ftsdc021_ops_select_card(u8 ip_idx);
u32
ftsdc021_ops_app_send_scr(u8 ip_idx);
u32
ftsdc021_ops_app_repo_wr_num(u8 ip_idx, u32* written_num);
u32
ftsdc021_ops_app_set_wr_blk_cnt(u8 ip_idx, u32 n_blocks);
u32
ftsdc021_ops_app_set_bus_width(u8 ip_idx, u32 width);
u32
ftsdc021_ops_sd_switch(u8 ip_idx, u32 mode, u32 group, u8 value, u8* resp);
u32
ftsdc021_ops_send_tune_block(u8 ip_idx);
u32
ftsdc021_ops_send_card_status(u8 ip_idx);
u32
ftsdc021_ops_send_sd_status(u8 ip_idx);

/* ftsdc021.c */
bool
ftsdc021_fill_adma_desc_table(u8 ip_idx, u32 total_data, u32 data_addr);
void
ftsdc021_delay(u32 ms);
u32
ftsdc021_send_data_command(u8 ip_idx, Transfer_Act act, u32 startAddr,
        u32 blkCnt, u32 blkSz, void* bufAddr);
u32
ftsdc021_send_vendor_cmd(u8 ip_idx, u32 argument);
u32
ftsdc021_transfer_data(u8 ip_idx, Transfer_Act act, u32* buffer, u32 length);
u32
ftsdc021_CheckCardInsert(u8 ip_idx);
void
ftsdc021_SetPower(u8 ip_idx, s16 power);
void
ftsdc021_SetSDClock(u8 ip_idx, u32 clock);
u32
ftsdc021_set_bus_width(u8 ip_idx, u8 width);
u32
ftsdc021_set_bus_speed_mode(u8 ip_idx, u8 speed);
u32
ftsdc021_read_scr(u8 ip_idx);
u32
ftsdc021_read_ext_csd(u8 ip_idx);
u32
ftsdc021_uhs1_mode(u8 ip_idx);
u32
ftsdc021_pulselatch_tuning(u8 ip_idx, u32 div);
u32
ftsdc021_execute_tuning(u8 ip_idx, u32 try);
void
ftsdc021_set_base_clock(u8 ip_idx, u32 clk);
void
ftsdc021_set_transfer_type(u8 ip_idx, Transfer_Type type, u32 line_bound);
void
ftsdc021_HCReset(u8 ip_idx, u8 type);
u32
ftsdc021_wait_for_state(u8 ip_idx, u32 state, u32 ms);
u32
ftsdc021_prepare_data(u8 ip_idx, u32 blk_cnt, u16 blk_sz, u32 buff_addr,
        Transfer_Act act);
u32
ftsdc021_set_transfer_mode(u8 ip_idx, u8 blk_cnt_en, u8 auto_cmd, u8 dir,
        u8 multi_blk);
u32
ftsdc021_send_abort_command(u8 ip_idx);
u32
ftsdc021_send_command(u8 ip_idx, u8 CmdIndex, u8 CmdType, u8 DataPresent,
        u8 RespType, u8 InhibitDATChk, u32 Argu);
u32
ftsdc021_card_read(u8 ip_idx, u32 startAddr, u32 blkcnt, u8* read_buf);
u32
ftsdc021_card_write(u8 ip_idx, u32 startAddr, u32 blkcnt, u8* write_buf);
u32
get_erase_group_size(u8 ip_idx, u32* erase_group_size);
u32
ftsdc021_card_erase(u8 ip_idx, u32 StartBlk, u32 BlkCount);
u32
ftsdc021_scan_cards(u8 ip_idx);    //, u8 boot_mode);
u32
ftsdc021_init(u8 ip_idx);
u32
ftsdc021_card_writable(u8 ip_idx);
void
ftsdc021_intr_en(u8 ip_idx, u8 is_init);
u32
ftsdc021_ops_register_int_func(u8 ip_idx, SDIO_ISR_handler handler);
u32
ftsdc021_ops_remove_int_func(u8 ip_idx);

u32
ftsdc021_enable_irq(u8 ip_idx, u16 sts);
u32
ftsdc021_disable_irq(u8 ip_idx, u16 sts);

void
ftsdc021_disable_card_detect(u8 ip_idx);
void
ftsdc021_enable_card_detect(u8 ip_idx);
void
ftsdc021_card_detection_init(u8 ip_idx);

void
ftsdc021_card_detection_handle(u8 ip_idx);

extern SDHostInfo SDHost[];

s32
sd_mutex_create(u8 ip_idx);
s32
sd_mutex_lock(u8 ip_idx);
s32
sd_mutex_unlock(u8 ip_idx);
extern u32 sdc_debug_level;
#endif
