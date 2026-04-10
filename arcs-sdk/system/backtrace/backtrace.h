#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* soc 层需实现 */
bool backtrace_addr_is_valid(uint32_t addr);
const char *backtrace_fault_reason(uint32_t exc_code);
const char *backtrace_fault_detail(uint32_t exc_code);
void backtrace_walk(uint32_t pc, uint32_t fp, uint32_t sp_start, uint32_t sp_end);

/* system 层提供的通用函数（供 soc 调用） */
void backtrace_stack_dump(uint32_t *data, size_t cnt);
void backtrace_show_tasks_info(void);
void backtrace_show_heap_info(void);

#endif
