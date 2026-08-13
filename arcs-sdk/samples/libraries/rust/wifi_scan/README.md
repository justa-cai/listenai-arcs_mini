# Rust WiFi Scan (`wifi_scan`)

Lists nearby WiFi access points from Rust using the SDK's `wifi_manager`
station API (R8a). It exercises `arcs::Wifi`:

```rust
let wifi = arcs::Wifi::init()?;     // wifi_mgr_init(wifi_mgr_ops_get())
wifi.sta_enable()?;
let mut aps = vec![arcs::ScanInfo::default(); 32];
let n = wifi.scan(&mut aps)?;       // blocking wifi_mgr_scan_ap; returns count
for ap in &aps[..n] {
    // ap.ssid(), ap.bssid(), ap.rssi, ap.channel, ap.encryption_mode
}
```

## Architecture (C bring-up + Rust feature)

`wifi_manager` is a flat C function API (not a `lisa_device`). The WiFi core
bring-up — `mac_manager` (MAC from chip-id efuse), `lisa_kv_init`, and the
**asynchronous** `lisa_wifi_init` — is platform boilerplate, so it lives in
`src/main.c`. It waits on the init-done callback (a binary semaphore) and then
hands off to `rust_main()`, which drives `arcs::Wifi`. This mirrors how
`display_gfx` does board-specific bus attach in C and the feature work in Rust.

No filesystem is needed: the KV link-dependency uses the flash-backed
EasyFlash backend (`CONFIG_LISA_KV_TYPE_EF`), and the scan path itself touches
no persistent storage.

## Build & flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/wifi_scan -DBOARD=arcs_evb
# flash build/rust_wifi_scan.bin with cskburn, then watch the serial console
```

WiFi is a feature of the LS26xx SoC on `arcs_evb` (no external module needed),
so the scan returns real nearby APs. Expected serial output:

```
=== Rust WiFi scan demo ===
wifi core init done
wifi_scan: wifi_manager init ok
wifi_scan: STA enabled
wifi_scan: found N AP(s)
wifi_scan: [0] ssid="..." rssi=-52 ch=6 enc=WPA2-PSK bssid=AA:BB:CC:DD:EE:FF
...
wifi_scan:done n=N
```

## Connecting (beyond scan)

`arcs::Wifi` also exposes `connect(ssid, password)` / `disconnect()` /
`status()`. Connecting associates at L2; obtaining an IP additionally requires
bringing the network interface up and starting DHCP (`net_if_up` +
`ls_dhcpc_start`) in the connection callback — that networking glue is C-side
and out of scope for this scan-focused sample.
