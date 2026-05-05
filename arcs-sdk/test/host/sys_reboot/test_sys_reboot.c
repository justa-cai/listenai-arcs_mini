#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "sys/reboot.h"

void sys_arch_reboot(int type);

enum reboot_event {
    EVENT_NONE = 0,
    EVENT_DISABLE_IRQ,
    EVENT_SOFT_RESET,
    EVENT_HARD_RESET,
    EVENT_WFI,
};

static enum reboot_event events[8];
static size_t event_count;
static jmp_buf wfi_env;

static void reset_events(void)
{
    size_t index;

    event_count = 0;
    for (index = 0; index < (sizeof(events) / sizeof(events[0])); ++index) {
        events[index] = EVENT_NONE;
    }
}

static void record_event(enum reboot_event event)
{
    if (event_count >= (sizeof(events) / sizeof(events[0]))) {
        fprintf(stderr, "too many events\n");
        exit(1);
    }

    events[event_count++] = event;
}

static void assert_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        exit(1);
    }
}

static void assert_event(size_t index, enum reboot_event expected)
{
    if ((index >= event_count) || (events[index] != expected)) {
        fprintf(stderr, "unexpected event at index %zu\n", index);
        exit(1);
    }
}

void fake_disable_irq(void)
{
    record_event(EVENT_DISABLE_IRQ);
}

void fake_soft_reset(void)
{
    record_event(EVENT_SOFT_RESET);
}

void sys_platform_sw_full_reset(void)
{
    record_event(EVENT_HARD_RESET);
}

void fake_wfi(void)
{
    record_event(EVENT_WFI);
    longjmp(wfi_env, 1);
}

static void test_sys_arch_reboot_dispatches_soft_and_hard(void)
{
    reset_events();
    sys_arch_reboot(SYS_REBOOT_SOFT);
    assert_true(event_count == 1, "soft path should emit one event");
    assert_event(0, EVENT_SOFT_RESET);

    reset_events();
    sys_arch_reboot(SYS_REBOOT_HARD);
    assert_true(event_count == 1, "hard path should emit one event");
    assert_event(0, EVENT_HARD_RESET);

    reset_events();
    sys_arch_reboot(1234);
    assert_true(event_count == 1, "invalid path should emit one event");
    assert_event(0, EVENT_HARD_RESET);
}

static void test_sys_reboot_disables_irq_before_dispatch(void)
{
    reset_events();

    if (setjmp(wfi_env) == 0) {
        sys_reboot(SYS_REBOOT_SOFT);
    }

    assert_true(event_count == 3, "sys_reboot should emit three events");
    assert_event(0, EVENT_DISABLE_IRQ);
    assert_event(1, EVENT_SOFT_RESET);
    assert_event(2, EVENT_WFI);
}

int main(void)
{
    test_sys_arch_reboot_dispatches_soft_and_hard();
    test_sys_reboot_disables_irq_before_dispatch();

    puts("sys_reboot host tests passed");
    return 0;
}
