/**
 * @file     venusa_ap.h
 * @brief    NMSIS Core Peripheral Access Layer Header File for
 *           Nuclei Demo SoC which support Nuclei N/NX class cores
 * @version  V1.00
 * @date     22. Nov 2019
*/
#ifndef INCLUDE_VENUSA_AP_H_
#define INCLUDE_VENUSA_AP_H_

#include <stddef.h>
#include <stdint.h>
#include "venusa_ap_base.h"
#include "mmio.h"

/** @defgroup VENUSA_Processor VenusA Processor
  * @brief VenusA Processor Core and Peripherals
  * @{
  */

/** @defgroup VENUSA_Peripheral_Registers Peripheral Registers
  * @brief Peripheral Register Definitions
  * @{
  */

/** @defgroup VENUSA_AON_Registers Always-On (AON) Registers
  * @brief Always-On Domain Registers
  * @{
  * @file aon_ctrl_reg.h
  * @file aon_iomux_reg.h
  * @file aon_timer_reg.h
  * @file aon_wdt_reg.h
  */
#include "aon_timer_reg.h"
#include "aon_wdt_reg.h"
#include "aon_ctrl_reg.h"
#include "aon_iomux_reg.h"
/** @} */ /* End of group VENUSA_AON_Registers */

/** @defgroup VENUSA_CMN_Registers Common Registers
  * @brief Common System Registers
  * @{
  * @file cmn_buscfg_reg.h
  * @file cmn_syscfg_reg.h
  */
#include "cmn_buscfg_reg.h"
#include "cmn_syscfg_reg.h"
/** @} */ /* End of group VENUSA_CMN_Registers */

/** @defgroup VENUSA_IO_Registers Input/Output Registers
  * @brief GPIO and IOMUX Registers
  * @{
  * @file core_iomux_reg.h
  * @file gpio_reg.h
  */
#include "core_iomux_reg.h"
#include "gpio_reg.h"
/** @} */ /* End of group VENUSA_IO_Registers */

/** @defgroup VENUSA_Communication_Registers Communication Interface Registers
  * @brief Communication Peripheral Registers
  * @{
  * @file i2c_reg.h
  * @file spi_reg.h
  * @file uart_reg.h
  * @file ir_reg.h
  */
#include "i2c_reg.h"
#include "spi_reg.h"
#include "uart_reg.h"
#include "ir_reg.h"
/** @} */ /* End of group VENUSA_Communication_Registers */

/** @defgroup VENUSA_Timer_Registers Timer and RTC Registers
  * @brief Timer, PWM and Calendar Registers
  * @{
  * @file dual_timer_reg.h
  * @file gpt_reg.h
  * @file calendar_reg.h
  */
#include "dual_timer_reg.h"
#include "gpt_reg.h"
#include "calendar_reg.h"
/** @} */ /* End of group VENUSA_Timer_Registers */

/** @defgroup VENUSA_Audio_Registers Audio Processing Registers
  * @brief Audio Codec and Processing Registers
  * @{
  * @file apc_reg.h
  * @file audio_codec_reg.h
  */
#include "apc_reg.h"
#include "audio_codec_reg.h"
/** @} */ /* End of group VENUSA_Audio_Registers */

/** @defgroup VENUSA_ADC_Registers Analog-to-Digital Converter Registers
  * @brief ADC and Sensor Interface Registers
  * @{
  * @file gpadc_reg.h
  */
#include "gpadc_reg.h"
/** @} */ /* End of group VENUSA_ADC_Registers */

/** @defgroup VENUSA_Flash_Registers Flash Memory Registers
  * @brief Flash Controller and Download Registers
  * @{
  * @file flashc_reg.h
  * @file flash_dl_reg.h
  */
#include "flashc_reg.h"
#include "flash_dl_reg.h"
/** @} */ /* End of group VENUSA_Flash_Registers */

/** @defgroup VENUSA_Watchdog_Registers Watchdog Timer Registers
  * @brief Watchdog Timer Registers
  * @{
  * @file wdt_reg.h
  */
#include "wdt_reg.h"
/** @} */ /* End of group VENUSA_Watchdog_Registers */

/** @defgroup VENUSA_Memory_Controller_Registers Memory Controller Registers
  * @brief Memory Controller and DMA Registers
  * @{
  * @file psram_mc_reg.h
  * @file gpdma2d_reg.h
  */
#include "psram_mc_reg.h"
#include "gpdma2d_reg.h"
/** @} */ /* End of group VENUSA_Memory_Controller_Registers */

/** @defgroup VENUSA_Storage_Interface_Registers Storage Interface Registers
  * @brief SDIO and Storage Interface Registers
  * @{
  * @file sdioh_reg.h
  */
#include "sdioh_reg.h"
/** @} */ /* End of group VENUSA_Storage_Interface_Registers */

/** @defgroup VENUSA_Input_Device_Registers Input Device Registers
  * @brief Keypad and Input Device Registers
  * @{
  * @file keysense_reg.h
  */
#include "keysense_reg.h"
/** @} */ /* End of group VENUSA_Input_Device_Registers */

/** @defgroup VENUSA_Security_Registers Security Registers
  * @brief Security and eFuse Registers
  * @{
  * @file efuse_ctrl_reg.h
  */
#include "efuse_ctrl_reg.h"
/** @} */ /* End of group VENUSA_Security_Registers */

/** @defgroup VENUSA_Image_Processing_Registers Image Processing Registers
  * @brief Image and Video Processing Registers
  * @{
  * @file image_vic_reg.h
  * @file jpeg_reg.h
  */
#include "image_vic_reg.h"
#include "jpeg_reg.h"
/** @} */ /* End of group VENUSA_Image_Processing_Registers */

/** @defgroup VENUSA_Display_Interface_Registers Display Interface Registers
  * @brief Display and LCD Interface Registers
  * @{
  * @file qspi_sensor_in_reg.h
  * @file qspi_lcd_reg.h
  * @file rgb_interface_reg.h
  * @file i8080_out_reg.h
  */
#include "qspi_sensor_in_reg.h"
#include "qspi_lcd_reg.h"
#include "rgb_interface_reg.h"
#include "i8080_out_reg.h"
/** @} */ /* End of group VENUSA_Display_Interface_Registers */

/** @defgroup VENUSA_USB_Registers USB Controller Registers
  * @brief USB Interface Registers
  * @{
  * @file usb_reg.h
  */
#include "usb_reg.h"
/** @} */ /* End of group VENUSA_USB_Registers */

/** @} */ /* End of group VENUSA_Peripheral_Registers */

/** @defgroup VENUSA_IP_Definitions IP Peripheral Definitions
  * @brief IP Peripheral Base Address Definitions and Register Access Macros
  * @details This group contains all the IP peripheral base address definitions
  *          and register access macros for the VenusA processor.
  * @{
  */

