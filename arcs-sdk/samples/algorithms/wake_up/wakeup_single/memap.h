#ifndef __ARCS_MEMAP_HEADER__
#define __ARCS_MEMAP_HEADER__

#define __KB__(x)  ((x) * 1024)
#define __MB__(x)  ((x) * 1024 * 1024)
#if CONFIG_ARCS_AP_CORE
#define MEM_ILM_BASE    0x00080000
#define MEM_ILM_SIZE    (__KB__(16))

#define MEM_DLM_BASE    0x00100000
#define MEM_DLM_SIZE    (__KB__(8))
#else
#define MEM_ILM_BASE    0x00280000
#define MEM_ILM_SIZE    (__KB__(16))

#define MEM_DLM_BASE    0x00300000
#define MEM_DLM_SIZE    (__KB__(8))
#endif

#define MEM_BTRAM_BASE  0x200C0000
#define MEM_BTRAM_SIZE  (__KB__(24))

/* FLASH 分配 */
#define MEM_TOTAL_FLASH_SIZE  __MB__(16)

#define MEM_AP_FLASH_BASE  0x30000000
#define MEM_AP_FLASH_SIZE  __KB__(1024)

#define MEM_CP_FLASH_BASE  (0x30000000 + __MB__(5))
#define MEM_CP_FLASH_SIZE  __MB__(8)
 
/* PSRAM 分配 */
#define MEM_AP_PSRAM_BASE  0x28000000
#define MEM_AP_PSRAM_SIZE  __MB__(8)

#define MEM_CP_PSRAM_BASE  ((MEM_AP_PSRAM_BASE) + (MEM_AP_PSRAM_SIZE))
#define MEM_CP_PSRAM_SIZE  __MB__(8)

#if CONFIG_ARCS_AP_CORE
#define MEM_PSRAM_BASE  MEM_AP_PSRAM_BASE
#define MEM_PSRAM_SIZE  MEM_AP_PSRAM_SIZE
#else
#define MEM_PSRAM_BASE  MEM_CP_PSRAM_BASE
#define MEM_PSRAM_SIZE  MEM_CP_PSRAM_SIZE
#endif

#if CONFIG_ARCS_AP_CORE
#define MEM_FLASH_BASE  MEM_AP_FLASH_BASE
#define MEM_FLASH_SIZE  MEM_AP_FLASH_SIZE
#else
#define MEM_FLASH_BASE  MEM_CP_FLASH_BASE
#define MEM_FLASH_SIZE  MEM_CP_FLASH_SIZE
#endif

#define __MEM_TOTAL_SRAM_START__ 0x20000080         //暂留128(0x80)字节给WiFi硬件
#define __MEM_TOTAL_SRAM_END__   0x20050000

/**
 * SRAM 应用侧分配
 * --------------------------------------------
 * 0x20000000     |     CP-WIFI-RAM(这段内存必须在 0x20000000 + 256KB区间)
 *     |          |
 *    (256K)      |     CP-SRAM
 *     |          |
 *     |          |     AP-WIFI-RAM (这段内存必须在 0x20000000 + 256KB区间)
 * -------------- |
 * 0x20040000     |     硬件接在CP核上的SRAM(地址0x20040000，总计64K)
 *    (8K)        |     IPC共享内存
 *    (28K)       |     LUNA CODE & DATA
 * -------------- |
 * 0x20049000     |     硬件接在AP核上的SRAM(地址0x20050000，总计384K)
 *    (62K)       |     AP-SRAM
 * --------------------------------------------
 * 0x20058800     |     AP算法使用(HardCode)
 *     |          |
 *   (350K)       |     ALGO
 *     |          |
 * --------------------------------------------
 * 0x200B0000     |
 *     |          |
 *   (24K)        |     LUNA 核专属内存
 *     |          |
 * --------------------------------------------
 * 0x200C0000
 */

/* 独立给算法使用 */
#define MEM_APRAM_BASE  0x20058800
#define MEM_APRAM_SIZE  (__KB__(350))

/* 注意,修改以下内存时, 必须与CP保持同步修改 */
#define MEM_CP_WIFI_RAM_BASE  __MEM_TOTAL_SRAM_START__
#define MEM_CP_WIFI_RAM_SIZE  (__KB__(64) - 128)        //暂留128(0x80)字节给WiFi硬件

