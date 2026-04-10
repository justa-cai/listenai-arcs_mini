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
 * @brief    GNU Linker Script for Nuclei N/NX based device
 * @version  V1.0.0
 * @date     17. Dec 2019
 ******************************************************************************/
OUTPUT_ARCH("riscv")
ENTRY(_start)

////////////////////////////////////////////////////////////////////////////////
/*
 *  R  - Read-only sections.
 *  W  - Read/write sections.
 *  X  - Sections containing executable code.
 *  A  - Allocated sections.
 * I/L - Initialized sections.
 *  !  - Invert the sense of any of the following attributes.
 */
#include "memap.h"
MEMORY {
    PSRAM(rwxa ) : ORIGIN = MEM_PSRAM_BASE, LENGTH = MEM_PSRAM_SIZE
    FLASH(rxa!w) : ORIGIN = MEM_FLASH_BASE, LENGTH = MEM_FLASH_SIZE
    SRAM(rwxa ) : ORIGIN = MEM_SRAM_BASE, LENGTH = MEM_SRAM_SIZE
    ILM(rwx ) : ORIGIN = MEM_ILM_BASE, LENGTH = MEM_ILM_SIZE
    DLM(rw ) : ORIGIN = MEM_DLM_BASE, LENGTH = MEM_DLM_SIZE

#if defined(MEM_IPC_BASE)
    /* 共享内存 */
    IPC_RAM(rwx) : ORIGIN = MEM_IPC_BASE, LENGTH = MEM_IPC_SIZE
#endif

#if defined(MEM_WFRAM_BASE)
    /* 这段内存预留给WIFI使用 */
    WIFI_RAM (rwx) : ORIGIN = MEM_WFRAM_BASE, LENGTH = MEM_WFRAM_SIZE
#endif
}

REGION_ALIAS("ITCM", ILM)
REGION_ALIAS("DTCM", DLM)

REGION_ALIAS("ROM", FLASH)
REGION_ALIAS("RAM", SRAM)

/* 如果不定义 MEM_WFRAM_BASE 的话, wifi ram从sram中分配 */
#if !defined(MEM_WFRAM_BASE)
REGION_ALIAS("WIFI_RAM", SRAM)
#endif

/* 如果不定义 MEM_IPC_BASE 的话, ipc shared ram从sram中分配 */
#if !defined(MEM_IPC_BASE)
REGION_ALIAS("IPC_RAM", SRAM)
#endif

#define SCATCOPY(LEN,VMA,LMA) LONG(LEN) LONG(VMA) LONG(LMA)
#define SCATLOAD(SCT) LONG(SIZEOF(SCT)) LONG(ADDR(SCT)) LONG(LOADADDR(SCT))
#define SCATZERO(SCT) LONG(SIZEOF(SCT)) LONG(ADDR(SCT))

