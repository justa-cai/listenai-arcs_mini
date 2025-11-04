#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "system_RISCVN300.h"
#include "nmsis_core.h"
#include "log_print.h"

#ifdef CFG_BACK_TRACE
#include "FreeRTOS.h"
#include "task.h"

#define RVB_CALL_STACK_MAX_DEPTH       16

#define rvb_println(...)                 printf(__VA_ARGS__)

extern uint32_t _rom_code_start[], _rom_code_end[], _ram_code_start[], _ram_code_end[];
extern uint32_t _sstack[], _estack[];
static char call_stack_info[RVB_CALL_STACK_MAX_DEPTH * (8 + 1)] = { 0 };

/**
 * Get current thread name
 */
static void rv_get_cur_thread_info(char **name, uint32_t *start, uint32_t *end)
{
    TaskHandle_t task_hdl;
    TaskStatus_t task_status = {0};

    task_hdl = xTaskGetCurrentTaskHandle();

    vTaskGetInfo(task_hdl, &task_status, pdFALSE, eInvalid);

    *name = (uint32_t)task_status.pcTaskName;
    *start = (uint32_t)task_status.pxStackBase;
    *end = (uint32_t)task_status.pxEndOfStack;
}

static uint32_t rv_ins_check_jp(uint32_t addr)
{
    uint32_t ins = 0, len = 0;

    ins = *((uint32_t *)addr);
    if ((ins & 0x3) == 3)
    {
        if ((ins & 0x0FFF) == (0x006F | (1<<7)))
        {
            len = 4;
        }
        else if ((ins & 0x0FFF) == (0x0067 | (1<<7)))
        {
            len = 4;
        }
    }
    else
    {
        uint16_t ins_c;
        ins_c = *((uint16_t*)(addr + 2));

        if ((ins_c & 0xF003) == (0x2 | (9<<12)))
        {
            len = 2;
        }
        else if ((ins_c & 0xE003) == (0x1 | (1<<13)))
        {
            len = 2;
        }
    }

    return len;
}

