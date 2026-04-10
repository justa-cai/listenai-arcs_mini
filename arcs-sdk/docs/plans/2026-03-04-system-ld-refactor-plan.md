# system.ld 标准化重构实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor system.ld into a standardized skeleton with externalized component sections and Kconfig-driven memory layout, eliminating memap.h.

**Architecture:** system.ld becomes a slim skeleton using CONFIG_MEM_xxx directly for MEMORY{}, with `#include` insertion points for component-provided .ld fragments. CMake macro `listenai_add_linker_section()` collects fragments at configure time and generates combined files in `${CMAKE_BINARY_DIR}/generated/`. Scatter table entries are also auto-generated.

**Tech Stack:** GNU ld linker scripts, C preprocessor, CMake, Kconfig

**Design doc:** `docs/plans/2026-03-04-system-ld-refactor-design.md`

**Strategy:** 渐进式实施，每个 Phase 结束后用 helloworld 编译验证。先保证最小变更可用，再逐步外迁 section。

---

## Key Files Reference

| File | Role |
|------|------|
| `startup/arcs/system.ld` | Main linker script (refactoring target, 656 lines) |
| `startup/arcs/memmap/memap.h` | Memory map header (to be deleted) |
| `startup/arcs/Kconfig.mem` | Memory Kconfig (to be moved to soc/) |
| `startup/arcs/Kconfig` | Startup Kconfig (rsources Kconfig.mem at L71) |
| `startup/arcs/CMakeLists.txt` | Calls `listenai_set_linker_script()` at L38, includes memmap at L33-35 |
| `soc/arcs/Kconfig` | SoC Kconfig (7 lines, to receive memory configs) |
| `cmake/extensions.cmake` | Build macros, `listenai_set_linker_script` at L382-427 |
| `samples/helloworld/` | Verification target (minimal: lisa_log + FreeRTOS only) |

## Technical Notes

- system.ld is preprocessed: `gcc -E -P -x assembler-with-cpp -include autoconf.h`
- All `CONFIG_xxx` symbols are available via autoconf.h during preprocessing
- Use `#include "generated/sections_xxx.ld"` (C preprocessor, not linker INCLUDE)
- SCATLOAD/SCATZERO macros are `#define`d in system.ld, so `#include`d fragments can use them
- helloworld 依赖的 section: `.init`, `.scatab`, `.text`, `.rodata`, `.data`, `.bss`, `.sys_init`, `.lisa_device_registry`, `.mapi`, `.heapstack`, `.itcm`, `.dtcm`, `.fast.*`
- helloworld 不依赖的 section: `.wifi.*`, `.ipc.*`, `.bt_heap`, `.shell*`, `.mcp_tool`, `.lisaui_apps`, `.luna.*`, `.trace`, `.extflash`

---

## Phase 1: MEMORY 块改造 — 最小可验证变更

> 目标: 仅改 MEMORY{} 块使用 CONFIG_MEM_xxx，其余不动。helloworld 编译通过。

### Task 1: Move memory Kconfig from startup to soc

**Files:**
- Modify: `soc/arcs/Kconfig`
- Modify: `startup/arcs/Kconfig` (L71)
- Delete: `startup/arcs/Kconfig.mem`

**Step 1: Insert Kconfig.mem content into soc/arcs/Kconfig**

Replace `soc/arcs/Kconfig` with the following (keep existing `rsource "arcs/hal/Kconfig"` at the end):

