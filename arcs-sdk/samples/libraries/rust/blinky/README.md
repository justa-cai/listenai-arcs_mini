# Rust Blinky on ARCS SDK

Blinks the on-board LED at 500ms cadence using `arcs::Gpio` and
`arcs::Thread::sleep_ms`. Demonstrates the minimal GPIO + scheduler surface
of the Rust HAL on top of the ARCS SDK.

## Board

`arcs_evb`. The on-board LED is wired to **`gpiob` pin 9** (PAD_B[9],
labelled "LED" / "I/O 电源指示灯" on the silkscreen). Update the constants
in `src/lib.rs` if you port the sample to a board with a different LED pin.

## Prerequisites

- Rust toolchain via [rustup](https://rustup.rs). First build auto-installs
  the version locked in `modules/rust/rust-toolchain.toml` (1.83.0) and the
  `riscv32imac-unknown-none-elf` target.

## Build

```sh
bash build.sh -b arcs_evb samples/libraries/rust/blinky
```

## Flash

```sh
../cskburn/build/cskburn/cskburn --reset-strategy cross-coupled \
    -C 6 -b 1500000 0x0 build/rust_blinky.bin
```

(Adjust port/baud as appropriate for your setup.)

## Expected behaviour

- LED on `gpiob:9` toggles every 500 ms (visible blink)
- Serial log shows `blinky:tick:N` every 4 toggles (every 2 s):

```
[I/rust_blinky] === Rust Blinky Starting ===
[I/rust] blinky:tick:0
[I/rust] blinky:tick:4
[I/rust] blinky:tick:8
...
```

If the LED does not toggle, double-check the pin assignment against
`boards/arcs_evb/README.md` before suspecting code or toolchain issues.
