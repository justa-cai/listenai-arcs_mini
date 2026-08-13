//! FFI binding for drivers/lisa_audio/lisa_audio.h
//!
//! Play path: play_config / play_write / play_control (start/flush/stop/gain
//! via the IOCTL cmd codes). Record path: record_config / record_control
//! (start/stop/pause/resume/gain). Unified callback: register_callback /
//! unregister_callback (driver context — keep handlers short, no panic).
//! Generic ioctl for phase compensation. play_write copies into the driver's
//! buffer pool and blocks when the pool is full (DMA drains it).
//!
//! Hand-written **subset** (enum values as `const`) — intentionally NOT routed
//! through bindgen-drift, like sys/display.rs. Struct layouts are anchored on
//! the C side in glue/arcs_rust_contract.c (constant dual-anchoring).

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── enum values (C enum = 4-byte int → u32) ──────────────────────────
// lisa_audio_rate_t
pub const RATE_8K: u32 = 8000;
pub const RATE_16K: u32 = 16000;
pub const RATE_48K: u32 = 48000;
// lisa_audio_channel_t
pub const CH_LEFT: u32 = 0x01;
pub const CH_RIGHT: u32 = 0x02;
pub const CH_STEREO: u32 = 0x03;
// lisa_audio_bits_t (value = bytes per sample)
pub const BIT_16: u32 = 2;
pub const BIT_24: u32 = 3;
pub const BIT_32: u32 = 4;

// lisa_audio_ioctl_cmd_t — record control commands (passed to record_control)
pub const IOCTL_RECORD_START: u32 = 0x00;
pub const IOCTL_RECORD_STOP: u32 = 0x01;
pub const IOCTL_RECORD_PAUSE: u32 = 0x02;
pub const IOCTL_RECORD_RESUME: u32 = 0x03;
pub const IOCTL_RECORD_RESET: u32 = 0x04;
pub const IOCTL_RECORD_SET_GAIN: u32 = 0x05;
pub const IOCTL_RECORD_SET_CHANNEL_GAIN: u32 = 0x09;

// lisa_audio_ioctl_cmd_t — play control commands (passed to play_control)
pub const IOCTL_PLAY_START: u32 = 0x20;
pub const IOCTL_PLAY_STOP: u32 = 0x21;
pub const IOCTL_PLAY_SET_GAIN: u32 = 0x22;
pub const IOCTL_PLAY_FLUSH: u32 = 0x27;

// lisa_audio_ioctl_cmd_t — generic commands (passed to ioctl, cmd is u8)
pub const IOCTL_SET_PHASE_COMPENSATION: u32 = 0x40;
pub const IOCTL_GET_PHASE_COMPENSATION: u32 = 0x41;

#[repr(C)]
pub struct AudioFormat {
    pub sample_rate: u32, // lisa_audio_rate_t
    pub channels: u32,    // lisa_audio_channel_t
    pub sample_bits: u32, // lisa_audio_bits_t
}

#[repr(C)]
pub struct AudioGain {
    pub analog_gain: i8,
    pub digital_gain: i8,
}

#[repr(C)]
pub struct AudioRecordChannelGain {
    pub left: AudioGain,
    pub right: AudioGain,
}

#[repr(C)]
pub struct AudioPlayConfig {
    pub format: AudioFormat,
    pub gain: AudioGain,
    pub buffer_count: u8,
    pub buffer_samples: u16,
}

#[repr(C)]
pub struct AudioRecordConfig {
    pub format: AudioFormat,
    pub gain: AudioGain,
    pub enable_hpf: bool,
    pub differential_input: bool,
}

#[repr(C)]
pub struct AudioEvent {
    pub record_buffer: *const c_void,
    pub echo_buffer: *const c_void,
    pub record_samples: u32,
    pub echo_samples: u32,
}

/// `lisa_audio_phase_compensation_t` — record/echo skip-sample alignment.
#[repr(C)]
pub struct AudioPhaseCompensation {
    pub record_skip_samples: u16,
    pub echo_skip_samples: u16,
}

/// `lisa_audio_callback_t` — `void (*)(const lisa_audio_event_t *, void *)`.
pub type AudioCallback = unsafe extern "C" fn(*const AudioEvent, *mut c_void);

// ── api vtable (mirrors lisa_audio_api_t) ────────────────────────────
#[repr(C)]
pub struct AudioApi {
    pub register_callback:
        Option<unsafe extern "C" fn(*mut LisaDevice, Option<AudioCallback>, *mut c_void) -> c_int>,
    pub unregister_callback:
        Option<unsafe extern "C" fn(*mut LisaDevice, Option<AudioCallback>) -> c_int>,
    pub record_config:
        Option<unsafe extern "C" fn(*mut LisaDevice, *const AudioRecordConfig) -> c_int>,
    pub record_control: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *mut c_void) -> c_int>,
    pub play_config: Option<unsafe extern "C" fn(*mut LisaDevice, *const AudioPlayConfig) -> c_int>,
    pub play_write: Option<unsafe extern "C" fn(*mut LisaDevice, *const c_void, u32) -> c_int>,
    pub play_get_buffer:
        Option<unsafe extern "C" fn(*mut LisaDevice, *mut *mut c_void, u32) -> c_int>,
    pub play_control: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *mut c_void) -> c_int>,
    pub ioctl: Option<unsafe extern "C" fn(*mut LisaDevice, u8, *mut c_void) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid audio device; `cfg` points to an initialised `AudioPlayConfig`.
