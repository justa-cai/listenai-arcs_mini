//! FFI binding for components/lisa_bluetooth (BLE advertising subset).
//!
//! Like wifi_manager, the LISA Bluetooth API is a **flat C function API** (no
//! `lisa_device` vtable). This binds the init + advertising-control surface;
//! the advertising *payload* is supplied by overriding the weak C symbols
//! `lisa_bt_get_adv_data` / `lisa_bt_get_scan_rsp_data` (done in the sample,
//! not here). The `BLE_AD_*` payload builders are C macros — their AD-type /
//! flag constants are replicated below so a payload byte array can be built in
//! Rust.
//!
//! Hand-written **subset** — intentionally NOT routed through bindgen-drift
//! (the `BLE_AD_*` macros and GAP enums are not callable symbols).

use core::ffi::c_int;

// enum lisa_ble_adv_type (passed as u8 to lisa_ble_adv_start)
pub const ADV_GEN: u8 = 0; // general connectable + scannable
pub const ADV_GEN_PAIRED: u8 = 1; // general, white-list filtered
pub const ADV_DIR: u8 = 2; // directed
pub const ADV_DIR_HDC: u8 = 3; // directed high-duty-cycle

// GAP AD-structure type bytes (lisa_bluetooth_gap.h)
pub const AD_TYPE_FLAGS: u8 = 0x01;
pub const AD_TYPE_NAME_SHORT: u8 = 0x08;
pub const AD_TYPE_NAME_COMPLETE: u8 = 0x09;
pub const AD_TYPE_TX_POWER: u8 = 0x0A;
pub const AD_TYPE_APPEARANCE: u8 = 0x19;
pub const AD_TYPE_SERVICE_DATA: u8 = 0x16;
pub const AD_TYPE_MANUFACTURER: u8 = 0xFF;

// GAP flags bitfield (the data byte of an AD_TYPE_FLAGS structure)
pub const FLAG_LE_LIMITED: u8 = 1 << 0;
pub const FLAG_LE_GENERAL: u8 = 1 << 1;
pub const FLAG_BREDR_NOT_SUPPORTED: u8 = 1 << 2;
pub const FLAG_SIMUL_LE_BREDR: u8 = 1 << 3;

/// `lisa_bluetooth_enable_cmp_cb_t` — ready callback, `status == 0` on success.
pub type EnableCmpCb = unsafe extern "C" fn(status: u16);

extern "C" {
    /// Initialize the LISA Bluetooth stack (creates the BT task). Returns 0 on
    /// success. `cb` (may be NULL) fires from the BT task when the stack is
    /// ready. Stack readiness is asynchronous; the control APIs below are
    /// event-queue based, so they may be called immediately after init.
    pub fn lisa_bluetooth_init(cb: Option<EnableCmpCb>) -> c_int;
    /// Queue an advertising-start request for set `adv_id`, type `ADV_*`.
    ///
    /// NOTE: despite the lisa_ble_api.h doc comment ("0 on success"), the
    /// local-stack implementation returns the FreeRTOS event-post result:
    /// `1` (`pdTRUE`) when the request was enqueued for the BT task, `0`
    /// (`pdFALSE`, queue full/missing) or `0xff` (alloc failure) on error.
    /// Actual advertising start/failure is reported asynchronously.
    pub fn lisa_ble_adv_start(adv_id: u8, adv_type: u8) -> u8;
    /// Queue an advertising-stop request for set `adv_id`. Same return
    /// convention as `lisa_ble_adv_start`.
    pub fn lisa_ble_adv_stop(adv_id: u8) -> u8;
}
