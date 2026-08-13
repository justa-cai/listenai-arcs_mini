/**
 ******************************************************************************
 * @file      gcc_riscv32.ld
 * @author    ListenAI application team
 * @brief     Linker script
 *                      8Mbytes     FLASH
 *                      16Kbytes    DTCMRAM
 *                      16Kbytes    ITCMRAM
 *                      128Kbytes   RAM
 *
 *            Set heap size, stack size and stack location according
 *            to application requirements.
 *
 *            Set memory bank area and size if external memory is used
 ******************************************************************************
 * @attentionsss
 *
 * <h2><center>&copy; Copyright (c) 2021 ListenAI.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ListenAI under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
#include "linker_script_config.h"

OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
  __HAL_LS_CFG_CORE_ROM_NICK (wxa!r)     : ORIGIN = __HAL_LS_CFG_CORE_ROM_BASE,        LENGTH = __HAL_LS_CFG_CORE_ROM_SIZE
  __HAL_LS_CFG_CORE0_ILM_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CORE0_ILM_BASE,       LENGTH = __HAL_LS_CFG_CORE0_ILM_SIZE
  __HAL_LS_CFG_CORE0_DLM_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CORE0_DLM_BASE,       LENGTH = __HAL_LS_CFG_CORE0_DLM_SIZE
  __HAL_LS_CFG_CORE1_ILM_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CORE1_ILM_BASE,       LENGTH = __HAL_LS_CFG_CORE1_ILM_SIZE
  __HAL_LS_CFG_CORE1_DLM_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CORE1_DLM_BASE,       LENGTH = __HAL_LS_CFG_CORE1_DLM_SIZE
  __HAL_LS_CFG_CMN_RAM0_NICK (wxa!r)     : ORIGIN = __HAL_LS_CFG_CMN_RAM0_BASE,        LENGTH = __HAL_LS_CFG_CMN_RAM0_SIZE
  __HAL_LS_CFG_CMN_RAM1_NICK (wxa!r)     : ORIGIN = __HAL_LS_CFG_CMN_RAM1_BASE,        LENGTH = __HAL_LS_CFG_CMN_RAM1_SIZE
  __HAL_LS_CFG_CMN_LUNA_RAM_NICK (wxa!r) : ORIGIN = __HAL_LS_CFG_CMN_LUNA_RAM_BASE,    LENGTH = __HAL_LS_CFG_CMN_LUNA_RAM_SIZE
  __HAL_LS_CFG_CMN_FLASH_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CMN_FLASH_BASE,       LENGTH = __HAL_LS_CFG_CMN_FLASH_SIZE
  __HAL_LS_CFG_CMN_PSRAM_NICK (wxa!r)    : ORIGIN = __HAL_LS_CFG_CMN_PSRAM_BASE,       LENGTH = __HAL_LS_CFG_CMN_PSRAM_SIZE
}

REGION_ALIAS("TEXT", __HAL_LS_CFG_CMN_TEXT_VMA)
REGION_ALIAS("TEXT_LMA", __HAL_LS_CFG_CMN_TEXT_LMA)

REGION_ALIAS("DATA", __HAL_LS_CFG_CMN_DATA_VMA)
REGION_ALIAS("DATA_LMA", __HAL_LS_CFG_CMN_DATA_LMA)

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = __HAL_LS_CFG_SYSTEM_STACK_SIZE);
  PROVIDE(__HEAP_SIZE_LIMIT = __HAL_LS_CFG_SYSTEM_HEAP_SIZE_LIMIT);
  PROVIDE(__SMP_CPU_CNT = __HAL_LS_CFG_CORE_CNT);
  PROVIDE(_text_lma = 0);
  PROVIDE(_text = 0);
  PROVIDE(_etext = 0);
  PROVIDE(_data_lma = 0);
  PROVIDE(_data = 0);
  PROVIDE(_edata = 0);  
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;

  .init           :
  {
    /* vector table locate at FLASH */
    . = ALIGN(512);
    *(.vtable)
    *(.vtable_s)
    KEEP (*(SORT_NONE(.init)))
    . = ALIGN(4);
  } >TEXT_LMA

  .copy_table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;
