# Rust I2C Bus-Scan Demo

Probes every 7-bit address on `i2c0` (SDA=PA22, SCL=PA23 on arcs_evb) via the
`arcs::I2c` HAL and logs which devices ACK. Runs to completion whether or not
anything is attached.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/i2c_scan -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_i2c_scan.bin
```

## Expected
Serial: `i2c_scan: probing ...`, a `found device at 0xNN` line per ACK, then
`i2c_scan:done found=N`.
