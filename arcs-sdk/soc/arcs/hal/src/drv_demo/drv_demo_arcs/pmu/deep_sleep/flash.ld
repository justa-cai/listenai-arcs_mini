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

OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
  ap_ilm (wxa!r)   : ORIGIN = 0x80000,    LENGTH = 16K
  ap_dlm (wxa!r)   : ORIGIN = 0x100000,   LENGTH = 8K
  cp_ilm (wxa!r)   : ORIGIN = 0x280000,   LENGTH = 16K
  cp_dlm (wxa!r)   : ORIGIN = 0x300000,   LENGTH = 8K
  wifi_ram (wxa!r) : ORIGIN = 0x20000000, LENGTH = 256K
  cp_ram (wxa!r)   : ORIGIN = 0x20040000, LENGTH = 64K
  ap_ram (wxa!r)   : ORIGIN = 0x20050000, LENGTH = 384K
  flash (rxa!w)    : ORIGIN = 0x30000000, LENGTH = 8M
}

REGION_ALIAS("AP_ILM", ap_ilm)
REGION_ALIAS("AP_DLM", ap_dlm)
REGION_ALIAS("CP_ILM", cp_ilm)
REGION_ALIAS("CP_DLM", cp_dlm)
REGION_ALIAS("WIFI_RAM", wifi_ram)
REGION_ALIAS("CP_RAM", cp_ram)
REGION_ALIAS("AP_RAM", ap_ram)
REGION_ALIAS("FLASH", flash)

