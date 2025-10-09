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

__FLASH_SIZE =   0x000B0000;    /* 640KB  FLASH   :0x20000000~ 0x2035ffff  */
__RAM_SIZE =     0x00010000;    /* 256KB  RAM     :0x20036000~ 0x2003ffff  */
__EXT_RAM_SIZE = 0x00008000;    /* 256KB  EXT_RAM :0x00280000~ 0x002c0000 16KB RAM :0x00300000~ 0x00340000  */

MEMORY
{
  ilm (rxa!w) :     ORIGIN = 0x30010000,   LENGTH = __FLASH_SIZE
  ram (wxa!r) :     ORIGIN = 0x20050000,   LENGTH = __RAM_SIZE
  SHRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
  ext_ram (wxa!r) : ORIGIN = 0x20040000,   LENGTH = __EXT_RAM_SIZE
}

REGION_ALIAS("FLASH", ilm)
REGION_ALIAS("RAM", ram)
REGION_ALIAS("DATA_LMA", ilm)

/* memory for LA dump */
PROVIDE(_slaram = ORIGIN(EXTRAM));
PROVIDE(_elaram = ORIGIN(EXTRAM) + LENGTH(EXTRAM));

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = 12K);
  PROVIDE(__HEAP_SIZE =  2K);
  PROVIDE(__SMP_CPU_CNT = 1);
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;

  /* memory for calibration */
  PROVIDE(_lcali = 16K);
  /* memory for trace */
  PROVIDE(_trace_len = 8K);

  /* shared RAM, put calibration and trace memeory at the beginning */
  SHARED (NOLOAD):
  {
      _scali = .;
      . += _lcali;
      _ecali = .;
      _trace_start = .;
      . += _trace_len;
      _trace_end = .;
      _sshram = . ;
      *(SHAREDRAMIPC)
      *(SHAREDRAM)
      _eshram = . ;
  } > SHRAM

  ASSERT  ((_eshram < ORIGIN(SHRAM) + LENGTH(SHRAM)), "Error: No enough sharedram")

  .init           :
  {
    *(.vtable)
    *(.vtable_s)
    KEEP (*(SORT_NONE(.init)))
    . = ALIGN(4);
  } >FLASH AT>FLASH

  .text           :
  {
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
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
  } >FLASH AT>FLASH

  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  } >FLASH AT>FLASH

  .cali_rodata    :
  {
    KEEP(*(.cali_rodata))
  } >FLASH AT>FLASH

  PROVIDE( _ilm_lma = LOADADDR(.text) );
  PROVIDE( _ilm = ADDR(.text) );
  PROVIDE( _eilm = . );
  PROVIDE( _text_lma = LOADADDR(.text) );
  PROVIDE( _text = ADDR(.text) );
  PROVIDE (_etext = .);
  PROVIDE (__etext = .);
  PROVIDE (etext = .);

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
    *(.srodata.cst16)
    *(.srodata.cst8)
    *(.srodata.cst4)
    *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.rdata)
    *(.rodata .rodata.*)
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
  } >RAM AT>DATA_LMA

  .tdata           : ALIGN(8)
  {
    PROVIDE( __tls_base = . );
    *(.tdata .tdata.* .gnu.linkonce.td.*)
  } >RAM AT>DATA_LMA

  PROVIDE( _data_lma = LOADADDR(.data) );
  PROVIDE( _data = ADDR(.data) );
  PROVIDE( _edata = . );
  PROVIDE( edata = . );

  PROVIDE( _fbss = . );
  PROVIDE( __bss_start = . );

  .tbss (NOLOAD)   : ALIGN(8)
  {
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
    PROVIDE( __bss_start__ = . );
    *(.sbss*)
    *(.gnu.linkonce.sb.*)
    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
    PROVIDE( __bss_end__ = . );
  } >RAM AT>RAM

  PROVIDE( _end = . );
  PROVIDE( end = . );

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

    . += __HEAP_SIZE;
    . = ALIGN(16);

    PROVIDE( __heap_top = . );
    PROVIDE( __heap_limit = . );
  } >RAM AT>RAM

  .stack ORIGIN(RAM) + LENGTH(RAM) - __TOT_STACK_SIZE (NOLOAD) :
  {
    . = ALIGN(16);
    PROVIDE( _heap_end = . );
    PROVIDE( __heap_end = . );
    PROVIDE( __StackLimit = . );
    PROVIDE( __StackBottom = . );
    . += __TOT_STACK_SIZE;
    . = ALIGN(16);
    PROVIDE( __StackTop = . );
    PROVIDE( _sp = . );
  } >RAM AT>RAM

  .ext_ram_sec ORIGIN(ext_ram):
  {
    PROVIDE( __ext_ram_start = . );
     . = . + __EXT_RAM_SIZE;
     . = ALIGN(16);
     PROVIDE( __ext_ram_end = . );
  } > ext_ram

  /* Check if data + heap + stack exceeds RAM limit */
  ASSERT(__StackLimit >= __heap_limit, "region RAM overflowed with stack")
  /*ASSERT(__etext + __bss_start__ - __data_start__ <= __ROM_BASE + __ROM_SIZE, "image overflowed") */
}