SECTIONS {
    PROVIDE(__STACK_SIZE = MEM_INTERRUPT_STACK_SIZE);
    PROVIDE(__HEAP_SIZE =  512);

    /* wifi 校准数据预留大小 */
    PROVIDE(_lcali = MEM_WIFI_CALIBRATION_SIZE);
    /* wifi 跟踪数据预留大小 */
    PROVIDE(_trace_len = MEM_WIFI_TRACE_SIZE);

    /* 优先将这个段放在前面, 确保wifi ram自动从sram分配时, 前面的地址给wifi使用 */
    .wifi.noinit (NOLOAD):
    {
        . = ALIGN(4);
        _wifi_ram_start = . ;
#if CONFIG_WIFI_CALI_BUF
        _scali = .;
        . += _lcali;
        _ecali = .;
#endif
        _trace_start = .;
        . += _trace_len;
        _trace_end = .;
        _sshram = . ;
        *(SHAREDRAMIPC)
        *(SHAREDRAM)
        _eshram = . ;
        . = ALIGN(4);

        _wifi_ram_end = . ;
    } > WIFI_RAM

/* WIFI外设只能访问 0x20000000 - 0x20000000 + 256 * 1024这段内存  */
ASSERT  ((_wifi_ram_start >= (0x20000000)), "error: wifi ram start must be greater than 0x20000000")
ASSERT  ((_wifi_ram_end <= (0x20000000 + 256 * 1024)), "error: wifi ram end must be less than 0x20000000 + 256 * 1024")

    .init : {
        /* Default Vector Table */
        . = ALIGN(4);
        PROVIDE(_rom_code_start = .);
        * (.vtable)
        * (.init)

        /* Starup Initialition Region */
        . = ALIGN(4);
        PROVIDE(_rom_code_end = .);
    } >ROM AT>ROM

    .scatab : ALIGN(4) {
        /* Prev Scatter Copy/Load Table */
        . = ALIGN(4);
        PROVIDE(__scat_copy_base = .);
            SCATLOAD(.itcm);
            SCATLOAD(.dtcm);
            SCATLOAD(.fast.text);
            SCATLOAD(.fast.rodata);
            SCATLOAD(.fast.data);
            SCATLOAD(.text);
            SCATLOAD(.rodata);
            SCATLOAD(.data);
            SCATLOAD(.shell);
            SCATLOAD(.mapi);
        PROVIDE(__scat_copy_last = .);

        /* Scatter Zero-Fill Table */
        . = ALIGN(4);
        PROVIDE(__scat_zero_base = .);
            SCATZERO(.dtcm.bss);
            SCATZERO(.fast.bss);
            SCATZERO(.bss);
        PROVIDE(__scat_zero_last = .);
        . = ALIGN(4);
    } >ROM AT>ROM

    .psram_scatab : ALIGN(4) {
        /* Prev Scatter Copy/Load Table */
        . = ALIGN(4);
        PROVIDE(__psram_scat_copy_base = .);
            SCATLOAD(.psram.text);
            SCATLOAD(.psram.data);
            SCATLOAD(.psram.rodata);
        PROVIDE(__psram_scat_copy_last = .);

        /* Scatter Zero-Fill Table */
        . = ALIGN(4);
        PROVIDE(__psram_scat_zero_base = .);
            SCATZERO(.psram.bss);
        PROVIDE(__psram_scat_zero_last = .);
        . = ALIGN(4);
    } >ROM AT>ROM

    .psram.data : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_data_start = .);
        * (.psram.data)
        * (.psram.data.*)
        *psram*.a: (.data .data.*)
        . = ALIGN(4);
        PROVIDE(__psram_data_end = .);
    } >PSRAM AT>ROM

    .psram.bss(NOLOAD): ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_bss_start = .);
        * (.psram.bss)
        * (.psram.bss.*)
        *psram*.a: (.sbss .sbss.* .bss .bss.*)
        . = ALIGN(4);
        PROVIDE(__psram_bss_end = .);
    } >PSRAM

    .psram.noinit(NOLOAD) : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_noinit_start = .);
        * (.psram.noinit)
        * (.psram.noinit.*)
        *psram*.a: (.noinit .noinit.*)
        . = ALIGN(4);
        PROVIDE(__psram_noinit_end = .);
    } >PSRAM

    ////////////////////////////////////////////////////////////////////////////
    // ROM/RAM => FAST/RAM
    .fast.text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_ram_code_start = .);
        * (.ramcode)
        * (.text.irq .text.xPortTaskSwitch .text.eclic_mtip_handler)
        * (.fast_text .fast_text.*)
        /* *heap.a: (.text .text.*) */
        *mapi.o (.text .text.*)
        PROVIDE(_ram_code_end = .);
        . = ALIGN(4);
    } >SRAM AT>ROM
    .itcm : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_itcm_code_start = .);
        * (.itcm_text .itcm_text.* .itcm*)
        PROVIDE(_itcm_code_end = .);
        . = ALIGN(4);
    } >ITCM AT>ROM

    .dtcm : ALIGN(4) {
        . = ALIGN(4);
        * (.dtcm .dtcm.*)
        . = ALIGN(4);
    } >DTCM AT>ROM

    .dtcm.bss (NOLOAD) : ALIGN(4) {
        . = ALIGN(4);
        * (.dtcm.bss .dtcm.bss*)
        . = ALIGN(4);
    } >DTCM

    .fast.rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_rodata .fast_rodata.*)
        *heap.a: (.rodata .rodata.*)
        . = ALIGN(4);
    } >RAM AT>ROM

    .fast.data : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_data .fast_data.*)
        *heap.a: (.data .data.*)
        . = ALIGN(4);
    } >RAM AT>ROM

    .fast.bss(NOLOAD) : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_bss .fast_bss.*)
        *heap.a: (.bss .bss.*)
        . = ALIGN(4); 
    } >RAM AT>RAM

    ////////////////////////////////////////////////////////////////////////////
    // ROM => ROM
    .shell : ALIGN(4) {
        . = ALIGN(4);
        _shell_items_start = .;
        KEEP(* (.shell.user))
        KEEP(* (.shell.priv))
        . = ALIGN(4);
    } >ROM AT>ROM

    .shell_command  :
    {
        _shell_command_start = .;
        KEEP (*(shellCommand))
        _shell_command_end = .;
    } >ROM AT>ROM

    .psram.text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_text_start = .);
        * (.psram.text)
        * (.psram.text.*)
        *libmodules_lvgl.a:*(.text .text.* .stext .stext.*)
        . = ALIGN(4);
        PROVIDE(__psram_text_end = .);
    } >PSRAM AT>ROM

    .text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_rom_code2_start = .);
        * (.stext .stext.*)
        * (.text .text.*)
        * (.gnu.linkonce.t.*)
        . = ALIGN(4);
        PROVIDE(_rom_code2_end = .);
    } >ROM AT>ROM

