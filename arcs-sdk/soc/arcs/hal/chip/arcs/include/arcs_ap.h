/*
 * arcs_ap.h
 *
 *  Created on:
 *
 */

#ifndef INCLUDE_ARCS_AP_H_
#define INCLUDE_ARCS_AP_H_

#include <stddef.h>
#include <stdint.h>
#include "arcs_ap_base.h"

/*****************************************************************************
 * Header for All peripherals
 ****************************************************************************/
#include "aon_ctrl_reg.h"
#include "aon_iomux_reg.h"
#include "core_iomux_reg.h"
#include "cmn_syscfg_reg.h"
#include "cmn_buscfg_reg.h"
#include "dual_timer_reg.h"
#include "gpio_reg.h"
#include "psram_mc_reg.h"
#include "rfif_reg.h"
#include "uart_reg.h"
#include "ap_cfg_reg.h"
// #include "dmac_reg.h" // conflict with current dma driver
#include "aon_timer_reg.h"
#include "usb_reg.h"
#include "sdioh_reg.h"
#include "sdiod_reg.h"
#include "gpt_reg.h"
#include "trng_reg.h"
#include "gpadc_reg.h"
//#include "qdec_reg.h"
#include "i2c_reg.h"
#include "ir_reg.h"
#include "spi_reg.h"
#include "qspi_lcd_reg.h"
#include "qspi_sensor_in_reg.h"
#include "image_vic_reg.h"
#include "jpeg_reg.h"
#include "image_d2blender_reg.h"
#include "rgb_interface_reg.h"
#include "apc_reg.h"
#include "audio_codec_reg.h"
#include "flashc_reg.h"
//#include "flash_dl_reg.h"
#include "bt_modem_reg.h"
#include "bt_ctrl_top_reg.h"
#include "crypto_aes_reg.h"
#include "crypto_ecc_reg.h"
#include "crypto_hsu_reg.h"
#include "keysense_reg.h"
#include "aon_wdt_reg.h"
#include "efuse_ctrl_reg.h"
//#include "aon_sleep_ctrl_reg.h"
#include "gp_dmac_reg.h"
#include "cmn_mailbox_reg.h"
#include "new_dfe_reg.h"
#include "wifi_ctrl_reg.h"
#include "wdt_reg.h"
#include "calendar_reg.h"
#include "wifi_crm_reg.h"
#include "wifi_mac_reg.h"
#include "wifi_mac_pl_reg.h"
#include "wifi_la_reg.h"



/** \brief CPU Internal Region Information */
typedef struct IRegion_Info {
    unsigned long iregion_base;         /*!< Internal region base address */
    unsigned long eclic_base;           /*!< eclic base address */
    unsigned long systimer_base;        /*!< system timer base address */
    unsigned long smp_base;             /*!< smp base address */
    unsigned long idu_base;             /*!< idu base address */
} IRegion_Info_Type;




/* =========================================================================================================================== */
/* ================                                  Exception Code Definition                                ================ */
/* =========================================================================================================================== */

typedef enum EXCn {
    /* =======================================  Nuclei N/NX Specific Exception Code  ======================================== */
    InsUnalign_EXCn          =   0,              /*!<  Instruction address misaligned */
    InsAccFault_EXCn         =   1,              /*!<  Instruction access fault */
    IlleIns_EXCn             =   2,              /*!<  Illegal instruction */
    Break_EXCn               =   3,              /*!<  Beakpoint */
    LdAddrUnalign_EXCn       =   4,              /*!<  Load address misaligned */
    LdFault_EXCn             =   5,              /*!<  Load access fault */
    StAddrUnalign_EXCn       =   6,              /*!<  Store or AMO address misaligned */
    StAccessFault_EXCn       =   7,              /*!<  Store or AMO access fault */
    UmodeEcall_EXCn          =   8,              /*!<  Environment call from User mode */
    SmodeEcall_EXCn          =   9,              /*!<  Environment call from S-mode */
    MmodeEcall_EXCn          =  11,              /*!<  Environment call from Machine mode */
    InsPageFault_EXCn        =  12,              /*!<  Instruction page fault */
    LdPageFault_EXCn         =  13,              /*!<  Load page fault */
    StPageFault_EXCn         =  15,              /*!<  Store or AMO page fault */
    NMI_EXCn                 =  0xfff,           /*!<  NMI interrupt */
} EXCn_Type;



