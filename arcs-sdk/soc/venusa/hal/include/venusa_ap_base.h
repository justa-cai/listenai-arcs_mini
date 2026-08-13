#ifndef INCLUDE_VENUSA_AP_BASE_H_
#define INCLUDE_VENUSA_AP_BASE_H_

#define DMAC_BASE                                                0x40000000


#define USBC_BASE              0x41000000UL // 0x4100_0000  0x41FF_FFFF  16MB  USBC  AHB-S

#define CMN_BUSCFG_BASE                                          0x43000000
#define CMN_SYSCFG_BASE                                          0x43100000
#define DUALTIMERS0_BASE                                         0x43200000
#define DUALTIMERS1_BASE                                         0x43300000
#define CALENDAR_TOP_BASE                                        0x43400000
#define GPADC_BASE                                               0x43500000
#define GPIO0_BASE                                               0x43600000
#define GPIO1_BASE                                               0x43700000
#define UART0_BASE                                               0x43800000
#define UART1_BASE                                               0x43900000
#define UART2_BASE                                               0x43A00000
#define I2C0_BASE                                                0x43B00000
#define I2C1_BASE                                                0x43C00000
#define IR_BASE                                                  0x43D00000
#define SPI0_BASE                                                0x43E00000
#define SPI1_BASE                                                0x43F00000
#define GPT_BASE                                                 0x44000000
#define CORE_IOMUX_BASE                                          0x44100000
#define FLASH_CTRL_BASE                                          0x44200000
#define FLASH_DL_BASE                                            0x44300000
#define CORE0_WDT_BASE                                           0x44400000
#define CORE1_WDT_BASE                                           0x44500000
#define PSRAM_CTRL_BASE                                          0x44600000
#define APC_BASE                                                 0x44700000
#define CODEC_BASE                                               0x44800000
#define GPDMA2D_BASE                                             0x44900000
#define SDIOH_BASE                                               0x44A00000
#define AON_CTRL_BASE                                            0x45000000
#define AON_IOMUX_BASE                                           0x45100000
#define KEYSENSE0_BASE                                           0x45200000
#define AON_TIMER_BASE                                           0x45300000
#define AON_WDT_BASE                                             0x45400000
#define EFUSE_CTRL_BASE                                          0x45500000
#define DVP_IN_BASE                                              0x42000800
#define DVP_BUF_BASE                                             0x42001000
#define JPEG_TOP_BASE                                            0x42001800
#define JPEG_CODEC_BASE                                          0x42002000
#define JPEG_PIXEL_BUF                                           0x42002800
#define JPEG_ECS_BUF                                             0x42003000
#define JPEG_HS_BUF                                              0x42003800
#define JPEG_HM_BUF                                              0x42004000
#define JPEG_HB_BUF                                              0x42004800
#define JPEG_QM_BUF                                              0x42005000
#define JPEG_DCT_BUF                                             0x42005800
#define JPEG_ZR0_BUF                                             0x42006000
#define JPEG_ZR1_BUF                                             0x42006800
#define JPEG_HENC_BUF                                            0x42007000
#define QSPI_IN_BASE                                             0x4200A000
#define QSPI_OUT_BASE                                            0x4200A800
#define RGB_OUT_BASE                                             0x4200B000
#define I8080_OUT_BASE                                           0x4200B800

/********** VIDEO ***************/
#define VIEDO_BASE              0x42000000UL
#define DVP_BASE                (VIEDO_BASE + 0x0800)
#define DVP_BUF                 (VIEDO_BASE + 0x1000)
#define JPEG_BASE               (VIEDO_BASE + 0x1800)
#define QSPI_SENSOR_IN_BASE     (VIEDO_BASE + 0xA000)
#define QSPI_LCD_BASE           (VIEDO_BASE + 0xA800)
#define RGB_BASE                (VIEDO_BASE + 0xB000)
#define RGB_BUF                 (VIEDO_BASE + 0xB0F4)
#define I8080_BASE              (VIEDO_BASE + 0xB800)
#define I8080_TXBUF             (VIEDO_BASE + 0xC000)
#define I8080_RXBUF             (I8080_BASE + 0x0030)


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

#define IRQ_IDU_VECTOR                                                 19
#define IRQ_CMN_TIMER0_VECTOR                                          20
#define IRQ_CMN_TIMER1_VECTOR                                          21
#define IRQ_CALENDAR_VECTOR                                            22
#define IRQ_GPADC_VECTOR                                               23
#define IRQ_GPIO0_VECTOR                                               24
#define IRQ_GPIO1_VECTOR                                               25
#define IRQ_UART0_VECTOR                                               26
#define IRQ_UART1_VECTOR                                               27
#define IRQ_UART2_VECTOR                                               28
#define IRQ_I2C0_VECTOR                                                29
#define IRQ_I2C1_VECTOR                                                30
#define IRQ_IR_VECTOR                                                  31
#define IRQ_SPI0_VECTOR                                                32
#define IRQ_SPI1_VECTOR                                                33
#define IRQ_GPT_VECTOR                                                 34
#define IRQ_FLASHC_VECTOR                                              35
#define IRQ_CORE0_WDT_VECTOR                                           36
#define IRQ_CORE1_WDT_VECTOR                                           37
#define IRQ_PSRAMC_VECTOR                                              38
#define IRQ_SDIOH_VECTOR                                               39
#define IRQ_APC_0_VECTOR                                               40
#define IRQ_APC_1_VECTOR                                               41
#define IRQ_LUNA_VECTOR                                                42
#define IRQ_DWDMA_0_VECTOR                                             43
#define IRQ_DWDMA_1_VECTOR                                             44
#define IRQ_DWDMA_2_VECTOR                                             45
#define IRQ_DWDMA_3_VECTOR                                             46
#define IRQ_DWDMA_4_VECTOR                                             47
#define IRQ_DWDMA_5_VECTOR                                             48
#define IRQ_GPDMA_0_VECTOR                                             49
#define IRQ_GPDMA_1_VECTOR                                             50
#define IRQ_GPDMA_2_VECTOR                                             51
#define IRQ_GPDMA_3_VECTOR                                             52
#define IRQ_GPDMA_4_VECTOR                                             53
#define IRQ_GPDMA_5_VECTOR                                             54
#define IRQ_GPDMA_COM_VECTOR                                           55
#define IRQ_USBC_VECTOR                                                56
#define IRQ_SOF_CNT_VECTOR                                             560  //lyt, fake
#define IRQ_DVP_VECTOR                                                 57
#define IRQ_QSPI_IN_VECTOR                                             58
#define IRQ_QSPI_OUT_VECTOR                                            59
#define IRQ_RGB_VECTOR                                                 60
#define IRQ_JPEG_VECTOR                                                61
#define IRQ_I8080_VECTOR                                               62
#define IRQ_AON_EFUSE_VECTOR                                           63
#define IRQ_AON_TIMER_VECTOR                                           64
#define IRQ_AON_WDT_VECTOR                                             65
#define IRQ_KEYSENSE_VECTOR                                            66
#define IRQ_AON_WAKEUP_VECTOR                                          67
#define IRQ_SOF_CNT_IRQ_VECTOR                                         68
#define IRQ_MAX                 69


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

    SOC_INT_MAX,
} IRQn_Type;

// External IRQn ID(intr_id) is from the hard-wired perspective,
// which has an offset mapped to the ECLIC IRQn
#define ECLIC_IRQn_OFFSET_CIDU_INTn     SOC_INT20_IRQn // IRQ_TIMER0_VECTOR

#endif /* INCLUDE_VENUSA_AP_BASE_H_ */