#if CONFIG_LINK_CPP_RUNTIME_SECTIONS
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

    /* C++ 异常处理支持 */
    .eh_frame_hdr :
    {
        *(.eh_frame_hdr)
        *(.eh_frame_entry .eh_frame_entry.*)
    } >ROM AT>ROM

    .eh_frame :
    {
        . = ALIGN(4);
        PROVIDE(__eh_frame = .);
        KEEP(*(.eh_frame))
        /**
        * As we are not linking with crtend.o, which includes the CIE terminator
        * (see __FRAME_END__ in libgcc sources), it is manually provided here.
        */
        LONG(0)
    } >ROM AT>ROM

    .gcc_except_table :
    {
        *(.gcc_except_table .gcc_except_table.*)
    } >ROM AT>ROM

    /* C++ 标准库静态数据 */
    .rodata.cst4 :
    {
        *(.rodata.cst4)
    } >ROM AT>ROM

    .rodata.cst8 :
    {
        *(.rodata.cst8)
    } >ROM AT>ROM
#endif // CONFIG_LINK_OPTION_NONE_SPECS

    .psram.rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.psram.rodata)
        * (.psram.rodata.*)
        . = ALIGN(4);
    } >PSRAM AT>ROM

    .rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.rodata .rodata.*)
        * (.strdat)
        . = ALIGN(4);
    } >ROM AT>ROM

    .lisaui_apps : {
        . = ALIGN(4);
        __lisaui_apps_start = .;
        KEEP (*(.lisaui*))
        . = ALIGN(4);
        __lisaui_apps_end = .;
    } >ROM AT>ROM

    ////////////////////////////////////////////////////////////////////////////
    // ROM => RAM
    .mapi : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__mod_stdc = .);
        KEEP(* (SORT(.mod.0.*)))
        PROVIDE_HIDDEN(__mod_rtos = .);
        KEEP(* (SORT(.mod.?.*)))
        PROVIDE_HIDDEN(__mod_ends = .);
        . = ALIGN(4);
    } >RAM AT>ROM

    .trace : ALIGN(4) {
        . = ALIGN(4);
        _trace_start = .;
        KEEP(*(TRACE))
        _trace_end = .;
        . = ALIGN(4);
    } >RAM AT>ROM

    .data : ALIGN(8) {
        . = ALIGN(8);
        * (.data .data.*)
        * (.gnu.linkonce.d.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        * (.sdata .sdata.*)
        * (.gnu.linkonce.s.*)
        . = ALIGN(8);
        * (.rdata .rdata.*)
        * (.gnu.linkonce.r.*)
        * (.srodata .srodata.*)
    } >RAM AT>ROM

#if defined(MEM_IPC_BASE)
    /* ipc shared RAM */
    .ipc.shared (NOLOAD):
    {
        _ipcshram = . ;
        *(SHAREDRAM_AMP_IPC_ENV)
        *(SHAREDRAM_AMP_IPC)
        *(SORT(.ipc.*))
        _eipcshram = . ;
    } > IPC_RAM
#endif

    PROVIDE(__tls_base = .);

    .psram.nocache_heap ORIGIN(PSRAM) (NOLOAD):{
        . = ALIGN(0x100000);
        PROVIDE(__psram_nocache_heap_start = .);
        * (.psram.nocache_heap)
        . = ALIGN(4);
        PROVIDE(__psram_nocache_heap_end = .);
    } >PSRAM



    ////////////////////////////////////////////////////////////////////////////
    // ZERO => RAM
    .bss(NOLOAD) : ALIGN(8) {
        . = ALIGN(8);
        * (.sbss .sbss.*)
        * (.gnu.linkonce.sb.*)
        * (.bss .bss.*)
        * (.gnu.linkonce.b.*)
        * (COMMON)
        . = ALIGN(4);
    } >RAM AT>RAM

    .noinit (NOLOAD): ALIGN(4) {
        . = ALIGN(4);
        * (.noinit)
        * (.noinit.*)
        . = ALIGN(4);
    } >RAM

    ////////////////////////////////////////////////////////////////////////////
    // ROM => ROM
    .stub : {
        KEEP (* (.stub))
    } >ROM AT>ROM
    PROVIDE( _end = . );
    PROVIDE( end = . );
    ////////////////////////////////////////////////////////////////////////////
	///* RTOS Heap Region (8 bytes aligned at least) */
    //.rtosheap(NOLOAD) : ALIGN(16) {
    //    . = ALIGN(ORIGIN(RAM) + LENGTH(RAM) - __HEAP_SIZE - __STACK_SIZE, 16);
    //} >RAM AT>RAM
    //PROVIDE(__vmaof_osheap = ADDR(.rtosheap));
    //PROVIDE(__lenof_osheap = SIZEOF(.rtosheap));

    /* Nuclei C Runtime Library requirements:
     * 1. heap need to be align at 16 bytes
     * 2. __heap_start and __heap_end symbol need to be defined
     * 3. reserved at least __HEAP_SIZE space for heap
     * 4. reserved at least __STACK_SIZE space for stack */
    .heapstack(NOLOAD) : ALIGN(16) {
        . = ALIGN(16);
        PROVIDE(__heap_start = .);
        . += __HEAP_SIZE;
        . = ALIGN(16);
        PROVIDE(__heap_end = .);
        PROVIDE(_sstack = .);
        PROVIDE(__StackLimit = .);
        . += __STACK_SIZE;
        . = ALIGN(16);
        PROVIDE(__StackTop = .);
        PROVIDE(_estack = .);
    } >RAM AT>RAM
}
