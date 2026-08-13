# Rust BLE Advertising (`ble_adv`)

Broadcasts BLE advertising packets as **`Rust-ARCS`** from Rust using the SDK's
`lisa_bluetooth` stack (R8b). It exercises `arcs::Bluetooth`:

```rust
let bt = arcs::Bluetooth::init()?;   // lisa_bluetooth_init
bt.start_advertising()?;             // lisa_ble_adv_start(0, ADV_GEN)
```

## Advertising payload (weak-symbol override)

`lisa_bluetooth` pulls the advertising / scan-response bytes from two **weak**
C symbols. The sample provides them from Rust with `#[no_mangle]` overrides,
returning a hand-built AD byte array (flags + complete local name):

```rust
static ADV_DATA: [u8; 14] = [
    0x02, arcs::bt::AD_TYPE_FLAGS,
          arcs::bt::FLAG_LE_GENERAL | arcs::bt::FLAG_BREDR_NOT_SUPPORTED,
    0x0A, arcs::bt::AD_TYPE_NAME_COMPLETE, b'R',b'u',b's',b't',b'-',b'A',b'R',b'C',b'S',
];
// One manufacturer-specific AD structure (len=5, type=0xFF, 4 bytes).
static SCAN_RSP: [u8; 6] = [0x05, arcs::bt::AD_TYPE_MANUFACTURER, 0xAB, 0x0A, 0x01, 0x02];
#[no_mangle] pub extern "C" fn lisa_bt_get_adv_data(len: *mut u8) -> *const u8 { .. }
#[no_mangle] pub extern "C" fn lisa_bt_get_scan_rsp_data(len: *mut u8) -> *const u8 { .. }
```

> **Important:** the SDK assembles a **CONNECTABLE + SCANNABLE** advertising set
> for `ADV_GEN`, and the controller **rejects a scannable set whose
> scan-response buffer is NULL** — advertising then never goes on air (the
> `lisa_ble_adv_start` call still "succeeds", since it only queues the request).
> `lisa_bt_get_scan_rsp_data` must therefore return a real, non-NULL buffer.
> (This was an actual bug here: returning NULL made the device invisible to
> scanners despite the serial log saying "advertising".)

The BLE stack runs on the CP core (`CONFIG_ARCS_CP_CORE=y`). The NVS/flash
bring-up that backs the persistent BD address is sample-local C boilerplate in
`src/main.c`; it hands off to `rust_main()` once NVS is up.

## Build & flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/ble_adv -DBOARD=arcs_evb
# flash build/rust_ble_adv.bin with cskburn, then watch the serial console
```

Expected serial output:

```
=== Rust BLE advertise demo ===
ble_adv: bluetooth stack init
ble_adv:advertising as "Rust-ARCS"
```

Open a BLE scanner app (nRF Connect, LightBlue, …) on a phone: the device shows
up as **`Rust-ARCS`** and is connectable (verified on `arcs_evb` — scanned and
connected).
