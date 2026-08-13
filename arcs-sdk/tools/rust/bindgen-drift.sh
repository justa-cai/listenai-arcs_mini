#!/usr/bin/env bash
# Compare hand-written sys/*.rs FFI bindings against bindgen-generated
# reference output. Fail (exit 1) if drift is detected.
#
# Requires: bindgen (cargo install bindgen-cli) and libclang.
set -euo pipefail

ARCS_BASE="$(cd "$(dirname "$0")/../.." && pwd)"
SYS_DIR="${ARCS_BASE}/modules/rust/arcs/src/sys"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

NORMALIZE="${ARCS_BASE}/tools/rust/bindgen-normalize.py"

INCLUDES=(
    -I "${ARCS_BASE}/system/log"
    -I "${ARCS_BASE}/system/os/inc"
    -I "${ARCS_BASE}/drivers/lisa_device"
    -I "${ARCS_BASE}/drivers/lisa_adc"
    -I "${ARCS_BASE}/drivers/lisa_flash"
    -I "${ARCS_BASE}/drivers/lisa_gpio"
    -I "${ARCS_BASE}/drivers/lisa_i2c"
    -I "${ARCS_BASE}/drivers/lisa_pwm"
    -I "${ARCS_BASE}/drivers/lisa_rtc"
    -I "${ARCS_BASE}/drivers/lisa_spi"
    -I "${ARCS_BASE}/drivers/lisa_uart"
    -I "${ARCS_BASE}/modules/easylogger/easylogger/inc"
    -I "${ARCS_BASE}/modules/freertos/include"
    -I "${ARCS_BASE}/modules/rust/arcs/glue"
    -I "${ARCS_BASE}/soc/arcs"
    -I "${ARCS_BASE}/soc/common"
)

bindgen_for() {
    local header="$1" out="$2"
    bindgen "$header" \
        --use-core --no-layout-tests --allowlist-file "$header" \
        -- "${INCLUDES[@]}" -target riscv32-unknown-none-elf \
        > "$out"
}

check() {
    local module="$1" header="$2"
    local ours="${SYS_DIR}/${module}.rs"
    local ref="${TMP_DIR}/${module}.ref.rs"
    bindgen_for "$header" "$ref"

    local ours_norm="${TMP_DIR}/${module}.ours.norm"
    local ref_norm="${TMP_DIR}/${module}.ref.norm"
    "$NORMALIZE" < "$ours" > "$ours_norm"
    "$NORMALIZE" < "$ref"  > "$ref_norm"

    if ! diff -u "$ours_norm" "$ref_norm" > "${TMP_DIR}/${module}.diff"; then
        echo "❌ DRIFT in sys/${module}.rs vs ${header}:"
        cat "${TMP_DIR}/${module}.diff"
        return 1
    fi
    echo "✅ sys/${module}.rs matches ${header}"
}

FAIL=0
check log    "${ARCS_BASE}/modules/rust/arcs/glue/arcs_rust_log.h" || FAIL=1
check thread "${ARCS_BASE}/system/os/inc/lisa_thread.h"            || FAIL=1
check sync   "${ARCS_BASE}/system/os/inc/lisa_mutex.h"             || FAIL=1
check device "${ARCS_BASE}/drivers/lisa_device/lisa_device.h"      || FAIL=1
check gpio   "${ARCS_BASE}/drivers/lisa_gpio/lisa_gpio.h"          || FAIL=1
check i2c    "${ARCS_BASE}/drivers/lisa_i2c/lisa_i2c.h"            || FAIL=1
check spi    "${ARCS_BASE}/drivers/lisa_spi/lisa_spi.h"            || FAIL=1
check uart   "${ARCS_BASE}/drivers/lisa_uart/lisa_uart.h"          || FAIL=1
check pwm    "${ARCS_BASE}/drivers/lisa_pwm/lisa_pwm.h"            || FAIL=1
check rtc    "${ARCS_BASE}/drivers/lisa_rtc/lisa_rtc.h"            || FAIL=1
check adc    "${ARCS_BASE}/drivers/lisa_adc/lisa_adc.h"            || FAIL=1
check flash  "${ARCS_BASE}/drivers/lisa_flash/lisa_flash.h"        || FAIL=1

if [ "$FAIL" -ne 0 ]; then
    echo ""
    echo "FFI binding drift detected. To resolve:"
    echo "  1. Inspect the diff(s) above"
    echo "  2. Update the affected sys/*.rs by hand to match bindgen output"
    echo "  3. Update the 'Last synced at commit' comment in the file header"
    exit 1
fi

echo ""
echo "✅ All FFI bindings consistent with C headers."