```kconfig
config SOC_ARCS
    bool

if SOC_ARCS

config MEM_CONFIG
    bool
    default y

menu "Memory Layout"

config MEM_INTERRUPT_STACK_SIZE
    hex "interrupt stack size"
    default 0x1000

menu "SRAM"
config MEM_SRAM_BASE
    hex "base address"
    default 0x20010000

config MEM_SRAM_SIZE
    hex "size"
    default 0x00040000
endmenu

menu "PSRAM"
config MEM_CMN_SRAM_BASE
    hex "common SRAM base address"
    default 0x20040000

config MEM_CMN_SRAM_SIZE
    hex "common SRAM size"
    default 0x00010000

config MEM_PSRAM_BASE
    hex "base address"
    default 0x28000000

config MEM_PSRAM_SIZE
    hex "size"
    default 0x00800000
endmenu

menu "Flash"
config MEM_FLASH_BASE
    hex "base address"
    default 0x30000000

config MEM_FLASH_SIZE
    hex "size"
    default 0x00800000
endmenu

menu "Local Memory (TCM)"
config MEM_ILM_BASE
    hex "ILM base address"
    default 0x00080000 if ARCS_AP_CORE
    default 0x00280000 if ARCS_CP_CORE

config MEM_ILM_SIZE
    hex "ILM size"
    default 0x00004000

config MEM_DLM_BASE
    hex "DLM base address"
    default 0x00100000 if ARCS_AP_CORE
    default 0x00300000 if ARCS_CP_CORE

config MEM_DLM_SIZE
    hex "DLM size"
    default 0x00002000
endmenu

menu "LUNA RAM"
config MEM_LUNA_BASE
    hex "base address"
    default 0x200B0000

config MEM_LUNA_SIZE
    hex "size"
    default 0x00006000
endmenu

menu "Bluetooth RAM"
config MEM_BTRAM_BASE
    hex "base address"
    default 0x200C0000

config MEM_BTRAM_SIZE
    hex "size"
    default 0x00008000
endmenu

menu "IPC Shared Memory"
config MEM_IPC_ISOLATED
    bool "Isolated IPC RAM region"
    default y

config MEM_IPC_BASE
    hex "base address"
    default 0x20050000
    depends on MEM_IPC_ISOLATED

config MEM_IPC_SIZE
    hex "size"
    default 0x00002000
    depends on MEM_IPC_ISOLATED
endmenu

menu "WiFi Memory"
config MEM_WFRAM_BASE
    hex "WiFi RAM base address"
    default 0x20000080
    help
        First 128 bytes reserved for WiFi hardware

config MEM_WFRAM_SIZE
    hex "WiFi RAM size"
    default 0x00010000

config MEM_WIFI_CALIBRATION_SIZE
    hex "WiFi calibration data size"
    default 0x4000 if ARCS_AP_CORE
    default 0

config MEM_WIFI_TRACE_SIZE
    hex "WiFi trace data size"
    default 0x2000 if ARCS_AP_CORE
    default 0
endmenu

endmenu # Memory Layout

rsource "arcs/hal/Kconfig"

endif # SOC_ARCS
```

**Step 2: Remove rsource from startup/arcs/Kconfig**

Delete line 71 (`rsource "Kconfig.mem"`) from `startup/arcs/Kconfig`.

**Step 3: Delete Kconfig.mem**

```bash
git rm startup/arcs/Kconfig.mem
```

**Step 4: Commit**

```
refactor(soc): move memory Kconfig from startup to soc/arcs/Kconfig
```

---

### Task 2: Refactor system.ld MEMORY block to use CONFIG_MEM_xxx

**Files:**
- Modify: `startup/arcs/system.ld` (lines 36-59 only, minimal change)
- Modify: `startup/arcs/CMakeLists.txt` (remove memmap include)

**Step 1: Replace memap.h include and MEMORY block**

In `startup/arcs/system.ld`, replace lines 36-59:

```c
/* OLD: #include "memap.h" */
/* Memory layout driven by Kconfig (CONFIG_MEM_xxx via autoconf.h) */
MEMORY {
    PSRAM(rwxa)  : ORIGIN = CONFIG_MEM_PSRAM_BASE, LENGTH = CONFIG_MEM_PSRAM_SIZE
#if CONFIG_BOOT
    FLASH(rxa!w) : ORIGIN = (CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE), LENGTH = (CONFIG_MEM_FLASH_SIZE - CONFIG_BOOT_FLASH_SIZE)
#else
    FLASH(rxa!w) : ORIGIN = CONFIG_MEM_FLASH_BASE, LENGTH = CONFIG_MEM_FLASH_SIZE
#endif
    SRAM(rwxa)   : ORIGIN = CONFIG_MEM_SRAM_BASE, LENGTH = CONFIG_MEM_SRAM_SIZE
    ILM(rwx)     : ORIGIN = CONFIG_MEM_ILM_BASE, LENGTH = CONFIG_MEM_ILM_SIZE
    DLM(rw)      : ORIGIN = CONFIG_MEM_DLM_BASE, LENGTH = CONFIG_MEM_DLM_SIZE
    BTRAM(rwxa)  : ORIGIN = CONFIG_MEM_BTRAM_BASE, LENGTH = CONFIG_MEM_BTRAM_SIZE
    LUNA_RAM(rwxa) : ORIGIN = CONFIG_MEM_LUNA_BASE, LENGTH = CONFIG_MEM_LUNA_SIZE

    WIFI_RAM(rwx) : ORIGIN = CONFIG_MEM_WFRAM_BASE, LENGTH = CONFIG_MEM_WFRAM_SIZE

#if CONFIG_MEM_IPC_ISOLATED
    IPC_RAM(rwx) : ORIGIN = CONFIG_MEM_IPC_BASE, LENGTH = CONFIG_MEM_IPC_SIZE
#endif
}
```

