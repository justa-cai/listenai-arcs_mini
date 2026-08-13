//! Safe wrapper over `sys::audio` — blocking PCM playback + record control.
//!
//! Play: `configure_play(&PlayConfig)` → `play_start()` → `write`/`write_all`
//! (blocks while the driver's DMA pool is full) → `flush()` (wait for the
//! last buffer to drain) → `play_stop()`.
//! Record: `configure_record(&RecordConfig)` → `register_callback_raw` →
//! `record_start()` … `record_stop()`. Record data is delivered through the
//! unified callback (driver context — raw `unsafe` registration only).
//! embedded-hal has no audio trait, so this is an inherent API.

use crate::{sys, Error, Result};
use core::ffi::{c_void, CStr};
use core::ptr::NonNull;

pub use sys::audio::{
    AudioCallback, AudioEvent, AudioRecordChannelGain, BIT_16, BIT_24, BIT_32, CH_LEFT, CH_RIGHT,
    CH_STEREO, RATE_16K, RATE_48K, RATE_8K,
};

/// Playback configuration. `Default` = 16 kHz, mono, 16-bit, a 12 × 256-sample
/// DMA pool, 0 dB gain.
#[derive(Clone, Copy)]
pub struct PlayConfig {
    pub sample_rate: u32, // RATE_*
    pub channels: u32,    // CH_*
    pub sample_bits: u32, // BIT_*
    pub analog_gain: i8,  // dB
    pub digital_gain: i8, // dB
    pub buffer_count: u8,
    pub buffer_samples: u16,
}

impl Default for PlayConfig {
    fn default() -> Self {
        Self {
            sample_rate: RATE_16K,
            channels: CH_LEFT,
            sample_bits: BIT_16,
            analog_gain: 0,
            digital_gain: 0,
            buffer_count: 12,
            buffer_samples: 256,
        }
    }
}

/// Record configuration. `Default` = 16 kHz, mono, 16-bit, 0 dB gain,
/// HPF on, single-ended input.
#[derive(Clone, Copy)]
pub struct RecordConfig {
    pub sample_rate: u32, // RATE_*
    pub channels: u32,    // CH_*
    pub sample_bits: u32, // BIT_*
    pub analog_gain: i8,  // dB
    pub digital_gain: i8, // dB
    pub enable_hpf: bool,
    pub differential_input: bool,
}

impl Default for RecordConfig {
    fn default() -> Self {
        Self {
            sample_rate: RATE_16K,
            channels: CH_LEFT,
            sample_bits: BIT_16,
            analog_gain: 0,
            digital_gain: 0,
            enable_hpf: true,
            differential_input: false,
        }
    }
}

/// Record/echo phase alignment (skip samples on each path).
#[derive(Clone, Copy, Default)]
pub struct PhaseCompensation {
    pub record_skip_samples: u16,
    pub echo_skip_samples: u16,
}

/// Handle to an audio controller (e.g. `"audio0"`).
#[derive(Clone, Copy)]
pub struct Audio {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Audio {}
unsafe impl Sync for Audio {}

impl Audio {
    /// Open an audio device by name (e.g. `c"audio0"`).
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    /// Configure the playback path. Call before `play_start`.
    pub fn configure_play(&self, cfg: &PlayConfig) -> Result<()> {
        let c = sys::audio::AudioPlayConfig {
            format: sys::audio::AudioFormat {
                sample_rate: cfg.sample_rate,
                channels: cfg.channels,
                sample_bits: cfg.sample_bits,
            },
            gain: sys::audio::AudioGain {
                analog_gain: cfg.analog_gain,
                digital_gain: cfg.digital_gain,
            },
            buffer_count: cfg.buffer_count,
            buffer_samples: cfg.buffer_samples,
        };
        Error::from_c(unsafe { sys::audio::play_config(self.dev.as_ptr(), &c) })
    }

    /// Start the DAC.
    pub fn play_start(&self) -> Result<()> {
        Error::from_c(unsafe {
            sys::audio::play_control(
                self.dev.as_ptr(),
                sys::audio::IOCTL_PLAY_START,
                core::ptr::null_mut(),
            )
        })
    }

    /// Stop the DAC.
    pub fn play_stop(&self) -> Result<()> {
        Error::from_c(unsafe {
            sys::audio::play_control(
                self.dev.as_ptr(),
                sys::audio::IOCTL_PLAY_STOP,
                core::ptr::null_mut(),
            )
        })
    }

    /// Block until all written data has finished playing.
    pub fn flush(&self) -> Result<()> {
        Error::from_c(unsafe {
            sys::audio::play_control(
                self.dev.as_ptr(),
                sys::audio::IOCTL_PLAY_FLUSH,
                core::ptr::null_mut(),
            )
        })
    }

    /// Write 16-bit PCM samples; returns the number of samples accepted (may be
    /// fewer than `samples.len()`). Blocks while the DMA pool is full.
    pub fn write(&self, samples: &[i16]) -> Result<usize> {
        if samples.is_empty() {
            return Ok(0);
        }
        // play_write returns a non-negative count, so check the sign by hand
        // rather than routing through Error::from_c (which asserts code < 0).
        let ret = unsafe {
            sys::audio::play_write(
                self.dev.as_ptr(),
                samples.as_ptr() as *const c_void,
                samples.len() as u32,
            )
        };
        if ret < 0 {
            Err(Error::from_negative(ret))
        } else {
            Ok(ret as usize)
        }
    }