/** @defgroup VENUSA_Bus_Control_IP Bus Control IP Definitions
  * @brief Bus Configuration and System Control IPs
  * @{
  */
#define IP_CMN_BUSCFG                                          ((CMN_BUSCFG_RegDef *) CMN_BUSCFG_BASE)  /*!< Common Bus Configuration IP */
#define IP_CMN_SYSCFG                                          ((CMN_SYSCFG_RegDef *) CMN_SYSCFG_BASE)  /*!< Common System Configuration IP */
#define IP_SYSCTRL                                             IP_CMN_SYSCFG                           /*!< System Control IP (alias) */
#define IP_SYSNODEF                                            IP_CMN_BUSCFG                           /*!< System Node Definition IP (alias) */
#define IP_CMN_SYS                                             IP_SYSCTRL                              /*!< Common System IP (alias) */
/** @} */ /* End of group VENUSA_Bus_Control_IP */

/** @defgroup VENUSA_Timer_IP Timer IP Definitions
  * @brief Timer and RTC IP Definitions
  * @{
  */
#define IP_DUALTIMERS0                                         ((DUAL_TIMER_RegDef *) DUALTIMERS0_BASE) /*!< Dual Timer 0 IP */
#define IP_DUALTIMERS1                                         ((DUAL_TIMER_RegDef *) DUALTIMERS1_BASE) /*!< Dual Timer 1 IP */
#define IP_CALENDAR_TOP                                        ((CALENDAR_RegDef *) CALENDAR_TOP_BASE)  /*!< Calendar/RTC IP */
#define IP_GPT                                                 ((GPT_RegDef *) GPT_BASE)                /*!< General Purpose Timer IP */
#define IP_AON_TIMER                                           ((AON_TIMER_RegDef *) AON_TIMER_BASE)    /*!< Always-On Timer IP */
/** @} */ /* End of group VENUSA_Timer_IP */

/** @defgroup VENUSA_GPIO_IP GPIO IP Definitions
  * @brief GPIO and IOMUX IP Definitions
  * @{
  */
#define IP_GPIO0                                               ((GPIO_RegDef *) GPIO0_BASE)            /*!< GPIO Port 0 IP */
#define IP_GPIO1                                               ((GPIO_RegDef *) GPIO1_BASE)            /*!< GPIO Port 1 IP */
#define IP_CORE_IOMUX                                          ((CORE_IOMUX_RegDef *) CORE_IOMUX_BASE) /*!< Core IOMUX IP */
#define IP_AON_IOMUX                                           ((AON_IOMUX_RegDef *) AON_IOMUX_BASE)   /*!< Always-On IOMUX IP */
/** @} */ /* End of group VENUSA_GPIO_IP */

/** @defgroup VENUSA_Communication_IP Communication IP Definitions
  * @brief Communication Interface IP Definitions
  * @{
  */
#define IP_UART0                                               ((UART_RegDef *) UART0_BASE)            /*!< UART 0 IP */
#define IP_UART1                                               ((UART_RegDef *) UART1_BASE)            /*!< UART 1 IP */
#define IP_UART2                                               ((UART_RegDef *) UART2_BASE)            /*!< UART 2 IP */
#define IP_I2C0                                                ((I2C_RegDef *) I2C0_BASE)              /*!< I2C 0 IP */
#define IP_I2C1                                                ((I2C_RegDef *) I2C1_BASE)              /*!< I2C 1 IP */
#define IP_IR                                                  ((IR_RegDef *) IR_BASE)                 /*!< Infrared IP */
#define IP_SPI0                                                ((SPI_RegDef *) SPI0_BASE)              /*!< SPI 0 IP */
#define IP_SPI1                                                ((SPI_RegDef *) SPI1_BASE)              /*!< SPI 1 IP */
#define IP_USBC                                                ((CSK_USB_RegDef *) USBC_BASE)          /*!< USB Controller IP */
/** @} */ /* End of group VENUSA_Communication_IP */

/** @defgroup VENUSA_Analog_IP Analog IP Definitions
  * @brief Analog and Sensor IP Definitions
  * @{
  */
#define IP_GPADC                                               ((GPADC_RegDef *) GPADC_BASE)           /*!< General Purpose ADC IP */
#define IP_KEYSENSE0                                           ((KEYSENSE_RegDef *) KEYSENSE0_BASE)    /*!< Key Sense IP */
/** @} */ /* End of group VENUSA_Analog_IP */

/** @defgroup VENUSA_Memory_IP Memory IP Definitions
  * @brief Memory Controller and Storage IP Definitions
  * @{
  */
#define IP_FLASH_CTRL                                          ((FLASHC_RegDef *) FLASH_CTRL_BASE)     /*!< Flash Controller IP */
#define IP_FLASH_DL                                            ((FLASH_DL_RegDef *) FLASH_DL_BASE)     /*!< Flash Download IP */
#define IP_PSRAM_CTRL                                          ((PSRAM_MC_RegDef *) PSRAM_CTRL_BASE)   /*!< PSRAM Controller IP */
#define IP_SDIOH                                               ((SDIOH_RegDef *) SDIOH_BASE)           /*!< SDIO Host IP */
/** @} */ /* End of group VENUSA_Memory_IP */

/** @defgroup VENUSA_Audio_IP Audio IP Definitions
  * @brief Audio Processing IP Definitions
  * @{
  */
#define IP_APC                                                 ((APC_RegDef *) APC_BASE)               /*!< Audio Processing Controller IP */
#define IP_CODEC                                               ((AUDIO_CODEC_RegDef *) CODEC_BASE)     /*!< Audio Codec IP */
/** @} */ /* End of group VENUSA_Audio_IP */

/** @defgroup VENUSA_DMA_IP DMA IP Definitions
  * @brief DMA Controller IP Definitions
  * @{
  */
#define IP_GPDMA2D                                             ((GPDMA2D_RegDef *) GPDMA2D_BASE)       /*!< GP DMA 2D IP */
/** @} */ /* End of group VENUSA_DMA_IP */

/** @defgroup VENUSA_AON_IP Always-On IP Definitions
  * @brief Always-On Domain IP Definitions
  * @{
  */
#define IP_AON_CTRL                                            ((AON_CTRL_RegDef *) AON_CTRL_BASE)     /*!< Always-On Control IP */
#define IP_AON_WDT                                             ((AON_WDT_RegDef *) AON_WDT_BASE)       /*!< Always-On Watchdog IP */
/** @} */ /* End of group VENUSA_AON_IP */

/** @defgroup VENUSA_Security_IP Security IP Definitions
  * @brief Security and Protection IP Definitions
  * @{
  */
#define IP_EFUSE_CTRL                                          ((EFUSE_CTRL_RegDef *) EFUSE_CTRL_BASE) /*!< eFuse Control IP */
#define IP_CORE0_WDT                                           ((WDT_RegDef *) CORE0_WDT_BASE)         /*!< Core 0 Watchdog IP */
#define IP_CORE1_WDT                                           ((WDT_RegDef *) CORE1_WDT_BASE)         /*!< Core 1 Watchdog IP */
/** @} */ /* End of group VENUSA_Security_IP */

