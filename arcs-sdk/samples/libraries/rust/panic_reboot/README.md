# Rust Panic-Reboot Demo

Demonstrates the opt-in **`panic-reboot`** feature of the `arcs` crate. With the
feature enabled (see `Cargo.toml`), the Rust panic handler logs the panic
message and then triggers a full software reset (`arcs::reboot()`), so a
panicked device **recovers (reboots)** instead of hanging forever on the SDK's
RISC-V fault handler (whose default behaviour is to dump registers and
`while(1)`).

> Default (feature off): a panic logs the message then `ebreak`s into the SDK
> fault handler and halts — best for interactive debugging of a stuck core.

Because nothing is persisted across the full reset, the demo panics again after
each reboot — i.e. it loops "log → panic → reset → log → panic → reset …",
which is exactly what makes the recovery visible on the serial console.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/panic_reboot -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_panic_reboot.bin
```

## Expected
Each cycle on the serial console:
```
=== Rust panic-reboot demo ===
panic_reboot: about to panic — feature panic-reboot will log then reset
E/rust-panic ... R4 panic-reboot demo: log then reboot (not hang)
<boot banner — chip reset, next cycle begins>
```
(Flash a different image to stop the loop.)
