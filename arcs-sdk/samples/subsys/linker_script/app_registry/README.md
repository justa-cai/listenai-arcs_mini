# 编译时注册表模式示例

## 功能说明

演示基于自定义链接器段实现"编译时注册、运行时遍历"的模块注册表模式。各模块在自己的源文件中使用宏注册，无需修改 `main.c`。该模式与 SDK 中 `LISA_DEVICE_REGISTER` / `SYS_INIT` 的实现原理相同。

## 硬件连接

无需外部连接，本示例仅演示链接脚本特性。

## 示例内容

1. 定义 `.app_registry` 只读段（`.ld` 片段 + CMake 注册）
2. 定义 `APP_MODULE_REGISTER()` 宏将注册条目放入该段
3. LED 和 Sensor 模块各自在独立的 `.c` 文件中注册
4. `main()` 通过链接器符号遍历所有注册模块并调用初始化函数

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
[I][registry] === App Registry Demo ===
[I][registry] Init module[0]: led
[I][led] LED module initialized
[I][registry] Init module[1]: sensor
[I][sensor] Sensor module initialized
[I][registry] All 2 modules initialized
```

模块按名称字典序排列（链接器 `SORT()` 保证）。

## 关键代码

### 注册宏定义（app_registry.h）

```c
typedef struct {
    const char *name;
    int (*init)(void);
} app_module_entry_t;

extern const app_module_entry_t __app_registry_start[];
extern const app_module_entry_t __app_registry_end[];

#define APP_MODULE_REGISTER(mod_name, init_fn)                            \
    static const app_module_entry_t __app_module_##mod_name               \
        __attribute__((used, section(".app_registry." #mod_name))) = {    \
            .name = #mod_name,                                            \
            .init = (init_fn),                                            \
    }
```

### 模块注册（独立 .c 文件）

```c
/* module_led.c — 无需修改 main.c */
#include "app_registry.h"

static int led_init(void)
{
    LOGI("LED module initialized");
    return 0;
}

APP_MODULE_REGISTER(led, led_init);
```

### 运行时遍历（main.c）

```c
for (entry = __app_registry_start; entry < __app_registry_end; entry++) {
    entry->init();
}
```

## 注意事项

1. **添加新模块**：只需新建 `.c` 文件 + `APP_MODULE_REGISTER()` + 加入 CMakeLists.txt，无需修改 `main.c`
2. **排序保证**：链接器 `SORT(.app_registry.*)` 按段名字典序排列，模块名决定初始化顺序
3. **只读段**：注册表放在 ROM（`>ROM AT>ROM`），无需散加载，节省 RAM
4. **SDK 同类机制**：`LISA_DEVICE_REGISTER` 使用 `.lisa_device_registry` 段，`SYS_INIT` 使用 `.sys_init` 段，原理完全相同