#if __riscv_xlen == 32

#ifndef __NUCLEI_CORE_REV
#define __NUCLEI_N_REV            0x0104    /*!< Core Revision r1p4 */
#else
#define __NUCLEI_N_REV            __NUCLEI_CORE_REV
#endif

#elif __riscv_xlen == 64

#ifndef __NUCLEI_CORE_REV
#define __NUCLEI_NX_REV           0x0100    /*!< Core Revision r1p0 */
#else
#define __NUCLEI_NX_REV           __NUCLEI_CORE_REV
#endif

#endif /* __riscv_xlen == 64 */


extern volatile IRegion_Info_Type SystemIRegionInfo;


/* ToDo: define the correct core features for the mars */
#define __ECLIC_PRESENT           1                     /*!< Set to 1 if ECLIC is present */
#define __ECLIC_BASEADDR          SystemIRegionInfo.eclic_base          /*!< Set to ECLIC baseaddr of your device */

//#define __ECLIC_INTCTLBITS        3                     /*!< Set to 1 - 8, the number of hardware bits are actually implemented in the clicintctl registers. */
#define __ECLIC_INTNUM            51                    /*!< Set to 1 - 1024, total interrupt number of ECLIC Unit */
#define __SYSTIMER_PRESENT        1                     /*!< Set to 1 if System Timer is present */
#define __SYSTIMER_BASEADDR       SystemIRegionInfo.systimer_base          /*!< Set to SysTimer baseaddr of your device */

#define __CIDU_PRESENT            0                     /*!< Set to 1 if CIDU is present */
#define __CIDU_BASEADDR           SystemIRegionInfo.idu_base              /*!< Set to cidu baseaddr of your device */


/*!< Set to 0, 1, or 2, 0 not present, 1 single floating point unit present, 2 double floating point unit present */
#if !defined(__riscv_flen)
#define __FPU_PRESENT             0
#elif __riscv_flen == 32
#define __FPU_PRESENT             1
#else
#define __FPU_PRESENT             2
#endif


/* __riscv_bitmanip/__riscv_dsp/__riscv_vector is introduced
 * in nuclei gcc 10.2 when b/p/v extension compiler option is selected.
 * For example:
 * -march=rv32imacb -mabi=ilp32 : __riscv_bitmanip macro will be defined
 * -march=rv32imacp -mabi=ilp32 : __riscv_dsp macro will be defined
 * -march=rv64imacv -mabi=lp64 : __riscv_vector macro will be defined
 */
#if defined(__riscv_bitmanip)
#define __BITMANIP_PRESENT        1                     /*!< Set to 1 if Bitmainpulation extension is present */
#else
#define __BITMANIP_PRESENT        0                     /*!< Set to 1 if Bitmainpulation extension is present */
#endif
#if defined(__riscv_dsp)
#define __DSP_PRESENT             1                     /*!< Set to 1 if Partial SIMD(DSP) extension is present */
#else
#define __DSP_PRESENT             0                     /*!< Set to 1 if Partial SIMD(DSP) extension is present */
#endif
#if defined(__riscv_vector)
#define __VECTOR_PRESENT          1                     /*!< Set to 1 if Vector extension is present */
#else
#define __VECTOR_PRESENT          0                     /*!< Set to 1 if Vector extension is present */
#endif

#define __PMP_PRESENT             1                     /*!< Set to 1 if PMP is present */
#define __PMP_ENTRY_NUM           16                    /*!< Set to 8 or 16, the number of PMP entries */

#define __SPMP_PRESENT            0                     /*!< Set to 1 if SPMP is present */
#define __SPMP_ENTRY_NUM          16                    /*!< Set to 8 or 16, the number of SPMP entries */

#ifndef __TEE_PRESENT
#define __TEE_PRESENT             0                     /*!< Set to 1 if TEE is present */
#endif

#ifndef RUNMODE_CONTROL
#define __ICACHE_PRESENT          1                     /*!< Set to 1 if I-Cache is present */
#define __DCACHE_PRESENT          1                     /*!< Set to 1 if D-Cache is present */
#define __CCM_PRESENT             1                     /*!< Set to 1 if Cache Control and Mantainence Unit is present */
#endif