/* Core0 */
#if BOOT_HARDID == 0
    LONG (__core0_ilm_text_lma)
    LONG (__core0_ilm_text_start__)
    LONG (__core0_ilm_text_end__)

    LONG (__core0_dlm_data_lma)
    LONG (__core0_dlm_data_start__)
    LONG (__core0_dlm_data_end__)
/* Core1 */
#else
    LONG (__core1_ilm_text_lma)
    LONG (__core1_ilm_text_start__)
    LONG (__core1_ilm_text_end__)

    LONG (__core1_dlm_data_lma)
    LONG (__core1_dlm_data_start__)
    LONG (__core1_dlm_data_end__)
#endif
/* Common RAM0 */
    LONG (__cmn_ram0_text_lma)
    LONG (__cmn_ram0_text_start__)
    LONG (__cmn_ram0_text_end__)

    LONG (__cmn_ram0_data_lma)
    LONG (__cmn_ram0_data_start__)
    LONG (__cmn_ram0_data_end__)

/* Common RAM1 */
    LONG (__cmn_ram1_text_lma)
    LONG (__cmn_ram1_text_start__)
    LONG (__cmn_ram1_text_end__)

    LONG (__cmn_ram1_data_lma)
    LONG (__cmn_ram1_data_start__)
    LONG (__cmn_ram1_data_end__)

/* Common LUNA RAM */
    LONG (__cmn_luna_ram_text_lma)
    LONG (__cmn_luna_ram_text_start__)
    LONG (__cmn_luna_ram_text_end__)

    LONG (__cmn_luna_ram_data_lma)
    LONG (__cmn_luna_ram_data_start__)
    LONG (__cmn_luna_ram_data_end__)

/* Common RAM section */
    LONG (__common_section_text_lma)
    LONG (__common_section_text_start__)
    LONG (__common_section_text_end__)

    LONG (__common_section_data_lma)
    LONG (__common_section_data_start__)
    LONG (__common_section_data_end__)
    __copy_table_end__ = .;
  } >TEXT_LMA

  .zero_table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;

/* Core0 */
#if BOOT_HARDID == 0
    LONG (__core0_dlm_bss_start__)
    LONG (__core0_dlm_bss_end__)

/* Core1 */
#else
    LONG (__core1_dlm_bss_start__)
    LONG (__core1_dlm_bss_end__)
#endif

/* Common RAM0 */
    LONG (__cmn_ram0_bss_start__)
    LONG (__cmn_ram0_bss_end__)

/* Common RAM1 */
    LONG (__cmn_ram1_bss_start__)
    LONG (__cmn_ram1_bss_end__)

/* Common LUNA RAM */
    LONG (__cmn_luna_ram_bss_start__)
    LONG (__cmn_luna_ram_bss_end__)

/* Common RAM section */
    LONG (__common_section_bss_start__)
    LONG (__common_section_bss_end__)

    __zero_table_end__ = .;
  } >TEXT_LMA

/* Core0 */
#if BOOT_HARDID == 0

  .core0_ilm_text           :
  {
  	. = ALIGN(4);
    __core0_ilm_text_start__ = .;
    *(.text.core0_ilm .text.core0_ilm.*)
    . = ALIGN(4);
    __core0_ilm_text_end__ = .;
  } >__HAL_LS_CFG_CORE0_ILM_NICK AT>TEXT_LMA

  __core0_ilm_text_lma = LOADADDR(.core0_ilm_text);

/* Core1 */
#else

  .core1_ilm_text           :
  {
  	. = ALIGN(4);
    __core1_ilm_text_start__ = .;
    *(.text.core1_ilm .text.core1_ilm.*)
    . = ALIGN(4);
    __core1_ilm_text_end__ = .;
  } >__HAL_LS_CFG_CORE1_ILM_NICK AT>TEXT_LMA

  __core1_ilm_text_lma = LOADADDR(.core1_ilm_text);

