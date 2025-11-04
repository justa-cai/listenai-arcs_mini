#if CFG_BACK_TRACE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "system_RISCVN300.h"
#include "nmsis_core.h"
#include "log_print.h"

#include "sysexec.h"
#include "multi_heap.h"

#include "FreeRTOS.h"
#include "task.h"

#define RVB_CALL_STACK_MAX_DEPTH 16

#define rvb_print(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define rvb_println(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)


struct stackframe {
    uint32_t fp;
    uint32_t ra;
};

#define ADDR_IN_RANGE(addr, start, end) (((addr) >= (start)) && ((addr) <= (end)))

#define ADDR_IN_SRAM(addr)  ADDR_IN_RANGE(addr, 0x20000000, 0x20000000 + (256 + 64 + 384 + 24) * 1024)
#define ADDR_IN_PSRAM(addr) ADDR_IN_RANGE(addr, 0x28000000, 0x28000000 + 16 * 1024 * 1024)

static bool addr_isvalid(uint32_t addr)
{
    if (ADDR_IN_SRAM(addr)) {
        return true;
    }

    if (ADDR_IN_PSRAM(addr)) {
        return true;
    }

    return false;
}


static void stack_dump(uint32_t *data, size_t cnt)
{
    for (size_t i = 0; i < cnt; i++) {
        if (i % 4 == 0) {
            rvb_print("\n%p: ", data);
        }
        rvb_print("%08lx ", (unsigned long)*data++);
    }

    rvb_print("\n");
}

static inline bool frame_isvalid(struct stackframe *frame, uint32_t sp_start, uint32_t sp_end)
{
    if ((uint32_t)frame < sp_start || (uint32_t)frame > sp_end) {
        return false;
    }

    if (!addr_isvalid((uint32_t)frame)) {
        return false;
    }

    if (!addr_isvalid(frame->fp)) {
        return false;
    }

    return true;
}

static inline void backtrace(uint32_t pc, uint32_t fp, uint32_t sp_start, uint32_t sp_end)
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
        stack_dump((uint32_t *)sp, ((uint32_t)task_status.pxEndOfStack - sp) / sizeof(uint32_t));
    }

    backtrace(pc, fp, (uint32_t)task_status.pxStackBase, (uint32_t)task_status.pxEndOfStack);
    rvb_println("backtrace end");
}

static void show_all_tasks_info()
{
    const char *task_state_str[] = {
        "running",
        "ready",
        "blocked",
        "suspended",
        "deleted",
        "invalid",
    };

    rvb_println("");
    rvb_println("**********************task info**********************");

    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize, uxTask;
    configRUN_TIME_COUNTER_TYPE ulTotalRunTime;
    uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        rvb_print("%-12s\t%-12s\t%-12s\t%-12s\t%-12s\t%-12s\n", "name", "state", "prio", "stack(byte)", "max-used(%)", "cpu(%)");
        for (uxTask = 0; uxTask < uxArraySize; uxTask++) {
            uint32_t stack_size =
                (uint32_t)pxTaskStatusArray[uxTask].pxEndOfStack - (uint32_t)pxTaskStatusArray[uxTask].pxStackBase;
            rvb_print("%-12s\t%-12s\t%-12ld\t%-12ld\t%-12.2f\t%-12.2f\n", pxTaskStatusArray[uxTask].pcTaskName,
                      task_state_str[pxTaskStatusArray[uxTask].eCurrentState],
                      (unsigned long)pxTaskStatusArray[uxTask].uxCurrentPriority,
                      (unsigned long)stack_size, 100.0f - (float)pxTaskStatusArray[uxTask].usStackHighWaterMark * 4 / (float)stack_size * 100,
                      (float)pxTaskStatusArray[uxTask].ulRunTimeCounter / (float)ulTotalRunTime * 100);
        }

        vPortFree(pxTaskStatusArray);
    } else {
        rvb_print("Failed to allocate memory for task status array.\n");
    }
    rvb_println("");
}

static void heap_travel_cb(void *start, void *end, multi_heap_info_t *info)
{
    rvb_println("%-12p\t%-12p\t%-12d\t%-12d\t%-12d", start, end, info->total_free_bytes,
                info->total_allocated_bytes, info->minimum_free_bytes);
}

void show_heap_info(void)
{
    void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
    rvb_println("**********************heap info**********************");
    rvb_println("%-12s\t%-12s\t%-12s\t%-12s\t%-12s", "start", "end", "free", "used", "free(min)");
    heap_caps_travel(heap_travel_cb);
    rvb_println("");
}

enum {
    NORMAL_MODE = 0,
    INTERRUPT_MODE,
    EXCE_MODE,
    NMI_MODE,
};

static const char *fault_detail_get(uint8_t exec_code)
{
    const char *mdcause_str[4] = {
        "reserved",
        "pmp",
        "bus",
        "nice"
    };

    uint32_t mdcause = __RV_CSR_READ(CSR_MDCAUSE);

    if (exec_code == 1 || exec_code == 5 || exec_code == 7) {
        return mdcause_str[mdcause];
    }

    return "unknown";
}

static const char *fault_reason_get(uint8_t code)
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

    return code >= sizeof(exc_reason_str)/sizeof(exc_reason_str[0]) ? "unknown" : exc_reason_str[code];
}

void rv_backtrace_fault(uint32_t sp, struct exec_frame *frame, uint32_t mstatus, uint32_t mstratch)
{
    (void)mstratch; // Suppress unused parameter warning
    
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
                fault_reason_get(exc_code), fault_detail_get(exc_code));

    rvb_println("interrupt stack: %lx~%lx, sp: %lx", (unsigned long)_sstack, (unsigned long)_estack, (unsigned long)sp);

    rvb_println("Fault on mode: %d(%s)", trap_before, sub_mode_str[trap_before]);

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        show_heap_info();
        show_all_tasks_info();
    } else {
        rvb_println("Rtos is not started");
    }

    if (trap_before == NORMAL_MODE) {
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            task_backtrace(xTaskGetCurrentTaskHandle(), frame->epc, frame->sp, frame->fp);
            return;
        }
    }

    if (sp < (uint32_t)_sstack || sp > (uint32_t)_estack) {
        rvb_println("Stack overflow\n");
    } else {
        rvb_println("Stack dump on interrupt stack");
        stack_dump((uint32_t *)sp, ((uint32_t)_estack - sp) / sizeof(uint32_t));
    }

    backtrace(frame->epc, frame->fp, (uint32_t)_sstack, (uint32_t)_estack);
}
#endif