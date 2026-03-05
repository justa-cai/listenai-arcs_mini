/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sys_init.c
 * @brief ARCS System Initialization Framework - Core Implementation
 */

#include "sys_init.h"
#include <stddef.h>
#include <string.h>

#define LEVEL_TABLE_SIZE   (SYS_INIT_LEVEL_MAX)  /* Total number of levels */

/* ========================================================================
 * Linker Symbol Declarations
 * ======================================================================== */

/* Declare section start/end symbols for each level */
extern sys_init_entry_t __sys_init_start[];
extern sys_init_entry_t __sys_init_end[];

/* PRE_SYSTEM_INIT: level=0, sub=00-99 */
extern sys_init_entry_t __sys_init_0_start[];
extern sys_init_entry_t __sys_init_0_end[];

/* PRE_DEVICES_INIT: level=1, sub=00-99 */
extern sys_init_entry_t __sys_init_1_start[];
extern sys_init_entry_t __sys_init_1_end[];

/* PRE_KERNEL: level=2, sub=00-99 */
extern sys_init_entry_t __sys_init_2_start[];
extern sys_init_entry_t __sys_init_2_end[];

/* POST_KERNEL: level=3, sub=00-99 */
extern sys_init_entry_t __sys_init_3_start[];
extern sys_init_entry_t __sys_init_3_end[];

/* PRE_APPLICATION: level=4, sub=00-99 */
extern sys_init_entry_t __sys_init_4_start[];
extern sys_init_entry_t __sys_init_4_end[];

/* ========================================================================
 * Internal Data Structures
 * ======================================================================== */

/**
 * @brief Level information structure
 */
typedef struct {
    sys_init_entry_t *start;    /* Section start address */
    sys_init_entry_t *end;      /* Section end address */
    const char *name;           /* Level name */
} level_info_t;

/**
 * @brief Get level information - inline function to avoid global array
 *
 * IMPORTANT: This function is designed to work BEFORE scatload!
 * - Uses only stack variables (no .data access)
 * - No global array (avoids .data.rel.ro section)
 * - Safe to call from PRE_SYSTEM_INIT context
 */
static inline void get_level_info(uint8_t level, level_info_t *info)
{
    /* Switch statement compiles to constants - no .data access */
    switch (level) {
        case SYS_INIT_LEVEL_PRE_SYSTEM_INIT:
            info->start = __sys_init_0_start;
            info->end = __sys_init_0_end;
            info->name = "PRE_SYSTEM_INIT";
            break;
        case SYS_INIT_LEVEL_PRE_DEVICES_INIT:
            info->start = __sys_init_1_start;
            info->end = __sys_init_1_end;
            info->name = "PRE_DEVICES_INIT";
            break;
        case SYS_INIT_LEVEL_PRE_KERNEL:
            info->start = __sys_init_2_start;
            info->end = __sys_init_2_end;
            info->name = "PRE_KERNEL";
            break;
        case SYS_INIT_LEVEL_POST_KERNEL:
            info->start = __sys_init_3_start;
            info->end = __sys_init_3_end;
            info->name = "POST_KERNEL";
            break;
        case SYS_INIT_LEVEL_PRE_APPLICATION:
            info->start = __sys_init_4_start;
            info->end = __sys_init_4_end;
            info->name = "PRE_APPLICATION";
            break;
        default:
            info->start = NULL;
            info->end = NULL;
            info->name = NULL;
            break;
    }
}

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * @brief Get number of init entries at a level
 */
static inline size_t get_level_count(uint8_t level)
{
    if (level >= LEVEL_TABLE_SIZE) {
        return 0;
    }

    level_info_t info;
    get_level_info(level, &info);
    return (size_t)(info.end - info.start);
}


/* ========================================================================
 * Public API Implementation
 * ======================================================================== */

int sys_init_run_level(uint8_t level)
{
    if (level >= LEVEL_TABLE_SIZE) {
        return -1;
    }

    level_info_t info;
    get_level_info(level, &info);
    sys_init_entry_t *entry_start = info.start;
    sys_init_entry_t *entry_end = info.end;

    /* Calculate number of entries */
    size_t count = entry_end - entry_start;

    if (count == 0) {
        return 0;
    }

    int success_count = 0;
    int failed_count = 0;

    /* Iterate and execute all init functions */
    for (size_t i = 0; i < count; i++) {
        sys_init_entry_t *entry = &entry_start[i];

        if (entry->init_fn == NULL) {
            continue;
        }

        /* Execute initialization function */
        int ret = entry->init_fn();

        if (ret == 0) {

            success_count++;
        } else {

            failed_count++;
        }
    }

    return success_count;
}


int sys_init_get_count(uint8_t level)
{
    if (level == 0xFF) {
        /* Count all levels */
        int total = 0;
        for (uint8_t i = 0; i < LEVEL_TABLE_SIZE; i++) {
            total += (int)get_level_count(i);
        }
        return total;
    }

    if (level >= LEVEL_TABLE_SIZE) {
        return -1;
    }

    return (int)get_level_count(level);
}

/* ========================================================================
 * Debug Interface (Optional)
 * ======================================================================== */

#ifdef CONFIG_SYS_INIT_DEBUG

/**
 * @brief Dump all registered initialization functions
 */
void sys_init_dump_all(void)
{
    printf("========================================");
    printf("  Registered System Init Functions");
    printf("========================================");

    for (uint8_t level = 0; level < LEVEL_TABLE_SIZE; level++) {
        level_info_t info;
        get_level_info(level, &info);
        size_t count = get_level_count(level);

        printf("");
        printf("[%s] - %d functions", info.name, (int)count);

        if (count == 0) {
            continue;
        }

        sys_init_entry_t *entry = info.start;
        for (size_t i = 0; i < count; i++, entry++) {
            printf("  [%d] %s",
                          entry->sub_priority,
                          entry->name ? entry->name : "unknown");
        }
    }

    printf("");
    printf("========================================");
}

#endif /* CONFIG_SYS_INIT_DEBUG */
