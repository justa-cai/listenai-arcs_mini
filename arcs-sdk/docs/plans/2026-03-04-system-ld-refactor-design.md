# system.ld 标准化重构设计

## 背景

当前 `startup/arcs/system.ld` 存在以下问题：

1. **memap.h 双模式** — CONFIG_MEM_CONFIG 开启用 Kconfig，关闭则回退硬编码，且各 sample 提供自己的 memap.h 覆盖
2. **所有自定义 section 硬编码** — `.lisa_device_registry`, `.sys_init`, `.mcp_tool`, `.lisaui_apps` 等全部写死在 system.ld
3. **条件编译散乱** — `#if CONFIG_WIFI`, `#if CONFIG_BLE`, `#if defined(MEM_IPC_BASE)` 穿插全文
4. **`listenai_append_linker_script` 未被使用** — 有机制但无组件调用

## 目标

- 废弃 `memap.h`，内存地址/长度统一由 `soc/arcs/Kconfig` 提供
- system.ld 精简为标准骨架（标准 ELF sections + INCLUDE 插入点）
- 各组件自定义 section 外迁到各自目录，通过 CMake 宏注册
- 构建时自动收集并分类生成 linker 片段
- 本次只处理单核场景，双核 sample 的 memap.h 暂保留

## 架构

```
soc/arcs/Kconfig               ← SoC 内存 Kconfig（地址 + 长度）
        ↓ autoconf.h
startup/arcs/system.ld          ← 精简骨架，直接使用 CONFIG_MEM_xxx
        ↓ INCLUDE
${CMAKE_BINARY_DIR}/generated/  ← 构建时自动收集
  sections_rom.ld               ← ROM region 的组件 section
  sections_ram.ld               ← RAM region 的组件 section
  sections_noload.ld            ← NOLOAD 的组件 section
  sections_psram.ld             ← PSRAM region 的组件 section
  scatload_entries.ld           ← scatter copy table entries
  scatzero_entries.ld           ← scatter zero table entries
```

## 详细设计

### 1. Kconfig 内存定义

在 `soc/arcs/Kconfig` 中新增 memory layout menu，提供所有内存区域的 base + size：

```kconfig
if SOC_ARCS

config MEM_CONFIG
    bool
    default y
    help
      Always use Kconfig-based memory configuration.

menu "Memory Layout"

config MEM_SRAM_BASE
    hex "SRAM base address"
    default 0x20050000

config MEM_SRAM_SIZE
    hex "SRAM size"
    default 0x60000

config MEM_PSRAM_BASE
    hex "PSRAM base address"
    default 0x28000000

config MEM_PSRAM_SIZE
    hex "PSRAM size"
    default 0x800000

config MEM_FLASH_BASE
    hex "Flash base address"
    default 0x30000000

config MEM_FLASH_SIZE
    hex "Flash size"
    default 0x800000

config MEM_ILM_BASE
    hex "ILM base address"
    default 0x00080000

config MEM_ILM_SIZE
    hex "ILM size"
    default 0x4000

config MEM_DLM_BASE
    hex "DLM base address"
    default 0x00100000

config MEM_DLM_SIZE
    hex "DLM size"
    default 0x2000

config MEM_BTRAM_BASE
    hex "BTRAM base address"
    default 0x200C0000

config MEM_BTRAM_SIZE
    hex "BTRAM size"
    default 0x6000

config MEM_LUNA_BASE
    hex "LUNA RAM base address"
    default 0x200B0000

config MEM_LUNA_SIZE
    hex "LUNA RAM size"
    default 0x4000

config MEM_INTERRUPT_STACK_SIZE
    hex "Interrupt stack size"
    default 0x1000

# 可选内存区域（WiFi RAM 已固定使用隔离模式，MEM_WFRAM_ISOLATED 已移除）
config MEM_WFRAM_BASE
    hex "WiFi RAM base address"
    default 0x20000080

config MEM_WFRAM_SIZE
    hex "WiFi RAM size"
    default 0x00010000

config MEM_IPC_ISOLATED
    bool "Isolated IPC shared memory region"
    default n

config MEM_IPC_BASE
    hex "IPC shared memory base address"
    default 0x20048000
    depends on MEM_IPC_ISOLATED

config MEM_IPC_SIZE
    hex "IPC shared memory size"
    default 0x2000
    depends on MEM_IPC_ISOLATED

endmenu

endif # SOC_ARCS
```

