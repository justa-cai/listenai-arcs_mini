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
/* Memory layout: SoC core regions + component-registered regions */
MEMORY {
    /* SoC core regions (Kconfig-driven) */
#if CONFIG_MEM_PSRAM_SIZE > 0
    PSRAM(rwxa)  : ORIGIN = CONFIG_MEM_PSRAM_BASE, LENGTH = CONFIG_MEM_PSRAM_SIZE
#endif
#if CONFIG_BOOT
    FLASH(rxa!w) : ORIGIN = (CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE), LENGTH = (CONFIG_MEM_FLASH_SIZE - CONFIG_BOOT_FLASH_SIZE)
#else
    FLASH(rxa!w) : ORIGIN = CONFIG_MEM_FLASH_BASE, LENGTH = CONFIG_MEM_FLASH_SIZE
#endif
    SRAM(rwxa)   : ORIGIN = CONFIG_MEM_SRAM_BASE, LENGTH = CONFIG_MEM_SRAM_SIZE
#if CONFIG_MEM_ILM_SIZE > 0
    ILM(rwx)     : ORIGIN = CONFIG_MEM_ILM_BASE, LENGTH = CONFIG_MEM_ILM_SIZE
    DLM(rw)      : ORIGIN = CONFIG_MEM_DLM_BASE, LENGTH = CONFIG_MEM_DLM_SIZE
#endif
    /* Component-registered MEMORY regions */
    #include "generated/snippets-memory.ld"
}

/* SoC core aliases */
#if CONFIG_MEM_ILM_SIZE > 0
REGION_ALIAS("ITCM", ILM)
REGION_ALIAS("DTCM", DLM)
#endif
REGION_ALIAS("ROM", FLASH)
REGION_ALIAS("RAM", SRAM)

/* Component-registered REGION_ALIAS */
#include "generated/snippets-aliases.ld"

#define SCATCOPY(LEN,VMA,LMA) LONG(LEN) LONG(VMA) LONG(LMA)
#define SCATLOAD(SCT) LONG(SIZEOF(SCT)) LONG(ADDR(SCT)) LONG(LOADADDR(SCT))
#define SCATZERO(SCT) LONG(SIZEOF(SCT)) LONG(ADDR(SCT))

SECTIONS {
    PROVIDE(__STACK_SIZE = CONFIG_MEM_INTERRUPT_STACK_SIZE);
    PROVIDE(__HEAP_SIZE = CONFIG_MEM_HEAP_SIZE);
#if CONFIG_MEM_PSRAM_SIZE > 0
    PROVIDE(psram_start = ORIGIN(PSRAM));
    PROVIDE(psram_end = ORIGIN(PSRAM) + LENGTH(PSRAM));
#endif

    /* === SLOT: SECTIONS_START — 需要最低地址的组件段 === */
    #include "generated/snippets-sections_start.ld"

    .init : {
        /* Default Vector Table */
        . = ALIGN(4);
        PROVIDE(_rom_code_start = .);
        * (.vtable)

        /* Starup Initialition Region */
        . = ALIGN(4);
        KEEP(startup.* (SORT_NONE(.init)))
        KEEP(scatload.* (SORT(.init)))
        PROVIDE(_rom_code_end = .);
    } >ROM AT>ROM

    .scatab : ALIGN(4) {
        /* Prev Scatter Copy/Load Table */
        . = ALIGN(4);
        PROVIDE(__scat_copy_base = .);
#if CONFIG_MEM_ILM_SIZE > 0
            SCATLOAD(.itcm);
            SCATLOAD(.dtcm);
#endif
            SCATLOAD(.fast.text);
            SCATLOAD(.fast.rodata);
            SCATLOAD(.fast.data);
            SCATLOAD(.text);
            SCATLOAD(.rodata);
            SCATLOAD(.data);
            #include "generated/snippets-scatload.ld"
        PROVIDE(__scat_copy_last = .);

        /* Scatter Zero-Fill Table */
        . = ALIGN(4);
        PROVIDE(__scat_zero_base = .);
#if CONFIG_MEM_ILM_SIZE > 0
            SCATZERO(.dtcm.bss);
#endif
            SCATZERO(.fast.bss);
            SCATZERO(.bss);
            #include "generated/snippets-scatzero.ld"
        PROVIDE(__scat_zero_last = .);
        . = ALIGN(4);
    } >ROM AT>ROM

#if CONFIG_MEM_PSRAM_SIZE > 0
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
        #include "generated/snippets-relocate-psram-data.ld"
        . = ALIGN(4);
        PROVIDE(__psram_data_end = .);
    } >PSRAM AT>ROM

    .psram.bss(NOLOAD): ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_bss_start = .);
        * (.psram.bss)
        * (.psram.bss.*)
        *psram*.a: (.sbss .sbss.* .bss .bss.*)
        #include "generated/snippets-relocate-psram-bss.ld"
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
#endif

    /* === SLOT: COMPONENTS — 组件专属段组 === */
    #include "generated/snippets-components.ld"

    ////////////////////////////////////////////////////////////////////////////
    // ROM/RAM => FAST/RAM
    .fast.text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_ram_code_start = .);
        * (.ramcode)
        * (.fast_text .fast_text.* .fast.text .fast.text.*)
        #include "generated/snippets-relocate-sram-text.ld"
        PROVIDE(_ram_code_end = .);
        . = ALIGN(4);
    } >SRAM AT>ROM