/* TEE feature depends on PMP */
#if defined(__TEE_PRESENT) && (__TEE_PRESENT == 1)
#if !defined(__PMP_PRESENT) || (__PMP_PRESENT != 1)
#error "__PMP_PRESENT must be defined as 1!"
#endif /* !defined(__PMP_PRESENT) || (__PMP_PRESENT != 1) */
#if !defined(__SPMP_PRESENT) || (__SPMP_PRESENT != 1)
#error "__SPMP_PRESENT must be defined as 1!"
#endif /* !defined(__SPMP_PRESENT) || (__SPMP_PRESENT != 1) */
#endif /* defined(__TEE_PRESENT) && (__TEE_PRESENT == 1) */

#ifndef __INC_INTRINSIC_API
#define __INC_INTRINSIC_API       0                     /*!< Set to 1 if intrinsic api header files need to be included */
#endif

#define __Vendor_SysTickConfig    0                     /*!< Set to 1 if different SysTick Config is used */
#define __Vendor_EXCEPTION        0                     /*!< Set to 1 if vendor exception hander is present */

/** @} */ /* End of group Configuration_of_NMSIS */


/* Define boot hart id */
#ifndef BOOT_HARTID
#define BOOT_HARTID               0                     /*!< Choosen boot hart id in current cluster when in soc system, need to align with the value defined in startup_<Device>.S, should start from 0, taken the mhartid bit 0-7 value */
#endif

#include <nmsis_core.h>                         /*!< Nuclei N/NX class processor and core peripherals */
/* ToDo: include your system_mars.h file
         replace 'Device' with your device name */
#include <nmsis_core.h>                         /*!< Nuclei N/NX class processor and core peripherals */
/* ToDo: include your system_mars.h file
         replace 'Device' with your device name */
#include "system_RISCVN300.h"                    /*!< riscv N300 System */



#ifndef   __COMPILER_BARRIER
  #define __COMPILER_BARRIER()                   __ASM volatile("":::"memory")
#endif


#define SOC_TIMER_FREQ              1000000



/*****************************************************************************
 * Macros for Register Access
 ****************************************************************************/
#define inw(reg)               (*((volatile unsigned int *) (reg)))
#define outw(reg, data)        ((*((volatile unsigned int *)(reg)))=(unsigned int)(data))
#define inb(reg)               (*((volatile unsigned char *) (reg)))
#define outb(reg, data)        ((*((volatile unsigned char *)(reg)))=(unsigned char)(data))

#ifndef __I
#define __I                     volatile const  /* 'read only' permissions      */
#endif
#ifndef __O
#define __O                     volatile        /* 'write only' permissions     */
#endif
#ifndef __IO
#define __IO                    volatile        /* 'read / write' permissions   */
#endif


/************************************************************************************
 * Linker definitions
 ************************************************************************************/
#define _AP_ILM_TEXT_SEC              ".text.ap_ilm"

#define _AP_DLM_DATA_SEC              ".data.ap_dlm"
#define _AP_DLM_BSS_SEC               ".bss.ap_dlm"

#define _CP_ILM_TEXT_SEC              ".text.cp_ilm"

#define _CP_DLM_DATA_SEC              ".data.cp_dlm"
#define _CP_DLM_BSS_SEC               ".bss.cp_dlm"

#define _WIFI_RAM_TEXT_SEC            ".text.wifi_ram"
#define _WIFI_RAM_DATA_SEC            ".data.wifi_ram"
#define _WIFI_RAM_BSS_SEC             ".bss.wifi_ram"

#define _CP_RAM_TEXT_SEC              ".text.cp_ram"
#define _CP_RAM_DATA_SEC              ".data.cp_ram"
#define _CP_RAM_BSS_SEC               ".bss.cp_ram"

#define _AP_RAM_TEXT_SEC              ".text.ap_ram"
#define _AP_RAM_DATA_SEC              ".data.ap_ram"
#define _AP_RAM_BSS_SEC               ".bss.ap_ram"

#define _TMP_RAM_SEC                  ".ramcode"
#define _TMP_PM_RAM_SEC               ".pm.ramcode"

