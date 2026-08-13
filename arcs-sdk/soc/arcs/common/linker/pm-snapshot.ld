/* PM PSRAM snapshot 区：在 PSRAM 内预留 NOLOAD section，
 * 由 HAL pm_impl 在睡眠/唤醒时把 ILM/DLM 等非 retain 段镜像到此。
 *
 * 历史上这块用独立 MEMORY region 实现（pm-memory.ld），与 PSRAM
 * 物理重叠，靠注释里"业务必须留出 N 字节"的人肉约束支撑——业务
 * .psram.* 长进尾部时 linker 不会报错，运行时 snapshot save 直接
 * 写坏业务数据。
 *
 * 现在改成 PSRAM 内的一个 NOLOAD section，`>PSRAM` 让 linker 把
 * 它计入 PSRAM 用量，业务段 + snapshot 长度超过 PSRAM 上限时
 * link-time 直接报错。本片段通过 SLOT POST_DATA 注入，落在所有
 * .psram.* 业务段之后；位置非 PSRAM 物理尾部，但 HAL 仅通过
 * `__pm_snapshot_psram_start/end` 符号读取地址，与物理位置无关。
 *
 * `__pm_snapshot_psram_start/end` 接口保持不变，pm_impl.h 通过
 * PM_SNAPSHOT_PSRAM_BASE/SIZE 宏消费，HAL 零改动。 */

#if CONFIG_MEM_PSRAM_SIZE > 0
    .pm.snapshot (NOLOAD) : ALIGN(16)
    {
        PROVIDE(__pm_snapshot_psram_start = .);
        . = . + CONFIG_PM_SNAPSHOT_PSRAM_SIZE;
        PROVIDE(__pm_snapshot_psram_end = .);
    } >PSRAM
#endif
