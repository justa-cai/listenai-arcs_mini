/*
 * arcs_ap_base.h
 *
 *  Created on: Oct 18, 2022
 *
 */

#ifndef INCLUDE_ARCS_AP_BASE_H_
#define INCLUDE_ARCS_AP_BASE_H_

/************************************************************************************
 * Memory map
 ************************************************************************************/

/************ SRAM **************************/
/************ AP LOCAL MEMORY **************************/
#define AP_ILM_ROM_REGION                       0x0UL
#define AP_ILM_ROM_REGION_SIZE                  0x10000UL
#define AP_ILM_RAM_REGION                       0x80000UL
#define AP_ILM_RAM_REGION_SIZE                  0x4000UL
#define AP_DLM_RAM_REGION                       0x100000UL
#define AP_DLM_RAM_REGION_SIZE                  0x2000UL

/************ CP LOCAL MEMORY **************************/
#define CP_ILM_ROM_REGION                       0x200000UL
#define CP_ILM_ROM_REGION_SIZE                  0x8000UL
#define CP_ILM_RAM_REGION                       0x280000UL
#define CP_ILM_RAM_REGION_SIZE                  0x4000UL
#define CP_DLM_RAM_REGION                       0x300000UL
#define CP_DLM_RAM_REGION_SIZE                  0x2000UL

/************ CIPHER REGION **************************/
#define CP_CIPHER_REGION_A                      0x08000000UL
#define CP_CIPHER_REGION_A_SIZE                 0x8000000UL
#define CP_CIPHER_REGION_B                      0x10000000UL
#define CP_CIPHER_REGION_B_SIZE                 0x8000000UL
#define CP_CIPHER_REGION_C                      0x18000000UL
#define CP_CIPHER_REGION_D_SIZE                 0x4000000UL
#define CP_CIPHER_REGION_D                      0x1c000000UL
#define CP_CIPHER_REGION_D_SIZE                 0x4000000UL

/************ COMMON MEMORY **************************/
#define CMN_RAM0_REGION                         0x20040000UL           //0x2004_0000  0x2004_FFFF  64KB
#define CMN_RAM0_REGION_SIZE                    0x10000UL
#define CMN_RAM1_REGION                         0x20050000UL           //0x2005_0000  0x200A_FFFF  384KB
#define CMN_RAM1_REGION_SIZE                    0x60000UL

/************ WIFI MEMORY **************************/
#define WIFI_RAM_REGION                         0x20000000UL           //0x2000_0000  0x2003_FFFF  256KB  WIFI RAM region: RAM (actual size : 256KB)
#define WIFI_RAM_REGION_SIZE                    0x40000UL

/************ LUNA MEMORY **************************/
#define LUNA_RAM_REGION                         0x200B0000UL           //0x200B_0000  0x200B_FFFF  64KB   LUNA RAM region: RAM (actual size :24KB)
#define LUNA_RAM_REGION_SIZE                    0x6000UL

/************ BTEM MEMORY **************************/
#define BTEM_RAM_REGION                         0x200C0000UL           //0X200C_0000  0x200F_FFFF  256KB  BT_EM RAM region: RAM (actual size:32KB)
#define BTEM_RAM_REGION_SIZE                    0x8000UL

/************ PSRAM MEMORY **************************/
#define CMN_PSRAM_REGION                        0x28000000UL           //0x2800_0000  0x2FFF_FFFF  128MB  PSRAM  region: PSRAM (actual size:up to die)
#define CMN_PSRAM_REGION_SIZE                   0x8000000UL

/************ FLASH MEMORY **************************/
#define CMN_FLASH_REGION                        0x30000000UL           //0x3000_0000  0x37FF_FFFF  128MB  FLASH region: FLASH (actual size:up to die)
#define CMN_FLASH_REGION_SIZE                   0x8000000UL


