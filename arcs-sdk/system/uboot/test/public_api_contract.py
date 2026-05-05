#!/usr/bin/env python3
"""Public API contract checks for uboot API/boot-image decoupling."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]


def read(rel_path: str) -> str:
    return (ROOT / rel_path).read_text(encoding="utf-8")


def assert_contains(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def assert_not_contains(text: str, needle: str, message: str) -> None:
    if needle in text:
        raise AssertionError(message)


def get_config_block(content: str, name: str) -> str:
    lines = content.splitlines()
    start = None

    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped in {f"config {name}", f"menuconfig {name}"}:
            start = index
            break

    if start is None:
        raise AssertionError(f"缺少配置块 {name}")

    for index in range(start + 1, len(lines)):
        line = lines[index]
        stripped = line.strip()
        if not stripped:
            continue
        if line[0].isspace():
            continue
        if stripped.startswith(("config ", "menuconfig ", "choice", "if ", "endif", "endchoice")):
            return "\n".join(lines[start:index])

    return "\n".join(lines[start:])


def get_cmake_set_block(content: str, name: str) -> str:
    pattern = re.compile(rf"set\({name}\s*(.*?)\n\)", re.S)
    match = pattern.search(content)
    if match is None:
        raise AssertionError(f"缺少 CMake 变量块 {name}")
    return match.group(0)


def assert_orsource_not_guarded_by_boot(content: str, message: str) -> None:
    lines = content.splitlines()
    for index, line in enumerate(lines):
        if 'orsource "uboot/Kconfig.public"' not in line:
            continue

        prev = index - 1
        while prev >= 0 and lines[prev].strip() == "":
            prev -= 1

        if prev >= 0 and lines[prev].strip() == "if BOOT":
            raise AssertionError(message)
        return

    raise AssertionError("system/Kconfig 缺少 uboot/Kconfig.public 的入口")


def assert_uncommented_line_present(content: str, expected: str, message: str) -> None:
    for line in content.splitlines():
        if line.strip() == expected:
            return
    raise AssertionError(message)


def check_system_cmake() -> None:
    content = read("system/CMakeLists.txt")
    assert_contains(
        content,
        "DEFINED CONFIG_BOOT_RECOVERY_API",
        "system/CMakeLists.txt 必须允许通过 CONFIG_BOOT_RECOVERY_API 接入 uboot/api",
    )
    assert_contains(
        content,
        "DEFINED CONFIG_BOOT_OTA_API",
        "system/CMakeLists.txt 必须允许通过 CONFIG_BOOT_OTA_API 接入 uboot/api",
    )
    assert_contains(
        content,
        "DEFINED CONFIG_BOOT_WDT_API",
        "system/CMakeLists.txt 必须允许通过 CONFIG_BOOT_WDT_API 接入 uboot/api",
    )
    assert_contains(
        content,
        "DEFINED CONFIG_BOOT_OTA_PACKAGE",
        "system/CMakeLists.txt 必须按 CONFIG_BOOT_OTA_PACKAGE 接入 uboot/api",
    )


def check_system_kconfig() -> None:
    content = read("system/Kconfig")
    assert_orsource_not_guarded_by_boot(
        content,
        "system/Kconfig 不应再把 uboot/Kconfig.public 入口放在 if BOOT 门控内",
    )


def check_kconfig_public() -> None:
    content = read("system/uboot/Kconfig.public")
    boot_adb_sync = get_config_block(content, "BOOT_ADB_SYNC")
    boot_adb_sdmmc_fs_root = get_config_block(content, "BOOT_ADB_SDMMC_FS_ROOT")
    assert_contains(content, "config BOOT_RECOVERY_API", "缺少 public 开关 BOOT_RECOVERY_API")
    assert_contains(content, "config BOOT_OTA_API", "缺少 public 开关 BOOT_OTA_API")
    assert_contains(content, "config BOOT_WDT_API", "缺少 public 开关 BOOT_WDT_API")
    assert_contains(content, "config BOOT_ADB_SDMMC_RAW", "缺少 BOOT_ADB_SDMMC_RAW 开关")
    assert_contains(content, "config BOOT_ADB_SDMMC_FS", "缺少 BOOT_ADB_SDMMC_FS 开关")
    assert_contains(content, "config BOOT_ADB_SDMMC_FS_ROOT", "缺少 BOOT_ADB_SDMMC_FS_ROOT 开关")
    assert_contains(
        boot_adb_sdmmc_fs_root,
        'default "/SD:/adb/"',
        "BOOT_ADB_SDMMC_FS 启用时默认 adb root 应切到 /SD:/adb/",
    )
    assert_contains(
        content,
        "default BOOT_ADB_SDMMC_FS_ROOT if BOOT_ADB_SDMMC_FS",
        "公共 adb root 应从 BOOT_ADB_SDMMC_FS_ROOT 派生",
    )
    assert_not_contains(
        content,
        'default "/RAM:/adb/" if BOOT_ADB_SYNC',
        "只开启 BOOT_ADB_SYNC 时不应再暴露文件系统默认根",
    )
    assert_not_contains(
        boot_adb_sync,
        "select FATFS_FILESYSTEM",
        "BOOT_ADB_SYNC 不应再隐式开启 TF FAT 文件系统",
    )
    assert_not_contains(
        boot_adb_sync,
        "select LSFS_FAT",
        "BOOT_ADB_SYNC 不应再隐式开启 LSFS FAT 适配层",
    )
    assert_not_contains(
        boot_adb_sync,
        "select DISK_DRIVER_SDMMC",
        "BOOT_ADB_SYNC 不应再隐式开启 SDMMC 磁盘驱动",
    )
    assert_not_contains(
        content,
        "config BOOT_OTA_API\n    bool \"Boot public OTA API\"\n    default n\n    select BOOT_OTA_PACKAGE",
        "BOOT_OTA_API 不应隐式耦合 BOOT_OTA_PACKAGE",
    )
    assert_contains(content, "menuconfig BOOT_OTA_PACKAGE", "缺少 BOOT_OTA_PACKAGE 总开关")
    assert_contains(
        content,
        "default BOOT_CONTROL_STORE_FLASH if BOOT_OTA_API",
        "BOOT_OTA_API 路径应默认 flash store，避免 APP 显式配置 backend",
    )
    assert_contains(
        content,
        "select LISA_FLASH",
        "BOOT_OTA_API 应内部吸收 flash backend 依赖，避免 APP 额外显式开启",
    )
    assert_contains(
        content,
        "BOOT_CONTROL_STORE_FLASH || BOOT_OTA_API",
        "BOOT_CONTROL_STORE_BASE_ADDR 需要在 BOOT_OTA_API 启用时可配置",
    )
    assert_contains(
        content,
        "default 0x3003E000 if BOOT_ADB_SYNC",
        "带 recovery + TF/ADB 的 boot 应默认把 control_store 放到 boot 尾部保留区",
    )


def check_boot_adb_sync_source() -> None:
    content = read("system/uboot/src/boot_adb_storage.h")
    assert_not_contains(
        content,
        "CONFIG_BOOT_ADB_PUSH_PULL_DEFAULT_ROOT",
        "boot adb storage 不应再引用不存在的 CONFIG_BOOT_ADB_PUSH_PULL_DEFAULT_ROOT",
    )
    assert_not_contains(
        content,
        "CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT",
        "boot adb storage 内部不应再直接引用通用的 CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_ADB_SDMMC_FS_ROOT",
        "boot adb storage 应改用 BOOT_ADB_SDMMC_FS_ROOT",
    )


def check_boot_cmake() -> None:
    content = read("system/boot.cmake")
    assert_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_BASE_ADDR",
        "system/boot.cmake 需要识别 control_store 地址是否落在 boot 保留区内",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_SIZE",
        "system/boot.cmake 需要在 boot 尾部额外预留 control_store 大小",
    )


def check_boot_psram_runtime_startup() -> None:
    content = read("system/uboot/src/start.S")
    scatload_pos = content.find("call scatload")
    scatload_psram_pos = content.find("call scatload_psram")
    system_init_pos = content.find("call SystemInit")
    runtime_fence_pos = content.find("fence.i", system_init_pos)
    main_pos = content.find("call main")

    if scatload_pos < 0:
        raise AssertionError("boot_s 启动路径缺少常规 scatload()")
    if scatload_psram_pos < 0:
        raise AssertionError("boot_s 启动路径缺少 scatload_psram()")
    if system_init_pos < 0:
        raise AssertionError("boot_s 启动路径缺少 SystemInit() 调用")
    if main_pos < 0:
        raise AssertionError("boot_s 启动路径缺少 main() 调用")
    if runtime_fence_pos < 0:
        raise AssertionError("boot_s 在切入 PSRAM runtime 前缺少最终的 fence.i")

    if not (scatload_pos < scatload_psram_pos < system_init_pos < runtime_fence_pos < main_pos):
        raise AssertionError("boot_s 必须先执行 scatload() / scatload_psram() / SystemInit() / fence.i，再进入 main()")

    assert_contains(
        content,
        ".section .boot_f.ramcode",
        "boot_s 二阶段入口必须先落在 SRAM loader 段，不能继续从 flash .init 直接执行",
    )
    assert_contains(
        content,
        "boot_s_early_exc_entry",
        "boot_s 早期 trap 入口必须留在 SRAM，避免二阶段回跳 flash XIP",
    )


def check_boot_psram_runtime_relocation() -> None:
    content = read("system/uboot/src/CMakeLists.txt")
    runtime_block = get_cmake_set_block(content, "BOOT_RUNTIME_PSRAM_SOURCES")
    sram_runtime_block = get_cmake_set_block(content, "BOOT_RUNTIME_SRAM_SOURCES")

    assert_contains(
        content,
        "listenai_code_relocate(FILES ${BOOT_RUNTIME_PSRAM_SOURCES} LOCATION PSRAM)",
        "boot runtime 源文件必须统一通过 listenai_code_relocate(... LOCATION PSRAM) 搬到 PSRAM",
    )
    assert_contains(
        content,
        "listenai_code_relocate(FILES ${BOOT_RUNTIME_SRAM_SOURCES} LOCATION SRAM)",
        "首批 runtime bring-up 源文件必须整体留在 SRAM",
    )

    for runtime_source in [
        "boot_config.c",
        "boot_flash.c",
        "boot_adb_runtime.c",
        "boot_ota.c",
    ]:
        assert_contains(
            runtime_block,
            runtime_source,
            f"BOOT_RUNTIME_PSRAM_SOURCES 应包含 {runtime_source}",
        )

    for loader_source in [
        "boot.c",
        "scatload.c",
        "system.c",
        "boot_stage_gate.c",
        "boot_default_app.c",
        "boot_reset_cause.c",
    ]:
        assert_not_contains(
            runtime_block,
            loader_source,
            f"BOOT_RUNTIME_PSRAM_SOURCES 不应包含 early-init / loader 侧文件 {loader_source}",
        )

    for sram_runtime_source in [
        "main.c",
        "syslog.c",
        "boot_watchdog.c",
        "IOMuxManager.c",
    ]:
        assert_contains(
            sram_runtime_block,
            sram_runtime_source,
            f"BOOT_RUNTIME_SRAM_SOURCES 应包含 {sram_runtime_source}",
        )
        assert_not_contains(
            runtime_block,
            sram_runtime_source,
            f"BOOT_RUNTIME_PSRAM_SOURCES 不应再包含首批 SRAM bring-up 文件 {sram_runtime_source}",
        )

    assert_contains(
        content,
        "listenai_code_relocate(FILES system.c LOCATION SRAM)",
        "SystemInit 及其早期状态必须整体留在 SRAM，避免在 PMP 就绪前访问 PSRAM data/bss",
    )


def check_boot_impure_state_relocation() -> None:
    extensions = read("cmake/extensions.cmake")
    boot_cmake = read("system/uboot/src/CMakeLists.txt")
    boot_ld = read("system/uboot/linker/boot-components.ld")

    assert_contains(
        extensions,
        "FILES;SECTIONS;EXCLUDE_OBJECTS",
        "listenai_code_relocate() 需要支持按库排除特定 archive member，避免 libc_a-impure.o 被整体搬进 PSRAM",
    )
    assert_contains(
        extensions,
        "EXCLUDE_FILE (",
        "listenai_code_relocate() 生成的 relocation 规则必须使用 EXCLUDE_FILE 排除 archive member",
    )
    assert_contains(
        boot_cmake,
        "listenai_code_relocate(LIBRARY c LOCATION PSRAM EXCLUDE_OBJECTS libc_a-impure.o libc_a-rand.o)",
        "boot 二阶段把 libc 搬到 PSRAM 时，必须排除 rand() 早期依赖的 archive member",
    )
    assert_contains(
        boot_cmake,
        "listenai_code_relocate(LIBRARY c_nano LOCATION PSRAM EXCLUDE_OBJECTS libc_a-impure.o libc_a-rand.o)",
        "boot 二阶段把 libc_nano 搬到 PSRAM 时，也必须排除 rand() 早期依赖的 archive member",
    )
    assert_contains(
        boot_ld,
        "*libc.a:libc_a-impure.o(.data .data.* .sdata .sdata.*)",
        "boot_f.data 必须显式接住 libc_a-impure.o 的 data/sdata，保证 rand() 早期状态落在 SRAM",
    )
    assert_contains(
        boot_ld,
        "*libc_nano.a:libc_a-impure.o(.data .data.* .sdata .sdata.*)",
        "boot_f.data 必须兼容 libc_nano.a 中的 impure 状态对象",
    )
    assert_contains(
        boot_ld,
        "*libc.a:libc_a-impure.o(.bss .bss.* .sbss .sbss.* COMMON)",
        "boot_f.bss 必须显式接住 libc_a-impure.o 的 bss/sbss，避免早期 reent 状态回落到 PSRAM",
    )
    assert_contains(
        boot_ld,
        "*libc_nano.a:libc_a-impure.o(.bss .bss.* .sbss .sbss.* COMMON)",
        "boot_f.bss 必须兼容 libc_nano.a 中的 impure 状态对象",
    )
    assert_contains(
        boot_ld,
        "*libc.a:libc_a-rand.o(.text .text.* .stext .stext.*)",
        "boot_f.ramcode 必须显式接住 libc_a-rand.o，保证 rand() 在 PSRAM 初始化前可执行",
    )
    assert_contains(
        boot_ld,
        "*libc_nano.a:libc_a-rand.o(.text .text.* .stext .stext.*)",
        "boot_f.ramcode 必须兼容 libc_nano.a 中的 rand() 早期代码",
    )


def check_boot_main_version_logging() -> None:
    content = read("system/uboot/src/main.c")
    assert_uncommented_line_present(
        content,
        '#include "boot_version.h"',
        "boot main 必须包含 boot_version.h 以便打印版本/commit",
    )
    assert_uncommented_line_present(
        content,
        'printk("boot version: %d.%d.%d\\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);',
        "boot main 启动日志必须打印 boot version",
    )
    assert_uncommented_line_present(
        content,
        'printk("boot commit: %s\\n", VERSION_COMMIT);',
        "boot main 启动日志必须打印 boot commit",
    )


def check_boot_stage2_no_flash_checker() -> None:
    content = read("system/uboot/cmake/boot_standalone.cmake")
    assert_contains(
        content,
        "_BOOT_STANDALONE_ENABLE_BOOT_SECOND_STAGE",
        "boot standalone 必须识别二阶段开关，才能对 boot_s 启用 no-flash-xip 检查",
    )
    assert_contains(
        content,
        "check_stage2_no_flash_xip.py",
        "boot standalone 必须接入二阶段 no-flash-xip 构建检查脚本",
    )


def check_api_cmake() -> None:
    content = read("system/uboot/api/CMakeLists.txt")
    assert_contains(
        content,
        "CONFIG_BOOT_RECOVERY_API",
        "uboot/api/CMakeLists.txt 必须按 CONFIG_BOOT_RECOVERY_API 编译 recovery API",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_OTA_API",
        "uboot/api/CMakeLists.txt 必须按 CONFIG_BOOT_OTA_API 编译 OTA API",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_WDT_API",
        "uboot/api/CMakeLists.txt 必须按 CONFIG_BOOT_WDT_API 编译 WDT API",
    )
    assert_contains(
        content,
        "CONFIG_BOOT",
        "uboot/api/CMakeLists.txt 需要兼容旧路径 CONFIG_BOOT",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_OTA_PACKAGE",
        "uboot/api/CMakeLists.txt 必须兼容 CONFIG_BOOT_OTA_PACKAGE",
    )
    assert_not_contains(
        content,
        "listenai_library_sources_ifdef(\n    CONFIG_BOOT\n",
        "uboot/api/CMakeLists.txt 不应再依赖 CONFIG_BOOT 才编译 API",
    )
    assert_not_contains(
        content,
        "CONFIG_BOOT_OTA_API\n    uboot_ota_api.c\n    ../src/boot_ota_lifecycle.c\n    ../src/boot_ota_request.c\n    boot_control_store_lisa_flash.c",
        "uboot/api/CMakeLists.txt 不应在 OTA API 路径硬编码 flash backend",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_FLASH",
        "uboot/api/CMakeLists.txt 必须支持 flash backend 条件编译",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_NONE",
        "uboot/api/CMakeLists.txt 必须支持 none backend 条件编译",
    )
    assert_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_TEST",
        "uboot/api/CMakeLists.txt 必须支持 test backend 条件编译",
    )
    assert_contains(
        content,
        "uboot_wdt_api.c",
        "uboot/api/CMakeLists.txt 必须编译 uboot_wdt_api.c",
    )


def check_app_control_store_flash_backend() -> None:
    content = read("system/uboot/api/boot_control_store_lisa_flash.c")
    assert_contains(
        content,
        "CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_ADDR",
        "APP 侧 flash control store 必须按 flash 物理基址换算 offset",
    )
    assert_not_contains(
        content,
        "CONFIG_BOOT_CONTROL_STORE_BASE_ADDR - CONFIG_MEM_FLASH_BASE",
        "APP 侧 flash control store 不能再基于 CONFIG_MEM_FLASH_BASE 计算 offset",
    )


def check_uboot_ota_api_header() -> None:
    content = read("system/uboot/include/uboot_ota_api.h")
    assert_contains(
        content,
        "typedef enum {\n    UBOOT_OTA_FAILURE_NONE = 0,",
        "uboot_ota_api.h 必须暴露 OTA failure reason 枚举",
    )
    assert_contains(
        content,
        "int uboot_ota_get_last_failure(uboot_ota_failure_info_t *info);",
        "uboot_ota_api.h 必须暴露最近一次 OTA 失败原因查询接口",
    )
    assert_not_contains(
        content,
        "int uboot_ota_reboot(void);",
        "uboot_ota_api.h 不应继续暴露重复的 reboot API，应直接复用系统侧软复位能力",
    )


def check_uboot_ota_api_source() -> None:
    content = read("system/uboot/api/uboot_ota_api.c")
    assert_not_contains(
        content,
        "int uboot_ota_reboot(void)",
        "uboot_ota_api.c 不应继续实现重复的 reboot API",
    )


def check_uboot_recovery_api_header() -> None:
    content = read("system/uboot/include/uboot_recovery_api.h")
    assert_not_contains(
        content,
        "int uboot_recovery_reboot(void);",
        "uboot_recovery_api.h 不应继续暴露 reboot API，APP 应直接调用 sys_reboot()",
    )


def check_uboot_recovery_api_source() -> None:
    content = read("system/uboot/api/uboot_recovery_api.c")
    assert_not_contains(
        content,
        "int uboot_recovery_reboot(void)",
        "uboot_recovery_api.c 不应继续实现 reboot API，APP 应直接调用 sys_reboot()",
    )
    assert_not_contains(
        content,
        "boot_ota_handoff_trigger_software_reboot",
        "uboot_recovery_api.c 不应继续依赖 uboot 内部 reboot helper",
    )


def check_boot_ota_handoff_header() -> None:
    content = read("system/uboot/src/boot_ota_handoff.h")
    assert_not_contains(
        content,
        "int boot_ota_handoff_trigger_software_reboot(void);",
        "boot_ota_handoff.h 不应继续声明 reboot helper",
    )


def check_boot_ota_handoff_source() -> None:
    content = read("system/uboot/src/boot_ota_handoff.c")
    assert_not_contains(
        content,
        "int boot_ota_handoff_trigger_software_reboot(void)",
        "boot_ota_handoff.c 不应继续实现 reboot helper",
    )
    assert_not_contains(
        content,
        "__HAL_PMU_WholeChip_RST_ENABLE();",
        "boot_ota_handoff.c 不应继续承载 reboot 能力",
    )


def check_app_only_trigger_sample() -> None:
    content = read("samples/subsys/uboot/app_only_trigger/src/main.c")
    assert_contains(
        content,
        '#include "sys/reboot.h"',
        "APP-only OTA sample 应包含 sys/reboot.h 以复用系统统一 reboot API",
    )
    assert_contains(
        content,
        "sys_reboot(SYS_REBOOT_SOFT);",
        "APP-only OTA sample 应通过 sys_reboot(SYS_REBOOT_SOFT) 触发软复位",
    )
    assert_not_contains(
        content,
        "uboot_ota_reboot()",
        "APP-only OTA sample 不应继续调用 uboot_ota_reboot()",
    )
    assert_not_contains(
        content,
        "__HAL_PMU_WholeChip_RST_ENABLE();",
        "APP-only OTA sample 不应继续直接调用 PMU 软复位宏",
    )


def check_boot_soft_reset_consolidation() -> None:
    for rel_path in [
        "system/uboot/src/main.c",
        "system/uboot/src/shell_cmds.c",
        "system/uboot/src/boot_watchdog.c",
        "system/uboot/src/boot_recovery_shell_cmds.c",
    ]:
        content = read(rel_path)
        assert_contains(
            content,
            '#include "PowerManager.h"',
            f"{rel_path} 应包含 PowerManager.h 以直接复用系统软复位宏",
        )
        assert_contains(
            content,
            "__HAL_PMU_WholeChip_RST_ENABLE();",
            f"{rel_path} 应直接使用 __HAL_PMU_WholeChip_RST_ENABLE()",
        )
        assert_not_contains(
            content,
            "IP_CMN_SYS->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;",
            f"{rel_path} 不应继续手写 whole-chip soft reset 寄存器序列",
        )
        assert_not_contains(
            content,
            "IP_CMN_SYS->REG_SW_RESET_CP0.all = 0xCAFE000A;",
            f"{rel_path} 不应继续手写 whole-chip soft reset pass key",
        )
        assert_not_contains(
            content,
            "IP_CMN_SYS->REG_SW_RESET_CP0.all = BOOT_OTA_CMN_RESET_PASS_KEY;",
            f"{rel_path} 不应继续保留 boot OTA 自定义软复位 pass key 写法",
        )


def main() -> None:
    check_system_cmake()
    check_system_kconfig()
    check_kconfig_public()
    check_boot_adb_sync_source()
    check_boot_cmake()
    check_api_cmake()
    check_boot_psram_runtime_startup()
    check_boot_psram_runtime_relocation()
    check_boot_impure_state_relocation()
    check_boot_main_version_logging()
    check_boot_stage2_no_flash_checker()
    check_app_control_store_flash_backend()
    check_uboot_ota_api_header()
    check_uboot_ota_api_source()
    check_uboot_recovery_api_header()
    check_uboot_recovery_api_source()
    check_boot_ota_handoff_header()
    check_boot_ota_handoff_source()
    check_app_only_trigger_sample()
    check_boot_soft_reset_consolidation()
    print("public_api_contract: ok")


if __name__ == "__main__":
    main()
