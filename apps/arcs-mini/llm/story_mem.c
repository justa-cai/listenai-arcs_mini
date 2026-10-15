// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "story_mem.h"
#include "story_log.h"
#include <string.h>

static const char *TAG = "mem";

#ifdef STORY_HOST
#include <time.h>
void story_mem_snapshot(story_mem_snapshot_t *s) { memset(s, 0, sizeof(*s)); }
int64_t story_time_us(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#else
#include "esp_heap_caps.h"

/* ARCS CP 移植：无 esp_timer，用 Nuclei mcycle 计数换算微秒。
 * SystemCoreClock 在 soc.c 启动阶段已由 SystemCoreClockUpdate() 填好。 */
extern volatile uint32_t SystemCoreClock;

static inline uint64_t cpuid_cycle(void)
{
    uint32_t hi, hi2, lo;
    do {
        __asm volatile("rdcycleh %0" : "=r"(hi));
        __asm volatile("rdcycle  %0" : "=r"(lo));
        __asm volatile("rdcycleh %0" : "=r"(hi2));
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

void story_mem_snapshot(story_mem_snapshot_t *s)
{
    const uint32_t INT = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    s->int_free = heap_caps_get_free_size(INT);
    s->int_largest = heap_caps_get_largest_free_block(INT);
    s->int_min_ever = heap_caps_get_minimum_free_size(INT);
    s->dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s->psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    s->psram_min_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}
int64_t story_time_us(void)
{
    return (int64_t)(cpuid_cycle() / (SystemCoreClock / 1000000u));
}
#endif

void story_mem_log(const char *label)
{
    story_mem_snapshot_t s;
    story_mem_snapshot(&s);
    SLOGI(TAG, "MEM %-14s int free=%6u lrg=%6u min=%6u dma=%6u | psram free=%7u lrg=%7u min=%7u",
          label, (unsigned)s.int_free, (unsigned)s.int_largest, (unsigned)s.int_min_ever,
          (unsigned)s.dma_free, (unsigned)s.psram_free, (unsigned)s.psram_largest,
          (unsigned)s.psram_min_ever);
}

void story_mem_log_delta(const char *label, const story_mem_snapshot_t *b,
                         const story_mem_snapshot_t *a)
{
    SLOGI(TAG, "MEM %-14s int %6u -> %6u (%+d) | psram %7u -> %7u (%+d)", label,
          (unsigned)b->int_free, (unsigned)a->int_free, (int)a->int_free - (int)b->int_free,
          (unsigned)b->psram_free, (unsigned)a->psram_free,
          (int)a->psram_free - (int)b->psram_free);
}