/************ Peripheral ********************/
#define CP_DMAC_BASE           0x40000000UL // 0x4000_0000  0x40FF_FFFF  16MB    HS-9  DMAC CP-S
#define CP_DMAC_Channel0_BASE  (CP_DMAC_BASE + 0x0000)
#define CP_DMAC_Channel1_BASE  (CP_DMAC_BASE + 0x0058)
#define CP_DMAC_Channel2_BASE  (CP_DMAC_BASE + 0x00B0)
#define CP_DMAC_Channel3_BASE  (CP_DMAC_BASE + 0x0108)
//#define CP_DMAC_Channel4_BASE  (CP_DMAC_BASE + 0x0160)
//#define CP_DMAC_Channel5_BASE  (CP_DMAC_BASE + 0x01B8)
//#define CP_DMAC_Channel6_BASE  (CP_DMAC_BASE + 0x0210)
//#define CP_DMAC_Channel7_BASE  (CP_DMAC_BASE + 0x0268)
#define USBC_BASE              0x41000000UL // 0x4100_0000  0x41FF_FFFF  16MB  USBC  AHB-S
#define USB_DM_BASE            0x42000000UL // 0x4200_0000  0x42FF_FFFF  16MB  USB DEBUG AHB-S
#define SDIO_DEVICE_BASE       0x43000000UL // 0x4300_0000  0x43FF_FFFF  16MB  SDIO Device AHB-S
#define SECURITY_BASE          0x44000000UL // 0x4400_0000  0x447F_FFFF   8MB  SECURITY
#define AES_BASE               (SECURITY_BASE + 0x000000)
#define ECC_BASE               (SECURITY_BASE + 0x010000)
#define HSU_BASE               (SECURITY_BASE + 0x020000)
#define VIEDO_BASE             0x45000000UL // 0x4500_0000  0x457F_FFFF  8MB     VEDIO
#define AP_PERPH_BASE          0x45800000UL // 0x4580_0000  0x45FF_FFFF  8MB     HS15  AP Peripheral Wrap
#define CMN_PERPH_BASE         0x46000000UL // 0x4600_0000  0x47FF_FFFF  32MB    HS6  CMN Peripheral Wrap
#define AON_PERPH_BASE         0x48000000UL // 0x4800_0000  0x48FF_FFFF  16MB    HS6  AON Peripheral Wrap
#define LUNA_BASE              0x49000000UL // 0x4900_0000  0x49FF_FFFF  16MB    HS13  DMAC AP-S
#define BT_SUB_BASE            0x4A000000UL // 0x4A00_0000  0x4AFF_FFFF  16MB    BT_SUB Wrap
#define WIFI_SUB_BASE          0x4B000000UL // 0X4B00_0000  0x4BFF_FFFF  16MV    WIFI_SUB Wrap
#define WIFI_DMA               0x4B600000UL // 0X4B60_0000                       WIFI_DMA


/********** fine-grained map *****************/
#define BASE_ADDR_BT_DM       (BT_SUB_BASE)
#define BASE_ADDR_BT_BT       (BT_SUB_BASE + 0x400)
#define BASE_ADDR_BT_BLE      (BT_SUB_BASE + 0x800)
#define BASE_ADDR_BT_EM       (BTEM_RAM_REGION)
#define FREQ_TABLE_EM         (BASE_ADDR_BT_EM + 0x100) //freq table offset = 0x100
#define BASE_ADDR_HSU         (WIFI_SUB_BASE + 0x930000)
#define BT_CTRL_BASE           0x4A100000UL //0x4A10_0000  0x4A1F_FFFF  1MB  BT_CTRL
#define BT_MODEM_BASE          0x4A200000UL //0x4A20_0000  0x4A2F_FFFF  1MB  BT_MODEM


/********** AP Peripheral Wrap ***************/
#define AP_SDIOH_BASE          0x45A00000UL   //0x45A0_0000  0x45AF_FFFF  1MB  SDIOH    SDIOH
#define AP_APC_BASE            0x45B00000UL   //0x45B0_0000  0x45BF_FFFF  1MB  APC    APC
#define AP_CODEC_BASE          0x45C00000UL   //0x45C0_0000  0x45CF_FFFF  1MB  CODEC    CODEC
#define AP_PSRAM_CTRL_BASE     0x47B00000UL   //0x4550_0000  0x455F_FFFF  1MB  PSRAM_CTRL    PSRAM_CTRL