/** @defgroup VENUSA_Image_Processing_IP Image Processing IP Definitions
  * @brief Image and Video Processing IP Definitions
  * @{
  */
#define IP_DVP_IN                                              ((IMAGE_VIC_RegDef *) DVP_IN_BASE)       /*!< DVP Input IP */
#define IP_JPEG_TOP                                            ((JPEG_RegDef *) JPEG_TOP_BASE)         /*!< JPEG Codec IP */
#define IP_QSPI_IN                                             ((QSPI_SENSOR_IN_RegDef *) QSPI_IN_BASE) /*!< QSPI Sensor Input IP */
#define IP_QSPI_OUT                                            ((QSPI_LCD_RegDef *) QSPI_OUT_BASE)     /*!< QSPI LCD Output IP */
#define IP_RGB_OUT                                             ((RGB_INTERFACE_RegDef *) RGB_OUT_BASE)  /*!< RGB Interface IP */
#define IP_I8080_OUT                                           ((I8080_OUT_RegDef *) I8080_OUT_BASE)   /*!< I8080 Interface IP */
/** @} */ /* End of group VENUSA_Image_Processing_IP */

/** @} */ /* End of group VENUSA_IP_Definitions */

/** @defgroup VENUSA_Core_Types Core Data Types
  * @brief Processor Core Data Types and Structures
  * @{
  */

/** @defgroup VENUSA_Region_Info Memory Region Information
  * @brief Internal Memory Region Configuration
  * @{
  */

/**
 * @struct IRegion_Info
 * @brief Internal Region Information Structure
 *
 * Contains base addresses for various internal memory regions and system components.
 */
typedef struct IRegion_Info {
    unsigned long iregion_base;         /**< Internal region base address */
    unsigned long eclic_base;           /**< ECLIC base address */
    unsigned long systimer_base;        /**< System timer base address */
    unsigned long smp_base;             /**< SMP base address */
    unsigned long idu_base;             /**< IDU base address */
} IRegion_Info_Type;
/** @} */ /* End of group VENUSA_Region_Info */

/** @defgroup VENUSA_Exception_Codes Exception Code Definitions
  * @brief Exception and Interrupt Codes
  * @{
  */
typedef enum EXCn {
    /* =======================================  Nuclei N/NX Specific Exception Code  ======================================== */
    InsUnalign_EXCn          =   0,              /*!< Instruction address misaligned */
    InsAccFault_EXCn         =   1,              /*!< Instruction access fault */
    IlleIns_EXCn             =   2,              /*!< Illegal instruction */
    Break_EXCn               =   3,              /*!< Breakpoint */
    LdAddrUnalign_EXCn       =   4,              /*!< Load address misaligned */
    LdFault_EXCn             =   5,              /*!< Load access fault */
    StAddrUnalign_EXCn       =   6,              /*!< Store or AMO address misaligned */
    StAccessFault_EXCn       =   7,              /*!< Store or AMO access fault */
    UmodeEcall_EXCn          =   8,              /*!< Environment call from User mode */
    SmodeEcall_EXCn          =   9,              /*!< Environment call from S-mode */
    MmodeEcall_EXCn          =  11,              /*!< Environment call from Machine mode */
    InsPageFault_EXCn        =  12,              /*!< Instruction page fault */
    LdPageFault_EXCn         =  13,              /*!< Load page fault */
    StPageFault_EXCn         =  15,              /*!< Store or AMO page fault */
    NMI_EXCn                 =  0xfff,           /*!< NMI interrupt */
} EXCn_Type;
/** @} */ /* End of group VENUSA_Exception_Codes */

/** @} */ /* End of group VENUSA_Core_Types */

/** @defgroup VENUSA_Core_Features Core Feature Definitions
  * @brief Processor Core Feature Macros
  * @{
  */
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

/* Define the correct core features */
#define __ECLIC_PRESENT           1                     /*!< Set to 1 if ECLIC is present */
#define __ECLIC_BASEADDR          0xE0020000            /*!< Set to ECLIC baseaddr of your device */

#define __ECLIC_INTCTLBITS        3                     /*!< Set to 1 - 8, the number of hardware bits are actually implemented in the clicintctl registers. */
#define __ECLIC_INTNUM            67                    /*!< Set to 1 - 1024, total interrupt number of ECLIC Unit */

#define __SYSTIMER_PRESENT        1                     /*!< Set to 1 if System Timer is present */
#define __SYSTIMER_BASEADDR       0xE0030000            /*!< Set to SysTimer baseaddr of your device */

#define __CIDU_PRESENT            1                     /*!< Set to 1 if CIDU is present */
#define __CIDU_BASEADDR           0x47000000            /*!< Set to cidu baseaddr of your device */

/*!< Set to 0, 1, or 2, 0 not present, 1 single floating point unit present, 2 double floating point unit present */
#if !defined(__riscv_flen)
#define __FPU_PRESENT             0
#elif __riscv_flen == 32
#define __FPU_PRESENT             1
#else
#define __FPU_PRESENT             2
#endif

/* RISC-V Extension Presence Detection */
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
#define __PMP_ENTRY_NUM           8                    /*!< Set to 8 or 16, the number of PMP entries */

#define __SPMP_PRESENT            0                     /*!< Set to 1 if SPMP is present */
#define __SPMP_ENTRY_NUM          16                    /*!< Set to 8 or 16, the number of SPMP entries */

#ifndef __TEE_PRESENT
#define __TEE_PRESENT             0                     /*!< Set to 1 if TEE is present */
#endif

#define __ICACHE_PRESENT          1                     /*!< Set to 1 if I-Cache is present */
#define __DCACHE_PRESENT          1                     /*!< Set to 1 if D-Cache is present */
#define __CCM_PRESENT             1                     /*!< Set to 1 if Cache Control and Mantainence Unit is present */

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

/* Define boot hart id */
#ifndef BOOT_HARTID
#define BOOT_HARTID               0                     /*!< Choosen boot hart id in current cluster when in soc system */
#endif

#include <nmsis_core.h>                         /*!< Nuclei N/NX class processor and core peripherals */
#include "system_RISCVN300.h"                    /*!< riscv N300 System */

/** @} */ /* End of group VENUSA_Core_Features */

/** @defgroup VENUSA_Compiler_Macros Compiler Specific Macros
  * @brief Compiler Attributes and Macros
  * @{
  */
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

#define __I                     volatile const  /*!< 'read only' permissions      */
#define __O                     volatile        /*!< 'write only' permissions     */
#define __IO                    volatile        /*!< 'read / write' permissions   */

/** @} */ /* End of group VENUSA_Compiler_Macros */

