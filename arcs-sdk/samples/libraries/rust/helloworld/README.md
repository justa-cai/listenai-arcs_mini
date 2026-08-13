# Rust Hello-World on ARCS SDK

A minimal Rust no_std sample. Demonstrates:

- Calling Rust from a C `main()` entry
- `arcs::entry!` macro injecting panic handler + global allocator + log init
- The `log` crate facade routed to easylogger
- `alloc` crate usage (Vec) backed by FreeRTOS `pvPortMalloc`

## Board

`arcs_evb` (only board supported in the Rust MVP).

## Prerequisites

- Rust toolchain via [rustup](https://rustup.rs). First build auto-installs the
  version locked in `modules/rust/rust-toolchain.toml` (1.83.0) and the
  `riscv32imac-unknown-none-elf` target.

## Build

```sh
bash build.sh -b arcs_evb samples/libraries/rust/helloworld
```

## Flash

```sh
../cskburn/build/cskburn/cskburn --reset-strategy cross-coupled \
    -C 6 -b 1500000 0x0 build/zephyr/zephyr.bin
```

(Adjust port/baud as appropriate for your setup.)

## Expected output

```
[I/rust_test] === Rust Language Support Test ===
[I/rust] Hello from Rust on ARCS SDK!
[I/rust] alloc test: sum=6
[I/rust_test] === Rust Test PASSED ===
```