#[inline]
pub unsafe fn play_config(dev: *mut LisaDevice, cfg: *const AudioPlayConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).play_config } {
        Some(f) => unsafe { f(dev, cfg) },
        None => -6,
    }
}

/// Returns the number of samples written (>= 0) or a negative error code.
///
/// # Safety
/// `dev` valid audio device; `buffer` points to `samples` PCM samples.
#[inline]
pub unsafe fn play_write(dev: *mut LisaDevice, buffer: *const c_void, samples: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).play_write } {
        Some(f) => unsafe { f(dev, buffer, samples) },
        None => -6,
    }
}

/// Issue a play control command (`IOCTL_PLAY_*`).
///
/// # Safety
/// `dev` valid audio device; `arg` as required by `cmd` (NULL for start/stop/
/// flush).
#[inline]
pub unsafe fn play_control(dev: *mut LisaDevice, cmd: u32, arg: *mut c_void) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).play_control } {
        Some(f) => unsafe { f(dev, cmd, arg) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid audio device; `cfg` points to an initialised
/// `AudioRecordConfig`.
#[inline]
pub unsafe fn record_config(dev: *mut LisaDevice, cfg: *const AudioRecordConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).record_config } {
        Some(f) => unsafe { f(dev, cfg) },
        None => -6,
    }
}

/// Issue a record control command (`IOCTL_RECORD_*`).
///
/// # Safety
/// `dev` valid audio device; `arg` as required by `cmd` (NULL for start/stop/
/// pause/resume, `*const AudioGain` for SET_GAIN, or
/// `*const AudioRecordChannelGain` for SET_CHANNEL_GAIN).
#[inline]
pub unsafe fn record_control(dev: *mut LisaDevice, cmd: u32, arg: *mut c_void) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).record_control } {
        Some(f) => unsafe { f(dev, cmd, arg) },
        None => -6,
    }
}

/// Register the unified audio event callback.
///
/// # Safety
/// `dev` valid audio device. The callback runs in driver context: it must not
/// panic, allocate, or block; `user_data` must stay valid until unregistered.
#[inline]
pub unsafe fn register_callback(
    dev: *mut LisaDevice,
    cb: AudioCallback,
    user_data: *mut c_void,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).register_callback } {
        Some(f) => unsafe { f(dev, Some(cb), user_data) },
        None => -6,
    }
}

/// Unregister a previously registered audio event callback.
///
/// # Safety
/// `dev` valid audio device; `cb` must match the registered callback.
#[inline]
pub unsafe fn unregister_callback(dev: *mut LisaDevice, cb: AudioCallback) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).unregister_callback } {
        Some(f) => unsafe { f(dev, Some(cb)) },
        None => -6,
    }
}

/// Issue a generic ioctl (`IOCTL_SET/GET_PHASE_COMPENSATION`, …; cmd is u8).
///
/// # Safety
/// `dev` valid audio device; `arg` as required by `cmd`.
#[inline]
pub unsafe fn ioctl(dev: *mut LisaDevice, cmd: u8, arg: *mut c_void) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AudioApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).ioctl } {
        Some(f) => unsafe { f(dev, cmd, arg) },
        None => -6,
    }
}

// ── 常量双锚定（Rust 侧）：与 glue/arcs_rust_contract.c 同字面值 ──────
const _: () = assert!(core::mem::size_of::<AudioFormat>() == 12);
const _: () = assert!(core::mem::size_of::<AudioGain>() == 2);
const _: () = assert!(core::mem::size_of::<AudioRecordChannelGain>() == 4);
const _: () = assert!(core::mem::offset_of!(AudioRecordChannelGain, right) == 2);
const _: () = assert!(core::mem::size_of::<AudioPlayConfig>() == 20);
const _: () = assert!(core::mem::offset_of!(AudioPlayConfig, gain) == 12);
const _: () = assert!(core::mem::offset_of!(AudioPlayConfig, buffer_samples) == 16);
const _: () = assert!(core::mem::size_of::<AudioRecordConfig>() == 16);
const _: () = assert!(core::mem::offset_of!(AudioRecordConfig, enable_hpf) == 14);
const _: () = assert!(core::mem::size_of::<AudioEvent>() == 16);
const _: () = assert!(core::mem::offset_of!(AudioEvent, record_samples) == 8);
const _: () = assert!(core::mem::size_of::<AudioPhaseCompensation>() == 4);