**Step 2: Replace MEM_xxx references in SECTIONS**

In the same file, replace remaining memap.h macro references:
- L88: `MEM_INTERRUPT_STACK_SIZE` → `CONFIG_MEM_INTERRUPT_STACK_SIZE`
- L93: `MEM_WIFI_CALIBRATION_SIZE` → `CONFIG_MEM_WIFI_CALIBRATION_SIZE`
- L95: `MEM_WIFI_TRACE_SIZE` → `CONFIG_MEM_WIFI_TRACE_SIZE`

Also remove the `MEM_FLASH_EX_BASE` conditionals (lines 47-48, 69-71, 429-437) or replace with a Kconfig equivalent if needed. For now, since `FLASH_EX` is not used in helloworld, wrap with a safe guard:

```c
/* Lines 47-48, 69-71: remove FLASH_EX MEMORY entries */
/* Lines 429-437: remove .extflash section (to be externalized later) */
```

**Step 3: Remove memmap include from CMakeLists.txt**

In `startup/arcs/CMakeLists.txt`, delete lines 33-35:
```cmake
if (NOT DEFINED CONFIG_MEM_CONFIG_USE_CUSTOM_FILE)
    listenai_include_directories(memmap)
endif()
```

**Step 4: Build helloworld to verify**

```bash
./build.sh -C -S samples/helloworld -DBOARD=arcs_evb
```

Expected: Build succeeds. MEMORY block resolves to same numeric addresses as before.

**Step 5: Commit**

```
refactor(linker): system.ld MEMORY block uses CONFIG_MEM_xxx directly

Remove memap.h dependency. Memory addresses are now resolved from
Kconfig symbols via autoconf.h during linker script preprocessing.
```

---

## Phase 2: CMake 基础设施 + INCLUDE 插入点

> 目标: 添加 CMake 宏和空 INCLUDE 插入点，但不移除任何 section。helloworld 编译通过。

### Task 3: Add CMake linker section collection infrastructure

**Files:**
- Modify: `cmake/extensions.cmake`

**Step 1: Add `listenai_add_linker_section()` macro**

Insert before `listenai_set_linker_script` (around line 380 in `cmake/extensions.cmake`):

```cmake
# ---------------------------------------------------------------------------
# Linker section fragment collection
# ---------------------------------------------------------------------------
# Register a linker script fragment for automatic collection.
# Usage:
#   listenai_add_linker_section(
#       FILE ${CMAKE_CURRENT_SOURCE_DIR}/sections.ld
#       REGION ROM|RAM|NOLOAD|PSRAM|LUNA
#       [SCATLOAD SCATLOAD_SECTION <section_name>]
#       [SCATZERO SCATZERO_SECTION <section_name>]
#   )
macro(listenai_add_linker_section)
    cmake_parse_arguments(LINKER_SEC
        "SCATLOAD;SCATZERO"
        "FILE;REGION;SCATLOAD_SECTION;SCATZERO_SECTION"
        ""
        ${ARGN}
    )

    if(NOT LINKER_SEC_FILE)
        message(FATAL_ERROR "listenai_add_linker_section: FILE is required")
    endif()
    if(NOT LINKER_SEC_REGION)
        message(FATAL_ERROR "listenai_add_linker_section: REGION is required")
    endif()

    string(TOLOWER "${LINKER_SEC_REGION}" _region_lower)
    set_property(GLOBAL APPEND PROPERTY LISTENAI_LINKER_SECTIONS_${_region_lower} "${LINKER_SEC_FILE}")

    if(LINKER_SEC_SCATLOAD AND LINKER_SEC_SCATLOAD_SECTION)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATLOAD_ENTRIES "${LINKER_SEC_SCATLOAD_SECTION}")
    endif()
    if(LINKER_SEC_SCATZERO AND LINKER_SEC_SCATZERO_SECTION)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATZERO_ENTRIES "${LINKER_SEC_SCATZERO_SECTION}")
    endif()
endmacro()

# Generate combined linker section files from collected fragments.
# Called internally by listenai_set_linker_script().
function(listenai_generate_linker_sections)
    set(_gen_dir "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_gen_dir}")

    foreach(_region rom ram noload psram luna)
        get_property(_files GLOBAL PROPERTY LISTENAI_LINKER_SECTIONS_${_region})
        set(_output "${_gen_dir}/sections_${_region}.ld")
        if(_files)
            set(_content "/* Auto-generated: ${_region} linker sections */\n")
            foreach(_file ${_files})
                file(READ "${_file}" _file_content)
                string(APPEND _content "\n/* From: ${_file} */\n${_file_content}\n")
            endforeach()
            file(WRITE "${_output}" "${_content}")
        else()
            file(WRITE "${_output}" "/* No custom sections for ${_region} */\n")
        endif()
    endforeach()

    # Scatter copy table entries
    get_property(_scatload GLOBAL PROPERTY LISTENAI_SCATLOAD_ENTRIES)
    set(_content "/* Auto-generated scatter copy entries */\n")
    foreach(_s ${_scatload})
        string(APPEND _content "    SCATLOAD(${_s})\n")
    endforeach()
    file(WRITE "${_gen_dir}/scatload_entries.ld" "${_content}")

    # Scatter zero table entries
    get_property(_scatzero GLOBAL PROPERTY LISTENAI_SCATZERO_ENTRIES)
    set(_content "/* Auto-generated scatter zero entries */\n")
    foreach(_s ${_scatzero})
        string(APPEND _content "    SCATZERO(${_s})\n")
    endforeach()
    file(WRITE "${_gen_dir}/scatzero_entries.ld" "${_content}")
endfunction()
```

