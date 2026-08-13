/**
 * @file clock_config.h
 * @brief Clock configuration definitions for the target board
 *
 * This header file contains all clock configuration definitions for the target board,
 * including PLL settings, core clocks, and peripheral clocks. It provides configuration
 * parameters for various clock domains with range checking to ensure valid settings.
 */

#ifndef INCLUDE_CLOCK_CONFIG_H_
#define INCLUDE_CLOCK_CONFIG_H_

/* Basic clock frequencies */
/** RC 32K clock frequency (32.768 kHz) */
#define BOARD_BOOTCLOCKRUN_RC032K_CLK                       32768UL
/** Crystal 32K clock frequency (32.768 kHz) */
#define BOARD_BOOTCLOCKRUN_XTAL32K_CLK                      32768UL
/** RC 24M clock frequency (24 MHz) */
#define BOARD_BOOTCLOCKRUN_RC024M_CLK                       24000000UL
/** Crystal 24M clock frequency (24 MHz) */
#define BOARD_BOOTCLOCKRUN_XTAL24M_CLK                      24000000UL

/* PLL Configuration */
/** System PLL enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CLK_DEF                   1
#define BOARD_BOOTCLOCKRUN_SYSPLL_DIVN_INTEG                50
#define BOARD_BOOTCLOCKRUN_SYSPLL_DIVN_FRAC                 0
/** System PLL output frequency (1.2 GHz) */
// 24Mhz * (int_n + frac_n/pow(2,27)) // Default 1.2Ghz
#define BOARD_BOOTCLOCKRUN_SYSPLL_CLK                       1200000000UL  // freq_xtal*n_prediv*(divn_integ + sdm_en*divn_frac/$pow(2,27))

/** System PLL core clock enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_DEF              1
/** System PLL core clock frequency (300 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK                  400000000UL
/** System PLL core clock configuration parameter */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA             CRM_IpSyspllCore_Div3_400MHz
/** Maximum allowed core clock frequency (400 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MAX              400000000UL
/** Minimum allowed core clock frequency (120 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MIN              120000000UL
#if(BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_MIN)
#error "CORE clock configure error, out of range!!!"
#endif

/** System PLL peripheral clock enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK_DEF              1
/** System PLL peripheral clock frequency (100 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK                  200000000UL
/** System PLL peripheral clock configuration parameter */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA             CRM_IpSyspllPeri_Div6_200MHz

/** System PLL flash clock enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_DEF             1
/** System PLL flash clock frequency (100 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK                 100000000UL
/** System PLL flash clock configuration parameter */
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA            CRM_IpSyspllFlash_Div12_100MHz
/** Maximum allowed flash clock frequency (133 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MAX             133000000UL
/** Minimum allowed flash clock frequency (75 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MIN             75000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_MIN)
#error "FLASH clock configure error, out of range!!!"
#endif

/** System PLL PSRAM clock enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_DEF             1
/** System PLL PSRAM clock frequency (200 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK                 240000000UL
/** System PLL PSRAM clock configuration parameter */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA            CRM_IpSyspllPsram_Div5_240MHz
/** Maximum allowed PSRAM clock frequency (400 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MAX             400000000UL
/** Minimum allowed PSRAM clock frequency (120 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MIN             120000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_MIN)
#error "PSRAM clock configure error, out of range!!!"
#endif

/** System PLL SDIO clock enable definition */
#define BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_DEF              1
/** System PLL SDIO clock frequency (200 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK                  200000000UL
/** System PLL SDIO clock configuration parameter */
#define BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CFG_PARA             CRM_IpSyspllSdio_Div6_200MHz
/** Maximum allowed SDIO clock frequency (400 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_MAX              400000000UL
/** Minimum allowed SDIO clock frequency (150 MHz) */
#define BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_MIN              150000000UL
#if (BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK > BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_MAX) || (BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK < BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_MIN)
#error "SDIO clock configure error, out of range!!!"
#endif