#endif

  .cmn_ram0_text           :
  {
  	. = ALIGN(4);
    __cmn_ram0_text_start__ = .;
    *(.text.cmn_ram0 .text.cmn_ram0.*)
    . = ALIGN(4);
    __cmn_ram0_text_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM0_NICK AT>TEXT_LMA

  __cmn_ram0_text_lma = LOADADDR(.cmn_ram0_text);

  .cmn_ram1_text           :
  {
  	. = ALIGN(4);
    __cmn_ram1_text_start__ = .;
    *(.text.cmn_ram1 .text.cmn_ram1.*)
    . = ALIGN(4);
    __cmn_ram1_text_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM1_NICK AT>TEXT_LMA

  __cmn_ram1_text_lma = LOADADDR(.cmn_ram1_text);

  .cmn_luna_ram_text           :
  {
  	. = ALIGN(4);
    __cmn_luna_ram_text_start__ = .;
    *(.text.cmn_luna_ram .text.cmn_luna_ram.*)
    . = ALIGN(4);
    __cmn_luna_ram_text_end__ = .;
  } >__HAL_LS_CFG_CMN_LUNA_RAM_NICK AT>TEXT_LMA

  __cmn_luna_ram_text_lma = LOADADDR(.cmn_luna_ram_text);

  .cmn_psram_text           :
  {
  	. = ALIGN(4);
    __cmn_psram_text_start__ = .;
    *(.text.cmn_psram .text.cmn_psram.*)
    . = ALIGN(4);
    __cmn_psram_text_end__ = .;
  } >__HAL_LS_CFG_CMN_PSRAM_NICK AT>TEXT_LMA

  __cmn_psram_text_lma = LOADADDR(.cmn_psram_text);

  .common_section_text           :
  {
    . = ALIGN(4);
    __common_section_text_start__ = .;
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
    __common_section_text_end__ = .;
  } >TEXT AT>TEXT_LMA

  __common_section_text_lma = LOADADDR(.common_section_text);

  .common_section_ro_text           :
  {
    . = ALIGN(8);
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.rdata)
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
  } >TEXT_LMA

  .fini           :
  {
    KEEP (*(SORT_NONE(.fini)))
  } >TEXT_LMA

  .preinit_array  :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >TEXT_LMA

  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >TEXT_LMA

  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >TEXT_LMA


/*************************DATA*********************************/
/* Core0 */
#if BOOT_HARDID == 0

  .core0_dlm_data           :
  {
  	. = ALIGN(4);
    __core0_dlm_data_start__ = .;
    *(.data.core0_dlm .data.core0_dlm.*)
    . = ALIGN(4);
    __core0_dlm_data_end__ = .;
  } >__HAL_LS_CFG_CORE0_DLM_NICK AT>TEXT_LMA

  __core0_dlm_data_lma = LOADADDR(.core0_dlm_data);

/* Core1 */
#else

  .core1_dlm_data           :
  {
  	. = ALIGN(4);
    __core1_dlm_data_start__ = .;
    *(.data.core1_dlm .data.core1_dlm.*)
    . = ALIGN(4);
    __core1_dlm_data_end__ = .;
  } >__HAL_LS_CFG_CORE1_DLM_NICK AT>TEXT_LMA

  __core1_dlm_data_lma = LOADADDR(.core1_dlm_data);

#endif

  .cmn_ram0_data           :
  {
  	. = ALIGN(4);
    __cmn_ram0_data_start__ = .;
    *(.data.cmn_ram0 .data.cmn_ram0.*)
    . = ALIGN(4);
    __cmn_ram0_data_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM0_NICK AT>TEXT_LMA

  __cmn_ram0_data_lma = LOADADDR(.cmn_ram0_data);

  .cmn_ram1_data           :
  {
  	. = ALIGN(4);
    __cmn_ram1_data_start__ = .;
    *(.data.cmn_ram1 .data.cmn_ram1.*)
    . = ALIGN(4);
    __cmn_ram1_data_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM1_NICK AT>TEXT_LMA

  __cmn_ram1_data_lma = LOADADDR(.cmn_ram1_data);

  .cmn_luna_ram_data           :
  {
  	. = ALIGN(4);
    __cmn_luna_ram_data_start__ = .;
    *(.data.cmn_luna_ram .data.cmn_luna_ram.*)
    . = ALIGN(4);
    __cmn_luna_ram_data_end__ = .;
  } >__HAL_LS_CFG_CMN_LUNA_RAM_NICK AT>TEXT_LMA

  __cmn_luna_ram_data_lma = LOADADDR(.cmn_luna_ram_data);

  .cmn_psram_data           :
  {
  	. = ALIGN(4);
    __cmn_psram_data_start__ = .;
    *(.data.cmn_psram .data.cmn_psram.*)
    . = ALIGN(4);
    __cmn_psram_data_end__ = .;
  } >__HAL_LS_CFG_CMN_PSRAM_NICK AT>TEXT_LMA

  __cmn_psram_data_lma = LOADADDR(.cmn_psram_data);

  .common_section_data            : ALIGN(8)
  {
    PROVIDE( __global_pointer$ = . + 0x800 );
    __common_section_data_start__ = .;
    KEEP(*(.data.ctest*))
    *(.data .data.*)
    *(.ramcode .ramcode.*)
    *(.gnu.linkonce.d.*)
    . = ALIGN(8);
    *(.sdata .sdata.* .sdata*)
    *(.gnu.linkonce.s.*)
    . = ALIGN(8);
    PROVIDE( __tls_base = . );
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    __common_section_data_end__ = .;
  } >DATA AT>DATA_LMA

  __common_section_data_lma = LOADADDR(.common_section_data);

/*************************BSS*********************************/
  /* Core0 */
#if BOOT_HARDID == 0

  .core0_dlm_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __core0_dlm_bss_start__ = .;
    *(.bss.core0_dlm .bss.core0_dlm.*)
    . = ALIGN(4);
    __core0_dlm_bss_end__ = .;
  } >__HAL_LS_CFG_CORE0_DLM_NICK AT>TEXT_LMA

/* Core1 */
#else

  .core1_dlm_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __core1_dlm_bss_start__ = .;
    *(.bss.core1_dlm .bss.core1_dlm.*)
    . = ALIGN(4);
    __core1_dlm_bss_end__ = .;
  } >__HAL_LS_CFG_CORE1_DLM_NICK AT>TEXT_LMA

#endif

  .cmn_ram0_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __cmn_ram0_bss_start__ = .;
    *(.bss.cmn_ram0 .bss.cmn_ram0.*)
    . = ALIGN(4);
    __cmn_ram0_bss_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM0_NICK AT>TEXT_LMA

  .cmn_ram1_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __cmn_ram1_bss_start__ = .;
    *(.bss.cmn_ram1 .bss.cmn_ram1.*)
    . = ALIGN(4);
    __cmn_ram1_bss_end__ = .;
  } >__HAL_LS_CFG_CMN_RAM1_NICK AT>TEXT_LMA

  .cmn_luna_ram_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __cmn_luna_ram_bss_start__ = .;
    *(.bss.cmn_luna_ram .bss.cmn_luna_ram.*)
    . = ALIGN(4);
    __cmn_luna_ram_bss_end__ = .;
  } >__HAL_LS_CFG_CMN_LUNA_RAM_NICK AT>TEXT_LMA

  .cmn_psram_bss (NOLOAD)          : ALIGN(8)
  {
  	. = ALIGN(4);
    __cmn_psram_bss_start__ = .;
    *(.bss.cmn_psram .bss.cmn_psram.*)
    . = ALIGN(4);
    __cmn_psram_bss_end__ = .;
  } >__HAL_LS_CFG_CMN_PSRAM_NICK AT>TEXT_LMA

  .common_section_bss (NOLOAD)   : ALIGN(8)
  {
    __common_section_bss_start__ = .;
    *(.sbss*)
    *(.gnu.linkonce.sb.*)
    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
   
   __common_section_bss_end__ = .;
   PROVIDE(__bss_start = .);
    PROVIDE( _end = . );
  } >DATA AT>DATA_LMA

  /* Nuclei C Runtime Library requirements:
   * 1. heap need to be align at 16 bytes
   * 2. __heap_start and __heap_end symbol need to be defined
   * 3. reserved at least __HEAP_SIZE space for heap
   */
  .heap (NOLOAD)   : ALIGN(16)
  {
    . = ALIGN(16);
    PROVIDE( __heap_start = . );
  } >DATA

  .stack ORIGIN(DATA) + LENGTH(DATA) - __TOT_STACK_SIZE (NOLOAD) :
  {
    . = ALIGN(16);
    PROVIDE( __heap_end = . );
    PROVIDE( __StackBottom = . );
    . += __TOT_STACK_SIZE;
    . = ALIGN(16);
    PROVIDE( __StackTop = . );
    PROVIDE( _sp = . );
  } >DATA

  PROVIDE(__HEAP_SIZE = __heap_end - __heap_start);
  ASSERT(__HEAP_SIZE >= __HEAP_SIZE_LIMIT, "HEAP size is under the size limit!!!")  
}
  
  