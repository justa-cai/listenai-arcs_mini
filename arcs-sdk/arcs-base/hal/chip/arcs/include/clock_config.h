/*
 * clock_config.h
 *
 *  Created on: Aug 20, 2022
 *      Author: EASON
 */

#ifndef INCLUDE_BSP_CLOCK_CONFIG_ARCS_H_
#define INCLUDE_BSP_CLOCK_CONFIG_ARCS_H_

#define IC_BOARD_FPGA_FIX_FREQ                              24000000
#define IC_BOARD_XTAL_FREQ                                  24000000
#define IC_BOARD_CMN32K_FREQ                                32000

#define BOARD_BOOTCLOCKRUN_VCO_CLK_DEF                       1
#define BOARD_BOOTCLOCKRUN_VCO_CLK_N                         50
#define BOARD_BOOTCLOCKRUN_VCO_CLK_FRAC_N                    0

// PLL CONFIGURE************************************************************************
// VCO Frequency
#define BOARD_BOOTCLOCKRUN_SYSPLL_CLK_DEF                   1
#define BOARD_BOOTCLOCKRUN_SYSPLL_CLK                       1200000000UL
#define BOARD_BOOTCLOCKRUN_BBPLL_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_BBPLL_CLK                        960000000UL
// PLL CONFIGURE************************************************************************END

// SRC CONFIGURE************************************************************************
#define BOARD_BOOTCLOCKRUN_BBPLL_CORE_CLK_DEF               0
#define BOARD_BOOTCLOCKRUN_BBPLL_CORE_CLK                   160000000UL
#define BOARD_BOOTCLOCKRUN_BBPLL_CORE_CFG_PARA              BBPLL_CRM_IpCore_160Mhz
#define BOARD_BOOTCLOCKRUN_BBPLL_CORE_CLK_MAX               960000000UL
#define BOARD_BOOTCLOCKRUN_BBPLL_CORE_CLK_MIN               96000000UL 

#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_DEF              1
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK                  300000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA             CRM_IpCore_300MHz
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MAX              300000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MIN              100000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MIN)
#error "CORE clock configure error, out of range!!!"
#endif
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_DEF             1
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK                 240000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA            CRM_IpPsram_240MHz
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MAX             240000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MIN             100000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MIN)
#error "PSRAM clock configure error, out of range!!!"
#endif
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK_DEF               1
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK                   100000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA              CRM_IpPeri_100MHz

#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_DEF              1
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK                  200000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA             CRM_IpFlash_200MHz
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MAX              200000000UL
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MIN              50000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MIN)
#error "FLASH clock configure error, out of range!!!"
#endif

// CORE CONFIGURE***********************************************************************START
// cmn_peri_pclk default configure
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_DEF            1
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N_MAX          15 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N              1
#if (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N > BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N == 0)
#error "cmn_peri_pclk divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M_MAX          31 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M              3
#if (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M > BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M == 0)
#error "cmn_peri_pclk divider m configure error"
#endif

// aon_cfg_pclk default configure
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_DEF             1
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N_MAX           31 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N               1
#if (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N > BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N == 0)
#error "aon_cfg_pclk divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M_MAX           63 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M               6
#if (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M > BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M == 0)
#error "aon_cfg_pclk divider m configure error"
#endif

// ap_peri_pclk default configure
#define BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_DEF             1
#define BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N_MAX           15 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N               1
#if (BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N > BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N == 0)
#error "ap_peri_pclk divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M_MAX           31 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M               3
#if (BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M > BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M == 0)
#error "ap_peri_pclk divider m configure error"
#endif

// hclk default configure
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_DEF                     1
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_SRC                     CRM_IpSrcCoreClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcCoreClk']
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_N_MAX                   15 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_HCLK_CLK_N > BOARD_BOOTCLOCKRUN_HCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_HCLK_CLK_N == 0)
#error "hclk divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_M_MAX                   31 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_HCLK_CLK_M > BOARD_BOOTCLOCKRUN_HCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_HCLK_CLK_M == 0)
#error "hclk divider m configure error"
#endif