/********** CMN CP AON Peripheral Wrap ***************/
#define AP_CFG_BASE            0x45800000UL
#define GPDMA_BASE             0x45900000UL
#define CMN_SYS_BASE           0x46000000UL //0x4600_0000  0x460F_FFFF  1MB  CMN_SYS
#define CMN_SYS_NODFT          0x46100000UL //0x4610_0000  0x461F_FFFF  1MB  CMN_SYS_NODFT
#define DUALTIMERS0_BASE       0x46200000UL //0x4620_0000  0x462F_FFFF  1MB  DUALTIMERS0
#define DUALTIMERS1_BASE       0x46300000UL //0x4630_0000  0x463F_FFFF  1MB  DUALTIMERS1
#define CALENDAR_TOP_BASE      0x46400000UL //0x4640_0000  0x464F_FFFF  1MB  CALENDAR_TOP
#define TRNG_BASE              0x46500000UL //0x4650_0000  0x465F_FFFF  1MB  TRNG
#define GPADC_BASE             0x46600000UL //0x4660_0000  0x466F_FFFF  1MB  GPADC
#define GPIOA_BASE             0x46700000UL //0x4670_0000  0x467F_FFFF  1MB  GPIOA
#define GPIOB_BASE             0x46800000UL //0x4680_0000  0x468F_FFFF  1MB  GPIOB
#define QDEC_BASE              0x46900000UL //0x4690_0000  0x469F_FFFF  1MB  QDEC
#define UART0_BASE             0x46A00000UL //0x46A0_0000  0x46AF_FFFF  1MB  UART0
#define UART1_BASE             0x46B00000UL //0x46B0_0000  0x46BF_FFFF  1MB  UART1
#define UART2_BASE             0x46C00000UL //0x46C0_0000  0x46CF_FFFF  1MB  UART2
#define I2C0_BASE              0x46D00000UL //0x46D0_0000  0x46DF_FFFF  1MB  I2C0
#define I2C1_BASE              0x46E00000UL //0x46E0_0000  0x46EF_FFFF  1MB  I2C1
#define IR_BASE                0x46F00000UL //0x46F0_0000  0x46FF_FFFF  1MB  IR
#define SPI0_BASE              0x47000000UL //0x4700_0000  0x470F_FFFF  1MB  SPI0
#define SPI1_BASE              0x47100000UL //0x4710_0000  0x471F_FFFF  1MB  SPI1
#define SPI2_BASE              0x47200000UL //0x4720_0000  0x472F_FFFF  1MB  SPI2
#define GPT_BASE               0x47300000UL //0x4730_0000  0x473F_FFFF  1MB  GPT
#define CMN_MAILBOX_BASE       0x47400000UL //0x4740_0000  0x474F_FFFF  1MB  CMN_MAILBOX
#define CORE_IOMUX_BASE        0x47500000UL //0x4750_0000  0x475F_FFFF  1MB  CORE_IOMUX

