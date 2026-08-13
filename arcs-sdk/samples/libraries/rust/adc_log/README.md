# Rust ADC Read-and-Log Demo

Reads two **internal** ADC channels via the `arcs::Adc` HAL and logs raw counts
plus a millivolt estimate:

- **ch6 = VBAT** (battery voltage, internally divided), 3.6 V reference
- **ch7 = TEMP** (on-die temperature sensor), 1.2 V reference

Both are internal, so no external pin or pinmux override is needed. Runs once to
completion.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/adc_log -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_adc_log.bin
```

## Expected
```
adc_log: VBAT(ch6) raw=<n> ~<mv>mV (pre-divider)
adc_log: TEMP(ch7) raw=<n> ~<mv>mV
adc_log:done
```
(10-bit conversions: raw is 0..=1023.)
