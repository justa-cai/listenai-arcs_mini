ENTRY(Reset_Handler)

INCLUDE "ld/patch_sym.ld"

__RO_ADDR = 0x100000;
__RO_SIZE = 0x100000;
__RW_ADDR = __HeapLimit;
__RW_SIZE = __StackLimit - __HeapLimit;

MEMORY
{
  FLASH (rx)  : org = __RO_ADDR, len = __RO_SIZE
  RAM   (rwx) : org = __RW_ADDR, len = __RW_SIZE
}

SECTIONS
{
    .text (__RO_ADDR) :
    {
    SHORT(0x5450)          /* magic number */
    BYTE(0x31) BYTE(0x30)  /* version 1.0  */
    KEEP(*(.FUNC_TAB))
    LONG(0x0)
    . = ALIGN(4);
    *(.text*)
    KEEP(*(.init))
    KEEP(*(.fini))

    /* .ctors */
    *crtbegin.o(.ctors)
    *crtbegin?.o(.ctors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
    *(SORT(.ctors.*))
    *(.ctors)

    /* .dtors */
    *crtbegin.o(.dtors)
    *crtbegin?.o(.dtors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
    *(SORT(.dtors.*))
    *(.dtors)

    *(.rodata*)

    KEEP(*(.eh_frame*))
    } > FLASH
    
    _data_patch_load = ALIGN (4);
    
    .data (__RW_ADDR) : AT(_data_patch_load) ALIGN(4)
    {
    _data_patch_start = ABSOLUTE(.);
    *(vtable)
    *(.data)
    *(.data.*)

    . = ALIGN(4);
    /* preinit data */
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    . = ALIGN(4);
    /* init data */
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);


    . = ALIGN(4);
    /* finit data */
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);

    KEEP(*(.jcr*))
    . = ALIGN(4);
    _data_patch_end = ABSOLUTE(.);
    } > RAM
    
    .bss (NOLOAD) : ALIGN(4)
    {
    _bss_patch_start = ABSOLUTE(.);
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);
    _bss_patch_end = ABSOLUTE(.);
    } > RAM AT > RAM
    
    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(_bss_patch_end - _data_patch_start  <= __RW_SIZE, "region RAM overflowed with stack")
    ASSERT(_data_patch_end - _data_patch_start + _data_patch_load - __RO_ADDR <= __RO_SIZE, "image overflowed")
}
