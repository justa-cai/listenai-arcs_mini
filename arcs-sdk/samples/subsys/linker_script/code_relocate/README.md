# 代码重定位示例

## 功能说明

演示如何使用 `listenai_code_relocate()` CMake API 将代码从 Flash 重定位到 SRAM 执行，降低中断响应延迟。支持按文件、按段名、按库三种重定位方式。

## 硬件连接

无需外部连接，本示例仅演示链接脚本特性。

## 示例内容

1. 在 CMakeLists.txt 中声明按段名重定位：将 `.text.irq_handler` 段搬入 SRAM
2. 在 C 代码中使用 `__attribute__((section()))` 将函数放入指定段
3. 运行时打印函数地址，对比 Flash 函数和 SRAM 函数的地址差异

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
[I][relocate] === Code Relocate Demo ===
[I][relocate] normal_function addr: 0x3000xxxx (PSRAM/Flash)
[I][relocate] simulated_irq_handler addr: 0x2001xxxx (SRAM)
[I][relocate] IRQ handler executed successfully from SRAM
```

普通函数地址在 `0x3000xxxx` 范围（默认 `.text` 段），重定位后的函数地址在 `0x2001xxxx` 范围（SRAM）。

## 关键代码

CMakeLists.txt 中声明重定位（无需修改 C 代码或链接脚本）：

```cmake
# 按段名重定位
listenai_code_relocate(SECTIONS .text.irq_handler LOCATION SRAM_TEXT)

# 按文件重定位（整个 .c 文件搬入 SRAM）
listenai_code_relocate(FILES fast_math.c LOCATION SRAM)

# 按库重定位（整个第三方库搬入 PSRAM）
listenai_code_relocate(LIBRARY modules_lvgl LOCATION PSRAM)
```

C 代码中将函数放入对应段：

```c
/* 将函数放入 .text.irq_handler 段，CMake 会将此段重定位到 SRAM */
__attribute__((section(".text.irq_handler")))
void simulated_irq_handler(void)
{
    static volatile uint32_t irq_count = 0;
    irq_count++;
}
```

## 验证方法

编译后通过符号表确认函数地址：

```bash
grep simulated_irq build/arcs.lst
```

## 注意事项

1. **按文件重定位**：`FILES` 模式会将源文件的所有段（`.text`、`.rodata`、`.data`、`.bss`）都搬入目标内存
2. **按段名重定位**：`SECTIONS` 模式最精细，只搬指定段名的内容
3. **按库重定位**：`LIBRARY` 参数使用 CMake target 名称（如 `modules_lvgl`），不是目录名
4. **SRAM 空间有限**：重定位会占用目标内存空间，注意避免溢出
