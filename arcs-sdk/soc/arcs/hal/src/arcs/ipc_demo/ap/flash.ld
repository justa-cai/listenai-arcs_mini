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
  flash (rxa!w) : ORIGIN = 0x30000000, LENGTH = 1M
  ilm (rxa!w) : ORIGIN = 0x00080000, LENGTH = 16K
  dlm (wxa!r) : ORIGIN = 0x00100000, LENGTH = 8K
  NETRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 45K /*lwip pbuf*/
  ram (wxa!r) : ORIGIN = 0x2005A000, LENGTH = 384K - 8K - 32K
/*"Please do not relocate IPCSHRAM. Otherwise, you need to update the value of IPC_SHARED_HDR_ADDR accordingly and recompile the firmware."*/
  IPCSHRAM (rwx) : ORIGIN = 0x20050000, LENGTH = 8K /*ipc*/
  psram(wxa!r) : ORIGIN = 0x28000000, LENGTH = 128K
}

REGION_ALIAS("ROM", flash)
REGION_ALIAS("RAM", ram)
REGION_ALIAS("DATA_LMA", flash)
REGION_ALIAS("ILM", ilm)
REGION_ALIAS("DLM", dlm)
REGION_ALIAS("PSRAM", psram)

SECTIONS
{
  /* To provide symbol __STACK_SIZE, __HEAP_SIZE and __SMP_CPU_CNT */
  PROVIDE(__STACK_SIZE = 4K);
  /* heap size defined in FreeRtosConfig.h, if RTOS module was not used, define heap size in ld */
  /*PROVIDE(__HEAP_SIZE = 80K);*/
  PROVIDE(__SMP_CPU_CNT = 1);
  __TOT_STACK_SIZE = __STACK_SIZE * __SMP_CPU_CNT;
  PROVIDE(psram_start = ORIGIN(psram));
  PROVIDE(psram_end = ORIGIN(psram) + LENGTH(psram));

  /* ipc shared RAM */
  IPCSHARED (NOLOAD):
  {
      _ipcshram = . ;
      *(SHAREDRAM_AMP_IPC_ENV)
      *(SHAREDRAM_AMP_IPC)
      _eipcshram = . ;
  } > IPCSHRAM

  /* NET RAM */
  NETRAM(NOLOAD):
  {
      *(IPC_WIFI_SHARE)
      *(SHAREDRAM)
  } > NETRAM

  .init           :
  {
    *(.vtable)
    *(.vtable_s)
    KEEP (*(SORT_NONE(.init)))
    . = ALIGN(4);
  } >ROM AT>ROM

  .text           :
  {
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
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

  PROVIDE(_rom_code_start = ORIGIN(ROM));
  PROVIDE(_rom_code_end   = .);
  .ramcode        : ALIGN(8)
  {
    *(.rodata .rodata.*)
    *(.ramcode)
    . = ALIGN(8);
  } >ILM AT>DATA_LMA

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
  } >RAM AT>RAM

  PROVIDE( _end = . );
  PROVIDE( end = . );

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
#ifdef PSRAM_HEAP
} >PSRAM AT>PSRAM
#else
} >RAM AT>RAM
#endif


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