#define _PM_TEXT_SEC                  ".text.pm"
/**
 * AP ILM TEXT SECTION
 */
#define _AP_ILM_TEXT              __attribute__ ((section (_AP_ILM_TEXT_SEC)))
#define _AP_ILM_TEXT_TAG(tag)     __attribute__ ((section (_AP_ILM_TEXT_SEC"."#tag)))

/**
 * AP DLM DATA AND BSS SECTION
 */
#define _AP_DLM_DATA              __attribute__ ((section (_AP_DLM_DATA_SEC)))
#define _AP_DLM_DATA_TAG(tag)     __attribute__ ((section (_AP_DLM_DATA_SEC"."#tag)))
#define _AP_DLM_BSS               __attribute__ ((section (_AP_DLM_BSS_SEC)))
#define _AP_DLM_BSS_TAG(tag)      __attribute__ ((section (_AP_DLM_BSS_SEC"."#tag)))

/**
 * CP ILM TEXT SECTION
 */
#define _CP_ILM_TEXT              __attribute__ ((section (_CP_ILM_TEXT_SEC)))
#define _CP_ILM_TEXT_TAG(tag)     __attribute__ ((section (_CP_ILM_TEXT_SEC"."#tag)))

/**
 * CP DLM DATA AND BSS SECTION
 */
#define _CP_DLM_DATA              __attribute__ ((section (_CP_DLM_DATA_SEC)))
#define _CP_DLM_DATA_TAG(tag)     __attribute__ ((section (_CP_DLM_DATA_SEC"."#tag)))
#define _CP_DLM_BSS               __attribute__ ((section (_CP_DLM_BSS_SEC)))
#define _CP_DLM_BSS_TAG(tag)      __attribute__ ((section (_CP_DLM_BSS_SEC"."#tag)))

/**
 * WIFI RAM SECTION
 */
#define _WIFI_RAM_TEXT              __attribute__ ((section (_WIFI_RAM_TEXT_SEC)))
#define _WIFI_RAM_TEXT_TAG(tag)     __attribute__ ((section (_WIFI_RAM_TEXT_SEC"."#tag)))
#define _WIFI_RAM_DATA              __attribute__ ((section (_WIFI_RAM_DATA_SEC)))
#define _WIFI_RAM_DATA_TAG(tag)     __attribute__ ((section (_WIFI_RAM_DATA_SEC"."#tag)))
#define _WIFI_RAM_BSS               __attribute__ ((section (_WIFI_RAM_BSS_SEC)))
#define _WIFI_RAM_BSS_TAG(tag)      __attribute__ ((section (_WIFI_RAM_BSS_SEC"."#tag)))

/**
 * CP RAM SECTION
 */
#define _CP_RAM_TEXT                __attribute__ ((section (_CP_RAM_TEXT_SEC)))
#define _CP_RAM_TEXT_TAG(tag)       __attribute__ ((section (_CP_RAM_TEXT_SEC"."#tag)))
#define _CP_RAM_DATA                __attribute__ ((section (_CP_RAM_DATA_SEC)))
#define _CP_RAM_DATA_TAG(tag)       __attribute__ ((section (_CP_RAM_DATA_SEC"."#tag)))
#define _CP_RAM_BSS                 __attribute__ ((section (_CP_RAM_BSS_SEC)))
#define _CP_RAM_BSS_TAG(tag)        __attribute__ ((section (_CP_RAM_BSS_SEC"."#tag)))

/**
 * AP RAM SECTION
 */
#define _AP_RAM_TEXT                __attribute__ ((section (_AP_RAM_TEXT_SEC)))
#define _AP_RAM_TEXT_TAG(tag)       __attribute__ ((section (_AP_RAM_TEXT_SEC"."#tag)))
#define _AP_RAM_DATA                __attribute__ ((section (_AP_RAM_DATA_SEC)))
#define _AP_RAM_DATA_TAG(tag)       __attribute__ ((section (_AP_RAM_DATA_SEC"."#tag)))
#define _AP_RAM_BSS                 __attribute__ ((section (_AP_RAM_BSS_SEC)))
#define _AP_RAM_BSS_TAG(tag)        __attribute__ ((section (_AP_RAM_BSS_SEC"."#tag)))

