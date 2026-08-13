# Rust embedded-hal Blink Demo

Proves the R1 `embedded-hal` trait layer + `#[arcs::main]` proc-macro:

- `blink_n` is **generic** over any `embedded_hal::digital::OutputPin` +
  `DelayNs` — it has no ARCS-specific types in its signature.
- `main` (via `#[arcs::main]`) wires it to the real LED through
  `arcs::GpioPin` (gpiob:9) and `arcs::Delay`.

## Board
`arcs_evb`. LED on `gpiob:9`.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/eh_blink -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_eh_blink.bin
```

## Expected
- LED blinks 5 times at ~200ms half-period.
- Serial: `eh_blink: blinking 5x ...` then `eh_blink:done`.