/** @defgroup VENUSA_Linker_Sections Linker Section Definitions
  * @brief Memory Section Definitions for Linker Script
  * @details This group contains all linker section definitions and attribute macros
  *          for memory placement in VenusA processor. These macros are used to
  *          place code and data in specific memory regions.
  * @{
  */

/** @defgroup VENUSA_Section_Names Linker Section Names
  * @brief Linker Section Name Definitions
  * @details Predefined section names for different memory regions
  * @{
  */
#define _CORE0_ILM_TEXT_SEC              ".text.core0_ilm"      /**< Core 0 Instruction Local Memory text section */
#define _CORE0_DLM_DATA_SEC              ".data.core0_dlm"      /**< Core 0 Data Local Memory data section */
#define _CORE0_DLM_BSS_SEC               ".bss.core0_dlm"       /**< Core 0 Data Local Memory BSS section */

#define _CORE1_ILM_TEXT_SEC              ".text.core1_ilm"      /**< Core 1 Instruction Local Memory text section */
#define _CORE1_DLM_DATA_SEC              ".data.core1_dlm"      /**< Core 1 Data Local Memory data section */
#define _CORE1_DLM_BSS_SEC               ".bss.core1_dlm"       /**< Core 1 Data Local Memory BSS section */

#define _CMN_RAM0_TEXT_SEC               ".text.cmn_ram0"       /**< Common RAM 0 text section */
#define _CMN_RAM0_DATA_SEC               ".data.cmn_ram0"       /**< Common RAM 0 data section */
#define _CMN_RAM0_BSS_SEC                ".bss.cmn_ram0"        /**< Common RAM 0 BSS section */

#define _CMN_RAM1_TEXT_SEC               ".text.cmn_ram1"       /**< Common RAM 1 text section */
#define _CMN_RAM1_DATA_SEC               ".data.cmn_ram1"       /**< Common RAM 1 data section */
#define _CMN_RAM1_BSS_SEC                ".bss.cmn_ram1"        /**< Common RAM 1 BSS section */

#define _CMN_LUNA_RAM_TEXT_SEC           ".text.cmn_luna_ram"   /**< Common Luna RAM text section */
#define _CMN_LUNA_RAM_DATA_SEC           ".data.cmn_luna_ram"   /**< Common Luna RAM data section */
#define _CMN_LUNA_RAM_BSS_SEC            ".bss.cmn_luna_ram"    /**< Common Luna RAM BSS section */

#define _CMN_PSRAM_TEXT_SEC              ".text.cmn_psram"      /**< Common PSRAM text section */
#define _CMN_PSRAM_DATA_SEC              ".data.cmn_psram"      /**< Common PSRAM data section */
#define _CMN_PSRAM_BSS_SEC               ".bss.cmn_psram"       /**< Common PSRAM BSS section */
/** @} */ /* End of group VENUSA_Section_Names */

/** @defgroup VENUSA_Section_Attributes Section Attribute Macros
  * @brief GCC Section Attribute Macros
  * @details Macros for placing variables and functions in specific sections
  * @{
  */

/** @defgroup VENUSA_Core0_Attributes Core 0 Section Attributes
  * @brief Core 0 specific section placement macros
  * @{
  */
#define _CORE0_ILM_TEXT                  __attribute__ ((section (_CORE0_ILM_TEXT_SEC)))                  /**< Place in Core 0 ILM text section */
#define _CORE0_ILM_TEXT_TAG(tag)         __attribute__ ((section (_CORE0_ILM_TEXT_SEC"."#tag)))           /**< Place in Core 0 ILM text section with tag */

#define _CORE0_DLM_DATA                  __attribute__ ((section (_CORE0_DLM_DATA_SEC)))                  /**< Place in Core 0 DLM data section */
#define _CORE0_DLM_DATA_TAG(tag)         __attribute__ ((section (_CORE0_DLM_DATA_SEC"."#tag)))           /**< Place in Core 0 DLM data section with tag */
#define _CORE0_DLM_BSS                   __attribute__ ((section (_CORE0_DLM_BSS_SEC)))                   /**< Place in Core 0 DLM BSS section */
#define _CORE0_DLM_BSS_TAG(tag)          __attribute__ ((section (_CORE0_DLM_BSS_SEC"."#tag)))            /**< Place in Core 0 DLM BSS section with tag */
/** @} */ /* End of group VENUSA_Core0_Attributes */

/** @defgroup VENUSA_Core1_Attributes Core 1 Section Attributes
  * @brief Core 1 specific section placement macros
  * @{
  */
#define _CORE1_ILM_TEXT                  __attribute__ ((section (_CORE1_ILM_TEXT_SEC)))                  /**< Place in Core 1 ILM text section */
#define _CORE1_ILM_TEXT_TAG(tag)         __attribute__ ((section (_CORE1_ILM_TEXT_SEC"."#tag)))           /**< Place in Core 1 ILM text section with tag */

#define _CORE1_DLM_DATA                  __attribute__ ((section (_CORE1_DLM_DATA_SEC)))                  /**< Place in Core 1 DLM data section */
#define _CORE1_DLM_DATA_TAG(tag)         __attribute__ ((section (_CORE1_DLM_DATA_SEC"."#tag)))           /**< Place in Core 1 DLM data section with tag */
#define _CORE1_DLM_BSS                   __attribute__ ((section (_CORE1_DLM_BSS_SEC)))                   /**< Place in Core 1 DLM BSS section */
#define _CORE1_DLM_BSS_TAG(tag)          __attribute__ ((section (_CORE1_DLM_BSS_SEC"."#tag)))            /**< Place in Core 1 DLM BSS section with tag */
/** @} */ /* End of group VENUSA_Core1_Attributes */

/** @defgroup VENUSA_Common_Attributes Common RAM Section Attributes
  * @brief Common RAM section placement macros
  * @{
  */
#define _CMN_RAM0_TEXT                   __attribute__ ((section (_CMN_RAM0_TEXT_SEC)))                   /**< Place in Common RAM 0 text section */
#define _CMN_RAM0_TEXT_TAG(tag)          __attribute__ ((section (_CMN_RAM0_TEXT_SEC"."#tag)))            /**< Place in Common RAM 0 text section with tag */

#define _CMN_RAM0_DATA                   __attribute__ ((section (_CMN_RAM0_DATA_SEC)))                   /**< Place in Common RAM 0 data section */
#define _CMN_RAM0_DATA_TAG(tag)          __attribute__ ((section (_CMN_RAM0_DATA_SEC"."#tag)))            /**< Place in Common RAM 0 data section with tag */
#define _CMN_RAM0_BSS                    __attribute__ ((section (_CMN_RAM0_BSS_SEC)))                    /**< Place in Common RAM 0 BSS section */
#define _CMN_RAM0_BSS_TAG(tag)           __attribute__ ((section (_CMN_RAM0_BSS_SEC"."#tag)))             /**< Place in Common RAM 0 BSS section with tag */