#define MEM_CP_SRAM_BASE  ((MEM_CP_WIFI_RAM_BASE) + (MEM_CP_WIFI_RAM_SIZE))
#define MEM_CP_SRAM_SIZE  (__KB__(192))

#define MEM_AP_WIFI_RAM_BASE  ((MEM_CP_SRAM_BASE) + (MEM_CP_SRAM_SIZE))
#define MEM_AP_WIFI_RAM_SIZE  (__KB__(0))

#define MEM_IPC_BASE  ((MEM_AP_WIFI_RAM_BASE) + (MEM_AP_WIFI_RAM_SIZE))
#define MEM_IPC_SIZE  (__KB__(8))

#define MEM_LUNA_BASE  ((MEM_IPC_BASE) + (MEM_IPC_SIZE))
#define MEM_LUNA_SIZE  (__KB__(28))

#define MEM_AP_SRAM_BASE  ((MEM_LUNA_BASE) + (MEM_LUNA_SIZE))
#define MEM_AP_SRAM_SIZE  (__KB__(62))

#if CONFIG_ARCS_CP_CORE
#define MEM_SRAM_BASE  MEM_CP_SRAM_BASE
#define MEM_SRAM_SIZE  MEM_CP_SRAM_SIZE

#define MEM_WFRAM_BASE  MEM_CP_WIFI_RAM_BASE
#define MEM_WFRAM_SIZE  MEM_CP_WIFI_RAM_SIZE
#else
#define MEM_SRAM_BASE  MEM_AP_SRAM_BASE
#define MEM_SRAM_SIZE  MEM_AP_SRAM_SIZE
#define MEM_WFRAM_BASE  MEM_AP_WIFI_RAM_BASE
#define MEM_WFRAM_SIZE  MEM_AP_WIFI_RAM_SIZE
#endif

/* 检查下内存是否超出 */
#if (MEM_IPC_BASE + MEM_IPC_SIZE) > __MEM_TOTAL_SRAM_END__
#error "mem sram overflow"
#endif

/* wifi sram 必须要在 0x20000000 ~ 0x20040000之间 */
#if (MEM_CP_WIFI_RAM_BASE < 0x20000000) || ((MEM_CP_WIFI_RAM_BASE + MEM_CP_WIFI_RAM_SIZE) > (0x20000000 + __KB__(256)))
#error "cp wifi sram error"
#endif

/* wifi sram 必须要在 0x20000000 ~ 0x20040000之间 */
#if (MEM_AP_WIFI_RAM_BASE < 0x20000000) || ((MEM_AP_WIFI_RAM_BASE + MEM_AP_WIFI_RAM_SIZE) > (0x20000000 + __KB__(256)))
#error "ap wifi sram error"
#endif

/* luna code & instruction & data 必须要在 0x20040000 ~ 0x200B0000 + 448(KB)之间 */
#if (MEM_LUNA_BASE < 0x20040000) || ((MEM_LUNA_BASE + MEM_LUNA_SIZE) > (0x20040000 + __KB__(448)))
#error "ap luna sram error"
#endif

/* 根据实际需要调整 */
#if CONFIG_ARCS_AP_CORE
/* 这个是SRAM HEAP大小的定义, 该heap从SRAM中分配 */
#define SRAM_HEAP_SIZE (__KB__(5))

// /* WIFI校准数据大小 */
// #define MEM_WIFI_CALIBRATION_SIZE (__KB__(16))
// #define MEM_WIFI_TRACE_SIZE (__KB__(1))
#define MEM_WIFI_CALIBRATION_SIZE (__KB__(16))
#define MEM_WIFI_TRACE_SIZE (__KB__(0))
#define MEM_WIFI_LA_DUMP_SIZE (__KB__(32))

/* 中断栈 */
#define MEM_INTERRUPT_STACK_SIZE  (4 * 1024)
#else
/* 在CP侧, 没有wifi的校准数据与跟踪数据, 这里定义为0 */
#define MEM_WIFI_CALIBRATION_SIZE (__KB__(0))
#define MEM_WIFI_TRACE_SIZE (__KB__(0))
/* 中断栈 */
#define MEM_INTERRUPT_STACK_SIZE  (4 * 1024)
#endif

#endif//__ARCS_MEMAP_HEADER__
