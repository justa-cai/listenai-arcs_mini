# Rust RTT Log Demo

Shows Rust `log` output flowing over **SEGGER RTT** (read via J-Link), in
addition to the UART console. The `arcs` Rust `log` facade bridges to the SDK's
easylogger frontend, which dispatches to every registered backend; enabling
`CONFIG_LOG_BACKEND_SEGGER_RTT=y` adds the RTT backend (`SEGGER_RTT_printf`)
alongside the console (UART) one — so no Rust-side code change is needed.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/rtt_log -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_rtt_log.bin
```

## Read RTT over J-Link
Samples run on the CP core, so connect with the CP JLinkScript (`jtagscan1`).
With `JLinkRTTLogger` (headless):
```sh
JLinkRTTLogger -Device ARCS -If cJTAG -Speed 4000 \
    -JLinkScriptFile <skill>/assets/jlink/jtagscan1.JLinkScript \
    -RTTChannel 0 /tmp/rtt.log
```
or attach `JLinkRTTClient` to a running `JLinkGDBServer`.

## Expected (RTT and UART both)
```
rtt_log: start — Rust log over SEGGER RTT
rtt_log: tick 0
rtt_log: tick 1
...
```
