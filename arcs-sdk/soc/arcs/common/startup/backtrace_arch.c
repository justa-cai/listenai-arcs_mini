#if CFG_BACK_TRACE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "system_RISCVN300.h"
#include "nmsis_core.h"
#include "log_print.h"

#include "sys_exec.h"
#include "backtrace.h"

#include "FreeRTOS.h"
#include "task.h"

#define rvb_print(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define rvb_println(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define RVB_CALL_STACK_MAX_DEPTH 16

struct stackframe {
    uint32_t fp;
    uint32_t ra;
};

#define ADDR_IN_RANGE(addr, start, end) (((addr) >= (start)) && ((addr) <= (end)))

#define ADDR_IN_SRAM(addr)  ADDR_IN_RANGE(addr, 0x20000000, 0x20000000 + (256 + 64 + 384 + 24) * 1024)
#define ADDR_IN_PSRAM(addr) ADDR_IN_RANGE(addr, 0x28000000, 0x28000000 + 16 * 1024 * 1024)

bool backtrace_addr_is_valid(uint32_t addr)
{
    if (ADDR_IN_SRAM(addr)) {
        return true;
    }

    if (ADDR_IN_PSRAM(addr)) {
        return true;
    }

    return false;
}

static inline bool frame_isvalid(struct stackframe *frame, uint32_t sp_start, uint32_t sp_end)
{
    if ((uint32_t)frame < sp_start || (uint32_t)frame > sp_end) {
        return false;
    }

    if (!backtrace_addr_is_valid((uint32_t)frame)) {
        return false;
    }

    if (!backtrace_addr_is_valid(frame->fp)) {
        return false;
    }

    return true;
}

void backtrace_walk(uint32_t pc, uint32_t fp, uint32_t sp_start, uint32_t sp_end)
{
    rvb_println("Possible backtrace:");
    rvb_print("riscv64-unknown-elf-addr2line -e build/" PROJECT_EXECUTABLE_NAME " -a ");
    rvb_print("%lx ", (unsigned long)pc);
    struct stackframe *frame;
    frame = (struct stackframe *)(fp - 2 * sizeof(uint32_t));

    while (frame_isvalid(frame, sp_start, sp_end)) {
        rvb_print("%lx ", (unsigned long)(frame->ra - 4));
        frame = (struct stackframe *)(frame->fp - 2 * sizeof(uint32_t));
    }
    rvb_println("");
}

const char *backtrace_fault_reason(uint32_t exc_code)
{
    static const char *exc_reason_str[] = {
        [CAUSE_MISALIGNED_FETCH] = "Instruction address misaligned",
        [CAUSE_FAULT_FETCH] = "Instruction access fault",
        [CAUSE_ILLEGAL_INSTRUCTION] = "Illegal instruction",
        [CAUSE_BREAKPOINT] = "Breakpoint",
        [CAUSE_MISALIGNED_LOAD] = "Load address misaligned",
        [CAUSE_FAULT_LOAD] = "Load access fault",
        [CAUSE_MISALIGNED_STORE] = "Store/AMO address misaligned",
        [CAUSE_FAULT_STORE] = "Store/AMO access fault",
        [CAUSE_USER_ECALL] = "User ecall",
        [CAUSE_SUPERVISOR_ECALL] = "Supervisor ecall",
        [CAUSE_MACHINE_ECALL] = "Machine ecall",
        [CAUSE_FETCH_PAGE_FAULT] = "Fetch page fault",
        [CAUSE_LOAD_PAGE_FAULT] = "Load page fault",
        [CAUSE_STORE_PAGE_FAULT] = "Store page fault",
        [0x18] = "stack overflow",
        [0x19] = "stack underflow",
    };

    return exc_code >= sizeof(exc_reason_str)/sizeof(exc_reason_str[0]) ? "unknown" : exc_reason_str[exc_code];
}