// cpu default configure
#define BOARD_BOOTCLOCKRUN_CPU_CLK_DEF                      1

// CORE CONFIGURE***********************************************************************END

// DEVICE CONFIGURE*********************************************************************START
// psram default configure
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_SRC                    CRM_IpSrcPsramClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_M_MAX                  31 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_PSRAM_CLK_M > BOARD_BOOTCLOCKRUN_PSRAM_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_PSRAM_CLK_M == 0)
#error "psram divider m configure error"
#endif
// flash default configure
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_SRC                    CRM_IpSrcFlashClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcFlashClk']
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_M_MAX                  31 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_M                      2
#if (BOARD_BOOTCLOCKRUN_FLASH_CLK_M > BOARD_BOOTCLOCKRUN_FLASH_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_FLASH_CLK_M == 0)
#error "flash divider m configure error"
#endif
// mtime default configure
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_M_MAX                  63 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_M                      24
#if (BOARD_BOOTCLOCKRUN_MTIME_CLK_M > BOARD_BOOTCLOCKRUN_MTIME_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_MTIME_CLK_M == 0)
#error "mtime divider m configure error"
#endif
// spi0 default configure
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF                     1
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_SRC                     CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_N_MAX                   7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_SPI0_CLK_N > BOARD_BOOTCLOCKRUN_SPI0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SPI0_CLK_N == 0)
#error "spi0 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_M_MAX                   15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_SPI0_CLK_M > BOARD_BOOTCLOCKRUN_SPI0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SPI0_CLK_M == 0)
#error "spi0 divider m configure error"
#endif
// uart0 default configure
#define BOARD_BOOTCLOCKRUN_UART0_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_UART0_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_UART0_CLK_N_MAX                  511 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_UART0_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART0_CLK_N > BOARD_BOOTCLOCKRUN_UART0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART0_CLK_N == 0)
#error "uart0 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_UART0_CLK_M_MAX                  1023 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_UART0_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART0_CLK_M > BOARD_BOOTCLOCKRUN_UART0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART0_CLK_M == 0)
#error "uart0 divider m configure error"
#endif
// spi1 default configure
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF                     1
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_SRC                     CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_N_MAX                   7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_SPI1_CLK_N > BOARD_BOOTCLOCKRUN_SPI1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SPI1_CLK_N == 0)
#error "spi1 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_M_MAX                   15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_SPI1_CLK_M > BOARD_BOOTCLOCKRUN_SPI1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SPI1_CLK_M == 0)
#error "spi1 divider m configure error"
#endif
// uart1 default configure
#define BOARD_BOOTCLOCKRUN_UART1_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_UART1_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_UART1_CLK_N_MAX                  511 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_UART1_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART1_CLK_N > BOARD_BOOTCLOCKRUN_UART1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART1_CLK_N == 0)
#error "uart1 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_UART1_CLK_M_MAX                  1023 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_UART1_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART1_CLK_M > BOARD_BOOTCLOCKRUN_UART1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART1_CLK_M == 0)
#error "uart1 divider m configure error"
#endif
// spi2 default configure
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_DEF                     1
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_SRC                     CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_N_MAX                   7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_SPI2_CLK_N > BOARD_BOOTCLOCKRUN_SPI2_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SPI2_CLK_N == 0)
#error "spi2 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_M_MAX                   15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_SPI2_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_SPI2_CLK_M > BOARD_BOOTCLOCKRUN_SPI2_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SPI2_CLK_M == 0)
#error "spi2 divider m configure error"
#endif
// uart2 default configure
#define BOARD_BOOTCLOCKRUN_UART2_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_UART2_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_UART2_CLK_N_MAX                  511 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_UART2_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART2_CLK_N > BOARD_BOOTCLOCKRUN_UART2_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART2_CLK_N == 0)
#error "uart2 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_UART2_CLK_M_MAX                  1023 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_UART2_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART2_CLK_M > BOARD_BOOTCLOCKRUN_UART2_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART2_CLK_M == 0)
#error "uart2 divider m configure error"
#endif
// gpt_t0 default configure
#define BOARD_BOOTCLOCKRUN_GPT_T0_CLK_DEF                   1
#define BOARD_BOOTCLOCKRUN_GPT_T0_CLK_SRC                   CRM_IpSrcXtalClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M_MAX                 15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M                     1
#if (BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M > BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M == 0)
#error "gpt_t0 divider m configure error"
#endif
// gpt_s default configure
#define BOARD_BOOTCLOCKRUN_GPT_S_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_GPT_S_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_GPT_S_CLK_M_MAX                  15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_GPT_S_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_GPT_S_CLK_M > BOARD_BOOTCLOCKRUN_GPT_S_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_GPT_S_CLK_M == 0)
#error "gpt_s divider m configure error"
#endif
// gpadc default configure
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_M_MAX                  1023 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_GPADC_CLK_M > BOARD_BOOTCLOCKRUN_GPADC_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_GPADC_CLK_M == 0)
#error "gpadc divider m configure error"
#endif
// ir_tx default configure
#define BOARD_BOOTCLOCKRUN_IR_TX_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_IR_TX_CLK_SRC                    CRM_IpSrcXtalClk // Clock source configure
#define BOARD_BOOTCLOCKRUN_IR_TX_CLK_M_MAX                  63 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_IR_TX_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_IR_TX_CLK_M > BOARD_BOOTCLOCKRUN_IR_TX_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_IR_TX_CLK_M == 0)
#error "ir_tx divider m configure error"
#endif
// rgb default configure
#define BOARD_BOOTCLOCKRUN_RGB_CLK_DEF                      1
#define BOARD_BOOTCLOCKRUN_RGB_CLK_SRC                      CRM_IpSrcXtalClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcPeriClk']
#define BOARD_BOOTCLOCKRUN_RGB_CLK_M_MAX                    7 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_RGB_CLK_M                        1
#if (BOARD_BOOTCLOCKRUN_RGB_CLK_M > BOARD_BOOTCLOCKRUN_RGB_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_RGB_CLK_M == 0)
#error "rgb divider m configure error"
#endif
// sdio_h default configure
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_DEF                   1
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_SRC                   CRM_IpSrcFlashClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcFlashClk']
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N_MAX                 7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N                     1
#if (BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N > BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N == 0)
#error "sdio_h divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M_MAX                 15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M                     1
#if (BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M > BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M == 0)
#error "sdio_h divider m configure error"
#endif
// qspi0 default configure
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_SRC                    CRM_IpSrcFlashClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcFlashClk']
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_N_MAX                  7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_QSPI0_CLK_N > BOARD_BOOTCLOCKRUN_QSPI0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_QSPI0_CLK_N == 0)
#error "qspi0 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_M_MAX                  15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_M                      2
#if (BOARD_BOOTCLOCKRUN_QSPI0_CLK_M > BOARD_BOOTCLOCKRUN_QSPI0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_QSPI0_CLK_M == 0)
#error "qspi0 divider m configure error"
#endif
// qspi1 default configure
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF                    1
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_SRC                    CRM_IpSrcFlashClk // Clock source configure, you can choice -> ['CRM_IpSrcXtalClk', 'CRM_IpSrcFlashClk']
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_N_MAX                  7 // Clock divider_n
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_QSPI1_CLK_N > BOARD_BOOTCLOCKRUN_QSPI1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_QSPI1_CLK_N == 0)
#error "qspi1 divider n configure error"
#endif
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_M_MAX                  15 // Clock divider_m
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_M                      2
#if (BOARD_BOOTCLOCKRUN_QSPI1_CLK_M > BOARD_BOOTCLOCKRUN_QSPI1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_QSPI1_CLK_M == 0)
#error "qspi1 divider m configure error"
#endif
// DEVICE CONFIGURE*********************************************************************END
#endif
