/*
 * SoC SRAM 布局静态校验
 *
 * 硬件约束：
 *   - WFRAM 必须落在 [0x20000000, 0x20040000)（由 wifi-noinit.ld 运行时位置校验）
 *   - SRAM/IPC 必须落在 [0x20000000, 0x20050000)
 *
 * 这里集中校验 SRAM 与 WFRAM/IPC 互不重叠，
 * 防止双核分区时 prj.conf 配错。原 sample memap.h 里的护栏迁移到这里。
 */

#define _DISJOINT(a_base, a_size, b_base, b_size) \
    (((a_base) + (a_size)) <= (b_base)) || ((a_base) >= ((b_base) + (b_size)))

ASSERT((CONFIG_MEM_SRAM_BASE >= 0x20000000),
    "error: SRAM base must be >= 0x20000000")
ASSERT(((CONFIG_MEM_SRAM_BASE + CONFIG_MEM_SRAM_SIZE) <= 0x20050000),
    "error: SRAM must end <= 0x20050000")

#if CONFIG_MEM_WFRAM_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_WFRAM_BASE, CONFIG_MEM_WFRAM_SIZE),
    "error: SRAM overlaps WFRAM, check prj.conf")
#endif

#if defined(CONFIG_MEM_IPC_ISOLATED) && CONFIG_MEM_IPC_ISOLATED && CONFIG_MEM_IPC_SIZE > 0
ASSERT(_DISJOINT(CONFIG_MEM_SRAM_BASE, CONFIG_MEM_SRAM_SIZE,
                 CONFIG_MEM_IPC_BASE, CONFIG_MEM_IPC_SIZE),
    "error: SRAM overlaps IPC region, check prj.conf")
#endif

#undef _DISJOINT