#define FLASH_CTRL_BASE        0x47600000UL //0x4760_0000  0x476F_FFFF  1MB  FLASH_CTRL
#define FLASH_DL_BASE          0x47700000UL //0x4770_0000  0x477F_FFFF  1MB  FLASH_DL
#define WIFI_SYSCTRL_BASE      0x4B100000UL //0x4B40_0000  0x4B4F_FFFF  1MB  WIFI_SYSCTRL
#define WIFI_CRM_BASE          0x4B400000UL //0x4B40_0000  0x4B4F_FFFF  1MB  WIFI_CRM
#define WIFI_MAC_CORE_BASE     0x4B700000UL //0x4B70_0000  0x4B70_07FF  1MB  WIFI_MAC_CORE
#define WIFI_MAC_PL_BASE       0x4B708000UL //0x4B70_8000  0x4B70_8FFF  1MB  WIFI_MAC_PL
#define WIFI_MACBYPASS_BASE    0x4B900000UL //0x4B90_0000  0x4B90_FFFF  1MB  WiFi_MACBYPASS
#define WF_CTRL_BASE           0x4BB00000UL //0x4BB0_0000  0x4BBF_FFFF  1MB  WF_CTRL
#define WF_LA_BASE             0x4B500000UL //0x4BB0_0000  0x4BBF_FFFF  1MB  WF_LA
#define NEW_DFE_BASE           0x4BA00000UL //0x47B0_0000  0x47BF_FFFF  512KB  NEW_DFE
#define RF_IF_BASE             0x47A00000UL //0x47A0_0000  0x47AF_FFFF  1MB  RF_IF
//#define PSRAM_CTRL_BASE        0x47B00000UL //0x4780_0000  0x478F_FFFF  512KB PSRAM_CTRL
//#define Reserved               0x47D00000UL //0x47D0_0000  0x47DF_FFFF  1MB  Reserved
//#define Reserved               0x47E00000UL //0x47E0_0000  0x47EF_FFFF  1MB  Reserved
//#define Reserved               0x47F00000UL //0x47F0_0000  0x47FF_FFFF  1MB  Reserved
#define AON_CTRL_BASE          0x48000000UL //0x4800_0000  0x480F_FFFF  1MB  AON_CTRL
#define AON_IOMUX_BASE         0x48100000UL //0x4810_0000  0x481F_FFFF  1MB  AON_IOMUX
#define KEYSENSE0_BASE         0x48200000UL //0x4820_0000  0x482F_FFFF  1MB  KEYSENSE0
#define KEYSENSE1_BASE         0x48300000UL //0x4830_0000  0x483F_FFFF  1MB  KEYSENSE1
#define AON_TIMER_BASE         0x48400000UL //0x4840_0000  0x484F_FFFF  1MB  AON_TIMER(iwdt)
#define AP_WDT_BASE            0x45D00000UL
#define CP_WDT_BASE            0x47800000UL
#define AON_WDT_BASE           0x48500000UL
#define EFUSE_CTRL_BASE        0x48600000UL //0x4860_0000  0x486F_FFFF  1MB  EFUSE_CTRL
#define CALENDAR_BASE          0x46400000UL
#define IMAGE_PROC_BASE        (VIEDO_BASE + 0x0000)
#define DVP_BASE               (VIEDO_BASE + 0x0800)
#define VIC_BUF                (VIEDO_BASE + 0x1000)
#define JPEG_BASE              (VIEDO_BASE + 0x1800)
#define JPEG_CODEC_BASE        (VIEDO_BASE + 0x2000)
#define JPEG_PIXEL_BUF         (VIEDO_BASE + 0x2800)
#define JPEG_ECS_BUF           (VIEDO_BASE + 0x3000)
#define JPEG_HS_BUF            (VIEDO_BASE + 0x3800)
#define JPEG_HM_BUF            (VIEDO_BASE + 0x4000)
#define JPEG_HB_BUF            (VIEDO_BASE + 0x4800)
#define JPEG_QM_BUF            (VIEDO_BASE + 0x5000)
#define JPEG_DCT_BUF           (VIEDO_BASE + 0x5800)
#define JPEG_ZR0_BUF           (VIEDO_BASE + 0x6000)
#define JPEG_ZR1_BUF           (VIEDO_BASE + 0x6800)
#define JPEG_HENC_BUF          (VIEDO_BASE + 0x7000)
#define D2BLENDER_BASE         (VIEDO_BASE + 0x7800)
#define D2FORE_BUF             (VIEDO_BASE + 0x8000)
#define D2BACK_BUF             (VIEDO_BASE + 0x8800)
#define D2MASK_BUF             (VIEDO_BASE + 0x9000)
#define D2OUT_BUF              (VIEDO_BASE + 0x9800)
#define DISPLAY0_BUF           (VIEDO_BASE + 0xA000)
#define DISPLAY1_BUF           (VIEDO_BASE + 0xA800)
#define QSPI_SENSOR_IN_BASE    (VIEDO_BASE + 0xA000)
#define QSPI_LCD_BASE          (VIEDO_BASE + 0xA800)
#define RGB_BASE               (VIEDO_BASE + 0xB000)