#define _CMN_RAM1_TEXT                   __attribute__ ((section (_CMN_RAM1_TEXT_SEC)))                   /**< Place in Common RAM 1 text section */
#define _CMN_RAM1_TEXT_TAG(tag)          __attribute__ ((section (_CMN_RAM1_TEXT_SEC"."#tag)))            /**< Place in Common RAM 1 text section with tag */

#define _CMN_RAM1_DATA                   __attribute__ ((section (_CMN_RAM1_DATA_SEC)))                   /**< Place in Common RAM 1 data section */
#define _CMN_RAM1_DATA_TAG(tag)          __attribute__ ((section (_CMN_RAM1_DATA_SEC"."#tag)))            /**< Place in Common RAM 1 data section with tag */
#define _CMN_RAM1_BSS                    __attribute__ ((section (_CMN_RAM1_BSS_SEC)))                    /**< Place in Common RAM 1 BSS section */
#define _CMN_RAM1_BSS_TAG(tag)           __attribute__ ((section (_CMN_RAM1_BSS_SEC"."#tag)))             /**< Place in Common RAM 1 BSS section with tag */

#define _CMN_LUNA_RAM_TEXT               __attribute__ ((section (_CMN_LUNA_RAM_TEXT_SEC)))               /**< Place in Common Luna RAM text section */
#define _CMN_LUNA_RAM_TEXT_TAG(tag)      __attribute__ ((section (_CMN_LUNA_RAM_TEXT_SEC"."#tag)))        /**< Place in Common Luna RAM text section with tag */

#define _CMN_LUNA_RAM_DATA               __attribute__ ((section (_CMN_LUNA_RAM_DATA_SEC)))               /**< Place in Common Luna RAM data section */
#define _CMN_LUNA_RAM_DATA_TAG(tag)      __attribute__ ((section (_CMN_LUNA_RAM_DATA_SEC"."#tag)))        /**< Place in Common Luna RAM data section with tag */
#define _CMN_LUNA_RAM_BSS                __attribute__ ((section (_CMN_LUNA_RAM_BSS_SEC)))                /**< Place in Common Luna RAM BSS section */
#define _CMN_LUNA_RAM_BSS_TAG(tag)       __attribute__ ((section (_CMN_LUNA_RAM_BSS_SEC"."#tag)))         /**< Place in Common Luna RAM BSS section with tag */

#define _CMN_PSRAM_TEXT                  __attribute__ ((section (_CMN_PSRAM_TEXT_SEC)))                  /**< Place in Common PSRAM text section */
#define _CMN_PSRAM_TEXT_TAG(tag)         __attribute__ ((section (_CMN_PSRAM_TEXT_SEC"."#tag)))           /**< Place in Common PSRAM text section with tag */

#define _CMN_PSRAM_DATA                  __attribute__ ((section (_CMN_PSRAM_DATA_SEC)))                  /**< Place in Common PSRAM data section */
#define _CMN_PSRAM_DATA_TAG(tag)         __attribute__ ((section (_CMN_PSRAM_DATA_SEC"."#tag)))           /**< Place in Common PSRAM data section with tag */
#define _CMN_PSRAM_BSS                   __attribute__ ((section (_CMN_PSRAM_BSS_SEC)))                   /**< Place in Common PSRAM BSS section */
#define _CMN_PSRAM_BSS_TAG(tag)          __attribute__ ((section (_CMN_PSRAM_BSS_SEC"."#tag)))            /**< Place in Common PSRAM BSS section with tag */
/** @} */ /* End of group VENUSA_Common_Attributes */

/** @} */ /* End of group VENUSA_Section_Attributes */

/** @defgroup VENUSA_Fast_Memory Fast Memory Selection
  * @brief Fast memory section selection based on boot hart ID
  * @details These macros select the appropriate fast memory sections
  *          based on which core is the boot hart
  * @{
  */

///* Fast Memory Section Selection based on Boot Hart ID */
//#if (BOOT_HARTID == 0) // Core0
///** @brief Fast text section for Core 0 boot */
//#define _FAST_TEXT                       _CORE0_ILM_TEXT
///** @brief Fast text section with tag for Core 0 boot */
//#define _FAST_TEXT_TAG(tag)              _CORE0_ILM_TEXT_TAG(tag)
///** @brief Fast data section for Core 0 boot */
//#define _FAST_DATA                       _CORE0_DLM_DATA
///** @brief Fast data section with tag for Core 0 boot */
//#define _FAST_DATA_TAG(tag)              _CORE0_DLM_DATA_TAG(tag)
///** @brief Fast BSS section for Core 0 boot */
//#define _FAST_BSS                        _CORE0_DLM_BSS
///** @brief Fast BSS section with tag for Core 0 boot */
//#define _FAST_BSS_TAG(tag)               _CORE0_DLM_BSS_TAG(tag)
//#else // Core1
///** @brief Fast text section for Core 1 boot */
//#define _FAST_TEXT                       _CORE1_ILM_TEXT
///** @brief Fast text section with tag for Core 1 boot */
//#define _FAST_TEXT_TAG(tag)              _CORE1_ILM_TEXT_TAG(tag)
///** @brief Fast data section for Core 1 boot */
//#define _FAST_DATA                       _CORE1_DLM_DATA
///** @brief Fast data section with tag for Core 1 boot */
//#define _FAST_DATA_TAG(tag)              _CORE1_DLM_DATA_TAG(tag)
///** @brief Fast BSS section for Core 1 boot */
//#define _FAST_BSS                        _CORE1_DLM_BSS
///** @brief Fast BSS section with tag for Core 1 boot */
//#define _FAST_BSS_TAG(tag)               _CORE1_DLM_BSS_TAG(tag)
//#endif

/** @brief Fast text section for any Core boot */
#define _FAST_TEXT                       __attribute__ ((section (".text.cmn_ram")))
/** @brief Fast data section for any Core boot */
#define _FAST_DATA                       __attribute__ ((section (".data.cmn_ram")))
/** @brief Fast BSS section for any Core boot */
#define _FAST_BSS                        __attribute__ ((section (".bss.cmn_ram")))

/** @brief Fast text section with tag for any Core boot */
#define _FT_TAG_SEC(tag)                ".text.cmn_ram."#tag
#define _FAST_TEXT_TAG(tag)             __attribute__ ((section (_FT_TAG_SEC(tag))))
/** @brief Fast data section with tag for any Core boot */
#define _FD_TAG_SEC(tag)                ".data.cmn_ram."#tag
#define _FAST_DATA_TAG(tag)             __attribute__ ((section (_FD_TAG_SEC(tag))))
/** @brief Fast BSS section with tag for any Core boot */
#define _FB_TAG_SEC(tag)                ".bss.cmn_ram."#tag
#define _FAST_BSS_TAG(tag)              __attribute__ ((section (_FB_TAG_SEC(tag))))


