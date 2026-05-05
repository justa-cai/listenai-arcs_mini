# Boot Features 描述子设计文档

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-04 | 整理 features descriptor 的内存布局、读写两侧实现、扩展指南 |

---

## 1. 设计目标

App 与 boot 是两个可独立升级的二进制，运行时存在多种新旧组合（新 app
与新 boot、新 app 与旧 boot、旧 app 与新 boot 等）。app 需要一种运行时
机制，在不依赖编译期 macro、不读取 boot 镜像内容、不假设 boot 版本号
的前提下，查询 boot 实际具备的能力，并据此决定使用新路径还是回退到旧
路径。

典型用例见 §power-management.md 中 `sys_arch_reboot` 的判断：当 boot
同时具备 `UBOOT_FEATURE_OTA` 与 `UBOOT_FEATURE_POWER_GUARD` 时，应用
POWER_EN AON 锁定 + CMN reset 路径；否则回退到原有的
`sys_platform_sw_full_reset`。

---

## 2. 整体方案

Boot 在编译期把一份固定结构的描述子放置到 flash 上的固定地址
（`CONFIG_BOOT_FEATURES_ADDR`，默认 0x30000180）。App 通过查询 API 按
该地址读取并校验 magic / version / size，校验通过后取出 features
bitmask；任一项不通过返回 0，由调用方据此降级到老 boot 兼容路径。

```
flash 地址空间 (0x30000000 起)
┌─────────────────────────────────────────────────┐
│ 0x30000000  ListenAI boot header (64 B)         │
│ 0x30000040  .vtable (中断向量表)                 │
│             ...                                  │
│ 0x30000180  ┌─ uboot_features_desc ─────────┐  │  ← CONFIG_BOOT_FEATURES_ADDR
│             │ magic = 'UBOF'                  │  │
│             │ version = 1                     │  │
│             │ size = 32                       │  │
│             │ features = bitmask              │  │
│             │ reserved[4]                     │  │
│             └─────────────────────────────────┘  │
│ 0x300001A0  ...                                  │
└─────────────────────────────────────────────────┘
```

数据格式与具体实现见 [include/uboot_features.h](../include/uboot_features.h)
（共享头）、[src/boot_features.c](../src/boot_features.c)（boot 侧实
例）与 [api/uboot_features_api.c](../api/uboot_features_api.c)（app 侧
查询 API）。

---

## 3. 描述子结构与版本约定

描述子总长 32 字节，包含 magic / version / size / features bitmask 四
个字段以及 16 字节的 reserved。具体定义见
[uboot_features.h](../include/uboot_features.h)。

字段语义：

- **magic** = `'UBOF'`：app 侧首要校验项，区分有效描述子与 boot 镜像
  在该地址的其它内容。
- **version**：当前为 1。结构布局发生不向后兼容的变更（例如非 reserved
  字段含义变化）时必须 bump，并在 app 侧补齐兼容判断。
- **size** = `sizeof(struct uboot_features_desc)`：app 侧也校验，确保
  双方对结构布局的理解一致。
- **features**：32 bit bitmask，当前定义 `UBOOT_FEATURE_OTA` (bit 0)
  与 `UBOOT_FEATURE_POWER_GUARD` (bit 1)。
- **reserved**：16 字节空间，预留给"加字段不需要 bump version"的兼容性
  扩展。

布局变更优先用 features bit；feature bit 集合为前向单调扩展，旧 app
读取含未知 bit 的描述子时只会忽略它，无需结构改动。

---

## 4. Boot 侧实现

[boot_features.c](../src/boot_features.c) 定义一个静态实例放入
`.uboot_features` 段，feature bit 由 `CONFIG_BOOT_OTA_PACKAGE` /
`CONFIG_BOOT_POWER_GUARD` 等编译开关条件拼接，无需独立维护。

