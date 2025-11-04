/*
 * Copyright (c) 2019 Nuclei Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/******************************************************************************
 * @file     gcc_evalsoc_ilm.ld
 * @brief    GNU Linker Script for Nuclei N/NX based device in ilm Download Mode
 * @version  V1.0.0
 * @date     17. Dec 2019
 ******************************************************************************/
OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
  flash (rxa!w) : ORIGIN = 0x30010000, LENGTH = 2M
  ilm (rxa!w) : ORIGIN = 0x00280000, LENGTH = 16K
  dlm (wxa!r) : ORIGIN = 0x00300000, LENGTH = 8K
  SHRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
  ram (wxa!r) : ORIGIN = 0x20000000, LENGTH = 320K
  apram (wxa!r) : ORIGIN = 0x20050000, LENGTH = 384K
  psram (wxa!r) : ORIGIN = 0x28000000, LENGTH = 1M
}

REGION_ALIAS("ROM", flash)
REGION_ALIAS("RAM", ram)
#if PSRAM_SEC==1
REGION_ALIAS("EXTRAM", psram)
#else
REGION_ALIAS("EXTRAM", apram)
#endif
REGION_ALIAS("DATA_LMA", flash)
REGION_ALIAS("ILM", ilm)
REGION_ALIAS("DLM", dlm)

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = 4K);
  /* heap size defined in FreeRtosConfig.h, if RTOS module was not used, define heap size in ld */
  /*PROVIDE(__HEAP_SIZE = 80K);*/
  PROVIDE(__BT_HEAP_SIZE = 24K);/*32K size for BT heap*/  
  PROVIDE(__SMP_CPU_CNT = 1);
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;

  /* memory for calibration */
  PROVIDE(_lcali = 16K);
  /* memory for trace */
  PROVIDE(_trace_len = 8K);

  /* shared RAM, put calibration and trace memeory at the beginning */
  SHARED (NOLOAD):
  {
#if CALI_BUF
      _scali = .;
      . += _lcali;
      _ecali = .;
#endif
#if SYS_TRACE
      _trace_start = .;
      . += _trace_len;
      _trace_end = .;
#endif
      _sshram = . ;
      *(WIFI_CALI)
      *(SHAREDRAMIPC)
      *(SHAREDRAM)
      _eshram = . ;
  } > RAM

  ASSERT  ((_eshram < ORIGIN(SHRAM) + LENGTH(SHRAM)), "Error: No enough sharedram")

  .init           :
  {
    *(.vtable)
    *(.vtable_s)
    KEEP (*(SORT_NONE(.init)))
    . = ALIGN(4);
  } >ROM AT>ROM

  .copy_table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__cp_ilm_text_lma)
    LONG (__cp_ilm_text_start__)
    LONG (__cp_ilm_text_end__)

    /* add your own "fast" sections here... */

    __copy_table_end__ = .;
  } >ROM

  .cp_ilm_text           :
  {
    . = ALIGN(4);
    __cp_ilm_text_start__ = .;
    /* NOTE: .ap_ilm_text SHOULD BE REMOVED if .cp_ilm_text is used, */
    /* or else .text.irq etc. sections will be placed into .ap_ilm_text!! */
    *(.text.cp_ilm .text.cp_ilm.* .text.entry .text.irq .text.irq.* .text.xPortTaskSwitch)
    . = ALIGN(4);
    __cp_ilm_text_end__ = .;
  } >ILM AT>ROM

  __cp_ilm_text_lma = LOADADDR(.cp_ilm_text);

  .text           :
  {
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
#if PSRAM_SEC==1 || PERF==1
    *(EXCLUDE_FILE (*liblwip.a *libumacdp.a *liblmacdp.a *libmoduledp.a *libplf.a *librtos.a) .text*)
#else
    *(.text .text.*)
#endif
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.rdata)
    /*
     * RSIC-V's gcc add some optimization codes in .rodata section
     * during the compiling time, this will messed up the flash op's
     * statemachine(all rodata are supposed to be placed on flash).
     * So we make a execption here for spiflash.o
     */
    *(EXCLUDE_FILE(*spiflash.o) .rodata)
    *(.rodata.*)
    *(.gnu.linkonce.t.*)
  } >ROM AT>ROM

  .fini           :
  {
    KEEP (*(SORT_NONE(.fini)))
  } >ROM AT>ROM

  .preinit_array  :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >ROM AT>ROM

  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >ROM AT>ROM

  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >ROM AT>ROM

  .ctors          :
  {
    /* gcc uses crtbegin.o to find the start of
     * the constructors, so we make sure it is
     * first.  Because this is a wildcard, it
     * doesn't matter if the user does not
     * actually link against crtbegin.o; the
     * linker won't look for a file to match a
     * wildcard.  The wildcard also means that it
     * doesn't matter which directory crtbegin.o
     * is in.
     */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
     * the crtend.o file until after the sorted ctors.
     * The .ctor section from the crtend file contains the
     * end of ctors marker and it must be last
     */
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  } >ROM AT>ROM

  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } >ROM AT>ROM

  .cali_rodata    :
  {
    KEEP(*(.cali_rodata))
  } >ROM AT>ROM

  .text_luna :
  {
      *(.sharedmem.text)
  } >ROM AT>ROM

  .data_luna (NOLOAD) : ALIGN(4)
  {
    *(.sharedmem.data)
  } >RAM AT>ROM

  PROVIDE(_rom_code_start = ORIGIN(ROM));
  PROVIDE(_rom_code_end   = .);
  .ramcode        : ALIGN(8)
  {
    *(.rodata .rodata.*)
    *(.ramcode)
    . = ALIGN(8);
  } >RAM AT>DATA_LMA

  PROVIDE(_ram_code_start = ADDR(.ramcode));
  PROVIDE( _ram_code_end  = .);
  PROVIDE( _ilm_lma = LOADADDR(.ramcode) );
  PROVIDE( _ilm = ADDR(.ramcode) );
  PROVIDE( _eilm = . );
  PROVIDE( _text_lma = LOADADDR(.ramcode) );
  PROVIDE( _text = ADDR(.ramcode) );
  PROVIDE (_etext = .);
  PROVIDE (__etext = .);
  PROVIDE (etext = .);

  .text2           :
  {
    __text2_start__ = .;
#if PSRAM_SEC==1 || PERF==1
    *(EXCLUDE_FILE (*librtos.a) .text*)
#endif
    . = ALIGN(4);
    __text2_end__ = .;
  } >EXTRAM AT>ROM

  PROVIDE (_text2_lma = _text_lma + ALIGN (SIZEOF(.ramcode), 8));
  PROVIDE (_stext2 = ADDR(.text2));
  PROVIDE (_etext2 = .);
  PROVIDE (__etext2 = .);
  PROVIDE (etext2 = .);

  .text3           :
  {
    __text3_start__ = .;
#if PSRAM_SEC==1 || PERF==1
    librtos.a *(.text*)
#endif
    . = ALIGN(4);
    __text3_end__ = .;
  } >EXTRAM AT>ROM

  PROVIDE (_text3_lma = _text2_lma + ALIGN (SIZEOF(.text2), 4));
  PROVIDE (_stext3 = ADDR(.text3));
  PROVIDE (_etext3 = .);
  PROVIDE (__etext3 = .);
  PROVIDE (etext3 = .);
  /* memory for LA dump */
  PROVIDE (_slaram =  ALIGN (_etext3, 8));
  PROVIDE(_elaram = ORIGIN(EXTRAM) + LENGTH(EXTRAM));

  .data            : ALIGN(8)
  {
    KEEP(*(.data.ctest*))
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    . = ALIGN(8);
    PROVIDE( __global_pointer$ = . + 0x800 );
    *(.sdata .sdata.* .sdata*)
    *(.gnu.linkonce.s.*)
    /* readonly data placed in RAM for access speed */
    . = ALIGN(8);
    *(.gnu.linkonce.r.*)
    /* below sections are used for rt-thread */
    . = ALIGN(4);
    __rt_init_start = .;
    KEEP(*(SORT(.rti_fn*)))
    __rt_init_end = .;
    . = ALIGN(4);
    __fsymtab_start = .;
    KEEP(*(FSymTab))
    __fsymtab_end = .;
    . = ALIGN(4);
    __vsymtab_start = .;
    KEEP(*(VSymTab))
    __vsymtab_end = .;
    . = ALIGN(8);
  } >DLM AT>DATA_LMA

  .tdata           : ALIGN(8)
  {
    PROVIDE( __tls_base = . );
    *(.tdata .tdata.* .gnu.linkonce.td.*)
  } >DLM AT>DATA_LMA

  PROVIDE( _data_lma = LOADADDR(.data) );
  PROVIDE( _data = ADDR(.data) );
  PROVIDE( _edata = . );
  PROVIDE( edata = . );

  .cali_luna_bss (NOLOAD) : ALIGN(8)
  {
    *(.cali_luna_bss)
  } >EXTRAM

  .tbss (NOLOAD)   : ALIGN(8)
  {
    *(.tbss .tbss.* .gnu.linkonce.tb.*)
    *(.tcommon)
    PROVIDE( __tls_end = . );
  } >RAM AT>RAM

  PROVIDE( _fbss = ADDR(.tbss) );
  PROVIDE( __bss_start = _fbss );

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
    . = ALIGN(8);
    *(.sharedmem.bss)
  } >RAM AT>RAM

  /* Nuclei C Runtime Library requirements:
   * 1. heap need to be align at 16 bytes
   * 2. __heap_start and __heap_end symbol need to be defined
   */
  .heap (NOLOAD)   : ALIGN(16)
  {
    . = ALIGN(16);

    __heap_start = . ;
    __heap_bottom = . ;
    *(MHEAP)

    . = ALIGN(16);

    __heap_top = . ;
    __heap_limit = . ;
    PROVIDE( __ext_ram_start = . );
     . = . + __BT_HEAP_SIZE;
     PROVIDE( __ext_ram_end = . );
  } >RAM AT>RAM

  PROVIDE( _end = . );
  PROVIDE( end = . );
  ASSERT  ((_end < ORIGIN(RAM) + LENGTH(RAM)), "Error: No enough ram")

  PROVIDE (_estack   = ORIGIN(DLM) + LENGTH(DLM));
  PROVIDE ( __StackTop  = _estack );
  PROVIDE( _sp = __StackTop );
  PROVIDE(__stack = __StackTop);

  /* system stack */
  PROVIDE (_sstack  = _estack - __STACK_SIZE); /* _estack is top of stack*/
  PROVIDE ( __StackLimit  = _sstack );
  PROVIDE ( __StackBottom  = __StackLimit );
  ASSERT  ((_sstack > _edata), "Error: No room left for the stack")
}