/* Core Clock Configuration */
/** HCLK enable definition */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_DEF                     1
/** HCLK clock source (System PLL core clock) */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_SRC                     CRM_IpSrcSyspllCore
/** Maximum HCLK divider N value */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_N_MAX                   15
/** HCLK divider N value */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_HCLK_CLK_N > BOARD_BOOTCLOCKRUN_HCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_HCLK_CLK_N == 0)
#error "hclk divider n configure error"
#endif
/** Maximum HCLK divider M value */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_M_MAX                   31
/** HCLK divider M value */
#define BOARD_BOOTCLOCKRUN_HCLK_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_HCLK_CLK_M > BOARD_BOOTCLOCKRUN_HCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_HCLK_CLK_M == 0)
#error "hclk divider m configure error"
#endif

/** Common peripheral PCLK enable definition */
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_DEF            1
/** Maximum common peripheral PCLK divider N value */
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N_MAX          15
/** Common peripheral PCLK divider N value */
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N              1
#if (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N > BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N == 0)
#error "cmn_peri_pclk divider n configure error"
#endif
/** Maximum common peripheral PCLK divider M value */
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M_MAX          31
/** Common peripheral PCLK divider M value */
#define BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M              4
#if (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M > BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M == 0)
#error "cmn_peri_pclk divider m configure error"
#endif

/** AON configuration PCLK enable definition */
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_DEF             1
/** Maximum AON configuration PCLK divider N value */
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N_MAX           31
/** AON configuration PCLK divider N value */
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N               1
#if (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N > BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N == 0)
#error "aon_cfg_pclk divider n configure error"
#endif
/** Maximum AON configuration PCLK divider M value */
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M_MAX           63
/** AON configuration PCLK divider M value */
#define BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M               8
#if (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M > BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M == 0)
#error "aon_cfg_pclk divider m configure error"
#endif

/* Peripheral Device Clock Configuration */
/** PSRAM clock enable definition */
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_DEF                    1
/** PSRAM clock source (System PLL PSRAM clock) */
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_SRC                    CRM_IpSrcSyspllPsram
/** Maximum PSRAM clock divider M value */
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_M_MAX                  1023
/** PSRAM clock divider M value */
#define BOARD_BOOTCLOCKRUN_PSRAM_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_PSRAM_CLK_M > BOARD_BOOTCLOCKRUN_PSRAM_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_PSRAM_CLK_M == 0)
#error "PSRAM divider m configure error"
#endif

/** MTIME clock enable definition */
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_DEF                    1
/** Maximum MTIME clock divider M value */
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_M_MAX                  63
/** MTIME clock divider M value */
#define BOARD_BOOTCLOCKRUN_MTIME_CLK_M                      48
#if (BOARD_BOOTCLOCKRUN_MTIME_CLK_M > BOARD_BOOTCLOCKRUN_MTIME_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_MTIME_CLK_M == 0)
#error "MTIME divider m configure error"
#endif

/** FLASH clock enable definition */
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_DEF                    1
/** FLASH clock source (System PLL flash clock) */
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_SRC                    CRM_IpSrcSyspllFlash
/** Maximum FLASH clock divider M value */
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_M_MAX                  31
/** FLASH clock divider M value */
#define BOARD_BOOTCLOCKRUN_FLASH_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_FLASH_CLK_M > BOARD_BOOTCLOCKRUN_FLASH_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_FLASH_CLK_M == 0)
#error "FLASH divider m configure error"
#endif

/** SPI0 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF                     1
#endif

/** SPI0 clock source (24 MHz core clock) */
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_SRC                     CRM_IpSrcCORE24M
/** Maximum SPI0 clock divider N value */
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_N_MAX                   7
/** SPI0 clock divider N value */
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_SPI0_CLK_N > BOARD_BOOTCLOCKRUN_SPI0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SPI0_CLK_N == 0)
#error "SPI0 divider n configure error"
#endif
/** Maximum SPI0 clock divider M value */
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_M_MAX                   15
/** SPI0 clock divider M value */
#define BOARD_BOOTCLOCKRUN_SPI0_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_SPI0_CLK_M > BOARD_BOOTCLOCKRUN_SPI0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SPI0_CLK_M == 0)
#error "SPI0 divider m configure error"
#endif

/* Peripheral Clock Configuration */

/* UART0 Configuration */
/** @brief UART0 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_UART0_CLK_DEF
#define BOARD_BOOTCLOCKRUN_UART0_CLK_DEF                    1
#endif

/**
 * @brief UART0 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_UART0_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for UART0 clock divider N */