const char *backtrace_fault_detail(uint32_t exc_code)
{
    const char *mdcause_str[4] = {
        "reserved",
        "pmp",
        "bus",
        "nice"
    };

    uint32_t mdcause = __RV_CSR_READ(CSR_MDCAUSE);

    if (exc_code == 1 || exc_code == 5 || exc_code == 7) {
        return mdcause_str[mdcause];
    }

    return "unknown";
}

enum {
    NORMAL_MODE = 0,
    INTERRUPT_MODE,
    EXCE_MODE,
    NMI_MODE,
};

static void task_backtrace(TaskHandle_t th, uint32_t pc, uint32_t sp, uint32_t fp)
{
    TaskStatus_t task_status = {0};
    vTaskGetInfo(th, &task_status, pdFALSE, eInvalid);

    rvb_println("Fault on task: %p(%s), stack:0x%08lx-0x%08lx, sp:0x%08lx\n", th, task_status.pcTaskName,
                (unsigned long)task_status.pxStackBase, (unsigned long)task_status.pxEndOfStack, (unsigned long)sp);
    if (sp < (uint32_t)task_status.pxStackBase || sp > (uint32_t)task_status.pxEndOfStack) {
        rvb_println("Stack overflow\n");
    } else {
        rvb_println("Stack dump on task: %p(%s)", th, task_status.pcTaskName);
        backtrace_stack_dump((uint32_t *)sp, ((uint32_t)task_status.pxEndOfStack - sp) / sizeof(uint32_t));
    }

    backtrace_walk(pc, fp, (uint32_t)task_status.pxStackBase, (uint32_t)task_status.pxEndOfStack);
    rvb_println("backtrace end");
}

void rv_backtrace_fault(uint32_t sp, struct exec_frame *frame, uint32_t mstatus, uint32_t mstratch)
{
    (void)mstratch;

    const char *sub_mode_str[] = {
        "normal",
        "interrupt",
        "exception",
        "nmi",
    };

    extern uint32_t _sstack[], _estack[];
    uint8_t trap_before = (frame->msubm & MSUBM_PTYP) >> 8;
    uint32_t exc_code = frame->mcause & 0x1F;

    if (trap_before == INTERRUPT_MODE || trap_before == NMI_MODE) {
        sp = frame->sp;
        sp += sizeof(struct EXC_Frame);
    } else if (trap_before == EXCE_MODE) {
        sp = frame->sp;
        sp += sizeof(struct exec_frame);
    } else if (trap_before == NORMAL_MODE) {
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            /* nothing */
        } else {
            sp = frame->sp;
        }
    }

    rvb_println("*****************************************************");
    rvb_println("Exception reason: %ld(%s), detail: %s", (unsigned long)exc_code,
                backtrace_fault_reason(exc_code), backtrace_fault_detail(exc_code));

    rvb_println("interrupt stack: %lx~%lx, sp: %lx", (unsigned long)_sstack, (unsigned long)_estack, (unsigned long)sp);

    rvb_println("Fault on mode: %d(%s)", trap_before, sub_mode_str[trap_before]);

    if (trap_before == NORMAL_MODE) {
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            task_backtrace(xTaskGetCurrentTaskHandle(), frame->epc, frame->sp, frame->fp);
            goto task_info_show;
        }
    }

    if (sp < (uint32_t)_sstack || sp > (uint32_t)_estack) {
        rvb_println("Stack overflow\n");
    } else {
        rvb_println("Stack dump on interrupt stack");
        backtrace_stack_dump((uint32_t *)sp, ((uint32_t)_estack - sp) / sizeof(uint32_t));
    }

    backtrace_walk(frame->epc, frame->fp, (uint32_t)_sstack, (uint32_t)_estack);

task_info_show:
    rvb_println("\n\n");
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        backtrace_show_heap_info();
        backtrace_show_tasks_info();
    } else {
        rvb_println("Rtos is not started");
    }
}
#endif
