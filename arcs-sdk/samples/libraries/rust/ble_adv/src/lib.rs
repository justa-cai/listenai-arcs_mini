#![no_std]

extern crate alloc;

// BLE advertising payload, built by hand from AD structures (each is
// `len, type, data...`):
//   - Flags: LE General Discoverable + BR/EDR not supported
//   - Complete Local Name: "Rust-ARCS" (9 bytes)
// Legacy advertising payload max is 31 bytes; this is 14.
static ADV_DATA: [u8; 14] = [
    0x02,
    arcs::bt::AD_TYPE_FLAGS,
    arcs::bt::FLAG_LE_GENERAL | arcs::bt::FLAG_BREDR_NOT_SUPPORTED,
    0x0A,
    arcs::bt::AD_TYPE_NAME_COMPLETE,
    b'R',
    b'u',
    b's',
    b't',
    b'-',
    b'A',
    b'R',
    b'C',
    b'S',
];

/// Override the SDK's weak advertising-data provider. Invoked from the BT task
/// when assembling the advertising set.
///
/// # Safety
/// `len` must be a valid writable `*mut u8` (the stack always passes one).
#[no_mangle]
pub unsafe extern "C" fn lisa_bt_get_adv_data(len: *mut u8) -> *const u8 {
    *len = ADV_DATA.len() as u8;
    ADV_DATA.as_ptr()
}

// Scan-response payload: one manufacturer-specific AD structure
// (len=5, type=0xFF, data 0xAB 0x0A 0x01 0x02). The SDK assembles a
// CONNECTABLE+SCANNABLE adv set, and the controller rejects a scannable set
// whose scan-response buffer is NULL — so we must return a real (non-NULL)
// buffer here (matching the C broadcaster sample), or nothing advertises.
static SCAN_RSP: [u8; 6] = [0x05, arcs::bt::AD_TYPE_MANUFACTURER, 0xAB, 0x0A, 0x01, 0x02];

/// Override the weak scan-response provider.
///
/// # Safety
/// `len` must be a valid writable `*mut u8` (the stack always passes one).
#[no_mangle]
pub unsafe extern "C" fn lisa_bt_get_scan_rsp_data(len: *mut u8) -> *const u8 {
    *len = SCAN_RSP.len() as u8;
    SCAN_RSP.as_ptr()
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    let bt = arcs::Bluetooth::init()?;
    arcs::log::info!("ble_adv: bluetooth stack init");

    bt.start_advertising()?;
    arcs::log::info!("ble_adv:advertising as \"Rust-ARCS\"");

    Ok(())
}
