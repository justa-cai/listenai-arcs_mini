//! FFI binding for modules/wifi_manager (WiFi station scan/connect).
//!
//! Unlike the lisa_device drivers, `wifi_manager` is a **flat C function API**
//! (no `lisa_device` vtable / `arcs_rust_dev_get_api`). The lower bring-up
//! (`mac_manager`, `lisa_kv`, `lisa_wifi_init` async readiness) is platform
//! glue done in C; this module binds the station-mode manager surface:
//! init / enable / scan / connect / status.
//!
//! Hand-written **subset** (struct fields mirror the C layout; enum values as
//! `const`) — intentionally NOT routed through bindgen-drift, like
//! sys/display.rs and sys/audio.rs.

use core::ffi::{c_int, c_void};

// wifi_mgr_wifi_encryption_mode_t (C enum = 4-byte int → u32)
pub const ENC_AUTO: u32 = 0;
pub const ENC_OPEN: u32 = 1;
pub const ENC_WEP: u32 = 2;
pub const ENC_WPA_PSK: u32 = 3;
pub const ENC_WPA2_PSK: u32 = 4;
pub const ENC_WPA_WPA2_PSK: u32 = 5;
pub const ENC_WPA2_ENTERPRISE: u32 = 6;
pub const ENC_WPA3_PSK: u32 = 7;
pub const ENC_WPA2_WPA3_PSK: u32 = 8;
pub const ENC_UNKNOWN: u32 = 9;

// wifi_mgr_connection_status_t (C enum = 4-byte int → u32)
pub const STA_CONNECTED: u32 = 0;
pub const STA_CONNECTING: u32 = 1;
pub const STA_DISCONNECTED: u32 = 2;
pub const STA_CONNECT_FAILED: u32 = 3;

/// `wifi_mgr_scan_info_t` — one scan result (64 bytes; note the 2-byte pad
/// after `bssid[18]`, which `#[repr(C)]` reproduces).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ScanInfo {
    pub ssid: [u8; 32],
    pub bssid: [u8; 18],
    pub channel: c_int,
    pub rssi: c_int,
    pub encryption_mode: u32,
}

/// `wifi_mgr_sta_config_t` — connect credentials / connected-AP info
/// (164 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct StaConfig {
    pub ssid: [u8; 32],
    pub pwd: [u8; 64],
    pub bssid: [u8; 18],
    pub channel: c_int,
    pub rssi: c_int,
    pub encryption_mode: u32,
    pub pmk: [u8; 32],
    pub pmk_valid: u8,
}

impl Default for ScanInfo {
    fn default() -> Self {
        // SAFETY: all-zero is a valid POD value for this #[repr(C)] struct.
        unsafe { core::mem::zeroed() }
    }
}

impl Default for StaConfig {
    fn default() -> Self {
        // SAFETY: all-zero is a valid POD value for this #[repr(C)] struct.
        unsafe { core::mem::zeroed() }
    }
}

extern "C" {
    /// Platform default wifi_manager ops vtable (pass to `wifi_mgr_init`).
    pub fn wifi_mgr_ops_get() -> *mut c_void;
    pub fn wifi_mgr_init(ops: *mut c_void) -> c_int;
    pub fn wifi_mgr_deinit() -> c_int;
    pub fn wifi_mgr_sta_enable() -> c_int;
    pub fn wifi_mgr_sta_disable() -> c_int;
    pub fn wifi_mgr_sta_is_enable() -> bool;
    /// Returns the number of APs found (>= 0) or a negative errno.
    /// `ap_info` is a caller-allocated array of `size` entries.
    pub fn wifi_mgr_scan_ap(ap_info: *mut ScanInfo, size: u32, asynchronous: bool) -> c_int;
    pub fn wifi_mgr_sta_connect(sta_config: *const StaConfig, asynchronous: bool) -> c_int;
    pub fn wifi_mgr_sta_disconnect(asynchronous: bool) -> c_int;
    /// Returns a `wifi_mgr_connection_status_t` value (STA_*).
    pub fn wifi_mgr_sta_get_status() -> u32;
    pub fn wifi_mgr_sta_get_connected_info(sta_info: *mut StaConfig) -> c_int;
}