/** @brief DMA memory alignment attribute */
#define _DMA_PRAM                         __attribute__((aligned(32))) /**< 32-byte alignment for DMA */

/** @brief DMA section marker */
#define _DMA                              /**< DMA related code marker */

/** @brief Fast read-only function attribute */
#define _FAST_FUNC_RO                     /**< Fast memory read-only function */

/** @brief Fast volatile data attribute */
#define _FAST_DATA_VI                     /**< Fast memory volatile data */

/** @brief Fast zero-initialized data attribute */
#define _FAST_DATA_ZI                     /**< Fast memory zero-initialized data */

/** @} */ /* End of group VENUSA_Fast_Memory */

/** @} */ /* End of group VENUSA_Linker_Sections */

/** @defgroup VENUSA_Compiler_Keywords Compiler Keywords
  * @brief Compiler-specific Keyword Definitions
  * @{
  */
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

#define NMI_EXPn                (-2)      /*!< NMI Exception Number */
/** @} */ /* End of group VENUSA_Compiler_Keywords */

/** @defgroup VENUSA_Interrupt_Management Interrupt Management
  * @brief Interrupt Control and Management Functions
  * @{
  */

/** @defgroup VENUSA_Interrupt_Priorities Interrupt Priority Levels
  * @brief Interrupt Priority Definitions
  * @{
  */
#define MAX_INTERRUPT_PRIORITY_RVAL     3  /*!< Maximum interrupt priority value */
#define MID_INTERRUPT_PRIORITY          2  /*!< Medium interrupt priority */
#define DEF_INTERRUPT_PRIORITY          0  /*!< Default interrupt priority */
#define DEF_INTERRUPT_LEVEL             0  /*!< Default interrupt level */
/** @} */ /* End of group VENUSA_Interrupt_Priorities */

#define SOC_EXTERNAL_MAP_TO_ECLIC_IRQn_OFFSET      20
#define IRQn_MAP_TO_EXT_ID(IRQn)                   (IRQn - SOC_EXTERNAL_MAP_TO_ECLIC_IRQn_OFFSET)


/** @defgroup VENUSA_Interrupt_Functions Interrupt Control Functions
  * @brief Global and Peripheral Interrupt Control Functions
  * @details This group contains functions for managing global interrupts and
  *          peripheral-specific interrupt control in the VenusA processor.
  * @{
  */

/**
 * @brief Check if Global Interrupts are enabled
 * @return uint8_t 1 if global interrupts are enabled, 0 if disabled
 * @details Reads the MSTATUS register to check the MIE (Machine Interrupt Enable) bit
 */
static inline uint8_t GINT_enabled(void)
{
    return ((__RV_CSR_READ(CSR_MSTATUS) & MSTATUS_MIE) == MSTATUS_MIE);
}

/**
 * @brief Enable Global Interrupts
 * @details Sets the MIE (Machine Interrupt Enable) bit in the MSTATUS register
 *          to allow the processor to respond to interrupts
 */
static inline void enable_GINT(void)
{
    __RV_CSR_SET(CSR_MSTATUS, MSTATUS_MIE);
}

/**
 * @brief Disable Global Interrupts
 * @details Clears the MIE (Machine Interrupt Enable) bit in the MSTATUS register
 *          to prevent the processor from responding to interrupts
 */
static inline void disable_GINT(void)
{
    __RV_CSR_CLEAR(CSR_MSTATUS, MSTATUS_MIE);
}

/**
 * @brief Interrupt Service Routine function pointer type
 * @details Function pointer type for interrupt service routines
 */
typedef void (*ISR)(void);

/**
 * @brief Register an Interrupt Service Routine
 * @param[in] irq_no Interrupt number to register
 * @param[in] isr Pointer to the interrupt service routine function
 * @param[out] isr_old Pointer to store the old ISR (can be NULL)
 * @details Registers a new ISR for the specified interrupt number and
 *          optionally returns the previously registered ISR
 */
void register_ISR(uint32_t irq_no, ISR isr, ISR* isr_old);

/**
 * @brief Check if a specific interrupt is enabled
 * @param[in] irq_no Interrupt number to check
 * @return uint8_t 1 if interrupt is enabled, 0 if disabled
 * @details Checks the enable status of a specific peripheral interrupt
 */
static inline uint8_t IRQ_enabled(uint32_t irq_no)
{
    return (uint8_t)ECLIC_GetEnableIRQ(irq_no);
}

/**
 * @brief Enable a specific interrupt
 * @param[in] irq_no Interrupt number to enable
 * @details Enables the specified peripheral interrupt
 */
static inline void enable_IRQ(uint32_t irq_no)
{
    ECLIC_EnableIRQ(irq_no);
}

/**
 * @brief Disable a specific interrupt
 * @param[in] irq_no Interrupt number to disable
 * @details Disables the specified peripheral interrupt
 */
static inline void disable_IRQ(uint32_t irq_no)
{
    ECLIC_DisableIRQ(irq_no);
}

/**
 * @brief Clear pending interrupt status
 * @param[in] irq_no Interrupt number to clear
 * @details Clears the pending status of the specified interrupt
 */
static inline void clear_IRQ(uint32_t irq_no)
{
    ECLIC_ClearPendingIRQ(irq_no);
}

/** @} */ /* End of group VENUSA_Interrupt_Functions */
/** @} */ /* End of group VENUSA_Interrupt_Management */

/** @defgroup VENUSA_Memory_Protection Memory Protection Unit
  * @brief MPU and Cache Control Functions
  * @details This module provides functions to configure and control the Memory Protection Unit (MPU)
  *          and cache settings for the VENUSA platform. It allows defining non-cacheable memory
  *          regions and device memory regions to optimize system performance and ensure memory
  *          access correctness.
  *
  * The MPU supports up to 8 configurable regions for each type (non-cacheable and device).
  * Each region can be individually enabled with specific base address and length parameters,
  * or disabled when not needed.
  *
  * @note All address parameters should be aligned to the MPU region requirements.
  * @note Length parameters should be power-of-two values and meet minimum size requirements.
  * @{
  */

/**
  * @brief Enable non-cacheable region 0
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_0(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 1
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_1(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 2
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_2(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 3
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_3(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 4
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_4(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 5
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_5(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 6
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_6(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable non-cacheable region 7
  * @param base_addr Base address of the memory region (must be properly aligned)
  * @param len Length of the memory region (must be power of two)
  * @return None
  * @note This region will be marked as non-cacheable in the memory system
  */
void non_cacheable_region_enable_7(uint32_t base_addr, uint32_t len);

/**
  * @brief Disable non-cacheable region 0
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_0();

/**
  * @brief Disable non-cacheable region 1
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_1();

/**
  * @brief Disable non-cacheable region 2
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_2();

/**
  * @brief Disable non-cacheable region 3
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_3();

/**
  * @brief Disable non-cacheable region 4
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_4();

/**
  * @brief Disable non-cacheable region 5
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_5();

/**
  * @brief Disable non-cacheable region 6
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_6();

/**
  * @brief Disable non-cacheable region 7
  * @return None
  * @note The region will be removed from the MPU configuration
  */
