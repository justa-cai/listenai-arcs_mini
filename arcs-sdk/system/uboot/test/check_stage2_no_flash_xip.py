#!/usr/bin/env python3
"""Reject boot_s runtime images that still execute from flash XIP."""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


FLASH_EXEC_SECTIONS = {".init", ".boot.text", ".text"}
STAGE2_RUNTIME_SECTIONS = {".itcm", ".fast.text", ".psram.text"}
STAGE2_LOADER_SYMBOLS = ("boot_s", "scatload", "scatload_psram")
REQUIRED_STAGE2_RAM_SYMBOLS = (
    "boot_s",
    "scatload",
    "scatload_psram",
    "exc_entry",
    "irq_entry",
    "xTaskGetTickCount",
    "elog_output",
)
REQUIRED_STAGE2_SRAM_SYMBOLS = ("SystemInit",)
REQUIRED_STAGE2_EARLY_EXEC_SYMBOLS = ("rand",)
REQUIRED_STAGE2_SRAM_STATE_SYMBOLS = (
    "HARTID",
    "SystemCoreClock",
    "SystemIRegionInfo",
    "SystemExceptionHandlers",
    "_impure_ptr",
    "_impure_data",
)
EARLY_STAGE2_SYMBOLS = (
    "boot_f",
    "scatload_boot_f",
    "scatload",
    "scatload_psram",
    "boot_default_app_is_valid",
    "boot_default_app_prepare_recovery",
    "boot_stage_gate_should_enter_second_stage",
    "boot_reset_cause_apply",
    "boot_prepare_handoff",
    "boot_jump_to_cp",
    "boot_ap_wdt_start",
    "boot_ap_wdt_stop",
    "SystemInit",
    "main",
    "syslog_init",
    "syslog_output_default",
    "boot_watchdog_init",
)
SAVE_RESTORE_PREFIXES = ("__riscv_save_", "__riscv_restore_")

SECTION_RE = re.compile(
    r"^\s*\[\s*\d+\]\s+(?P<name>\S+)\s+\S+\s+(?P<addr>[0-9a-fA-F]+)\s+\S+\s+(?P<size>[0-9a-fA-F]+)\b"
)
SYMBOL_RE = re.compile(
    r"^(?P<name>[^|]+)\|(?P<addr>[0-9a-fA-F]+)\|(?P<class>[^|]*)\|(?P<type>[^|]*)\|(?P<size>[^|]*)\|(?P<line>[^|]*)\|(?P<section>[^\t]+)"
)
INSTRUCTION_RE = re.compile(
    r"^(?P<caller>[0-9a-fA-F]+):\s+[0-9a-fA-F]+\s+(?P<mnemonic>[A-Za-z0-9._]+)(?P<operands>.*)$"
)
TARGET_RE = re.compile(r"(?:#\s+|\s)(?P<target>[0-9a-fA-F]+)\s+<(?P<name>[^>]+)>\s*$")

CONTROL_TRANSFER_MNEMONICS = {
    "call",
    "j",
    "jal",
    "jalr",
    "jr",
    "ret",
    "tail",
    "c.j",
    "c.jal",
    "c.jalr",
    "c.jr",
}


class Range:
    def __init__(self, name: str, start: int, end: int) -> None:
        self.name = name
        self.start = start
        self.end = end

    def contains(self, addr: int) -> bool:
        return self.start <= addr < self.end


class Symbol:
    def __init__(self, name: str, addr: int, size: int, section: str) -> None:
        self.name = name
        self.addr = addr
        self.size = size
        self.section = section

    @property
    def end(self) -> int:
        return self.addr + self.size


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--relf", required=True, type=Path)
    parser.add_argument("--symb", required=True, type=Path)
    parser.add_argument("--lst", required=True, type=Path)
    return parser.parse_args()


