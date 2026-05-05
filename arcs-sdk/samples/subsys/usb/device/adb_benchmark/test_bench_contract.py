#!/usr/bin/env python3

from pathlib import Path
import sys

SAMPLE_DIR = Path(__file__).resolve().parent
README = SAMPLE_DIR / "README.md"
PRJ_CONF = SAMPLE_DIR / "prj.conf"
MAIN_C = SAMPLE_DIR / "src" / "main.c"
FS_C = SAMPLE_DIR / "src" / "fs.c"
SCRIPT = SAMPLE_DIR / "bench_adb_push.sh"
SAMPLE_YAML = SAMPLE_DIR / "sample.yaml"


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def read_text(path: Path, errors: list[str]) -> str:
    if not path.exists():
        errors.append(f"missing {path}")
        return ""
    return path.read_text(encoding="utf-8")


def main() -> int:
    errors: list[str] = []
    readme_text = read_text(README, errors)
    prj_conf_text = read_text(PRJ_CONF, errors)
    main_text = read_text(MAIN_C, errors)
    fs_text = read_text(FS_C, errors)
    script_text = read_text(SCRIPT, errors)
    sample_yaml_text = read_text(SAMPLE_YAML, errors)

    require('CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT="/NAND:/adb_bench/"' in prj_conf_text,
            "prj.conf must set the default ADB root to /NAND:/adb_bench/", errors)
    require('flash ready at /NAND:/adb_bench/' in main_text or 'flash ready at /NAND:/adb_bench/' in fs_text,
            "device logs must advertise flash benchmark readiness", errors)
    require('sd ready at /SD:/adb_bench/' in main_text or 'sd ready at /SD:/adb_bench/' in fs_text,
            "device logs must advertise sd benchmark readiness", errors)
    require('sd not ready' in main_text or 'sd not ready' in fs_text,
            "device logs must explicitly cover the missing-sd case", errors)
    require('--targets' in script_text, "benchmark script must support --targets", errors)
    require('/RAW/NAND/' in script_text, "benchmark script must target raw NAND path for flash", errors)
    require('/SD:/adb_bench/test.bin' in script_text, "benchmark script must target sd path", errors)
    require('.cache' in script_text, "benchmark script must persist artifacts under .cache", errors)
    require('adb push' in readme_text, "README must document adb push benchmark usage", errors)
    require('flash' in readme_text and 'TF' in readme_text, "README must describe flash and TF targets", errors)
    require('build_only: true' in sample_yaml_text, "sample.yaml should stay build_only", errors)

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    print("PASS: adb_benchmark sample contract is wired for flash and sd push benchmarking")
    return 0


if __name__ == "__main__":
    sys.exit(main())