void non_cacheable_region_disable_7();

/**
  * @brief Enable device region 0
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_0(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 1
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_1(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 2
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_2(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 3
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_3(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 4
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_4(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 5
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_5(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 6
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_6(uint32_t base_addr, uint32_t len);

/**
  * @brief Enable device region 7
  * @param base_addr Base address of the device memory region
  * @param len Length of the device memory region
  * @return None
  * @note This region will be configured with device memory attributes (non-cacheable, non-bufferable)
  */
void device_region_enable_7(uint32_t base_addr, uint32_t len);

/**
  * @brief Disable device region 0
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_0();

/**
  * @brief Disable device region 1
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_1();

/**
  * @brief Disable device region 2
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_2();

/**
  * @brief Disable device region 3
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_3();

/**
  * @brief Disable device region 4
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_4();

/**
  * @brief Disable device region 5
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_5();

/**
  * @brief Disable device region 6
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_6();

/**
  * @brief Disable device region 7
  * @return None
  * @note The device region will be removed from the MPU configuration
  */
void device_region_disable_7();

/**
  * @brief Initialize the Memory Protection Unit
  * @return None
  * @details This function initializes the MPU with default settings and prepares it for region configuration.
  *          It should be called before any other MPU configuration functions.
  * @note This function must be called once at system initialization
  */
extern void mpu_init( void );

/** @} */ /* End of group VENUSA_Memory_Protection */

/** @defgroup VENUSA_DMA_Definitions DMA Channel Definitions
  * @brief DMA Channel Selection Macros and Configuration
  * @details This group contains DMA channel selection macros for both common DMA
  *          and general purpose DMA controllers in the VenusA processor.
  * @{
  */

/** @defgroup VENUSA_CMN_DMA_SEL0 Common DMA Selector 0
  * @brief Common DMA Channel 0 Hardware Selection IDs
  * @details Hardware selection IDs for Common DMA controller channel 0
  * @{
  */
#define HAL_CMN_DMA_SEL0_HSID_0_UART0_RX                                            0  /**< UART0 Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_1_UART0_TX                                            1  /**< UART0 Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_2_UART1_RX                                            2  /**< UART1 Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_3_UART1_TX                                            3  /**< UART1 Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_4_UART2_RX                                            4  /**< UART2 Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_5_UART2_TX                                            5  /**< UART2 Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_6_SPI0_RX                                             6  /**< SPI0 Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_7_SPI0_TX                                             7  /**< SPI0 Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_8_SPI1_RX                                             8  /**< SPI1 Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_9_SPI1_TX                                             9  /**< SPI1 Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_10_IR_RX                                              10 /**< Infrared Receive DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_11_IR_TX                                              11 /**< Infrared Transmit DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_12_GPADC                                              12 /**< General Purpose ADC DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_13_I2C0                                               13 /**< I2C0 DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_14_I2C1                                               14 /**< I2C1 DMA channel */
#define HAL_CMN_DMA_SEL0_HSID_15_DVP_DMA                                            15 /**< DVP (Digital Video Port) DMA channel */
/** @} */ /* End of group VENUSA_CMN_DMA_SEL0 */

/** @defgroup VENUSA_GP_DMA_SEL0 General Purpose DMA Selector 0
  * @brief General Purpose DMA Channel 0 Hardware Selection IDs
  * @details Hardware selection IDs for General Purpose DMA controller channel 0
  * @{
  */
#define HAL_GP_DMA_SEL0_HSID0_RGB_DMA                                              0  /**< RGB Interface DMA channel */
#define HAL_GP_DMA_SEL0_HSID1_QSPI_OUT_DMA                                         1  /**< QSPI Output DMA channel */
#define HAL_GP_DMA_SEL0_HSID2_QSPI_IN_DMA                                          2  /**< QSPI Input DMA channel */
#define HAL_GP_DMA_SEL0_HSID3_DVP_DMA                                              3  /**< DVP Interface DMA channel */
#define HAL_GP_DMA_SEL0_HSID4_JPG_DMA_E                                            4  /**< JPEG Encoder DMA channel */
#define HAL_GP_DMA_SEL0_HSID5_JPG_DMA_P                                            5  /**< JPEG Processor DMA channel */
#define HAL_GP_DMA_SEL0_HSID6_UART0_RX                                             6  /**< UART0 Receive DMA channel */
#define HAL_GP_DMA_SEL0_HSID7_UART0_TX                                             7  /**< UART0 Transmit DMA channel */
#define HAL_GP_DMA_SEL0_HSID8_APC_DMA_TX_0                                         8  /**< APC Transmit Channel 0 DMA */
#define HAL_GP_DMA_SEL0_HSID9_APC_DMA_TX_1                                         9  /**< APC Transmit Channel 1 DMA */
#define HAL_GP_DMA_SEL0_HSID10_APC_DMA_TX_2                                        10 /**< APC Transmit Channel 2 DMA */
#define HAL_GP_DMA_SEL0_HSID11_I8080_DMA                                           11 /**< I8080 Interface DMA channel */
#define HAL_GP_DMA_SEL0_HSID12_APC_DMA_RX_0                                        12 /**< APC Receive Channel 0 DMA */
#define HAL_GP_DMA_SEL0_HSID13_APC_DMA_RX_1                                        13 /**< APC Receive Channel 1 DMA */
#define HAL_GP_DMA_SEL0_HSID14_APC_DMA_RX_2                                        14 /**< APC Receive Channel 2 DMA */
#define HAL_GP_DMA_SEL0_HSID15_APC_DMA_RX_3                                        15 /**< APC Receive Channel 3 DMA */
/** @} */ /* End of group VENUSA_GP_DMA_SEL0 */

/** @defgroup VENUSA_CMN_DMA_SEL1 Common DMA Selector 1
  * @brief Common DMA Channel 1 Hardware Selection IDs
  * @details Hardware selection IDs for Common DMA controller channel 1
  * @{
  */
