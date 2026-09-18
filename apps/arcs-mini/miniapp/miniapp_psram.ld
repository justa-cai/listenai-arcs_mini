/* The default linker puts small constants (.srodata) in SRAM alongside data.
 * Lua's math library pulls in libm constants, so collect those here as well.
 * Register both sections in the startup scatter tables in CMakeLists.txt.
 */
.miniapp.psram.data : ALIGN(8) {
    *libapp-miniapp.a:*(.data .data.* .sdata .sdata.* .rdata .rdata.* .srodata .srodata.*)
    *libminiapp_lua.a:*(.data .data.* .sdata .sdata.* .rdata .rdata.* .srodata .srodata.*)
    *libm.a:*(.data .data.* .sdata .sdata.* .rdata .rdata.* .srodata .srodata.*)
    . = ALIGN(8);
} >PSRAM AT>ROM

.miniapp.psram.bss (NOLOAD) : ALIGN(8) {
    *libapp-miniapp.a:*(.bss .bss.* .sbss .sbss.* COMMON)
    *libminiapp_lua.a:*(.bss .bss.* .sbss .sbss.* COMMON)
    *libm.a:*(.bss .bss.* .sbss .sbss.* COMMON)
    . = ALIGN(8);
} >PSRAM
