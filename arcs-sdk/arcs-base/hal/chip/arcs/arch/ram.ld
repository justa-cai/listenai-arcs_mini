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
  ap_ilm (wxa!r)   : ORIGIN = 0x80000,    LENGTH = 16K
  ap_dlm (wxa!r)   : ORIGIN = 0x100000,   LENGTH = 8K
  cp_ilm (wxa!r)   : ORIGIN = 0x280000,   LENGTH = 16K
  cp_dlm (wxa!r)   : ORIGIN = 0x300000,   LENGTH = 8K
/*
  ram0 (rxa!w) : ORIGIN = 0x20040000, LENGTH = 64K
  ram1 (wxa!r) : ORIGIN = 0x20050000, LENGTH = 384K
*/
  ram0 (rxa!w) : ORIGIN = 0x20040000, LENGTH = 128K
  ram1 (wxa!r) : ORIGIN = 0x20060000, LENGTH = 320K
}

REGION_ALIAS("ROM", ram0)
REGION_ALIAS("RAM", ram1)
REGION_ALIAS("DATA_LMA", ram0)

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = 4K);
  PROVIDE(__HEAP_SIZE = 4K);
  PROVIDE(__SMP_CPU_CNT = 1);
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;

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

    LONG (__ap_ilm_text_lma)
    LONG (__ap_ilm_text_start__)
    LONG (__ap_ilm_text_end__)

    LONG (__cp_ilm_text_lma)
    LONG (__cp_ilm_text_start__)
    LONG (__cp_ilm_text_end__)

    /* data section*/

    LONG (__ap_dlm_data_lma)
    LONG (__ap_dlm_data_start__)
    LONG (__ap_dlm_data_end__)

    LONG (__cp_dlm_data_lma)
    LONG (__cp_dlm_data_start__)
    LONG (__cp_dlm_data_end__)

    /* add your own "fast" sections here... */

    __copy_table_end__ = .;
  } >ROM AT>ROM

  .zero_table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;

    LONG (__ap_dlm_bss_start__)
    LONG (__ap_dlm_bss_end__)

    LONG (__cp_dlm_bss_start__)
    LONG (__cp_dlm_bss_end__)

    __zero_table_end__ = .;
  } >ROM AT>ROM

  .ap_ilm_text           :
  {
    . = ALIGN(4);
    __ap_ilm_text_start__ = .;
    *(.text.ap_ilm .text.ap_ilm.* .text.entry .text.irq .text.irq.* .text.xPortTaskSwitch)
    . = ALIGN(4);
    __ap_ilm_text_end__ = .;
  } >ap_ilm AT>ROM

  __ap_ilm_text_lma = LOADADDR(.ap_ilm_text);

  .cp_ilm_text           :
  {
    . = ALIGN(4);
    __cp_ilm_text_start__ = .;
    /* NOTE: .ap_ilm_text SHOULD BE REMOVED if .cp_ilm_text is used, */
    /* or else .text.irq etc. sections will be placed into .ap_ilm_text!! */   
    *(.text.cp_ilm .text.cp_ilm.* .text.entry .text.irq .text.irq.* .text.xPortTaskSwitch)
    . = ALIGN(4);
    __cp_ilm_text_end__ = .;
  } >cp_ilm AT>ROM

  __cp_ilm_text_lma = LOADADDR(.cp_ilm_text);

  .ap_dlm_data           :
  {
    . = ALIGN(4);
    __ap_dlm_data_start__ = .;
    *(.data.ap_dlm .data.ap_dlm.*)
    . = ALIGN(4);
    __ap_dlm_data_end__ = .;
  } >ap_dlm AT>ROM

  __ap_dlm_data_lma = LOADADDR(.ap_dlm_data);

  .cp_dlm_data           :
  {
    . = ALIGN(4);
    __cp_dlm_data_start__ = .;
    *(.data.cp_dlm .data.cp_dlm.*)
    . = ALIGN(4);
    __cp_dlm_data_end__ = .;
  } >cp_dlm AT>ROM

  __cp_dlm_data_lma = LOADADDR(.cp_dlm_data);

  .text           :
  {
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
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

  .ap_dlm_bss (NOLOAD)           :
  {
    . = ALIGN(4);
    __ap_dlm_bss_start__ = .;
    *(.bss.ap_dlm .bss.ap_dlm.*)
    . = ALIGN(4);
    __ap_dlm_bss_end__ = .;
  } >ap_dlm AT>ap_dlm

  .cp_dlm_bss (NOLOAD)           :
  {
    . = ALIGN(4);
    __cp_dlm_bss_start__ = .;
    *(.bss.cp_dlm .bss.cp_dlm.*)
    . = ALIGN(4);
    __cp_dlm_bss_end__ = .;
  } >cp_dlm AT>cp_dlm

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
}