REGION_ALIAS("RAM", ap_ram)

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = 4K);
  PROVIDE(__HEAP_SIZE_LIMIT = 4K);
  PROVIDE(__SMP_CPU_CNT = 1);
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;

  .init           :
  {
    KEEP(*(.OTA_HEADER)) /* OTA header */
    KEEP(*(.SIGN_DATA))  /* signature data */
    /* vector table locate at FLASH */
    . = ALIGN(512);
    *(.vtable)
    *(.vtable_s)
    KEEP (*(SORT_NONE(.init)))
    . = ALIGN(4);
  } >FLASH AT>FLASH

  .copy_table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__ap_ilm_text_lma)
    LONG (__ap_ilm_text_start__)
    LONG (__ap_ilm_text_end__)

    LONG (__cp_ilm_text_lma)
    LONG (__cp_ilm_text_start__)
    LONG (__cp_ilm_text_end__)

    LONG (__wifi_ram_text_lma)
    LONG (__wifi_ram_text_start__)
    LONG (__wifi_ram_text_end__)

    LONG (__cp_ram_text_lma)
    LONG (__cp_ram_text_start__)
    LONG (__cp_ram_text_end__)

    LONG (__ap_ram_text_lma)
    LONG (__ap_ram_text_start__)
    LONG (__ap_ram_text_end__)

    /* data section*/

    LONG (__ap_dlm_data_lma)
    LONG (__ap_dlm_data_start__)
    LONG (__ap_dlm_data_end__)

    LONG (__cp_dlm_data_lma)
    LONG (__cp_dlm_data_start__)
    LONG (__cp_dlm_data_end__)

    LONG (__wifi_ram_data_lma)
    LONG (__wifi_ram_data_start__)
    LONG (__wifi_ram_data_end__)

    LONG (__cp_ram_data_lma)
    LONG (__cp_ram_data_start__)
    LONG (__cp_ram_data_end__)

    LONG (__ap_ram_data_lma)
    LONG (__ap_ram_data_start__)
    LONG (__ap_ram_data_end__)

    /* add your own "fast" sections here... */

    __copy_table_end__ = .;
  } >FLASH

  .zero_table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;

    LONG (__ap_dlm_bss_start__)
    LONG (__ap_dlm_bss_end__)

    LONG (__cp_dlm_bss_start__)
    LONG (__cp_dlm_bss_end__)

    LONG (__wifi_ram_bss_start__)
    LONG (__wifi_ram_bss_end__)

    LONG (__cp_ram_bss_start__)
    LONG (__cp_ram_bss_end__)

    LONG (__ap_ram_bss_start__)
    LONG (__ap_ram_bss_end__)

    __zero_table_end__ = .;
  } >FLASH

  .ap_ilm_text           :
  {
  	. = ALIGN(4);
    __ap_ilm_text_start__ = .;
    *(.text.ap_ilm .text.ap_ilm.*)
    . = ALIGN(4);
    __ap_ilm_text_end__ = .;
  } >AP_ILM AT>FLASH

  __ap_ilm_text_lma = LOADADDR(.ap_ilm_text);

  .cp_ilm_text           :
  {
  	. = ALIGN(4);
    __cp_ilm_text_start__ = .;
    *(.text.cp_ilm .text.cp_ilm.*)
    . = ALIGN(4);
    __cp_ilm_text_end__ = .;
  } >CP_ILM AT>FLASH

  __cp_ilm_text_lma = LOADADDR(.cp_ilm_text);

  .wifi_ram_text           :
  {
  	. = ALIGN(4);
    __wifi_ram_text_start__ = .;
    *(.text.wifi_ram .text.wifi_ram.*)
    . = ALIGN(4);
    __wifi_ram_text_end__ = .;
  } >WIFI_RAM AT>FLASH

  __wifi_ram_text_lma = LOADADDR(.wifi_ram_text);

  .cp_ram_text           :
  {
  	. = ALIGN(4);
    __cp_ram_text_start__ = .;
    *(.text.cp_ram .text.cp_ram.*)
    . = ALIGN(4);
    __cp_ram_text_end__ = .;
  } >CP_RAM AT>FLASH

  __cp_ram_text_lma = LOADADDR(.cp_ram_text);

  .ap_ram_text           :
  {
  	. = ALIGN(4);
    __ap_ram_text_start__ = .;
    *(.text.ap_ram .text.ap_ram.*)
    . = ALIGN(4);
    __ap_ram_text_end__ = .;
  } >AP_RAM AT>FLASH

  __ap_ram_text_lma = LOADADDR(.ap_ram_text);

  .ap_dlm_data           :
  {
  	. = ALIGN(4);
    __ap_dlm_data_start__ = .;
    *(.data.ap_dlm .data.ap_dlm.*)
    . = ALIGN(4);
    __ap_dlm_data_end__ = .;
  } >AP_DLM AT>FLASH

  __ap_dlm_data_lma = LOADADDR(.ap_dlm_data);

  .cp_dlm_data           :
  {
  	. = ALIGN(4);
    __cp_dlm_data_start__ = .;
    *(.data.cp_dlm .data.cp_dlm.*)
    . = ALIGN(4);
    __cp_dlm_data_end__ = .;
  } >CP_DLM AT>FLASH

  __cp_dlm_data_lma = LOADADDR(.cp_dlm_data);

  /* Code section located at FLASH */
  .text           :
  {
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
    _etext = .;
  } >FLASH AT>FLASH

  PROVIDE( _text_lma = LOADADDR(.text) );
  PROVIDE( _text = ADDR(.text) );

  .ro_text           :
  {
    /* readonly data placed in FLASH */
    . = ALIGN(8);
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.rdata)
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
  } >FLASH AT>FLASH

  .fini           :
  {
    KEEP (*(SORT_NONE(.fini)))
  } >FLASH AT>FLASH

  .preinit_array  :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH AT>FLASH

  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH AT>FLASH

  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH AT>FLASH

  .wifi_ram_data           :
  {
  	. = ALIGN(4);
    __wifi_ram_data_start__ = .;
    *(.data.wifi_ram .data.wifi_ram.*)
    . = ALIGN(4);
    __wifi_ram_data_end__ = .;
  } >WIFI_RAM AT>FLASH

  __wifi_ram_data_lma = LOADADDR(.wifi_ram_data);

  .cp_ram_data           :
  {
  	. = ALIGN(4);
    __cp_ram_data_start__ = .;
    *(.data.cp_ram .data.cp_ram.*)
    . = ALIGN(4);
    __cp_ram_data_end__ = .;
  } >CP_RAM AT>FLASH

  __cp_ram_data_lma = LOADADDR(.cp_ram_data);

  .ap_ram_data           :
  {
  	. = ALIGN(4);
    __ap_ram_data_start__ = .;
    *(.data.ap_ram .data.ap_ram.*)
    . = ALIGN(4);
    __ap_ram_data_end__ = .;
  } >AP_RAM AT>FLASH

  __ap_ram_data_lma = LOADADDR(.ap_ram_data);

  .data            : ALIGN(8)
  {
    PROVIDE( __global_pointer$ = . + 0x800 );
    KEEP(*(.data.ctest*))
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    . = ALIGN(8);
    *(.sdata .sdata.* .sdata*)
    *(.gnu.linkonce.s.*)
    . = ALIGN(8);
  } >RAM AT>FLASH

  .tdata           : ALIGN(8)
  {
    PROVIDE( __tls_base = . );
    *(.tdata .tdata.* .gnu.linkonce.td.*)
  } >RAM AT>FLASH

  PROVIDE( _data_lma = LOADADDR(.data) );
  PROVIDE( _data = ADDR(.data) );
  PROVIDE( _edata = . );
  PROVIDE( edata = . );

  .ap_dlm_bss (NOLOAD)           :
  {
  	. = ALIGN(4);
    __ap_dlm_bss_start__ = .;
    *(.bss.ap_dlm .bss.ap_dlm.*)
    . = ALIGN(4);
    __ap_dlm_bss_end__ = .;
  } >AP_DLM AT>FLASH

  .cp_dlm_bss (NOLOAD)           :
  {
  	. = ALIGN(4);
    __cp_dlm_bss_start__ = .;
    *(.bss.cp_dlm .bss.cp_dlm.*)
    . = ALIGN(4);
    __cp_dlm_bss_end__ = .;
  } >CP_DLM AT>FLASH

  .wifi_ram_bss (NOLOAD)           :
  {
  	. = ALIGN(4);
    __wifi_ram_bss_start__ = .;
    *(.bss.wifi_ram .bss.wifi_ram.*)
    . = ALIGN(4);
    __wifi_ram_bss_end__ = .;
  } >WIFI_RAM AT>FLASH

  .cp_ram_bss  (NOLOAD)          :
  {
  	. = ALIGN(4);
    __cp_ram_bss_start__ = .;
    *(.bss.cp_ram .bss.cp_ram.*)
    . = ALIGN(4);
    __cp_ram_bss_end__ = .;
  } >CP_RAM AT>FLASH

  .ap_ram_bss  (NOLOAD)          :
  {
  	. = ALIGN(4);
    __ap_ram_bss_start__ = .;
    *(.bss.ap_ram .bss.ap_ram.*)
    . = ALIGN(4);
    __ap_ram_bss_end__ = .;
  } >AP_RAM AT>FLASH

  .tbss (NOLOAD)   : ALIGN(8)
  {
    PROVIDE( __bss_start = . );
    *(.tbss .tbss.* .gnu.linkonce.tb.*)
    *(.tcommon)
    PROVIDE( __tls_end = . );
  } >RAM AT>RAM

  .tbss_space (NOLOAD) : ALIGN(8)
  {
    . = . + SIZEOF(.tbss);
  } >RAM AT>RAM

  .bss (NOLOAD)   : ALIGN(8)
  {
    *(.sbss*)
    *(.gnu.linkonce.sb.*)
    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
   
    PROVIDE( _end = . );
    PROVIDE( end = . );
  } >RAM AT>RAM

  /* Nuclei C Runtime Library requirements:
   * 1. heap need to be align at 16 bytes
   * 2. __heap_start and __heap_end symbol need to be defined
   * 3. reserved at least __HEAP_SIZE space for heap
   */
  .heap (NOLOAD)   : ALIGN(16)
  {
    . = ALIGN(16);
    PROVIDE( __heap_start = . );
    PROVIDE( __heap_bottom = . );
  } >RAM AT>RAM

  .stack ORIGIN(RAM) + LENGTH(RAM) - __TOT_STACK_SIZE (NOLOAD) :
  {
    . = ALIGN(16);
    PROVIDE( __heap_top = . );
    PROVIDE( __heap_limit = . );
    PROVIDE( _heap_end = . );
    PROVIDE( __heap_end = . );
    PROVIDE( __StackLimit = . );
    PROVIDE( __StackBottom = . );
    . += __TOT_STACK_SIZE;
    . = ALIGN(16);
    PROVIDE( __StackTop = . );
    PROVIDE( _sp = . );
  } >RAM AT>RAM

  __HEAP_SIZE = __heap_end - __heap_start;

  ASSERT(__HEAP_SIZE >= __HEAP_SIZE_LIMIT, "HEAP size is under the size limit!!!")
}
  
  