/********** Alias **********/
#define CMN_SYSCTRL_BASE        CMN_SYS_BASE
#define CMN_IOMUX_BASE          CORE_IOMUX_BASE
#define GPIO0_BASE              GPIOA_BASE
#define GPIO1_BASE              GPIOB_BASE
#define CMN_FLASHC_BASE         FLASH_CTRL_BASE
#define SDIOH_BASE              AP_SDIOH_BASE
#define TIMER0_BASE             DUALTIMERS0_BASE
#define TIMER1_BASE             DUALTIMERS1_BASE
#define DMAC_BASE               CP_DMAC_BASE
#define AP_DMAC_BASE            CP_DMAC_BASE
#define SDIOD_BASE              SDIO_DEVICE_BASE


/************************************************************************************
 * IRQ Vector
 ************************************************************************************/
#define IRQ_Reserved0_VECTOR     0               /*!<  Internal reserved */
#define IRQ_Reserved1_VECTOR     1               /*!<  Internal reserved */
#define IRQ_Reserved2_VECTOR     2               /*!<  Internal reserved */
#define IRQ_Software_VECTOR      3               /*!<  Software interrupt */
#define IRQ_Reserved4_VECTOR     4               /*!<  Internal reserved */
#define IRQ_Reserved5_VECTOR     5               /*!<  Internal reserved */
#define IRQ_Reserved6_VECTOR     6               /*!<  Internal reserved */
#define IRQ_Timer_VECTOR         7               /*!<  Timer Interrupt */
#define IRQ_Reserved8_VECTOR     8               /*!<  Internal reserved */
#define IRQ_Reserved9_VECTOR     9               /*!<  Internal reserved */
#define IRQ_Reserved10_VECTOR   10               /*!<  Internal reserved */
#define IRQ_External_VECTOR     11               /*!<  External Interrupt */
#define IRQ_Reserved12_VECTOR   12               /*!<  Internal reserved */
#define IRQ_Reserved13_VECTOR   13               /*!<  Internal reserved */
#define IRQ_Reserved14_VECTOR   14               /*!<  Internal reserved */
#define IRQ_Reserved15_VECTOR   15               /*!<  Internal reserved */
#define IRQ_InterCore_VECTOR    16               /*!<  CIDU Inter Core Interrupt */
#define IRQ_Reserved17_VECTOR   17               /*!<  Internal reserved */
#define IRQ_Reserved18_VECTOR   18               /*!<  Internal reserved */

#define IRQ_DMAC_GP_VECTOR      19
#define IRQ_DMAC_VECTOR         20
#define IRQ_DMAC_AP_VECTOR      IRQ_DMAC_VECTOR
#define IRQ_AES_VECTOR          21
#define IRQ_ECC_VECTOR          22
#define IRQ_HSU_VECTOR          23
#define IRQ_SHA_VECTOR          IRQ_HSU_VECTOR
#define IRQ_USBC_VECTOR         24
#define IRQ_SDIOH_VECTOR        25
#define IRQ_SDIOD_VECTOR        26
#define IRQ_VIC_VECTOR          27
#define IRQ_FLASHC_VECTOR       28
#define IRQ_PSRAMC_VECTOR       29
#define IRQ_LUNA_VECTOR         30
#define IRQ_TIMER0_VECTOR       31
#define IRQ_CP_TIMER0_VECTOR    IRQ_TIMER0_VECTOR
#define IRQ_TIMER1_VECTOR       32
#define IRQ_CP_TIMER1_VECTOR    IRQ_TIMER1_VECTOR
#define IRQ_GPT_VECTOR          33
#define IRQ_CALENDAR_VECTOR     34
#define IRQ_TRNG_VECTOR         35
#define IRQ_GPADC_VECTOR        36
#define IRQ_GPIOA_VECTOR        37
#define IRQ_GPIOB_VECTOR        38
#define IRQ_QDEC_VECTOR         39
#define IRQ_UART0_VECTOR        40
#define IRQ_UART1_VECTOR        41
#define IRQ_UART2_VECTOR        42
#define IRQ_I2C0_VECTOR         43
#define IRQ_I2C1_VECTOR         44
#define IRQ_IR_VECTOR           45
#define IRQ_SPI0_VECTOR         46
#define IRQ_SPI1_VECTOR         47
#define IRQ_SPI2_VECTOR         48
#define IRQ_APC_VECTOR          49
#define IRQ_AON_KS0_VECTOR      50
#define IRQ_AON_KS1_VECTOR      51
#define IRQ_AON_TIMER_VECTOR    52
#define IRQ_AON_WDT_VECTOR      53
#define IRQ_AON_WKUP_VECTOR     54
#define IRQ_AON_EFUSE_VECTOR    55
#define IRQ_BT_VECTOR           56
#define IRQ_WF_VECTOR           57
#define IRQ_RCCAL_DONE_VECTOR   58
#define IRQ_RFIF_IRQn           59
#define IRQ_BTTXRXEN_VECTOR     60
#define IRQ_BT_DAC_TRIG_VECTOR  61
#define IRQ_MAILBOX_0_VECTOR    62
#define IRQ_MAILBOX_1_VECTOR    63
#define IRQ_MAILBOX_2_VECTOR    64
#define IRQ_MAILBOX_3_VECTOR    65
#define IRQ_WIFI_DBG_VECTOR     66
#define IRQ_HOST_VECTOR         67
#define IRQ_AP_WDT_VECTOR       68
#define IRQ_SOF_CNT_VECTOR      69
#define IRQ_IPC_VECTOR          70
#define IRQ_DVP_VECTOR          71
#define IRQ_JPG_VECTOR          72
#define IRQ_QSPI_IN_VECTOR      73
#define IRQ_QSPI_LCD_VECTOR     74
#define IRQ_WF_LP_WKUP_VECTOR   75
#define IRQ_RGB_VECTOR          76
#define IRQ_DMAC_GP_IMG_VECTOR  77
#define IRQ_MAX                 78


