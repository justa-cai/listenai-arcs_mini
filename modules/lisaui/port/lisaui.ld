SECTIONS {

    .lisaui_section : {
        . = ALIGN(4);
        __lisaui_group_level1_start = .;
        KEEP (*(.lisaui.group.level1.*))
        __lisaui_group_level1_end = .;
        . = ALIGN(4);
        __lisaui_group_level2_start = .;
        KEEP (*(.lisaui.group.level2.*))
        __lisaui_group_level2_end = .;
        . = ALIGN(4);
        __lisaui_pages_start = .;
        KEEP (*(.lisaui.pages.*))
        __lisaui_pages_end = .;
    }
}

INSERT AFTER .data;