### 2. system.ld 精简骨架

system.ld 保留：
- `MEMORY {}` 块 — 直接使用 CONFIG_MEM_xxx
- `REGION_ALIAS` — 标准区域别名
- `.init` — 启动向量表和初始化代码
- `.scatab` / `.psram_scatab` — scatter table（包含 INCLUDE 引用生成的 entries）
- `.text` / `.rodata` / `.data` / `.bss` — 标准 ELF sections
- `.itcm` / `.dtcm` / `.fast.*` — TCM sections（SoC 核心功能）
- `.heapstack` — heap/stack 布局
- 各 region 的 `INCLUDE generated/sections_xxx.ld` 插入点
- C++ runtime sections（CONFIG_LINK_CPP_RUNTIME_SECTIONS 守卫）

### 3. CMake 宏

```cmake
# 注册组件的 linker section 片段
# FILE: .ld 片段文件路径
# REGION: ROM | RAM | NOLOAD | PSRAM — 归类到哪个 INCLUDE 点
# SCATLOAD: 是否自动加入 scatter copy table
# SCATZERO: 是否自动加入 scatter zero table
macro(listenai_add_linker_section)
    cmake_parse_arguments(ARG "SCATLOAD;SCATZERO" "FILE;REGION" "" ${ARGN})
    # 追加到对应 REGION 的全局属性
    set_property(GLOBAL APPEND PROPERTY LISTENAI_LINKER_SECTIONS_${ARG_REGION} ${ARG_FILE})
    if(ARG_SCATLOAD)
        # 从 .ld 片段中解析 section 名称，追加到 scatload 列表
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATLOAD_SECTIONS ${ARG_FILE})
    endif()
    if(ARG_SCATZERO)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATZERO_SECTIONS ${ARG_FILE})
    endif()
endmacro()

# 在 listenai_set_linker_script 中调用，生成所有 generated/*.ld 文件
function(listenai_generate_linker_sections)
    foreach(region ROM RAM NOLOAD PSRAM)
        get_property(files GLOBAL PROPERTY LISTENAI_LINKER_SECTIONS_${region})
        set(output "${CMAKE_BINARY_DIR}/generated/sections_${region}.ld")
        if(files)
            # cat all fragment files into one
            add_custom_command(
                OUTPUT ${output}
                COMMAND cat ${files} > ${output}
                DEPENDS ${files}
            )
        else()
            # create empty file
            file(WRITE ${output} "/* No custom sections for ${region} */\n")
        endif()
    endforeach()
    # Generate scatload/scatzero entries similarly
endfunction()
```

### 4. 外迁的 section 及其归属

| Section | 归属组件 | Region | 备注 |
|---------|---------|--------|------|
| `.lisa_device_registry` | `components/lisa_device/` 或 `startup/arcs/` | ROM | 设备自注册框架 |
| `.sys_init` | `startup/arcs/` | ROM | 多级初始化框架 |
| `.mcp_tool` | 对应 MCP 组件 | ROM | MCP 工具注册 |
| `.lisaui_apps` | `components/lisaui/` | ROM | UI app 注册 |
| `.shell` / `.shell_command` | `components/shell/` | ROM | Shell 命令注册 |
| `.wifi.noinit` | WiFi 组件 | NOLOAD | WiFi 专用内存 |
| `.ipc.shared` | IPC 组件 | NOLOAD | 共享内存 |
| `.bt_heap` | BLE 组件 | NOLOAD (PSRAM) | BT heap |
| `.mapi` | 模块系统 | RAM | 模块注册表 |
| `.trace` | trace 组件 | RAM | 跟踪数据 |
| `.luna.*` | luna 组件 | LUNA region | 共享内存段 |
| `.psram.*` | PSRAM 管理 | PSRAM | PSRAM 段（保留在 system.ld 或独立） |
| `.extflash` | 外部 Flash | ROM_EX | 可选 |

### 5. 不变式

- 最终生成的 linker.ld 功能与现有完全等价
- 编译产物 bin 不变（可通过 objdump 对比验证）
- 所有现有 sample 仍可正常编译
- scatter table 生成结果与手写等价

## 范围限制

- 仅处理单核标准场景
- 双核 sample 的 memap.h 暂保留
- C++ runtime sections 保留在 system.ld（CONFIG 守卫）