    /// Write every sample, looping over partial writes.
    pub fn write_all(&self, mut samples: &[i16]) -> Result<()> {
        while !samples.is_empty() {
            let n = self.write(samples)?;
            if n == 0 {
                return Err(Error::Io(0));
            }
            samples = &samples[n.min(samples.len())..];
        }
        Ok(())
    }

    /// Set play gain while running.
    pub fn play_set_gain(&self, analog_gain: i8, digital_gain: i8) -> Result<()> {
        let mut g = sys::audio::AudioGain {
            analog_gain,
            digital_gain,
        };
        Error::from_c(unsafe {
            sys::audio::play_control(
                self.dev.as_ptr(),
                sys::audio::IOCTL_PLAY_SET_GAIN,
                &mut g as *mut _ as *mut c_void,
            )
        })
    }

    /// Configure the record path. Call before `record_start`.
    pub fn configure_record(&self, cfg: &RecordConfig) -> Result<()> {
        let c = sys::audio::AudioRecordConfig {
            format: sys::audio::AudioFormat {
                sample_rate: cfg.sample_rate,
                channels: cfg.channels,
                sample_bits: cfg.sample_bits,
            },
            gain: sys::audio::AudioGain {
                analog_gain: cfg.analog_gain,
                digital_gain: cfg.digital_gain,
            },
            enable_hpf: cfg.enable_hpf,
            differential_input: cfg.differential_input,
        };
        Error::from_c(unsafe { sys::audio::record_config(self.dev.as_ptr(), &c) })
    }

    fn record_control(&self, cmd: u32, arg: *mut c_void) -> Result<()> {
        Error::from_c(unsafe { sys::audio::record_control(self.dev.as_ptr(), cmd, arg) })
    }

    /// Start the ADC; record data arrives via the registered callback.
    pub fn record_start(&self) -> Result<()> {
        self.record_control(sys::audio::IOCTL_RECORD_START, core::ptr::null_mut())
    }

    /// Stop the ADC.
    pub fn record_stop(&self) -> Result<()> {
        self.record_control(sys::audio::IOCTL_RECORD_STOP, core::ptr::null_mut())
    }

    /// Pause recording (keep configuration).
    pub fn record_pause(&self) -> Result<()> {
        self.record_control(sys::audio::IOCTL_RECORD_PAUSE, core::ptr::null_mut())
    }

    /// Resume a paused recording.
    pub fn record_resume(&self) -> Result<()> {
        self.record_control(sys::audio::IOCTL_RECORD_RESUME, core::ptr::null_mut())
    }

    /// Set uniform record gain while running.
    pub fn record_set_gain(&self, analog_gain: i8, digital_gain: i8) -> Result<()> {
        let mut g = sys::audio::AudioGain {
            analog_gain,
            digital_gain,
        };
        self.record_control(
            sys::audio::IOCTL_RECORD_SET_GAIN,
            &mut g as *mut _ as *mut c_void,
        )
    }

    /// Set left/right physical ADC/PDM record gains while running.
    pub fn record_set_channel_gain(
        &self,
        left_analog_gain: i8,
        left_digital_gain: i8,
        right_analog_gain: i8,
        right_digital_gain: i8,
    ) -> Result<()> {
        let mut g = sys::audio::AudioRecordChannelGain {
            left: sys::audio::AudioGain {
                analog_gain: left_analog_gain,
                digital_gain: left_digital_gain,
            },
            right: sys::audio::AudioGain {
                analog_gain: right_analog_gain,
                digital_gain: right_digital_gain,
            },
        };
        self.record_control(
            sys::audio::IOCTL_RECORD_SET_CHANNEL_GAIN,
            &mut g as *mut _ as *mut c_void,
        )
    }

    /// Set record/echo phase compensation.
    pub fn set_phase_compensation(&self, comp: PhaseCompensation) -> Result<()> {
        let mut c = sys::audio::AudioPhaseCompensation {
            record_skip_samples: comp.record_skip_samples,
            echo_skip_samples: comp.echo_skip_samples,
        };
        Error::from_c(unsafe {
            sys::audio::ioctl(
                self.dev.as_ptr(),
                sys::audio::IOCTL_SET_PHASE_COMPENSATION as u8,
                &mut c as *mut _ as *mut c_void,
            )
        })
    }

    /// Register the unified audio event callback (record + echo data).
    ///
    /// # Safety
    /// The callback runs in **driver context**: it must not panic, allocate,
    /// or block. `user_data` must stay valid until `unregister_callback_raw`.
    pub unsafe fn register_callback_raw(
        &self,
        cb: AudioCallback,
        user_data: *mut c_void,
    ) -> Result<()> {
        Error::from_c(unsafe { sys::audio::register_callback(self.dev.as_ptr(), cb, user_data) })
    }

    /// Unregister a previously registered audio event callback.
    ///
    /// # Safety
    /// `cb` must match the callback passed to `register_callback_raw`.
    pub unsafe fn unregister_callback_raw(&self, cb: AudioCallback) -> Result<()> {
        Error::from_c(unsafe { sys::audio::unregister_callback(self.dev.as_ptr(), cb) })
    }
}
