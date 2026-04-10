    /* System Init 系统初始化段
     * Use two-step sorting to handle sub_priority 0-99 correctly:
     * Step 1: Process 0-9 (single digit)
     * Step 2: Process 10-99 (double digit)
     * This ensures correct numeric order despite lexicographic sorting */
    .sys_init : {
        . = ALIGN(4);
        __sys_init_start = .;

        /* PRE_SYSTEM_INIT: level=0, sub_priority=0-99
         * BEFORE scatload - NO GLOBAL VARIABLES! */
        __sys_init_0_start = .;
        KEEP (*(SORT(.sys_init.0[0-9])))      /* sub_priority 0-9 */
        KEEP (*(SORT(.sys_init.0[1-9][0-9]))) /* sub_priority 10-99 */
        __sys_init_0_end = .;

        /* PRE_DEVICES_INIT: level=1, sub_priority=0-99
         * Before lisa_device_init() */
        __sys_init_1_start = .;
        KEEP (*(SORT(.sys_init.1[0-9])))      /* sub_priority 0-9 */
        KEEP (*(SORT(.sys_init.1[1-9][0-9]))) /* sub_priority 10-99 */
        __sys_init_1_end = .;

        /* PRE_KERNEL: level=2, sub_priority=0-99
         * Before vTaskStartScheduler() */
        __sys_init_2_start = .;
        KEEP (*(SORT(.sys_init.2[0-9])))      /* sub_priority 0-9 */
        KEEP (*(SORT(.sys_init.2[1-9][0-9]))) /* sub_priority 10-99 */
        __sys_init_2_end = .;

        /* POST_KERNEL: level=3, sub_priority=0-99
         * After vTaskStartScheduler() */
        __sys_init_3_start = .;
        KEEP (*(SORT(.sys_init.3[0-9])))      /* sub_priority 0-9 */
        KEEP (*(SORT(.sys_init.3[1-9][0-9]))) /* sub_priority 10-99 */
        __sys_init_3_end = .;

        /* PRE_APPLICATION: level=4, sub_priority=0-99
         * Before main() */
        __sys_init_4_start = .;
        KEEP (*(SORT(.sys_init.4[0-9])))      /* sub_priority 0-9 */
        KEEP (*(SORT(.sys_init.4[1-9][0-9]))) /* sub_priority 10-99 */
        __sys_init_4_end = .;

        . = ALIGN(4);
        __sys_init_end = .;
    } >ROM AT>ROM