static int32_t rv_ins_check_range(uint32_t pc)
{
    if ((pc > (uint32_t)_rom_code_start && pc < (uint32_t)_rom_code_end)
                || (((uint32_t)_ram_code_start < (uint32_t)_ram_code_end) && (pc > (uint32_t)_ram_code_start && pc < (uint32_t)_ram_code_end)))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static uint32_t rv_ins_check_addi(uint32_t ins)
{
    uint32_t imm = 0;

    if ( ((ins & 0x3) == 0x1) && ((ins & 0xE000) == (0x3<<13)) && ( (((ins & 0xF80)>>7) == 0x2) ) )
    {
        if (ins & (1<<2)) imm |= 1<<5;
        if (ins & (1<<3)) imm |= 1<<7;
        if (ins & (1<<4)) imm |= 1<<8;
        if (ins & (1<<5)) imm |= 1<<6;
        if (ins & (1<<6)) imm |= 1<<4;

        imm = (~imm + 1) & 0x1FF;
    }
    else if ( ((ins & 0x3) == 0x1) && ((ins & 0xE000) == 0x0) && ( (((ins & 0xF80)>>7) == 0x2) ) )
    {
        imm = (ins & 0x7F)>>2;
        imm = (~imm + 1) & 0x1F;
    }

    return imm;
}

static bool rv_ins_check_frame(uint32_t pc, uint32_t *imm)
{
    bool got = false;
    int32_t i;
    uint32_t ins;

    ins = *((uint32_t *)pc);
    if ((ins & 0xFFF) == ((5<<7) | 0x6F))
    {
        got = true;
        for (i = 0; i < 10; i++, pc += 2)
        {
            ins = *((uint32_t *)pc);
            if ((*imm = rv_ins_check_addi(ins)))
                break;
        }
    }

    return got;
}

static uint32_t rv_skip_frame(uint32_t pc)
{
    uint32_t i = 0, imm = 0;

    while (i++ < 1000)
    {
        pc -= 2;
        if ((rv_ins_check_frame(pc, &imm)))
            break;
    }

    return imm;
}
/**
 * backtrace function call stack
 *
 * @param buffer call stack buffer
 * @param max buffer size
 * @param sp stack pointer
 *
 * @return depth
 */
static size_t rv_backtrace_call_stack(uint32_t *buffer, size_t max, uint32_t sp, uint32_t stack_start, uint32_t stack_end, int32_t skip_disable)
{
    uint32_t pc, imm, depth = 0;

    for (; sp < stack_end; sp += sizeof(size_t))
    {
        pc = *((uint32_t *)sp);
        if (pc % 2)
            continue;

        if (rv_ins_check_range(pc) && (rv_ins_check_jp(pc - sizeof(size_t))))
        {
            buffer[depth++] = pc;
            if (!skip_disable && (imm = rv_skip_frame(pc)))
            {
                sp += imm;
            }
            if (depth >= max)
                break;
        }
    }

    return depth;
}

/**
 * dump function call stack
 *
 * @param sp stack pointer
 */
static void print_call_stack(uint32_t sp, EXC_Frame_Type *frame, uint32_t exc_code, uint32_t stack_start, uint32_t stack_end)
{
    uint32_t call_stack_buf[RVB_CALL_STACK_MAX_DEPTH] = {0};
    uint32_t i = 0, cur_depth, skip_disable = 0;
    uint32_t len = 0;

    if ((exc_code != CAUSE_ILLEGAL_INSTRUCTION) && rv_ins_check_range(frame->epc))
    {
        len = rv_skip_frame(frame->epc);
        call_stack_buf[i++] = frame->epc;
        /*Maybe no frame*/
        if (!len && rv_ins_check_range(frame->ra) && rv_ins_check_jp(frame->ra - sizeof(size_t)))
        {
            len = rv_skip_frame(frame->ra);
            call_stack_buf[i++] = frame->ra;
        }
        sp += len;
    }
    else if ((exc_code == CAUSE_ILLEGAL_INSTRUCTION) && rv_ins_check_range(frame->ra) && rv_ins_check_jp(frame->ra - sizeof(size_t)))
    {
        len = rv_skip_frame(frame->ra);
        sp += len;
        call_stack_buf[i++] = frame->ra;
    }

    if (!len) skip_disable = 1;

    cur_depth  = rv_backtrace_call_stack(&call_stack_buf[i], (RVB_CALL_STACK_MAX_DEPTH - i), sp, stack_start, stack_end, skip_disable);
    cur_depth += i;
    for (i = 0; i < cur_depth; i++) {
        sprintf(call_stack_info + i * (8 + 1), "%08lx", (unsigned long)call_stack_buf[i]);
        call_stack_info[i * (8 + 1) + 8] = ' ';
    }

    if (cur_depth) {
        rvb_println("Show more call stack info by run: addr2line -e [elf] -a -f  %.*s\n", cur_depth * (8 + 1),
                call_stack_info);
    } else {
        rvb_println("Dump call stack has an error\n");
    }
}

static void dump_stack(uint32_t stack_start, uint32_t stack_end, uint32_t *stack_pointer)
{
    rvb_println("===== stack information =====\n");
    for (; (uint32_t) stack_pointer < stack_end; stack_pointer++) {
        rvb_println("  addr: %08x    data: %08x\n", stack_pointer, *stack_pointer);
    }
    rvb_println("=============================\n");
}

/**
 * backtrace for fault
 * @note only call once
 *
 */
void rv_backtrace_fault(uint32_t sp, EXC_Frame_Type *frame, uint32_t mstatus, uint32_t mstratch)
{
    char *task_name = NULL;
    uint32_t stack_start = 0, stack_end = 0, exc_code = frame->cause & 0x1F;

    rvb_println("Exception: ");
    switch (exc_code)
    {
        case CAUSE_MISALIGNED_FETCH:
            rvb_println("Instruction address misaligned\n");
            break;
        case CAUSE_FAULT_FETCH:
            rvb_println("Instruction access fault\n");
            break;
        case CAUSE_ILLEGAL_INSTRUCTION:
            rvb_println("Illegal instruction\n");
            break;
        case CAUSE_MISALIGNED_LOAD:
            rvb_println("Load address misalignedinstruction\n");
            break;
        case CAUSE_FAULT_LOAD:
            rvb_println("Load access fault\n");
            break;
        case CAUSE_MISALIGNED_STORE:
            rvb_println("Store/AMO address misaligned\n");
            break;
        case CAUSE_FAULT_STORE:
            rvb_println("Store/AMO access fault\n");
            break;
        default:
            rvb_println("%d\n", exc_code);
            break;
    }

    if (((mstatus & MSTATUS_MPP) == PRV_U) || ((frame->msubm & MSUBM_PTYP) == 0))
    {
        rv_get_cur_thread_info(&task_name, &stack_start, &stack_end);
        if (!task_name)
            task_name = "NO_NAME";

        rvb_println("Fault on task:%s stack:0x%08x-0x%08x\n", task_name, stack_start, stack_end);
    }
    else if ((frame->msubm & MSUBM_PTYP) == (1<<8))
    {
        rvb_println("Fault on interrupt");
        sp = mstratch;
        stack_start = (uint32_t)_sstack;
        stack_end   = (uint32_t)_estack;
        rvb_println(" stack:%08x %08x\n", stack_start, stack_end);
    }
    else if ((mstratch & 0xF0000000) && (frame->msubm & MSUBM_PTYP) == (2<<8))
    {
        rvb_println("MSTRATCH has not been initialized\n");
    }

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        stack_start = (uint32_t)_sstack;
        stack_end   = (uint32_t)_estack;
        rvb_println("RTOS has not started\n");
    }

    /* check stack overflow */
    if (stack_start && (sp < stack_start))
    {
        rvb_println("Error: stack(%08x) was overflow\n", sp);
        sp = stack_start;
    }
    else if (stack_end && (sp > stack_end))
    {
        rvb_println("Error: stack(%08x) was overflow\n", sp);
        sp = stack_end;
    }

    /* dump stack information */
    dump_stack(stack_start, stack_end, (uint32_t *)sp);

    print_call_stack(sp, frame, exc_code, stack_start, stack_end);
}

#endif
