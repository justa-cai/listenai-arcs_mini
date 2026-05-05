#!/usr/bin/env python3

import argparse
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[3]
APP_MAIN = SCRIPT_DIR / "src" / "main.c"
SHELL_CFG = SCRIPT_DIR / "src" / "shell_cfg_user.h"
BENCH = SCRIPT_DIR / "verify_boot_adb_shell.sh"
README = SCRIPT_DIR / "README.md"
PRJ_CONF = SCRIPT_DIR / "prj.conf"
UPGRADE_FW_PRJ_CONF = SCRIPT_DIR / "upgrade_fw" / "prj.conf"
SAMPLE_YAML = SCRIPT_DIR / "sample.yaml"
BOOT_CONFIG = REPO_ROOT / "build" / "boot" / ".config"
SYSTEM_CMAKE = REPO_ROOT / "system" / "CMakeLists.txt"
BOOT_ADB_STORAGE = REPO_ROOT / "system" / "uboot" / "src" / "boot_adb_storage.c"
BOOT_MAIN = REPO_ROOT / "system" / "uboot" / "src" / "main.c"
BOOT_CMAKE = REPO_ROOT / "system" / "uboot" / "CMakeLists.txt"
BOOT_BRIDGE_CMAKE = REPO_ROOT / "system" / "boot.cmake"

NORMAL_RUNNING_MARKER = "APP enter boot recovery sample running"
NORMAL_REQUEST_MARKER = "APP requested boot recovery mode, rebooting"
RECOVERY_REASON_MARKER = "boot reason: soft req"
RECOVERY_BOOT_ADB_MARKER = "boot adb: start reason=soft req"
RAW_SDRAW_PATH = "/RAW/SDRAW/0/1000"
RAW_FLASH_FAST_PATH = "/RAW/FLASH/40000/10000"
RAW_NAND_FAST_PATH = "/RAW/NAND/40000/10000"

FORBIDDEN_HOST_TRIGGER_STRINGS = [
    "RECOVERY_TRIGGER_CMD",
    "RECOVERY_MANUAL",
    "recovery_trigger_instructions.txt",
    "Manual recovery trigger required",
    "Press ENTER after you have armed explicit recovery and reset the board",
]

FORBIDDEN_INTERACTIVE_SHELL_STRINGS = [
    "record_interactive_shell",
    "interactive_shell.txt",
    "interactive `adb shell`",
]

FORBIDDEN_OTA_STRINGS = [
    "partab.bin",
    "ota.tar",
    "ota.txz",
    "easyflash_reset.bin",
    "uboot_ota_start_from_ota_partition",
    "uboot_ota_reboot",
    "CONFIG_BOOT_OTA_PACKAGE=y",
]

APP_RUNTIME_STEPS = [
    'printf("APP enter boot recovery sample running\\n");',
    "flush_recovery_log();",
    "uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT)",
    'printf("APP requested boot recovery mode, rebooting\\n");',
    "flush_recovery_log();",
    "sys_reboot(SYS_REBOOT_SOFT)",
]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require_contains(errors: list[str], path: Path, content: str, needle: str) -> None:
    if needle not in content:
        errors.append(f"missing {needle!r} in {path}")


def require_absent(errors: list[str], path: Path, content: str, needle: str) -> None:
    if needle in content:
        errors.append(f"unexpected {needle!r} in {path}")


def require_order(errors: list[str], path: Path, content: str, needles: list[str]) -> None:
    position = -1
    for needle in needles:
        next_position = content.find(needle, position + 1)
        if next_position < 0:
            errors.append(f"missing ordered marker {needle!r} in {path}")
            return
        position = next_position


