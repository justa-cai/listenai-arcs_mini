#!/usr/bin/env python3

from pathlib import Path
import sys


SCRIPT = Path(__file__).resolve().parent / "verify_boot_adb_shell.sh"


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def main() -> int:
    errors: list[str] = []
    text = SCRIPT.read_text(encoding="utf-8")

    require(
        'OUT_ROOT=${OUT_ROOT:-"$REPO_ROOT/build/boot_adb_shell_bench"}' not in text,
        "default OUT_ROOT still lives under build/ and will be deleted by build.sh -C",
        errors,
    )
    require(
        'fail=missing build/build_root_context.txt' not in text,
        "capture_build_truth still treats missing build_root_context.txt as a hard failure",
        errors,
    )
    require(
        'fail=build/boot/app.config missing CONFIG_ADB_SHELL=y' not in text,
        "capture_build_truth still expects CONFIG_ADB_SHELL=y in build/boot/app.config",
        errors,
    )
    require(
        'fail=build/boot/app.config missing CONFIG_ADB_SYNC=y' not in text,
        "capture_build_truth still expects CONFIG_ADB_SYNC=y in build/boot/app.config",
        errors,
    )
    require(
        'fail=build/boot/app.config missing CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT="/SD:/adb/"' not in text,
        "capture_build_truth still expects CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT in build/boot/app.config",
        errors,
    )
    require(
        'if ! "$@" >>"$outfile" 2>&1; then' not in text,
        "record_command still negates command status and will hide real failures",
        errors,
    )

    source_only_call = text.find("run_sample_contract_check --source-only")
    build_call = text.find('if ! record_command build bash "$REPO_ROOT/build.sh" -C -S samples/subsys/uboot/app_enter_boot_recovery -DBOARD="$BOARD"; then')
    build_truth_call = text.find('if ! capture_build_truth "$build_arcs_base"; then')
    full_contract_call = text.rfind("run_sample_contract_check")

    require(source_only_call >= 0, "missing source-only contract check before PREFLIGHT_ONLY return", errors)
    require(build_call >= 0, "missing build command in main()", errors)
    require(build_truth_call >= 0, "missing build truth capture in main()", errors)
    require(full_contract_call >= 0, "missing full contract check in main()", errors)

    if build_call >= 0 and build_truth_call >= 0 and full_contract_call >= 0:
        require(
            build_call < build_truth_call < full_contract_call,
            "full contract check must run after build and build truth capture",
            errors,
        )

    if source_only_call >= 0:
        preflight_return = text.find("if (( PREFLIGHT_ONLY == 1 )); then")
        require(
            source_only_call < preflight_return,
            "source-only contract check should happen before PREFLIGHT_ONLY short-circuit",
            errors,
        )

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    print("PASS: recovery bench keeps artifacts outside build/ and orders contract checks correctly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
