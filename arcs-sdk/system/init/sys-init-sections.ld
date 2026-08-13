    /* System Init section: collect SYS_INIT entries into a level-sorted table.
     * Use two-step sorting so sub-priority 0-99 stays in numeric order. */
    .sys_init : {
        . = ALIGN(4);
        __sys_init_start = .;

        /* PRE_SYSTEM_INIT: level=0, sub_priority=0-99 */
        __sys_init_0_start = .;
        KEEP (*(SORT(.sys_init.0[0-9])))
        KEEP (*(SORT(.sys_init.0[1-9][0-9])))
        __sys_init_0_end = .;

        /* PRE_DEVICES_INIT: level=1, sub_priority=0-99 */
        __sys_init_1_start = .;
        KEEP (*(SORT(.sys_init.1[0-9])))
        KEEP (*(SORT(.sys_init.1[1-9][0-9])))
        __sys_init_1_end = .;

        /* PRE_KERNEL: level=2, sub_priority=0-99 */
        __sys_init_2_start = .;
        KEEP (*(SORT(.sys_init.2[0-9])))
        KEEP (*(SORT(.sys_init.2[1-9][0-9])))
        __sys_init_2_end = .;

        /* POST_KERNEL: level=3, sub_priority=0-99 */
        __sys_init_3_start = .;
        KEEP (*(SORT(.sys_init.3[0-9])))
        KEEP (*(SORT(.sys_init.3[1-9][0-9])))
        __sys_init_3_end = .;

        /* PRE_APPLICATION: level=4, sub_priority=0-99 */
        __sys_init_4_start = .;
        KEEP (*(SORT(.sys_init.4[0-9])))
        KEEP (*(SORT(.sys_init.4[1-9][0-9])))
        __sys_init_4_end = .;

        . = ALIGN(4);
        __sys_init_end = .;
    } >ROM AT>ROM