#define __RAMCODE__                 __attribute__ ((section (_TMP_RAM_SEC)))
#define _PM_TEXT_TEXT               __attribute__ ((section (_PM_TEXT_SEC)))
#define _PM_RAM_TEXT                __attribute__ ((section (_TMP_PM_RAM_SEC)))

//TODO: Adjust these sections
//fast function
#define _FAST_TEXT                  __attribute__ ((section (".fast_text")))
#define _FAST_TEXT_TAG(tag)         __attribute__ ((section (".fast_text""."#tag)))
//fast data (initialized)
#define _FAST_DATA                 __attribute__ ((section (".fast_data")))
#define _FAST_DATA_TAG(tag)        __attribute__ ((section (".fast_data""."#tag)))
//fast data (zero initialized)
#define _FAST_BSS                  __attribute__ ((section (".fast_bss")))
#define _FAST_BSS_TAG(tag)         __attribute__ ((section (".fast_bss""."#tag)))
#define _FAST_FUNC_RO

#ifdef CONFIG_ARCS_HAL_FAST_FUNC_SRAM_SECTION
#define _FAST_FUNC_SRAM           __attribute__ ((section (CONFIG_ARCS_HAL_FAST_FUNC_SRAM_SECTION)))
#else
#define _FAST_FUNC_SRAM           __attribute__ ((section (".fast_text")))
#endif

//fast data value initialize
//#define _FAST_DATA_VI           __attribute__ ((section (".data2")))
#define _FAST_DATA_VI
//fast data zero initialize
//#define _FAST_DATA_ZI           __attribute__ ((section (".bss2")))
#define _FAST_DATA_ZI

#define _DMA                    _FAST_DATA_ZI
#define _DMA_PRAM               __attribute__((aligned(32)))


/************************************************************************************
 * Cache definitions
 ************************************************************************************/
enum cache_t { ICACHE, DCACHE };

static inline unsigned long ICACHE_WAY(enum cache_t cache)
{
    return 2;
}

static inline unsigned long DCACHE_WAY(enum cache_t cache)
{
    return 4;
}

static inline unsigned long CACHE_LINE_SIZE(enum cache_t cache)
{
    return 32;
}

/** @addtogroup Peripheral_declaration
  * @{
  */

#define IP_SYSCTRL             ((CMN_SYSCFG_RegDef *) CMN_SYS_BASE)
#define IP_CMN_SYS             IP_SYSCTRL
#define IP_SYSNODEF            ((CMN_BUSCFG_RegDef *) CMN_SYS_NODFT)
// #define IP_DMA                 ((DMA_TypeDef *) DMAC_BASE)