#if CONFIG_MEM_ILM_SIZE > 0
    .itcm : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_itcm_code_start = .);
        * (.itcm_text .itcm_text.* .itcm*)
        #include "generated/snippets-relocate-itcm.ld"
        PROVIDE(_itcm_code_end = .);
        . = ALIGN(4);
    } >ITCM AT>ROM

    .dtcm : ALIGN(4) {
        . = ALIGN(4);
        * (.dtcm .dtcm.*)
        #include "generated/snippets-relocate-dtcm-data.ld"
        . = ALIGN(4);
    } >DTCM AT>ROM

    .dtcm.bss (NOLOAD) : ALIGN(4) {
        . = ALIGN(4);
        * (.dtcm.bss .dtcm.bss*)
        #include "generated/snippets-relocate-dtcm-bss.ld"
        . = ALIGN(4);
    } >DTCM
#endif

    .fast.rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_rodata .fast_rodata.* .fast.rodata .fast.rodata.*)
        #include "generated/snippets-relocate-sram-rodata.ld"
        . = ALIGN(4);
    } >RAM AT>ROM

    .fast.data : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_data .fast_data.* .fast.data .fast.data.*)
        #include "generated/snippets-relocate-sram-data.ld"
        . = ALIGN(4);
    } >RAM AT>ROM

    .fast.bss(NOLOAD) : ALIGN(4) {
        . = ALIGN(4);
        * (.fast_bss .fast_bss.* .fast.bss .fast.bss.*)
        #include "generated/snippets-relocate-sram-bss.ld"
        . = ALIGN(4);
    } >RAM AT>RAM

    /* === SLOT: ROM — 组件 ROM 段 === */
    #include "generated/snippets-rom.ld"

#if CONFIG_MEM_PSRAM_SIZE > 0
    .psram.text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__psram_text_start = .);
        * (.psram.text)
        * (.psram.text.*)
        #include "generated/snippets-relocate-psram-text.ld"
        . = ALIGN(4);
        PROVIDE(__psram_text_end = .);
    } >PSRAM AT>ROM
#endif

    .text : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_rom_code2_start = .);
        * (.stext .stext.*)
        * (.text .text.*)
        * (.gnu.linkonce.t.*)
        . = ALIGN(4);
        PROVIDE(_rom_code2_end = .);
    } >ROM AT>ROM

    /* === SLOT: POST_TEXT — .text 之后的组件段（C++ runtime 等） === */
    #include "generated/snippets-post_text.ld"

#if CONFIG_MEM_PSRAM_SIZE > 0
    .psram.rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.psram.rodata)
        * (.psram.rodata.*)
        #include "generated/snippets-relocate-psram-rodata.ld"
        . = ALIGN(4);
    } >PSRAM AT>ROM
#endif

    .rodata : ALIGN(4) {
        . = ALIGN(4);
        * (.rodata .rodata.*)
        * (.strdat)
        . = ALIGN(4);
    } >ROM AT>ROM

    /* === SLOT: RAM — 组件 RAM 加载段 === */
    #include "generated/snippets-ram.ld"

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

    /* === SLOT: POST_DATA — .data 之后的组件段 === */
    #include "generated/snippets-post_data.ld"

    PROVIDE(__tls_base = .);

#if CONFIG_MEM_PSRAM_SIZE > 0
    .psram.nocache_heap ORIGIN(PSRAM) (NOLOAD):{
        . = ALIGN(0x100000);
        PROVIDE(__psram_nocache_heap_start = .);
        * (.psram.nocache_heap)
        . = ALIGN(4);
        PROVIDE(__psram_nocache_heap_end = .);

    } >PSRAM
#endif

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

    /* === SLOT: SECTIONS_END — 末尾组件段 === */
    #include "generated/snippets-sections_end.ld"
}