#define BOARD_BOOTCLOCKRUN_UART0_CLK_N_MAX                  511
/** @brief UART0 clock divider N value */
#define BOARD_BOOTCLOCKRUN_UART0_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART0_CLK_N > BOARD_BOOTCLOCKRUN_UART0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART0_CLK_N == 0)
#error "UART0 divider n configure error"
#endif

/** @brief Maximum value for UART0 clock divider M */
#define BOARD_BOOTCLOCKRUN_UART0_CLK_M_MAX                  1023
/** @brief UART0 clock divider M value */
#define BOARD_BOOTCLOCKRUN_UART0_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART0_CLK_M > BOARD_BOOTCLOCKRUN_UART0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART0_CLK_M == 0)
#error "UART0 divider m configure error"
#endif

/* SPI1 Configuration */
/** @brief SPI1 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF                     1
#endif

/**
 * @brief SPI1 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_SRC                     CRM_IpSrcCORE24M

/** @brief Maximum value for SPI1 clock divider N */
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_N_MAX                   7
/** @brief SPI1 clock divider N value */
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_N                       1
#if (BOARD_BOOTCLOCKRUN_SPI1_CLK_N > BOARD_BOOTCLOCKRUN_SPI1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SPI1_CLK_N == 0)
#error "SPI1 divider n configure error"
#endif

/** @brief Maximum value for SPI1 clock divider M */
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_M_MAX                   15
/** @brief SPI1 clock divider M value */
#define BOARD_BOOTCLOCKRUN_SPI1_CLK_M                       1
#if (BOARD_BOOTCLOCKRUN_SPI1_CLK_M > BOARD_BOOTCLOCKRUN_SPI1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SPI1_CLK_M == 0)
#error "SPI1 divider m configure error"
#endif

/* UART1 Configuration */
/** @brief UART1 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_UART1_CLK_DEF
#define BOARD_BOOTCLOCKRUN_UART1_CLK_DEF                    1
#endif

/**
 * @brief UART1 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_UART1_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for UART1 clock divider N */
#define BOARD_BOOTCLOCKRUN_UART1_CLK_N_MAX                  511
/** @brief UART1 clock divider N value */
#define BOARD_BOOTCLOCKRUN_UART1_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART1_CLK_N > BOARD_BOOTCLOCKRUN_UART1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART1_CLK_N == 0)
#error "UART1 divider n configure error"
#endif

/** @brief Maximum value for UART1 clock divider M */
#define BOARD_BOOTCLOCKRUN_UART1_CLK_M_MAX                  1023
/** @brief UART1 clock divider M value */
#define BOARD_BOOTCLOCKRUN_UART1_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART1_CLK_M > BOARD_BOOTCLOCKRUN_UART1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART1_CLK_M == 0)
#error "UART1 divider m configure error"
#endif

/* SDIOH Configuration */
/** @brief SDIOH clock enable definition */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_DEF                    1

/**
 * @brief SDIOH clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllSdio
 */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_SRC                    CRM_IpSrcSyspllSdio

/** @brief Maximum value for SDIOH clock divider N */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_N_MAX                  7
/** @brief SDIOH clock divider N value */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_SDIOH_CLK_N > BOARD_BOOTCLOCKRUN_SDIOH_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_SDIOH_CLK_N == 0)
#error "SDIOH divider n configure error"
#endif

/** @brief Maximum value for SDIOH clock divider M */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_M_MAX                  15
/** @brief SDIOH clock divider M value */
#define BOARD_BOOTCLOCKRUN_SDIOH_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_SDIOH_CLK_M > BOARD_BOOTCLOCKRUN_SDIOH_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_SDIOH_CLK_M == 0)
#error "SDIOH divider m configure error"
#endif

/* UART2 Configuration */
/** @brief UART2 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_UART2_CLK_DEF
#define BOARD_BOOTCLOCKRUN_UART2_CLK_DEF                    1
#endif

/**
 * @brief UART2 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_UART2_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for UART2 clock divider N */