链接到固定地址由 [arcs-sdk/soc/common/system.ld:82-91](../../../soc/common/system.ld#L82-L91)
完成：在 `.vtable` 之后显式将 location counter 设为
`CONFIG_BOOT_FEATURES_ADDR`，并通过 `KEEP(*(.uboot_features))` 拉入
boot_features.c 编出的段。设值之前的 `ASSERT` 保证 `.vtable` 不会越界
覆盖描述子地址。

`.vtable` 长度依 IRQ 数浮动，无法依赖自然偏移定位描述子，因此必须显式
跳到固定偏移。该处理只能放在 `system.ld` 的 SECTIONS 主体内，无法迁
移到 SLOT 片段（fragment ld 不支持在 SLOT 层级强制绝对地址，详见
commit `Revert "refactor(linker): move uboot_features section to
fragment ld"`）。

[boot.c](../src/boot.c) 在 `CONFIG_BOOT_FEATURES` 启用时取一次描述子地
址作为锚点，避免 `--gc-sections` 将 boot_features.c 整个 object 视作无
引用裁掉。

---

## 5. App 侧 API

公开接口位于 [include/uboot_features_api.h](../include/uboot_features_api.h)：

- `uint32_t uboot_features_query(void)` —— 返回 features bitmask；任一
  校验项不通过返回 0。
- `bool uboot_features_has(uint32_t feature)` —— 测试单个 bit 是否置位。

实现要点见 [api/uboot_features_api.c](../api/uboot_features_api.c)：

- magic / version / size 三重校验，任一不通过则返回 0。
- 描述子内容在 flash 中只读不变，第一次查询后缓存到 BSS，hot path 调
  用无 flash 访问开销。
- 解析描述子地址的函数声明为 `weak`，便于 host 测试 mock。

### 5.1 现有调用点

| 调用点 | 决策 |
|---|---|
| [power_manager.c:46](../../../../src/middleware/power/power_manager.c#L46) `power_gpio_init` | POWER_GUARD 启用时 POWER_EN GPIO 用 OUTPUT_HIGH 接管已 latch；否则 OUTPUT_LOW（兼容旧 boot 启动后由 app 拉起 latch 的语义） |
| [power_manager.c:123](../../../../src/middleware/power/power_manager.c#L123) `power_shutdown` | USB 插入且 POWER_GUARD 启用时走 `uboot_shutdown_request` 的 stage0 假关机；否则降级直接拉低 PWR_LOCK |
| [sys_reboot.c:13](../../../soc/arcs/common/sys_reboot.c#L13) `sys_arch_reboot` | OTA + POWER_GUARD 都启用时启用 POWER_EN AON force-output + CMN reset；否则按原 switch 分支处理 |

---

## 6. 扩展指南

### 6.1 新增 feature bit（推荐路径）

在 [uboot_features.h](../include/uboot_features.h) 增加新的 bit：

```c
#define UBOOT_FEATURE_NEW_THING   (1u << 2)
```

在 [boot_features.c](../src/boot_features.c) 的 `features` 拼接里按编
译开关条件加入：

```c
.features =
    ...
#ifdef CONFIG_BOOT_NEW_THING
    UBOOT_FEATURE_NEW_THING |
#endif
    0u,
```

App 侧调用 `uboot_features_has` 判断分支：

```c
if (uboot_features_has(UBOOT_FEATURE_NEW_THING)) {
    new_path();
} else {
    legacy_path();
}
```

不需要 bump `UBOOT_FEATURES_VERSION`：旧 app 读取含未知 bit 的描述子
时仅忽略该位。

### 6.2 修改描述子结构

非必要不修改。如需新增非 bit 字段：

- 优先占用 reserved 空间，结构总大小不变，亦不需要 bump version。app
  侧若使用新字段，需要自行约定无效值（典型为 0 或 0xFFFFFFFF 表示旧
  boot 未填）。
- 若 reserved 不足，必须 bump `UBOOT_FEATURES_VERSION` 并放宽 app 侧
  的 size 校验逻辑（按实际 size 决定可用字段范围）。该路径破坏强校验
  语义，只在不可避免时使用。

---

## 7. Kconfig

| 选项 | 作用 |
|---|---|
| `BOOT_FEATURES` | Boot 侧编入描述子。与 BOOT 总开关解耦，standalone boot sample 也可启用。 |
| `BOOT_FEATURES_API` | App 侧启用查询 API。 |
| `BOOT_FEATURES_ADDR` | 描述子的 flash 绝对地址。boot 镜像、app 镜像、SoC linker 三方必须保持一致；默认 0x30000180。 |

---

## 8. 实施约束

1. **三方地址必须一致**：boot 镜像编译、app 镜像编译、SoC linker 都使
   用 `CONFIG_BOOT_FEATURES_ADDR`，修改时三处需同步。
2. **`.vtable` 长度不得超过描述子地址**：linker 的 `ASSERT` 会在编译
   期检查。IRQ 表过大时需要调高 `CONFIG_BOOT_FEATURES_ADDR`。
3. **缓存 scope 是单次启动**：缓存在 BSS，每次复位清零，因此 boot 升
   级后第一次起来的 app 即可读到新 features。
4. **不应作为通用 boot/app IPC 通道**：描述子是只读的能力声明，跨阶段
   的可变状态使用 `boot_info` (AON RSVD4) 或 flash NVS。
5. **不应在 ISR / 早期 startup 中调用**：首次查询会读 flash，须在
   PSRAM / cache 可用之后调用。`power_init` 这一时机是安全的。

---

## 9. 相关源码索引

| 文件 | 作用 |
|---|---|
| [include/uboot_features.h](../include/uboot_features.h) | 描述子结构、magic、feature bit |
| [include/uboot_features_api.h](../include/uboot_features_api.h) | App 侧 API 头 |
| [api/uboot_features_api.c](../api/uboot_features_api.c) | App 侧实现（带缓存与校验） |
| [src/boot_features.c](../src/boot_features.c) | Boot 侧描述子实例 |
| [arcs-sdk/soc/common/system.ld](../../../soc/common/system.ld) | 将 `.uboot_features` 段钉到 `CONFIG_BOOT_FEATURES_ADDR` |
| [src/boot.c](../src/boot.c) | 描述子地址锚点（防 gc-sections） |
| [Kconfig.public](../Kconfig.public) | BOOT_FEATURES / BOOT_FEATURES_API / BOOT_FEATURES_ADDR |