#define IP_AP_CFG              ((AP_CFG_RegDef*) AP_CFG_BASE)
#define IP_GPDMA               ((GP_DMAC_RegDef*) GPDMA_BASE)
#define IP_DMA2D                IP_GPDMA
#define IP_CMN_IOMUX           ((CORE_IOMUX_RegDef *) CORE_IOMUX_BASE)
#define IP_AON_IOMUX           ((AON_IOMUX_RegDef *) AON_IOMUX_BASE)
#define IP_SDIOH               ((SDIOH_RegDef *) AP_SDIOH_BASE)
#define IP_SDIOD               ((SDIOD_RegDef *) SDIO_DEVICE_BASE)
#define IP_UART0               ((UART_RegDef *) UART0_BASE)
#define IP_UART1               ((UART_RegDef *) UART1_BASE)
#define IP_UART2               ((UART_RegDef *) UART2_BASE)
#define IP_I2C0                ((I2C_RegDef *) I2C0_BASE)
#define IP_I2C1                ((I2C_RegDef *) I2C1_BASE)
#define IP_SPI0                ((SPI_RegDef *) SPI0_BASE)
#define IP_SPI1                ((SPI_RegDef *) SPI1_BASE)
#define IP_SPI2                ((SPI_RegDef *) SPI2_BASE)
#define IP_FLASH_CTRL          ((FLASHC_RegDef *) FLASH_CTRL_BASE)
#define IP_AUDIO_APC           ((APC_RegDef *) AP_APC_BASE)
#define IP_AUDIO_CODEC         ((AUDIO_CODEC_RegDef *) AP_CODEC_BASE)
#define IP_GPIOA               ((GPIO_RegDef *) GPIOA_BASE)
#define IP_GPIOB               ((GPIO_RegDef *) GPIOB_BASE)
#define IP_AON_CTRL            ((AON_CTRL_RegDef *) AON_CTRL_BASE)
#define IP_BT_MODEM            ((BT_MODEM_RegDef *) BT_MODEM_BASE)
#define IP_WIFI_CTRL           ((WIFI_CTRL_RegDef*) WF_CTRL_BASE)
#define IP_JPEG                ((JPEG_RegDef *) JPEG_BASE)
#define IP_RFIF                ((RFIF_RegDef *) RF_IF_BASE)
#define IP_GPADC               ((GPADC_RegDef *) GPADC_BASE)
#define IP_IR                  ((IR_RegDef *) IR_BASE)
#define IP_EFUSE_CTRL          ((EFUSE_CTRL_RegDef*) EFUSE_CTRL_BASE)
#define IP_PSRAM_CTRL          ((PSRAM_MC_RegDef*) AP_PSRAM_CTRL_BASE)
#define IP_USBC                ((CSK_USB_RegDef *) USBC_BASE)
#define IP_AES                 ((CRYPTO_AES_RegDef *) AES_BASE)
//#define IP_SHA                 ((CRYPTO_SHA_RegDef *) SHA_BASE)
#define IP_ECC                 ((CRYPTO_ECC_RegDef *) ECC_BASE)
#define IP_HSU                 ((CRYPTO_HSU_RegDef *) HSU_BASE)
#define IP_AON_TIMER           ((AON_TIMER_RegDef *)AON_TIMER_BASE)
#define IP_BT_CTRL             ((BT_CTRL_TOP_RegDef *) BT_CTRL_BASE)
#define IP_AP_WDT              ((WDT_RegDef *) AP_WDT_BASE)
#define IP_CP_WDT              ((WDT_RegDef *) CP_WDT_BASE)
#define IP_AON_WDT             ((AON_WDT_RegDef *) AON_WDT_BASE)
#define IP_D2BLENDER           ((IMAGE_D2BLENDER_RegDef *)D2BLENDER_BASE)
#define IP_QSPI_SENSOR_IN      ((QSPI_SENSOR_IN_RegDef *)QSPI_SENSOR_IN_BASE)
#define IP_QSPI_LCD            ((QSPI_LCD_RegDef *)QSPI_LCD_BASE)
#define IP_DVP                 ((IMAGE_VIC_RegDef *)DVP_BASE)
#define IP_RGB                 ((RGB_INTERFACE_RegDef *)RGB_BASE)

#define IP_KEYSENSE0           ((KEYSENSE_RegDef *) KEYSENSE0_BASE)
#define IP_KEYSENSE1           ((KEYSENSE_RegDef *) KEYSENSE1_BASE)
#define IP_TIMER0              ((DUAL_TIMER_RegDef *) DUALTIMERS0_BASE)
#define IP_TIMER1              ((DUAL_TIMER_RegDef *) DUALTIMERS1_BASE)
#define IP_GPT                 ((GPT_RegDef *) GPT_BASE)
#define IP_MAILBOX             ((CMN_MAILBOX_RegDef *) CMN_MAILBOX_BASE)
#define IP_NEW_DFE             ((NEW_DFE_RegDef *)NEW_DFE_BASE)
#define IP_WIFI_CRM            ((WIFI_CRM_RegDef *)WIFI_CRM_BASE)
#define IP_WIFI_MAC_CORE       ((WIFI_MAC_RegDef *)WIFI_MAC_CORE_BASE)
#define IP_WIFI_MAC_PL         ((WIFI_MAC_PL_RegDef *)WIFI_MAC_PL_BASE)
#define IP_WF_CTRL             ((WIFI_CTRL_RegDef *)WF_CTRL_BASE)
#define IP_WF_LA               ((WIFI_LA_RegDef *)WF_LA_BASE)

#define IP_CALENDAR            ((CALENDAR_RegDef *)CALENDAR_BASE)
#define IP_TRNG                ((TRNG_RegDef*)  TRNG_BASE)


#ifndef __ASM
#define __ASM                   __asm     /*!< asm keyword for GNU Compiler */
#endif