def parse_kconfig(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in read_text(path).splitlines():
        if line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
            continue
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def require_config_value(errors: list[str], path: Path, config: dict[str, str], key: str, expected: str) -> None:
    actual = config.get(key)
    if actual != expected:
        errors.append(f"expected {key}={expected!r} in {path}, got {actual!r}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source-only",
        action="store_true",
        help="Only validate source-level contract files; skip generated boot config checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    errors: list[str] = []
    app_text = read_text(APP_MAIN)
    shell_cfg_text = read_text(SHELL_CFG)
    bench_text = read_text(BENCH)
    readme_text = read_text(README)
    prj_conf_text = read_text(PRJ_CONF)
    upgrade_fw_prj_conf_text = read_text(UPGRADE_FW_PRJ_CONF)
    sample_yaml_text = read_text(SAMPLE_YAML)
    system_cmake_text = read_text(SYSTEM_CMAKE)
    boot_storage_text = read_text(BOOT_ADB_STORAGE)
    boot_main_text = read_text(BOOT_MAIN)
    boot_cmake_text = read_text(BOOT_CMAKE)
    boot_bridge_cmake_text = read_text(BOOT_BRIDGE_CMAKE)

    for needle in [
        '#include "FreeRTOS.h"',
        '#include "task.h"',
        '#include "sys/reboot.h"',
        '#include "uboot_recovery_api.h"',
        "#define RECOVERY_LOG_DRAIN_DELAY_MS 200U",
        "static void flush_recovery_log(void)",
        NORMAL_RUNNING_MARKER,
        NORMAL_REQUEST_MARKER,
        "fflush(stdout);",
        "vTaskDelay(pdMS_TO_TICKS(RECOVERY_LOG_DRAIN_DELAY_MS));",
        "uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT)",
        "sys_reboot(SYS_REBOOT_SOFT)",
    ]:
        require_contains(errors, APP_MAIN, app_text, needle)

    require_absent(errors, APP_MAIN, app_text, "uboot_recovery_reboot()")

    require_order(errors, APP_MAIN, app_text, APP_RUNTIME_STEPS)

    for needle in FORBIDDEN_OTA_STRINGS:
        require_absent(errors, APP_MAIN, app_text, needle)
        require_absent(errors, PRJ_CONF, prj_conf_text, needle)
        require_absent(errors, SAMPLE_YAML, sample_yaml_text, needle)

    for needle in [
        '#include "FreeRTOS.h"',
        '#include "task.h"',
        '#define SHELL_DEFAULT_USER "boot"',
    ]:
        require_contains(errors, SHELL_CFG, shell_cfg_text, needle)

    for needle in [
        "CONFIG_BOOT=y",
        "CONFIG_BOOT_SECOND_STAGE=y",
        "CONFIG_BOOT_ADB=y",
        "CONFIG_BOOT_ADB_SHELL=y",
        "CONFIG_BOOT_ADB_SYNC=y",
    ]:
        require_contains(errors, PRJ_CONF, prj_conf_text, needle)

    require_contains(
        errors,
        UPGRADE_FW_PRJ_CONF,
        upgrade_fw_prj_conf_text,
        "CONFIG_BOOT_OTA_PACKAGE=y",
    )

    require_contains(errors, SAMPLE_YAML, sample_yaml_text, 'file: "arcs.bin"')
    require_contains(errors, SAMPLE_YAML, sample_yaml_text, 'address: "0x0"')
    require_contains(errors, SAMPLE_YAML, sample_yaml_text, 'pattern: "boot adb: start reason=soft req"')

    for needle in [
        NORMAL_RUNNING_MARKER,
        NORMAL_REQUEST_MARKER,
        RECOVERY_REASON_MARKER,
        RECOVERY_BOOT_ADB_MARKER,
        'record_command burn_main "$REPO_ROOT/tools/burn/cskburn" -C arcs -s "$port" -b "$BURN_BAUD" --verify-all 0x0 "$REPO_ROOT/build/arcs.bin"',
        "serial_normal.txt",
        "serial_recovery_wait.txt",
        RAW_SDRAW_PATH,
        RAW_FLASH_FAST_PATH,
        RAW_NAND_FAST_PATH,
    ]:
        require_contains(errors, BENCH, bench_text, needle)

    for needle in [
        "burn_partab",
        "burn_ota",
        "Did not observe upgraded-firmware",
    ] + FORBIDDEN_HOST_TRIGGER_STRINGS + FORBIDDEN_INTERACTIVE_SHELL_STRINGS:
        require_absent(errors, BENCH, bench_text, needle)

    for needle in [
        "不依赖 OTA",
        "build/arcs.bin",
        "`0x0`",
        NORMAL_RUNNING_MARKER,
        NORMAL_REQUEST_MARKER,
        RECOVERY_REASON_MARKER,
        RECOVERY_BOOT_ADB_MARKER,
        "BOOT_ADB recovery bench",
        "--preflight-only",
        "只烧录 `arcs.bin`",
        '`upgrade status`',
        '`adb push` / `adb pull`',
        '`adb push --sync`',
        '`SDRAW` flow：`adb push` / `adb pull`',
        RAW_SDRAW_PATH,
        RAW_FLASH_FAST_PATH,
        RAW_NAND_FAST_PATH,
    ]:
        require_contains(errors, README, readme_text, needle)

    for needle in [
        "uboot_ota_start_from_ota_partition",
        "uboot_ota_reboot",
        "CONFIG_BOOT_OTA_PACKAGE=y",
    ] + FORBIDDEN_HOST_TRIGGER_STRINGS + FORBIDDEN_INTERACTIVE_SHELL_STRINGS:
        require_absent(errors, README, readme_text, needle)

    for needle in [
        "if(DEFINED CONFIG_BOOT",
        "add_subdirectory(uboot/api)",
    ]:
        require_contains(errors, SYSTEM_CMAKE, system_cmake_text, needle)

    for needle in [
        '#include "lisa_device.h"',
        '#include "lisa_sdmmc.h"',
        'lisa_device_get("sdmmc0")',
        "lisa_sdmmc_probe(",
    ]:
        require_contains(errors, BOOT_ADB_STORAGE, boot_storage_text, needle)

    require_absent(errors, BOOT_ADB_STORAGE, boot_storage_text, "sdmmc_hard_init()")
    require_contains(errors, BOOT_MAIN, boot_main_text, "lisa_device_init();")

    for needle in [
        "project(boot NONE)",
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)",
        "include(${CMAKE_CURRENT_LIST_DIR}/cmake/boot_standalone.cmake)",
    ]:
        require_contains(errors, BOOT_CMAKE, boot_cmake_text, needle)

    require_contains(errors, BOOT_BRIDGE_CMAKE, boot_bridge_cmake_text, '"CONFIG_HEAP_SIZE=0x40000"')
    require_contains(errors, BOOT_BRIDGE_CMAKE, boot_bridge_cmake_text, '"CONFIG_PSRAM_HEAP_SIZE=0x300000"')

    if not args.source_only and not BOOT_CONFIG.exists():
        errors.append(f"missing generated boot config {BOOT_CONFIG}")
    elif not args.source_only:
        boot_config = parse_kconfig(BOOT_CONFIG)
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_BOOT_ADB", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_BOOT_ADB_SHELL", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_BOOT_ADB_SYNC", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_HEAP_SIZE", "0x40000")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_ADB_SHELL", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_ADB_SYNC", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_FILE_SYSTEM", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_FATFS_FILESYSTEM", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_LSFS", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_LSFS_FAT", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT", '"/SD:/adb/"')
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_PSRAM_HEAP_SIZE", "0x300000")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_SDK_MODULE_LETTER_SHELL", "y")
        require_config_value(
            errors,
            BOOT_CONFIG,
            boot_config,
            "CONFIG_SDK_MODULE_LETTER_SHELL_CONFIG_FILE",
            '"shell_cfg_user.h"',
        )
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_LISA_DEVICE", "y")
        require_config_value(errors, BOOT_CONFIG, boot_config, "CONFIG_LISA_SDMMC_DEVICE", "y")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    if args.source_only:
        print("PASS: app_enter_boot_recovery source contract matches the app-to-boot recovery design")
    else:
        print("PASS: app_enter_boot_recovery contract matches the app-to-boot recovery design")
    return 0


if __name__ == "__main__":
    sys.exit(main())
