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
#endif