#define BOARD_BOOTCLOCKRUN_UART2_CLK_N_MAX                  511
/** @brief UART2 clock divider N value */
#define BOARD_BOOTCLOCKRUN_UART2_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_UART2_CLK_N > BOARD_BOOTCLOCKRUN_UART2_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_UART2_CLK_N == 0)
#error "UART2 divider n configure error"
#endif

/** @brief Maximum value for UART2 clock divider M */
#define BOARD_BOOTCLOCKRUN_UART2_CLK_M_MAX                  1023
/** @brief UART2 clock divider M value */
#define BOARD_BOOTCLOCKRUN_UART2_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_UART2_CLK_M > BOARD_BOOTCLOCKRUN_UART2_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_UART2_CLK_M == 0)
#error "UART2 divider m configure error"
#endif

/* VIC_OUT Configuration */
/** @brief VIC_OUT clock enable definition */
#define BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_DEF                  1

/**
 * @brief VIC_OUT clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_SRC                  CRM_IpSrcCORE24M

/** @brief Maximum value for VIC_OUT clock divider M */
#define BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M_MAX                511
/** @brief VIC_OUT clock divider M value */
#define BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M                    1
#if (BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M > BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M == 0)
#error "VIC_OUT divider m configure error"
#endif

/* GPT Configuration */
/** @brief GPT clock enable definition */
#define BOARD_BOOTCLOCKRUN_GPT_CLK_DEF                      1

/**
 * @note Clock is only generated from CORE24M source
 */

/** @brief Maximum value for GPT clock divider M */
#define BOARD_BOOTCLOCKRUN_GPT_CLK_M_MAX                    31
/** @brief GPT clock divider M value */
#define BOARD_BOOTCLOCKRUN_GPT_CLK_M                        1
#if (BOARD_BOOTCLOCKRUN_GPT_CLK_M > BOARD_BOOTCLOCKRUN_GPT_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_GPT_CLK_M == 0)
#error "GPT divider m configure error"
#endif

/* I8080 Configuration */
/** @brief I8080 clock enable definition */
#define BOARD_BOOTCLOCKRUN_I8080_CLK_DEF                    1

/**
 * @brief I8080 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_I8080_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for I8080 clock divider M */
#define BOARD_BOOTCLOCKRUN_I8080_CLK_M_MAX                  15
/** @brief I8080 clock divider M value */
#define BOARD_BOOTCLOCKRUN_I8080_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_I8080_CLK_M > BOARD_BOOTCLOCKRUN_I8080_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_I8080_CLK_M == 0)
#error "I8080 divider m configure error"
#endif

/* IR Configuration */
/** @brief IR clock enable definition */
#define BOARD_BOOTCLOCKRUN_IR_CLK_DEF                       1

/**
 * @note Clock is only generated from CORE24M source
 */

/** @brief Maximum value for IR clock divider M */
#define BOARD_BOOTCLOCKRUN_IR_CLK_M_MAX                     63
/** @brief IR clock divider M value */
#define BOARD_BOOTCLOCKRUN_IR_CLK_M                         4
#if (BOARD_BOOTCLOCKRUN_IR_CLK_M > BOARD_BOOTCLOCKRUN_IR_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_IR_CLK_M == 0)
#error "IR divider m configure error"
#endif

/* GPADC Configuration */
/** @brief GPADC clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF                    1
#endif

/**
 * @note Clock is only generated from CORE24M source
 */

/** @brief Maximum value for GPADC clock divider M */
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_M_MAX                  1023
/** @brief GPADC clock divider M value */
#define BOARD_BOOTCLOCKRUN_GPADC_CLK_M                      12
#if (BOARD_BOOTCLOCKRUN_GPADC_CLK_M > BOARD_BOOTCLOCKRUN_GPADC_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_GPADC_CLK_M == 0)
#error "GPADC divider m configure error"
#endif

/* I2S0 Configuration */
/** @brief I2S0 clock enable definition */
#define BOARD_BOOTCLOCKRUN_I2S0_CLK_DEF                     1

/**
 * @brief I2S0 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllAud
 */
#define BOARD_BOOTCLOCKRUN_I2S0_CLK_SRC                     CRM_IpSrcCORE24M

