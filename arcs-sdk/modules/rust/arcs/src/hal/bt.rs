//! Safe wrapper over `sys::bt` — BLE advertising.
//!
//! `Bluetooth::init()` brings up the stack; `start_advertising()` begins a
//! connectable+scannable advertising set. The advertising **payload** is not
//! set here — the application provides it by defining the weak C symbols
//! `lisa_bt_get_adv_data` / `lisa_bt_get_scan_rsp_data` (see the `ble_adv`
//! sample). NVS/flash bring-up for the BD address is platform C glue done
//! before `Bluetooth::init()`. There is no embedded-hal BLE trait, so this is
//! an inherent API.

use crate::sys::bt as sys;
use crate::{Error, Result};
use core::sync::atomic::{AtomicBool, AtomicU16, Ordering};

pub use crate::sys::bt::{
    ADV_DIR, ADV_DIR_HDC, ADV_GEN, ADV_GEN_PAIRED, AD_TYPE_APPEARANCE, AD_TYPE_FLAGS,
    AD_TYPE_MANUFACTURER, AD_TYPE_NAME_COMPLETE, AD_TYPE_NAME_SHORT, AD_TYPE_SERVICE_DATA,
    AD_TYPE_TX_POWER, FLAG_BREDR_NOT_SUPPORTED, FLAG_LE_GENERAL, FLAG_LE_LIMITED,
    FLAG_SIMUL_LE_BREDR,
};

/// Map a `lisa_ble_adv_*` event-post result to `Result`. The local stack
/// returns `pdTRUE` (1) when the request was enqueued; `0` (`pdFALSE`) or
/// `0xff` (alloc failure) signal it could not be posted.
fn check(ret: u8) -> Result<()> {
    if ret == 1 {
        Ok(())
    } else {
        Err(Error::Io(ret as i32))
    }
}

// The enable-complete callback (`lisa_ble_notify_enable_cmp`) carries no
// user-data pointer, so readiness is relayed through these globals. Init is a
// one-time, single-threaded bring-up, so a plain atomic flag suffices.
static BT_READY: AtomicBool = AtomicBool::new(false);
static BT_STATUS: AtomicU16 = AtomicU16::new(0);

/// `lisa_bluetooth_enable_cmp_cb_t` trampoline — runs on the BT task.
unsafe extern "C" fn on_enable_cmp(status: u16) {
    BT_STATUS.store(status, Ordering::SeqCst);
    BT_READY.store(true, Ordering::SeqCst);
}

/// Handle to the (singleton) BLE stack.
#[derive(Clone, Copy)]
pub struct Bluetooth {
    _priv: (),
}

impl Bluetooth {
    /// Initialize the Bluetooth stack and block until it has finished enabling
    /// (the BT task + event queue are ready). Advertising started before this
    /// completes would race the not-yet-created event queue.
    pub fn init() -> Result<Self> {
        BT_READY.store(false, Ordering::SeqCst);
        Error::from_c(unsafe { sys::lisa_bluetooth_init(Some(on_enable_cmp)) })?;

        // Wait for the enable-complete callback (RF calibration can take ~1-2 s).
        let mut waited_ms = 0u32;
        while !BT_READY.load(Ordering::SeqCst) {
            if waited_ms >= 10_000 {
                return Err(Error::Timeout);
            }
            crate::Thread::sleep_ms(20);
            waited_ms += 20;
        }

        let status = BT_STATUS.load(Ordering::SeqCst);
        if status != 0 {
            return Err(Error::Io(status as i32));
        }
        Ok(Self { _priv: () })
    }

    /// Start the default advertising set (id 0) as general
    /// connectable+scannable.
    pub fn start_advertising(&self) -> Result<()> {
        self.start_advertising_set(0, sys::ADV_GEN)
    }

    /// Start advertising set `adv_id` of type `ADV_*`.
    pub fn start_advertising_set(&self, adv_id: u8, adv_type: u8) -> Result<()> {
        check(unsafe { sys::lisa_ble_adv_start(adv_id, adv_type) })
    }

    /// Stop advertising set `adv_id`.
    pub fn stop_advertising_set(&self, adv_id: u8) -> Result<()> {
        check(unsafe { sys::lisa_ble_adv_stop(adv_id) })
    }

    /// Stop the default advertising set (id 0).
    pub fn stop_advertising(&self) -> Result<()> {
        self.stop_advertising_set(0)
    }
}