**Step 2: Update `listenai_set_linker_script()`**

At the beginning of the macro (line 382), add generation call. Also ensure `${CMAKE_BINARY_DIR}` is in the preprocessor include path:

```cmake
macro(listenai_set_linker_script linker_script)
    # Generate collected linker section fragments
    listenai_generate_linker_sections()

    # ... existing code ...
    # After the INCLUDE_FLAGS loop, add:
    list(APPEND INCLUDE_FLAGS "-I${CMAKE_BINARY_DIR}")

    # ... rest unchanged ...
endmacro()
```

**Step 3: Build helloworld to verify (no functional change yet)**

```bash
./build.sh -C -S samples/helloworld -DBOARD=arcs_evb
```

Check that `build/generated/` directory is created with empty .ld files.

**Step 4: Commit**

```
feat(build): add listenai_add_linker_section CMake macro

Components can register .ld fragments that are auto-collected at
configure time into generated/sections_{region}.ld files.
```

---

### Task 4: Add INCLUDE insertion points to system.ld (no section removal)

**Files:**
- Modify: `startup/arcs/system.ld`

**Step 1: Add `#include` directives at strategic positions**

Add the following `#include` lines in system.ld. Since the generated files are currently empty, this is a no-op:

1. **Before `.wifi.noinit`** (around line 99): add NOLOAD insertion point
```c
    /* === External NOLOAD sections === */
    #include "generated/sections_noload.ld"

    /* 优先将这个段放在前面... (existing .wifi.noinit) */
```

2. **After `.psram.noinit`** (around line 214): add PSRAM insertion point
```c
    } >PSRAM

    /* === External PSRAM sections === */
    #include "generated/sections_psram.ld"
```

3. **After `.luna.bss`** (around line 237): add LUNA insertion point
```c
    } >LUNA

    /* === External LUNA sections === */
    #include "generated/sections_luna.ld"
```

4. **Before `.text`** (around line 319): add ROM insertion point
```c
    /* === External ROM sections === */
    #include "generated/sections_rom.ld"

    .text : ALIGN(4) {
```

5. **Before `.mapi`** (around line 531): add RAM insertion point
```c
    /* === External RAM sections === */
    #include "generated/sections_ram.ld"

    .mapi : ALIGN(4) {
```

6. **Inside `.scatab`** scatter tables: add generated entries
```c
            SCATLOAD(.luna.data);
            /* === External scatter copy entries === */
            #include "generated/scatload_entries.ld"
        PROVIDE(__scat_copy_last = .);

        ...

            SCATZERO(.luna.bss);
            /* === External scatter zero entries === */
            #include "generated/scatzero_entries.ld"
        PROVIDE(__scat_zero_last = .);
```

