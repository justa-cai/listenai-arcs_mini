#ifndef __ARCS_MEMAP_HEADER__
#define __ARCS_MEMAP_HEADER__

/* LLM 分支 v2 内存分割契约（AP 跑 LLM 服务，CP 跑全量业务）。
 * 与 apps/arcs-mini/memap.h 必须保持一致，改一处必须同步另一处。
 *
 *   PSRAM 16MB: AP 8MB (0x28000000) | CP 8MB (0x28800000)
 *   Flash 16MB: boot 0x0 | AP+模型 0x40000..0x680000 | CP 0x700000..0xB00000
 *   CP 镜像 flash: 0x30700000（app 分区 0x700000）
 */

#define MEM_AP_PSRAM_BASE  0x28000000u
#define MEM_AP_PSRAM_SIZE  0x00800000u

#define MEM_CP_PSRAM_BASE  0x28800000u
#define MEM_CP_PSRAM_SIZE  0x00800000u

/* CP 镜像 flash 起始（释放 CP 用），须与 apps/arcs-mini prj.conf
 * CONFIG_MEM_FLASH_BASE 一致 */
#define MEM_CP_FLASH_BASE  0x30700000u

#endif /* __ARCS_MEMAP_HEADER__ */
