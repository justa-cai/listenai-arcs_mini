//! Safe wrapper over `sys::wifi` — station-mode scan / connect.
//!
//! The WiFi core must already be brought up (mac_manager + `lisa_wifi_init`
//! readiness) before `Wifi::init()`; that bring-up is platform glue done in C
//! (see the `wifi_scan` sample's `main.c`). The `wifi_manager` itself is a
//! global singleton, so `Wifi` is a zero-sized handle. There is no embedded-hal
//! WiFi trait, so this is an inherent API.

use crate::sys::wifi as sys;
use crate::{Error, Result};
use core::ffi::CStr;

pub use crate::sys::wifi::{
    ScanInfo, StaConfig, ENC_AUTO, ENC_OPEN, ENC_WEP, ENC_WPA2_ENTERPRISE, ENC_WPA2_PSK,
    ENC_WPA2_WPA3_PSK, ENC_WPA3_PSK, ENC_WPA_PSK, ENC_WPA_WPA2_PSK, STA_CONNECTED, STA_CONNECTING,
    STA_CONNECT_FAILED, STA_DISCONNECTED,
};

/// Interpret a fixed-size NUL-terminated C string field as `&str`
/// (lossy: returns `""` on non-UTF-8 or a missing terminator).
fn cstr_field(buf: &[u8]) -> &str {
    match CStr::from_bytes_until_nul(buf) {
        Ok(c) => c.to_str().unwrap_or(""),
        Err(_) => "",
    }
}

impl ScanInfo {
    /// The AP SSID as text.
    pub fn ssid(&self) -> &str {
        cstr_field(&self.ssid)
    }
    /// The AP BSSID as text (`"AA:BB:CC:DD:EE:FF"`).
    pub fn bssid(&self) -> &str {
        cstr_field(&self.bssid)
    }
}

/// Human-readable name for an `ENC_*` encryption mode.
pub fn encryption_name(mode: u32) -> &'static str {
    match mode {
        sys::ENC_AUTO => "AUTO",
        sys::ENC_OPEN => "OPEN",
        sys::ENC_WEP => "WEP",
        sys::ENC_WPA_PSK => "WPA-PSK",
        sys::ENC_WPA2_PSK => "WPA2-PSK",
        sys::ENC_WPA_WPA2_PSK => "WPA/WPA2-PSK",
        sys::ENC_WPA2_ENTERPRISE => "WPA2-ENT",
        sys::ENC_WPA3_PSK => "WPA3-PSK",
        sys::ENC_WPA2_WPA3_PSK => "WPA2/WPA3-PSK",
        _ => "UNKNOWN",
    }
}

/// Handle to the (singleton) WiFi station manager.
#[derive(Clone, Copy)]
pub struct Wifi {
    _priv: (),
}

impl Wifi {
    /// Initialize `wifi_manager` with the platform ops. The WiFi core must
    /// already be ready (see module docs).
    pub fn init() -> Result<Self> {
        let ops = unsafe { sys::wifi_mgr_ops_get() };
        if ops.is_null() {
            return Err(Error::NotFound);
        }
        Error::from_c(unsafe { sys::wifi_mgr_init(ops) })?;
        Ok(Self { _priv: () })
    }

    /// Enable station mode (required before scanning / connecting).
    pub fn sta_enable(&self) -> Result<()> {
        Error::from_c(unsafe { sys::wifi_mgr_sta_enable() })
    }

    /// Disable station mode (also stops auto-connect).
    pub fn sta_disable(&self) -> Result<()> {
        Error::from_c(unsafe { sys::wifi_mgr_sta_disable() })
    }

    /// Whether station mode is currently enabled.
    pub fn is_sta_enabled(&self) -> bool {
        unsafe { sys::wifi_mgr_sta_is_enable() }
    }

    /// Blocking scan. Fills `out` and returns the number of APs found
    /// (capped at `out.len()`).
    pub fn scan(&self, out: &mut [ScanInfo]) -> Result<usize> {
        if out.is_empty() {
            return Ok(0);
        }
        // Returns a non-negative count, so sign-check by hand rather than
        // routing through Error::from_c (which asserts code < 0).
        let n = unsafe { sys::wifi_mgr_scan_ap(out.as_mut_ptr(), out.len() as u32, false) };
        if n < 0 {
            Err(Error::from_negative(n))
        } else {
            Ok((n as usize).min(out.len()))
        }
    }

    /// Connect to an AP by SSID + password (blocking). Encryption is
    /// auto-detected. DHCP / IP acquisition is a separate step (the network
    /// interface must be brought up in the connection callback in C).
    pub fn connect(&self, ssid: &str, password: &str) -> Result<()> {
        let mut cfg = StaConfig::default();
        copy_cstr(&mut cfg.ssid, ssid)?;
        copy_cstr(&mut cfg.pwd, password)?;
        cfg.encryption_mode = sys::ENC_AUTO;
        Error::from_c(unsafe { sys::wifi_mgr_sta_connect(&cfg, false) })
    }

    /// Disconnect from the current AP (blocking).
    pub fn disconnect(&self) -> Result<()> {
        Error::from_c(unsafe { sys::wifi_mgr_sta_disconnect(false) })
    }

    /// Current connection status (one of the `STA_*` constants).
    pub fn status(&self) -> u32 {
        unsafe { sys::wifi_mgr_sta_get_status() }
    }

    /// Fetch info about the currently connected AP.
    pub fn connected_info(&self) -> Result<StaConfig> {
        let mut info = StaConfig::default();
        Error::from_c(unsafe { sys::wifi_mgr_sta_get_connected_info(&mut info) })?;
        Ok(info)
    }
}

/// Copy `src` into a fixed C-string buffer, leaving room for the NUL.
fn copy_cstr(dst: &mut [u8], src: &str) -> Result<()> {
    let bytes = src.as_bytes();
    if bytes.len() >= dst.len() {
        return Err(Error::InvalidArg);
    }
    dst[..bytes.len()].copy_from_slice(bytes);
    dst[bytes.len()] = 0;
    Ok(())
}
