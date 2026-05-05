#ifndef __ARCS_MEMAP_HEADER__
#define __ARCS_MEMAP_HEADER__

#define __KB__(x) ((x) * 1024)
#define __MB__(x) ((x) * 1024 * 1024)

/*
 * ILM / DLM (CP core 内部指令/数据存储器)
 */
#define MEM_ILM_BASE  0x00280000
#define MEM_ILM_SIZE  (__KB__(16))

#define MEM_DLM_BASE  0x00300000
#define MEM_DLM_SIZE  (__KB__(8))

/* BT RAM */
#define MEM_BTRAM_BASE  0x200C0000
#define MEM_BTRAM_SIZE  (__KB__(32))

/*
 * Flash (flash 起始地址 + boot 占用 16KB)
 */
#define MEM_FLASH_BASE  (0x30000000 + __KB__(16))
#define MEM_FLASH_SIZE  (__MB__(8) - __KB__(16))

/*
 * PSRAM: 8MB，AP 侧全量使用
 */
#define MEM_PSRAM_BASE  0x28000000
#define MEM_PSRAM_SIZE  __MB__(8)

/*
 * SRAM 布局（单核模式，无 IPC）：
 *   0x20000000       WiFi 硬件预留 128 字节
 *   0x20000080       WIFI_RAM (64KB)
 *   0x20010000       SRAM (640KB = 256KB + 384KB)
 *   0x200B0000       LUNA_RAM (24KB)
 *
 * 注：IPC_RAM 不单独划分，由 system.ld fallback 到 SRAM 内分配
 */
// #define MEM_WFRAM_BASE  0x20000080
// #define MEM_WFRAM_SIZE  (__KB__(64) - 0x80)

#define MEM_SRAM_BASE  0x20000000
#define MEM_SRAM_SIZE  (__KB__(320))

#define MEM_LUNA_BASE  0x200B0000
#define MEM_LUNA_SIZE  (__KB__(24))

/*
 * WiFi 相关数据区大小（AP 侧）
 */
#define MEM_WIFI_CALIBRATION_SIZE  (__KB__(16))
#define MEM_WIFI_TRACE_SIZE        (__KB__(0))

/* 中断栈 */
#define MEM_INTERRUPT_STACK_SIZE  (__KB__(4))

/* wifi sram 必须在 0x20000000 ~ 0x20040000 之间 */
// #if (MEM_WFRAM_BASE < 0x20000000) || ((MEM_WFRAM_BASE + MEM_WFRAM_SIZE) > (0x20000000 + __KB__(256)))
// #error "wifi sram out of range"
// #endif

#endif /* __ARCS_MEMAP_HEADER__ */