#define HAL_CMN_DMA_SEL1_HSID_0_RGB_DMA                                             0  /**< RGB Interface DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_1_QSPI_OUT_DMA                                        1  /**< QSPI Output DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_2_QSPI_IN_DMA                                         2  /**< QSPI Input DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_3_DVP_DMA                                             3  /**< DVP Interface DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_4_JPG_DMA_E                                           4  /**< JPEG Encoder DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_5_JPG_DMA_P                                           5  /**< JPEG Processor DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_6_UART1_RX                                            6  /**< UART1 Receive DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_7_UART1_TX                                            7  /**< UART1 Transmit DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_8_APC_DMA_TX_0                                        8  /**< APC Transmit Channel 0 DMA */
#define HAL_CMN_DMA_SEL1_HSID_9_APC_DMA_TX_1                                        9  /**< APC Transmit Channel 1 DMA */
#define HAL_CMN_DMA_SEL1_HSID_10_APC_DMA_TX_2                                       10 /**< APC Transmit Channel 2 DMA */
#define HAL_CMN_DMA_SEL1_HSID_11_I8080_DMA                                          11 /**< I8080 Interface DMA channel */
#define HAL_CMN_DMA_SEL1_HSID_12_APC_DMA_RX_0                                       12 /**< APC Receive Channel 0 DMA */
#define HAL_CMN_DMA_SEL1_HSID_13_APC_DMA_RX_1                                       13 /**< APC Receive Channel 1 DMA */
#define HAL_CMN_DMA_SEL1_HSID_14_APC_DMA_RX_2                                       14 /**< APC Receive Channel 2 DMA */
#define HAL_CMN_DMA_SEL1_HSID_15_APC_DMA_RX_3                                       15 /**< APC Receive Channel 3 DMA */
/** @} */ /* End of group VENUSA_CMN_DMA_SEL1 */

/** @defgroup VENUSA_GP_DMA_SEL1 General Purpose DMA Selector 1
  * @brief General Purpose DMA Channel 1 Hardware Selection IDs
  * @details Hardware selection IDs for General Purpose DMA controller channel 1
  * @{
  */
#define HAL_GP_DMA_SEL1_HSID0_UART0_RX                                             0  /**< UART0 Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID1_UART0_TX                                             1  /**< UART0 Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID2_UART1_RX                                             2  /**< UART1 Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID3_UART1_TX                                             3  /**< UART1 Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID4_UART2_RX                                             4  /**< UART2 Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID5_UART2_TX                                             5  /**< UART2 Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID6_SPI0_RX                                              6  /**< SPI0 Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID7_SPI0_TX                                              7  /**< SPI0 Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID8_SPI1_RX                                              8  /**< SPI1 Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID9_SPI1_TX                                              9  /**< SPI1 Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID10_IR_RX                                               10 /**< Infrared Receive DMA channel */
#define HAL_GP_DMA_SEL1_HSID11_IR_TX                                               11 /**< Infrared Transmit DMA channel */
#define HAL_GP_DMA_SEL1_HSID12_GPADC                                               12 /**< General Purpose ADC DMA channel */
#define HAL_GP_DMA_SEL1_HSID13_I2C0                                                13 /**< I2C0 DMA channel */
#define HAL_GP_DMA_SEL1_HSID14_I2C1                                                14 /**< I2C1 DMA channel */
#define HAL_GP_DMA_SEL1_HSID15_GPADC                                               15 /**< General Purpose ADC DMA channel (alternate) */
/** @} */ /* End of group VENUSA_GP_DMA_SEL1 */
/** @} */ /* End of group VENUSA_DMA_Definitions */

/*
| DMAC     | hsid | hssel=0        | hssel=1        |
| ---------| ---- | -------------- | -------------- |
| CMNDMA   | 0    | uart0_rx       | rgb_dma        |
| CMNDMA   | 1    | uart0_tx       | qspi_out_dma   |
| CMNDMA   | 2    | uart1_rx       | qspi_in_dma    |
| CMNDMA   | 3    | uart1_tx       | dvp_dma        |
| CMNDMA   | 4    | uart2_rx       | jpg_dma_e      |
| CMNDMA   | 5    | uart2_tx       | jpg_dma_p      |
| CMNDMA   | 6    | spi0_rx        | uart1_rx       |
| CMNDMA   | 7    | spi0_tx        | uart1_tx       |
| CMNDMA   | 8    | spi1_rx        | apc_dma_tx[0]  |
| CMNDMA   | 9    | spi1_tx        | apc_dma_tx[1]  |
| CMNDMA   | 10   | ir_rx          | apc_dma_tx[2]  |
| CMNDMA   | 11   | ir_tx          | i8080_dma      |
| CMNDMA   | 12   | gpadc          | apc_dma_rx[0]  |
| CMNDMA   | 13   | i2c0           | apc_dma_rx[1]  |
| CMNDMA   | 14   | i2c1           | apc_dma_rx[2]  |
| CMNDMA   | 15   | dvp_dma        | apc_dma_rx[3]  |
| GPDMA2D  | 0    | rgb_dma        | uart0_rx       |
| GPDMA2D  | 1    | qspi_out_dma   | uart0_tx       |
| GPDMA2D  | 2    | qspi_in_dma    | uart1_rx       |
| GPDMA2D  | 3    | dvp_dma        | uart1_tx       |
| GPDMA2D  | 4    | jpg_dma_e      | uart2_rx       |
| GPDMA2D  | 5    | jpg_dma_p      | uart2_tx       |
| GPDMA2D  | 6    | uart0_rx       | spi0_rx        |
| GPDMA2D  | 7    | uart0_tx       | spi0_tx        |
| GPDMA2D  | 8    | apc_dma_tx[0]  | spi1_rx        |
| GPDMA2D  | 9    | apc_dma_tx[1]  | spi1_tx        |
| GPDMA2D  | 10   | apc_dma_tx[2]  | ir_rx          |
| GPDMA2D  | 11   | i8080_dma      | ir_tx          |
| GPDMA2D  | 12   | apc_dma_rx[0]  | gpadc          |
| GPDMA2D  | 13   | apc_dma_rx[1]  | i2c0           |
| GPDMA2D  | 14   | apc_dma_rx[2]  | i2c1           |
| GPDMA2D  | 15   | apc_dma_rx[3]  | gpadc          |
*/

typedef enum {
    DMA_TYPE_CMNDMA = 0,
    DMA_TYPE_GPDMA2D,
} dma_type_t;

/**
 * @brief set handshake ID of DMA
 * @param[in] dma: DMA_TYPE_CMNDMA or DMA_TYPE_GPDMA2D
 * @param[in] hssel: 0 or 1
 * @param[in] hsid: 0~15 (HAL_CMN_DMA_SELx_HSIDx_xxxxxx)
 */
static inline void HAL_DMA_HANDSHAKE(dma_type_t dma, uint8_t hssel, uint8_t hsid) {
    if(dma == DMA_TYPE_CMNDMA) {
        if (hssel) {
            IP_SYSCTRL->REG_DMA_HS.all |= 0x1 << hsid;
        } else {
            IP_SYSCTRL->REG_DMA_HS.all &= ~(0x1 << hsid);
        }
    } else {
        if (hssel) {
            IP_SYSCTRL->REG_DMA_HS.all |= 0x1 << (16 + hsid);
        } else {
            IP_SYSCTRL->REG_DMA_HS.all &= ~(0x1 << (16 + hsid));
        }
    }
}


/** @} */ /* End of group VENUSA_Processor */

#endif /* INCLUDE_VENUSA_AP_H_ */