/* I2S1 Configuration */
/** @brief I2S1 clock enable definition */
#define BOARD_BOOTCLOCKRUN_I2S1_CLK_DEF                     1

/**
 * @brief I2S1 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllAud
 */
#define BOARD_BOOTCLOCKRUN_I2S1_CLK_SRC                     CRM_IpSrcCORE24M

/* RGB Configuration */
/** @brief RGB clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_RGB_CLK_DEF
#define BOARD_BOOTCLOCKRUN_RGB_CLK_DEF                      1
#endif

/**
 * @brief RGB clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_RGB_CLK_SRC                      CRM_IpSrcCORE24M

/** @brief Maximum value for RGB clock divider M */
#define BOARD_BOOTCLOCKRUN_RGB_CLK_M_MAX                    15
/** @brief RGB clock divider M value */
#define BOARD_BOOTCLOCKRUN_RGB_CLK_M                        1
#if (BOARD_BOOTCLOCKRUN_RGB_CLK_M > BOARD_BOOTCLOCKRUN_RGB_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_RGB_CLK_M == 0)
#error "RGB divider m configure error"
#endif

/* VIC Configuration */
/** @brief VIC clock enable definition */
#define BOARD_BOOTCLOCKRUN_VIC_CLK_DEF                      1

/**
 * @brief VIC clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllAud
 */
#define BOARD_BOOTCLOCKRUN_VIC_CLK_SRC                      CRM_IpSrcCORE24M

/* QSPI1 Configuration */
/** @brief QSPI1 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF                    1
#endif

/**
 * @brief QSPI1 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for QSPI1 clock divider N */
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_N_MAX                  7
/** @brief QSPI1 clock divider N value */
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_QSPI1_CLK_N > BOARD_BOOTCLOCKRUN_QSPI1_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_QSPI1_CLK_N == 0)
#error "QSPI1 divider n configure error"
#endif

/** @brief Maximum value for QSPI1 clock divider M */
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_M_MAX                  15
/** @brief QSPI1 clock divider M value */
#define BOARD_BOOTCLOCKRUN_QSPI1_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_QSPI1_CLK_M > BOARD_BOOTCLOCKRUN_QSPI1_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_QSPI1_CLK_M == 0)
#error "QSPI1 divider m configure error"
#endif

/* QSPI0 Configuration */
/** @brief QSPI0 clock enable definition */
#ifndef BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF                    1
#endif

/**
 * @brief QSPI0 clock source selection
 * @details Available options: CRM_IpSrcCORE24M, CRM_IpSrcSyspllPeri
 */
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_SRC                    CRM_IpSrcCORE24M

/** @brief Maximum value for QSPI0 clock divider N */
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_N_MAX                  7
/** @brief QSPI0 clock divider N value */
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_N                      1
#if (BOARD_BOOTCLOCKRUN_QSPI0_CLK_N > BOARD_BOOTCLOCKRUN_QSPI0_CLK_N_MAX) || (BOARD_BOOTCLOCKRUN_QSPI0_CLK_N == 0)
#error "QSPI0 divider n configure error"
#endif

/** @brief Maximum value for QSPI0 clock divider M */
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_M_MAX                  15
/** @brief QSPI0 clock divider M value */
#define BOARD_BOOTCLOCKRUN_QSPI0_CLK_M                      1
#if (BOARD_BOOTCLOCKRUN_QSPI0_CLK_M > BOARD_BOOTCLOCKRUN_QSPI0_CLK_M_MAX) || (BOARD_BOOTCLOCKRUN_QSPI0_CLK_M == 0)
#error "QSPI0 divider m configure error"
#endif

// DAC default configure
#define BOARD_BOOTCLOCKRUN_DAC_CLK_DEF                      1
/** DAC clock source (24 MHz core clock) */
#define BOARD_BOOTCLOCKRUN_DAC_CLK_SRC                      CRM_IpSrcCORE24M

/** ADC clock enable definition */
#define BOARD_BOOTCLOCKRUN_ADC_CLK_DEF                      1
/** ADC clock source (24 MHz core clock) */
#define BOARD_BOOTCLOCKRUN_ADC_CLK_SRC                      CRM_IpSrcCORE24M

#endif /* INCLUDE_CLOCK_CONFIG_H_ */