typedef enum IRQn {
    /* =======================================  Nuclei Core Specific Interrupt Numbers  ======================================== */

    Reserved0_IRQn            =   0,              /*!<  Internal reserved */
    Reserved1_IRQn            =   1,              /*!<  Internal reserved */
    Reserved2_IRQn            =   2,              /*!<  Internal reserved */
    SysTimerSW_IRQn           =   3,              /*!<  System Timer SW interrupt */
    Reserved3_IRQn            =   4,              /*!<  Internal reserved */
    Reserved4_IRQn            =   5,              /*!<  Internal reserved */
    Reserved5_IRQn            =   6,              /*!<  Internal reserved */
    SysTimer_IRQn             =   7,              /*!<  System Timer Interrupt */
    Reserved6_IRQn            =   8,              /*!<  Internal reserved */
    Reserved7_IRQn            =   9,              /*!<  Internal reserved */
    Reserved8_IRQn            =  10,              /*!<  Internal reserved */
    Reserved9_IRQn            =  11,              /*!<  Internal reserved */
    Reserved10_IRQn           =  12,              /*!<  Internal reserved */
    Reserved11_IRQn           =  13,              /*!<  Internal reserved */
    Reserved12_IRQn           =  14,              /*!<  Internal reserved */
    Reserved13_IRQn           =  15,              /*!<  Internal reserved */
    InterCore_IRQn            =  16,              /*!<  CIDU Inter Core Interrupt */
    Reserved15_IRQn           =  17,              /*!<  Internal reserved */
    Reserved16_IRQn           =  18,              /*!<  Internal reserved */

    /* ===========================================  demosoc Specific Interrupt Numbers  ========================================= */
    /* ToDo: add here your device specific external interrupt numbers. 19~1023 is reserved number for user. Maxmum interrupt supported
             could get from clicinfo.NUM_INTERRUPT. According the interrupt handlers defined in startup_Device.s
             eg.: Interrupt for Timer#1       eclic_tim1_handler   ->   TIM1_IRQn */
    SOC_INT19_IRQn           = 19,                /*!< Device Interrupt */
    SOC_INT20_IRQn           = 20,                /*!< Device Interrupt */
    SOC_INT21_IRQn           = 21,                /*!< Device Interrupt */
    SOC_INT22_IRQn           = 22,                /*!< Device Interrupt */
    SOC_INT23_IRQn           = 23,                /*!< Device Interrupt */
    SOC_INT24_IRQn           = 24,                /*!< Device Interrupt */
    SOC_INT25_IRQn           = 25,                /*!< Device Interrupt */
    SOC_INT26_IRQn           = 26,                /*!< Device Interrupt */
    SOC_INT27_IRQn           = 27,                /*!< Device Interrupt */
    SOC_INT28_IRQn           = 28,                /*!< Device Interrupt */
    SOC_INT29_IRQn           = 29,                /*!< Device Interrupt */
    SOC_INT30_IRQn           = 30,                /*!< Device Interrupt */
    SOC_INT31_IRQn           = 31,                /*!< Device Interrupt */
    SOC_INT32_IRQn           = 32,                /*!< Device Interrupt */
    SOC_INT33_IRQn           = 33,                /*!< Device Interrupt */
    SOC_INT34_IRQn           = 34,                /*!< Device Interrupt */
    SOC_INT35_IRQn           = 35,                /*!< Device Interrupt */
    SOC_INT36_IRQn           = 36,                /*!< Device Interrupt */
    SOC_INT37_IRQn           = 37,                /*!< Device Interrupt */
    SOC_INT38_IRQn           = 38,                /*!< Device Interrupt */
    SOC_INT39_IRQn           = 39,                /*!< Device Interrupt */
    SOC_INT40_IRQn           = 40,                /*!< Device Interrupt */
    SOC_INT41_IRQn           = 41,                /*!< Device Interrupt */
    SOC_INT42_IRQn           = 42,                /*!< Device Interrupt */
    SOC_INT43_IRQn           = 43,                /*!< Device Interrupt */
    SOC_INT44_IRQn           = 44,                /*!< Device Interrupt */
    SOC_INT45_IRQn           = 45,                /*!< Device Interrupt */
    SOC_INT46_IRQn           = 46,                /*!< Device Interrupt */
    SOC_INT47_IRQn           = 47,                /*!< Device Interrupt */
    SOC_INT48_IRQn           = 48,                /*!< Device Interrupt */
    SOC_INT49_IRQn           = 49,                /*!< Device Interrupt */
    SOC_INT50_IRQn           = 50,                /*!< Device Interrupt */
    SOC_INT51_IRQn           = 51,                /*!< Device Interrupt */
    SOC_INT52_IRQn           = 52,                /*!< Device Interrupt */
    SOC_INT53_IRQn           = 53,                /*!< Device Interrupt */
    SOC_INT54_IRQn           = 54,                /*!< Device Interrupt */
    SOC_INT55_IRQn           = 55,                /*!< Device Interrupt */
    SOC_INT56_IRQn           = 56,                /*!< Device Interrupt */
    SOC_INT57_IRQn           = 57,                /*!< Device Interrupt */
    SOC_INT58_IRQn           = 58,                /*!< Device Interrupt */
    SOC_INT59_IRQn           = 59,                /*!< Device Interrupt */
    SOC_INT60_IRQn           = 60,                /*!< Device Interrupt */
    SOC_INT61_IRQn           = 61,                /*!< Device Interrupt */
    SOC_INT62_IRQn           = 62,                /*!< Device Interrupt */
    SOC_INT63_IRQn           = 63,                /*!< Device Interrupt */
    SOC_INT64_IRQn           = 64,                /*!< Device Interrupt */
    SOC_INT65_IRQn           = 65,                /*!< Device Interrupt */
    SOC_INT66_IRQn           = 66,                /*!< Device Interrupt */
    SOC_INT67_IRQn           = 67,                /*!< Device Interrupt */
    SOC_INT68_IRQn           = 68,                /*!< Device Interrupt */
    SOC_INT69_IRQn           = 69,                /*!< Device Interrupt */
    SOC_INT70_IRQn           = 70,                /*!< Device Interrupt */
    SOC_INT71_IRQn           = 71,                /*!< Device Interrupt */
    SOC_INT72_IRQn           = 72,                /*!< Device Interrupt */
    SOC_INT73_IRQn           = 73,                /*!< Device Interrupt */
    SOC_INT74_IRQn           = 74,                /*!< Device Interrupt */
    SOC_INT75_IRQn           = 75,                /*!< Device Interrupt */
    SOC_INT76_IRQn           = 76,                /*!< Device Interrupt */
    SOC_INT77_IRQn           = 77,                /*!< Device Interrupt */

    SOC_INT_MAX,
} IRQn_Type;


#endif /* INCLUDE_ARCS_AP_BASE_H_ */
