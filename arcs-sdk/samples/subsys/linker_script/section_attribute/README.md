# C 段属性宏示例

## 功能说明

演示如何在 C 代码中使用 SDK 提供的段属性宏（`_FAST_TEXT`、`__psram_bss__`、`__noinit__` 等），将函数和变量放入指定内存区域，无需修改链接脚本或 CMakeLists.txt。

## 硬件连接

无需外部连接，本示例仅演示链接脚本特性。

## 示例内容

1. 使用 `_FAST_TEXT` 将 CRC 计算函数放入 SRAM 快速执行
2. 使用 `_FAST_DATA` 将查找表放入 SRAM 快速访问
3. 使用 `__psram_bss__` 将大缓冲区放入 PSRAM 节省 SRAM
4. 使用 `__noinit__` 标记复位计数器，软复位后保留值
5. 使用 `__psram_data__` 将已初始化常量放入 PSRAM

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```text
[I][section] === Section Attribute Demo ===
[I][section] _FAST_TEXT   fast_crc=0x20010000, result=0xxxxxxxxx
[I][section] _FAST_DATA   lookup_table=0x20011394, [0]=0x00
[I][section] __psram_bss__  frame_buffer=0x28000014, size=32768
[I][section] __noinit__   boot_count=xxx (retained across soft reset)
[I][section] __psram_data__ welcome_msg=0x28000000, "Hello from PSRAM!"
```

地址说明：`0x2001xxxx` 为 SRAM，`0x2800xxxx` 为 PSRAM 数据区。

## 核心 API

| 宏 | 定义位置 | 目标段 | 适用场景 |
|-----|---------|--------|---------|
| `_FAST_TEXT` | `arcs_ap.h` | `.fast_text`（SRAM） | 中断处理、音频回调 |
| `_FAST_DATA` | `arcs_ap.h` | `.fast_data`（SRAM） | 小型查找表 |
| `_FAST_BSS` | `arcs_ap.h` | `.fast_bss`（SRAM） | SRAM 零初始化数据 |
| `__psram_bss__` | `sysutils.h` | `.psram.bss`（PSRAM） | 大缓冲区 |
| `__psram_data__` | `sysutils.h` | `.psram.data`（PSRAM） | 大型常量表 |
| `__psram_code__` | `sysutils.h` | `.psram.text`（PSRAM） | PSRAM 中执行的代码 |
| `__noinit__` | `sysutils.h` | `.noinit` | 复位保留数据 |
| `__itcm_text__` | `sysutils.h` | `.itcm.text`（ITCM） | 极低延迟代码 |
| `__dtcm_data__` | `sysutils.h` | `.dtcm.data`（DTCM） | 极低延迟数据 |

## 关键代码

```c
#include <arcs_ap.h>
#include <sysutils.h>

/* SRAM 快速函数：中断处理等对延迟敏感的场景 */
_FAST_TEXT
static uint32_t fast_crc(const uint8_t *data, size_t len)
{
    /* CRC 计算... */
}

/* PSRAM 大缓冲区：节省 SRAM 空间 */
__psram_bss__ static uint8_t frame_buffer[32 * 1024];

/* 复位保留：软复位后值不丢失 */
__noinit__ static uint32_t boot_count;
```

## 注意事项

1. **头文件引入**：`_FAST_TEXT` 等宏定义在 `arcs_ap.h`，`__psram_bss__` 等定义在 `sysutils.h`，使用前需包含对应头文件
2. **PSRAM 依赖**：`__psram_*` 系列宏需要硬件支持 PSRAM 且 `CONFIG_MEM_PSRAM_SIZE > 0`
3. **noinit 初始值**：`__noinit__` 变量首次上电为随机值，应用中需做有效性判断
4. **SRAM 容量**：`_FAST_TEXT` / `_FAST_DATA` 会占用 SRAM，仅对性能关键路径使用