#ifndef __INLINE
#define __INLINE                inline    /*!< inline keyword for GNU Compiler */
#endif

#ifndef __ALWAYS_STATIC_INLINE
#define __ALWAYS_STATIC_INLINE  __attribute__((always_inline)) static inline
#endif

#ifndef __STATIC_INLINE
#define __STATIC_INLINE         static inline
#endif

#define NMI_EXPn                (-2)      /* NMI Exception */



/**
  * @}
  */

/*****************************************************************************
 * System clock
 ****************************************************************************/

// IC_BOARD == 1 ---> ASIC
// IC_BOARD == 0 ---> FPGA

#if IC_BOARD // ASIC

#define DEF_MAIN_FREQUENCE    (300000000) // 300 MHz

static inline uint32_t CPUFREQ() {
    extern uint32_t CRM_GetCpuFreq();
    return CRM_GetCpuFreq();
}

static inline uint32_t HCLKFREQ() {
    extern uint32_t CRM_GetHclkFreq();
    return CRM_GetHclkFreq();
}

static inline uint32_t PCLKFREQ() {
    extern uint32_t CRM_GetAp_peri_pclkFreq();
    return CRM_GetAp_peri_pclkFreq();
}

#else // FPGA

#define DEF_MAIN_FREQUENCE    (24000000) // 24MHZ

static inline uint32_t CPUFREQ() {
    return (DEF_MAIN_FREQUENCE);
}

static inline uint32_t HCLKFREQ() {
    return (DEF_MAIN_FREQUENCE);
}

static inline uint32_t PCLKFREQ() {
    return (DEF_MAIN_FREQUENCE);
}

#endif // IC_BOARD

static inline uint32_t Chip_Type(){
    __RV_CSR_SET(CSR_MSTATUS, MSTATUS_FS);

    return ((__RV_CSR_READ(CSR_MSTATUS) & MSTATUS_FS) >> 13);
}


// evaluate frequency division coefficient: N / M (N <= M),
// the divided frequency CAN be more than, less than or equal to freq_out_req.
// return value: N @ high 16bits, M @ low 16bits. 0 indicates failure.
// *freq_out_p = freq_out_divided
uint32_t eval_freq_divNM(uint16_t N_max, uint16_t M_max, uint32_t freq_in,
                        uint32_t freq_out_req, int32_t *freq_out_p);

// evaluate frequency division coefficient: N / M (N <= M),
// the divided frequency CANNOT be more than freq_out_max!
// return value: N @ high 16bits, M @ low 16bits. 0 indicates failure.
// *freq_out_p = freq_out_divided
uint32_t eval_freq_divNM2(uint16_t N_max, uint16_t M_max, uint32_t freq_in,
                        uint32_t freq_out_max, int32_t *freq_out_p);

/* API for software to reboot/full_reset the chip */
void sys_platform_sw_full_reset(void);

/************************************************************************************
 * GI(Global Interrupt) and IRQ vector inline functions
 ************************************************************************************/
//The greater value of level, the higher priority
#define MAX_INTERRUPT_PRIORITY_RVAL     3
#define MID_INTERRUPT_PRIORITY          2
#define DEF_INTERRUPT_PRIORITY          0
#define DEF_INTERRUPT_LEVEL             0

// Is Global Interrupt enabled? 0 = disabled, 1 = enabled
static inline uint8_t GINT_enabled()
{
    return ((__RV_CSR_READ(CSR_MSTATUS) & MSTATUS_MIE) == MSTATUS_MIE);
}

// Enable GINT
static inline void enable_GINT()
{
	__RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
}

// Disable GINT
static inline void disable_GINT()
{
	__RV_CSR_CLEAR(CSR_MSTATUS, MSTATUS_MIE);
}

// ISR function prototype
typedef void (*ISR)(void);

// Register ISR into Interrupt Vector Table
void register_ISR(uint32_t irq_no, ISR isr, ISR* isr_old);


static inline uint32_t IRQ_enabled(uint32_t irq_no)
{
    return ECLIC_GetEnableIRQ((IRQn_Type)irq_no);
}

static inline void enable_IRQ(uint32_t irq_no)
{
	ECLIC_EnableIRQ((IRQn_Type)irq_no);
}