**Step 2: Build helloworld to verify**

```bash
./build.sh -C -S samples/helloworld -DBOARD=arcs_evb
```

Expected: Build succeeds. The empty `#include`d files have no effect.

**Step 3: Commit**

```
refactor(linker): add INCLUDE insertion points for external sections

Prepare system.ld for incremental section externalization. Currently
all generated files are empty — no functional change.
```

---

## Phase 3: 逐步外迁 section（从 helloworld 不依赖的开始）

> 目标: 每外迁一个 section 就编译验证一次。先迁移 helloworld 不使用的 section（最安全），再迁移核心 section。

### Task 5: Extract WiFi sections

**Files:**
- Create: WiFi 组件目录下的 `sections.ld` (定位: `components/lisa_wifi/` 或 HAL WiFi 目录)
- Modify: 对应 `CMakeLists.txt`
- Modify: `startup/arcs/system.ld` (删除 `.wifi.noinit`, `.wifi_la_dump`, 以及 scatter table 中不相关的条目)

**Step 1: Create WiFi .ld fragment**

文件内容即 system.ld 中的 `.wifi.noinit` section (L100-125) + `.wifi_la_dump` (L216-222)，将 `MEM_WIFI_CALIBRATION_SIZE` 等替换为 `CONFIG_MEM_WIFI_CALIBRATION_SIZE`（如果 Task 2 未处理）。加上 ASSERT。

**Step 2: Register in CMakeLists.txt**

```cmake
listenai_add_linker_section(FILE ${CMAKE_CURRENT_SOURCE_DIR}/sections.ld REGION NOLOAD)
```

**Step 3: Remove from system.ld**

删除 `.wifi.noinit` (L100-125), ASSERT (L124-125), `.wifi_la_dump` (L216-222)。

**Step 4: Build helloworld + verify**

**Step 5: Commit**

```
refactor(linker): extract WiFi sections to component .ld fragment
```

---

### Task 6: Extract BLE section (.bt_heap)

同模式: 创建 fragment → 注册 → 从 system.ld 删除 → 编译验证 → 提交。

**Fragment 内容:**
```c
#if CONFIG_BLE
.bt_heap (NOLOAD): ALIGN(16) {
    . = ALIGN(16);
    PROVIDE( __ext_ram_start = . );
    . = . + __BT_HEAP_SIZE;
    PROVIDE( __ext_ram_end = . );
} >PSRAM
#endif
```

**归属:** `components/lisa_bluetooth/sections.ld`

---

### Task 7: Extract IPC section (.ipc.shared)

**Fragment 内容:**
```c
#if CONFIG_MEM_IPC_ISOLATED
.ipc.shared (NOLOAD):
{
    _ipcshram = . ;
    *(SHAREDRAM_AMP_IPC_ENV)
    *(SHAREDRAM_AMP_IPC)
    KEEP(*(SORT(.ipc.*)))
    _eipcshram = . ;
    *(.ic_lock_shared_mem)
} > IPC_RAM
#endif
```

**归属:** `components/ipclsf/sections.ld` 或 `soc/arcs/hal/chip/arcs/ipc/sections.ld`

---

### Task 8: Extract Shell sections (.shell, .shell_command)

**Fragment 内容:**
```c
.shell : ALIGN(4) {
    . = ALIGN(4);
    _shell_items_start = .;
    KEEP(* (.shell.user))
    KEEP(* (.shell.priv))
    . = ALIGN(4);
} >ROM AT>ROM

.shell_command : {
    _shell_command_start = .;
    KEEP (*(shellCommand))
    _shell_command_end = .;
} >ROM AT>ROM
```

**归属:** `components/lisa_shell/sections.ld`

**注意:** 同时从 `.scatab` scatter table 中移除 `SCATLOAD(.shell)` (ROM→ROM 实际是 no-op)。

---

### Task 9: Extract .mcp_tool 和 .lisaui_apps

搜索 `.mcp_tool` 和 `.lisaui*` section attribute 的使用位置，确定归属组件，创建 fragment。

如果找不到明确归属组件，可暂时放在 `startup/arcs/sections_misc.ld` 中，后续再细分。

---

### Task 10: Extract LUNA sections (.luna.*)

