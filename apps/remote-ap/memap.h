#ifndef __ARCS_MEMAP_HEADER__
#define __ARCS_MEMAP_HEADER__

/* LLM 分支内存分割契约（AP 桩 + CP 大内存）。
 * 与 apps/arcs-mini/memap.h 必须保持一致，改一处必须同步另一处。
 *
 *   PSRAM 16MB: AP 1MB (0x28000000) | CP 15MB (0x28100000)
 *   CP 镜像 flash: 0x30600000（app 分区 0x600000）
 */

#define MEM_AP_PSRAM_BASE  0x28000000u
#define MEM_AP_PSRAM_SIZE  0x00100000u

#define MEM_CP_PSRAM_BASE  0x28100000u
#define MEM_CP_PSRAM_SIZE  0x00F00000u

/* CP 镜像 flash 起始（释放 CP 用），须与 apps/arcs-mini prj.conf
 * CONFIG_MEM_FLASH_BASE 一致 */
#define MEM_CP_FLASH_BASE  0x30600000u

#endif /* __ARCS_MEMAP_HEADER__ */