static inline void disable_IRQ(uint32_t irq_no)
{
	ECLIC_DisableIRQ((IRQn_Type)irq_no);
}

static inline void clear_IRQ(uint32_t irq_no)
{
	ECLIC_ClearPendingIRQ((IRQn_Type)irq_no);
}

void non_cacheable_region_enable(uint32_t base_addr, uint32_t len);

void non_cacheable_region_disable(void);

void device_region_enable(uint32_t base_addr, uint32_t len);

void device_region_disable(void);


/************************************************************************************
 * DMA definitions
 ************************************************************************************/
// TODO: modify DMA definitions accordingly

// Hardware handshaking ID of each peripheral with DMA Controller
// default configuration (ap_dma_hs_sel_x = 0)
#define DMA_HSID_IR_RX          0
#define DMA_HSID_IR_TX          1
#define DMA_HSID_UART1_RX       2
#define DMA_HSID_UART1_TX       3
#define DMA_HSID_SPI2_RX        4
#define DMA_HSID_SPI2_TX        5
#define DMA_HSID_SPI1_RX        6
#define DMA_HSID_SPI1_TX        7
#define DMA_HSID_UART0_RX       8
#define DMA_HSID_UART0_TX       9
#define DMA_HSID_SPI0_RX        10
#define DMA_HSID_SPI0_TX        11
#define DMA_HSID_GPT_RX0        12
#define DMA_HSID_GPT_TX0        13
#define DMA_HSID_GPT_RX1        14
#define DMA_HSID_GPT_TX1        15

// configuration (ap_dma_hs_sel_x = 1)
#define DMA_HSID1_UART2_RX      0
#define DMA_HSID1_UART2_TX      1
#define DMA_HSID1_GPT_RX2       2
#define DMA_HSID1_GPT_TX2       3
// 4 items gap
#define DMA_HSID1_SPI0_RX       8
#define DMA_HSID1_SPI0_TX       9
#define DMA_HSID1_GPADC         10
#define DMA_HSID1_I2C1          11
#define DMA_HSID1_GPT_RX3       12
#define DMA_HSID1_GPADC_1       13 // repeated
#define DMA_HSID1_GPT_TX3       14
#define DMA_HSID1_I2C0          15

// Number of DMA channels & FIFO depth of each channel
#define DMA_NUMBER_OF_CHANNELS           ((uint8_t) 4)
static const uint8_t DMA_CHANNELS_FIFO_DEPTH[DMA_NUMBER_OF_CHANNELS] = { 64, 64, 64, 64}; //in bytes

//default or preferred DMA channel definition
#define DMA_CH_UART_TX_DEF          ((uint8_t) 3)   // UART
#define DMA_CH_UART_RX_DEF          ((uint8_t) 0)   // UART
#define DMA_CH_SPI_TX_DEF           ((uint8_t) 2)   // SPI
#define DMA_CH_SPI_RX_DEF           ((uint8_t) 1)   // SPI

// AHB master interface of memory that stores LLI (Linked List Item) for channel
#define DMAH_CH_LMS     0


#define CP_DMA_Channel0        ((DMA_Channel_TypeDef *) CP_DMAC_Channel0_BASE)
#define CP_DMA_Channel1        ((DMA_Channel_TypeDef *) CP_DMAC_Channel1_BASE)
#define CP_DMA_Channel2        ((DMA_Channel_TypeDef *) CP_DMAC_Channel2_BASE)
#define CP_DMA_Channel3        ((DMA_Channel_TypeDef *) CP_DMAC_Channel3_BASE)
#define CP_DMA_Channel4        ((DMA_Channel_TypeDef *) CP_DMAC_Channel4_BASE)
#define CP_DMA_Channel5        ((DMA_Channel_TypeDef *) CP_DMAC_Channel5_BASE)
#define CP_DMA_Channel6        ((DMA_Channel_TypeDef *) CP_DMAC_Channel6_BASE)
#define CP_DMA_Channel7        ((DMA_Channel_TypeDef *) CP_DMAC_Channel7_BASE)
#define AP_DMA_Channel0        ((DMA_Channel_TypeDef *) AP_DMAC_Channel0_BASE)

extern void mpu_init( void );


#endif /* INCLUDE_ARCS_AP_H_ */

