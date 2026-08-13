# Rust SPI Loopback Demo

Exercises the `arcs::Spi` HAL (which also implements `embedded_hal::spi::SpiBus`)
on `spi0`:

1. **TX-only** `write` of 3 bytes — proves the async-start → ISR
   completion-callback → semaphore plumbing end to end (the lisa SPI
   `transfer`/`write`/`read` ops are asynchronous; the HAL blocks on a
   completion semaphore released from the SPI ISR).
2. **Full-duplex** `transfer` of a 4-byte pattern. With a MOSI↔MISO jumper the
   received bytes mirror the transmitted ones (`rx == tx`); without one the demo
   still runs to completion and reports "no loopback".

On `arcs_evb`, `src/main.c` routes `spi0` to CLK=PA15, MOSI=PA14, MISO=PA13,
CS=PA12 (overriding the empty weak `lisa_spi0_pinmux()` in
`boards/arcs_evb/pinmux.c`, mirroring `samples/drivers/devices/lisa_spi/master`).
To see the loopback path, jumper PA14 (MOSI) ↔ PA13 (MISO).

Config: 1 MHz, mode 0, 8-bit, MSB-first, hardware CS, interrupt transfer
(matches the C `LISA_SPI_CONFIG_DEFAULT()`).

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/spi_loopback -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_spi_loopback.bin
```

## Expected
Serial output ends with:
```
spi_loopback: spi0 configured @ 1MHz mode0 8-bit MSB, hw-CS
spi_loopback: write 3 bytes ok
spi_loopback: xfer tx=[a5 5a c3 3c] rx=[..]
spi_loopback: no loopback jumper (MISO idle) — transfer path OK   # or: loopback detected
spi_loopback:done ok=0                                            # ok=1 with a jumper
```
