# 自定义段注入示例

## 功能说明

演示如何通过 `listenai_add_linker_section()` 注入自定义链接器段，并通过 `listenai_add_linker_scatter()` 注册启动时的数据复制。实现从 ROM 加载、在 RAM 中运行的自定义数据段。

## 硬件连接

无需外部连接，本示例仅演示链接脚本特性。

## 示例内容

1. 编写 `.ld` 片段文件定义 `.app_config` 段（`>RAM AT>ROM`）
2. 在 CMakeLists.txt 中注册段到 ROM slot
3. 注册散加载表条目，启动代码自动将数据从 ROM 复制到 RAM
4. 在 C 代码中将配置项放入该段，运行时通过链接器符号遍历

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
[I][section] === Custom Section Demo ===
[I][section]   config[0]: baud_rate = 115200
[I][section]   config[1]: retry_count = 3
[I][section]   config[2]: timeout_ms = 5000
[I][section] Total config entries: 3
```

## 关键代码

### .ld 片段文件

```c
/* linker/app-config-sections.ld */
    .app_config : ALIGN(4) {
        . = ALIGN(4);
        __app_config_start = .;
        KEEP(*(.app_config))
        KEEP(*(.app_config.*))
        . = ALIGN(4);
        __app_config_end = .;
    } >RAM AT>ROM
```

### CMakeLists.txt 注册

```cmake
# 注入段定义到 ROM slot
listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/linker/app-config-sections.ld
    SLOT ROM
    SORT_KEY "50-app"
)

# 注册散加载：启动时从 ROM 复制到 RAM
listenai_add_linker_scatter(SCATLOAD .app_config)
```

### C 代码使用

```c
/* 链接器导出的段起止符号 */
extern const app_config_entry_t __app_config_start[];
extern const app_config_entry_t __app_config_end[];

/* 将数据放入自定义段 */
#define APP_CONFIG_DEFINE(cfg_key, cfg_value)                              \
    static const app_config_entry_t __app_cfg_##cfg_key                   \
        __attribute__((used, section(".app_config." #cfg_key))) = {       \
            .key   = #cfg_key,                                            \
            .value = (cfg_value),                                         \
    }

APP_CONFIG_DEFINE(baud_rate,   115200);
APP_CONFIG_DEFINE(timeout_ms,  5000);

/* 运行时遍历 */
for (entry = __app_config_start; entry < __app_config_end; entry++) {
    LOGI("  %s = %ld", entry->key, (long)entry->value);
}
```

## 验证方法

```bash
# 确认最终链接脚本包含该段和散加载条目
grep app_config build/linker.ld
```

## 注意事项

1. **KEEP() 必需**：自定义段中的符号可能未被直接引用，必须用 `KEEP()` 防止链接器垃圾回收
2. **SCATLOAD vs SCATZERO**：`SCATLOAD` 用于从 ROM 复制到 RAM 的段，`SCATZERO` 用于需要零填充的 BSS 段
3. **SLOT 选择**：段定义必须放在正确的 SLOT 中，确保在链接脚本中的位置合理
4. **__attribute__((used))**：配合 `KEEP()` 使用，防止编译器优化掉未直接引用的变量