**Fragment 内容:**
```c
.luna.text : ALIGN(8) {
    . = ALIGN(8);
    *(.sharedmem.text)
} >LUNA AT>ROM

.luna.data : ALIGN(8) {
    . = ALIGN(8);
    *(.sharedmem.data)
} >LUNA AT>ROM

.luna.bss(NOLOAD) : ALIGN(8) {
    . = ALIGN(8);
    *(.sharedmem.bss)
} >LUNA
```

**归属:** 共享内存/双核组件。

**注意:** 同时从 `.scatab` 移除 `SCATLOAD(.luna.text)`, `SCATLOAD(.luna.data)`, `SCATZERO(.luna.bss)` 并改用 CMake 注册 scatter entries。

---

### Task 11: Extract .trace section

**Fragment 内容:**
```c
.trace : ALIGN(4) {
    . = ALIGN(4);
    _trace_start = .;
    KEEP(*(TRACE))
    _trace_end = .;
    . = ALIGN(4);
} >RAM AT>ROM
```

**归属:** `soc/arcs/hal/chip/arcs/TraceRecorder/sections.ld`

---

### Task 12: Extract core sections (.lisa_device_registry, .sys_init, .mapi)

> 这些是 helloworld 依赖的核心 section，需要格外小心验证。

**Step 1: Extract .lisa_device_registry**

```c
.lisa_device_registry : {
    . = ALIGN(4);
    __lisa_device_registry_start = .;
    KEEP (*(.lisa_device_registry))
    KEEP (*(SORT(.lisa_device_registry.*)))
    . = ALIGN(4);
    __lisa_device_registry_end = .;
} >ROM AT>ROM
```

**归属:** `drivers/lisa_device/sections.ld`
**验证:** helloworld 编译 + 检查 `__lisa_device_registry_start/end` 符号存在。

**Step 2: Extract .sys_init**

完整的 5 级初始化 section（约 40 行），移至 `startup/arcs/sections_sys_init.ld`。
**验证:** helloworld 编译 + 检查 `__sys_init_*` 符号。

**Step 3: Extract .mapi**

```c
.mapi : ALIGN(4) {
    . = ALIGN(4);
    PROVIDE_HIDDEN(__mod_stdc = .);
    KEEP(* (SORT(.mod.0.*)))
    PROVIDE_HIDDEN(__mod_rtos = .);
    KEEP(* (SORT(.mod.?.*)))
    PROVIDE_HIDDEN(__mod_ends = .);
    . = ALIGN(4);
} >RAM AT>ROM
```

**归属:** `startup/arcs/sections_mapi.ld`
**Scatter:** 注册 `SCATLOAD .mapi`，同时从 system.ld `.scatab` 中移除硬编码的 `SCATLOAD(.mapi)`。
**验证:** helloworld 编译。

**Step 4: Commit**

```
refactor(linker): extract core sections (device_registry, sys_init, mapi)
```

---

## Phase 4: 清理

### Task 13: Delete memap.h and final cleanup

**Step 1:** 确认 codebase 中无 `#include "memap.h"` 残留引用（system.ld 已在 Task 2 移除）

**Step 2:** 删除 `startup/arcs/memmap/memap.h` 和目录

**Step 3:** 删除 `Kconfig.mem` 中的 `CONFIG_MEM_CONFIG_USE_CUSTOM_FILE`（如果还在）

**Step 4:** 最终验证

```bash
./build.sh -C -S samples/helloworld -DBOARD=arcs_evb
```

检查:
- `build/generated/` 目录包含所有 sections_*.ld 文件
- `build/linker.ld` 中 MEMORY 块地址正确
- 所有外迁 section 通过 `#include` 正确插入
- 无 memap.h 残留

**Step 5: Commit**

```
refactor(linker): delete memap.h, complete system.ld standardization
```

---

## Execution Order and Dependencies

```
Phase 1 — 最小可验证
  Task 1 (Move Kconfig)
    ↓
  Task 2 (MEMORY 块改造) → ✅ helloworld 编译

Phase 2 — INCLUDE 基础设施
  Task 3 (CMake 宏)
    ↓
  Task 4 (INCLUDE 插入点) → ✅ helloworld 编译

Phase 3 — 逐步外迁 section
  Task 5-11 (非核心 section, 可逐个做) → 每步 ✅ helloworld 编译
    ↓
  Task 12 (核心 section: device_registry, sys_init, mapi) → ✅ helloworld 编译

Phase 4 — 清理
  Task 13 (删除 memap.h) → ✅ 最终验证
```

每个 Task 独立提交，失败可单独回滚。