def parse_sections(path: Path) -> Dict[str, Range]:
    sections = {}  # type: Dict[str, Range]

    for line in path.read_text(encoding="utf-8").splitlines():
        match = SECTION_RE.match(line)
        if match is None:
            continue

        name = match.group("name")
        start = int(match.group("addr"), 16)
        size = int(match.group("size"), 16)
        sections[name] = Range(name=name, start=start, end=start + size)

    return sections


def parse_symbols(path: Path) -> Dict[str, Symbol]:
    symbols = {}  # type: Dict[str, Symbol]

    for line in path.read_text(encoding="utf-8").splitlines():
        match = SYMBOL_RE.match(line)
        if match is None:
            continue

        name = match.group("name").strip()
        section = match.group("section").strip().split("\t", 1)[0]
        size_text = match.group("size").strip()
        size = int(size_text, 16) if size_text else 0
        symbols[name] = Symbol(
            name=name,
            addr=int(match.group("addr"), 16),
            size=size,
            section=section,
        )

    return symbols


def find_section(addr: int, sections: Dict[str, Range]) -> Optional[str]:
    for section in sections.values():
        if section.contains(addr):
            return section.name
    return None


def build_stage2_regions(sections: Dict[str, Range], symbols: Dict[str, Symbol]) -> List[Range]:
    regions = []  # type: List[Range]

    for section_name in STAGE2_RUNTIME_SECTIONS:
        section = sections.get(section_name)
        if section is not None and section.start != section.end:
            regions.append(section)

    for symbol_name in STAGE2_LOADER_SYMBOLS:
        symbol = symbols.get(symbol_name)
        if symbol is None or symbol.size == 0:
            continue
        regions.append(Range(name=symbol.name, start=symbol.addr, end=symbol.end))

    return regions


def address_in_regions(addr: int, regions: List[Range]) -> bool:
    return any(region.contains(addr) for region in regions)


def build_named_regions(symbols: Dict[str, Symbol], names: Tuple[str, ...]) -> List[Range]:
    regions = []  # type: List[Range]

    for name in names:
        symbol = symbols.get(name)
        if symbol is None or symbol.size == 0:
            continue
        regions.append(Range(name=symbol.name, start=symbol.addr, end=symbol.end))

    return regions


def check_early_stage_save_restore(lst_path: Path, symbols: Dict[str, Symbol]) -> List[str]:
    failures = []  # type: List[str]
    early_stage_regions = build_named_regions(symbols, EARLY_STAGE2_SYMBOLS)

    if not early_stage_regions:
        return failures

    for line in lst_path.read_text(encoding="utf-8").splitlines():
        match = INSTRUCTION_RE.match(line)
        if match is None:
            continue

        mnemonic = match.group("mnemonic")
        if mnemonic not in CONTROL_TRANSFER_MNEMONICS:
            continue

        caller = int(match.group("caller"), 16)
        if not address_in_regions(caller, early_stage_regions):
            continue

        target_match = TARGET_RE.search(match.group("operands"))
        if target_match is None:
            continue

        target_name = target_match.group("name")
        if not target_name.startswith(SAVE_RESTORE_PREFIXES):
            continue

        target_symbol = symbols.get(target_name)
        if target_symbol is None or target_symbol.section not in STAGE2_RUNTIME_SECTIONS:
            continue

        failures.append(
            "二阶段早期路径错误依赖了尚未就绪的 runtime helper: "
            f"caller=0x{caller:08x} target=0x{target_symbol.addr:08x} "
            f"target_symbol={target_name} target_section={target_symbol.section}"
        )

    return failures


def check_required_symbols(symbols: Dict[str, Symbol]) -> List[str]:
    failures = []  # type: List[str]

    for name in REQUIRED_STAGE2_RAM_SYMBOLS:
        symbol = symbols.get(name)
        if symbol is None:
            failures.append(f"缺少关键二阶段符号: {name}")
            continue

        if symbol.section in FLASH_EXEC_SECTIONS:
            failures.append(
                f"关键二阶段符号仍落在 flash XIP: {name} @ 0x{symbol.addr:08x} section={symbol.section}"
            )

    return failures


def check_required_sram_symbols(symbols: Dict[str, Symbol]) -> List[str]:
    failures = []  # type: List[str]

    for name in REQUIRED_STAGE2_SRAM_SYMBOLS:
        symbol = symbols.get(name)
        if symbol is None:
            failures.append(f"缺少关键二阶段早期符号: {name}")
            continue

        if symbol.section in FLASH_EXEC_SECTIONS:
            failures.append(
                f"关键二阶段早期符号仍落在 flash XIP: {name} @ 0x{symbol.addr:08x} section={symbol.section}"
            )

    return failures


def check_required_sram_state_symbols(symbols: Dict[str, Symbol]) -> List[str]:
    failures = []  # type: List[str]

    for name in REQUIRED_STAGE2_SRAM_STATE_SYMBOLS:
        symbol = symbols.get(name)
        if symbol is None:
            failures.append(f"缺少关键二阶段早期状态符号: {name}")
            continue

        if symbol.section.startswith(".psram"):
            failures.append(
                f"关键二阶段早期状态符号错误落在 PSRAM: {name} @ 0x{symbol.addr:08x} section={symbol.section}"
            )

    return failures


def check_required_early_exec_symbols(symbols: Dict[str, Symbol]) -> List[str]:
    failures = []  # type: List[str]

    for name in REQUIRED_STAGE2_EARLY_EXEC_SYMBOLS:
        symbol = symbols.get(name)
        if symbol is None:
            continue

        if symbol.section in FLASH_EXEC_SECTIONS or symbol.section.startswith(".psram"):
            failures.append(
                f"关键二阶段早期可执行符号错误落在非早期可执行区: "
                f"{name} @ 0x{symbol.addr:08x} section={symbol.section}"
            )

    return failures


def check_stage2_calls(lst_path: Path,
                       sections: Dict[str, Range],
                       stage2_regions: List[Range]) -> List[str]:
    failures = []  # type: List[str]

    for line in lst_path.read_text(encoding="utf-8").splitlines():
        match = INSTRUCTION_RE.match(line)
        if match is None:
            continue

        mnemonic = match.group("mnemonic")
        if mnemonic not in CONTROL_TRANSFER_MNEMONICS:
            continue

        target_match = TARGET_RE.search(match.group("operands"))
        if target_match is None:
            continue

        caller = int(match.group("caller"), 16)
        target = int(target_match.group("target"), 16)

        if not address_in_regions(caller, stage2_regions):
            continue

        target_section = find_section(target, sections)
        if target_section not in FLASH_EXEC_SECTIONS:
            continue

        failures.append(
            "二阶段代码仍直接跳到 flash XIP: "
            f"caller=0x{caller:08x} target=0x{target:08x} "
            f"target_section={target_section} symbol={target_match.group('name')}"
        )

    return failures


def main() -> int:
    args = parse_args()
    sections = parse_sections(args.relf)
    symbols = parse_symbols(args.symb)
    stage2_regions = build_stage2_regions(sections, symbols)

    failures = []
    failures.extend(check_required_symbols(symbols))
    failures.extend(check_required_sram_symbols(symbols))
    failures.extend(check_required_sram_state_symbols(symbols))
    failures.extend(check_required_early_exec_symbols(symbols))
    failures.extend(check_early_stage_save_restore(args.lst, symbols))
    failures.extend(check_stage2_calls(args.lst, sections, stage2_regions))

    if not failures:
        print("check_stage2_no_flash_xip: ok")
        return 0

    print("check_stage2_no_flash_xip: fail", file=sys.stderr)
    for failure in failures[:40]:
        print(f" - {failure}", file=sys.stderr)
    if len(failures) > 40:
        print(f" - ... 其余 {len(failures) - 40} 条已省略", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
