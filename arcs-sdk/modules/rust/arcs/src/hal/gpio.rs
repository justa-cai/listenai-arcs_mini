//! Safe wrapper over `sys::gpio` + `sys::device`.

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;
use core::sync::atomic::{AtomicPtr, Ordering};

/// A handle to a GPIO controller (e.g. "gpioa").
///
/// The underlying `lisa_device_t*` is owned by the SDK device registry, so
/// `Gpio` does not free it on drop. The wrapper is `Clone + Copy`-shaped
/// rather than RAII — multiple `Gpio` values for the same device are fine.
#[derive(Clone, Copy)]
pub struct Gpio {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Gpio {}
unsafe impl Sync for Gpio {}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Level {
    Low,
    High,
}

#[derive(Default, Clone, Copy)]
pub struct OutputOpts {
    pub init_high: bool,
    pub pull_up: bool,
    pub pull_down: bool,
}

#[derive(Default, Clone, Copy)]
pub struct InputOpts {
    pub pull_up: bool,
    pub pull_down: bool,
    pub debounce: bool,
}

#[derive(Clone, Copy)]
pub enum IrqMode {
    Rising,
    Falling,
    Both,
    LevelHigh,
    LevelLow,
}

impl IrqMode {
    fn to_c(self) -> u32 {
        match self {
            Self::Rising => sys::gpio::IRQ_EDGE_RISING,
            Self::Falling => sys::gpio::IRQ_EDGE_FALLING,
            Self::Both => sys::gpio::IRQ_EDGE_BOTH,
            Self::LevelHigh => sys::gpio::IRQ_LEVEL_HIGH,
            Self::LevelLow => sys::gpio::IRQ_LEVEL_LOW,
        }
    }
}

impl Gpio {
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    pub fn configure_output(&self, pin: u32, opts: OutputOpts) -> Result<()> {
        let mut flags = sys::gpio::FLAG_OUTPUT
            | if opts.init_high {
                sys::gpio::FLAG_OUTPUT_INIT_HIGH
            } else {
                sys::gpio::FLAG_OUTPUT_INIT_LOW
            };
        if opts.pull_up {
            flags |= sys::gpio::FLAG_PULL_UP;
        }
        if opts.pull_down {
            flags |= sys::gpio::FLAG_PULL_DOWN;
        }
        Error::from_c(unsafe { sys::gpio::configure(self.dev.as_ptr(), pin, flags) })
    }

    pub fn configure_input(&self, pin: u32, opts: InputOpts) -> Result<()> {
        let mut flags = sys::gpio::FLAG_INPUT;
        if opts.pull_up {
            flags |= sys::gpio::FLAG_PULL_UP;
        }
        if opts.pull_down {
            flags |= sys::gpio::FLAG_PULL_DOWN;
        }
        if opts.debounce {
            flags |= sys::gpio::FLAG_DEBOUNCE;
        }
        Error::from_c(unsafe { sys::gpio::configure(self.dev.as_ptr(), pin, flags) })
    }

    pub fn write(&self, pin: u32, level: Level) -> Result<()> {
        let v = match level {
            Level::Low => 0,
            Level::High => 1,
        };
        Error::from_c(unsafe { sys::gpio::write_pin(self.dev.as_ptr(), pin, v) })
    }

    pub fn read(&self, pin: u32) -> Result<Level> {
        let v = unsafe { sys::gpio::read_pin(self.dev.as_ptr(), pin) };
        match v {
            0 => Ok(Level::Low),
            1 => Ok(Level::High),
            negative if negative < 0 => Err(Error::from_negative(negative)),
            _ => Err(Error::Io(v)),
        }
    }

    /// Register an IRQ handler. Returns a token; dropping it disables and
    /// un-registers the IRQ for safety.
    ///
    /// Implementation: leaks the boxed callback for the lifetime of
    /// `IrqRegistration` — Drop reclaims it via Box::from_raw.
    pub fn on_irq<F: FnMut(u32) + Send + 'static>(
        &self,
        pin: u32,
        mode: IrqMode,
        cb: F,
    ) -> Result<IrqRegistration> {
        let boxed: alloc::boxed::Box<dyn FnMut(u32) + Send + 'static> = alloc::boxed::Box::new(cb);
        let raw = alloc::boxed::Box::into_raw(alloc::boxed::Box::new(boxed));

        unsafe extern "C" fn trampoline(pin: u32, ud: *mut core::ffi::c_void) {
            let f: &mut alloc::boxed::Box<dyn FnMut(u32) + Send + 'static> =
                unsafe { &mut *(ud as *mut _) };
            f(pin);
        }

        let rc = unsafe {
            sys::gpio::configure_irq(
                self.dev.as_ptr(),
                pin,
                mode.to_c(),
                trampoline,
                raw as *mut core::ffi::c_void,
            )
        };
        if let Err(e) = Error::from_c(rc) {
            unsafe {
                drop(alloc::boxed::Box::from_raw(raw));
            }
            return Err(e);
        }
        let _ = unsafe { sys::gpio::enable_irq(self.dev.as_ptr(), pin) };

        Ok(IrqRegistration {
            dev: self.dev,
            pin,
            cb_raw: AtomicPtr::new(raw as *mut _),
        })
    }
}

pub struct IrqRegistration {
    dev: NonNull<sys::device::LisaDevice>,
    pin: u32,
    cb_raw: AtomicPtr<()>,
}

unsafe impl Send for IrqRegistration {}

impl Drop for IrqRegistration {
    fn drop(&mut self) {
        // Disable IRQ via configure_irq with a no-op callback, then reclaim
        // the boxed user callback.
        unsafe extern "C" fn noop(_pin: u32, _ud: *mut core::ffi::c_void) {}
        let _ = unsafe {
            sys::gpio::configure_irq(self.dev.as_ptr(), self.pin, 0, noop, core::ptr::null_mut())
        };
        let raw = self.cb_raw.swap(core::ptr::null_mut(), Ordering::AcqRel);
        if !raw.is_null() {
            let _: alloc::boxed::Box<alloc::boxed::Box<dyn FnMut(u32) + Send + 'static>> =
                unsafe { alloc::boxed::Box::from_raw(raw as *mut _) };
        }
    }
}

/// A single, pre-configured GPIO pin — the unit the `embedded-hal` digital
/// traits operate on. Obtain via [`Gpio::pin`]. Cheap to copy (holds a
/// Copy device handle + pin number).
#[derive(Clone, Copy)]
pub struct GpioPin {
    gpio: Gpio,
    pin: u32,
}

impl Gpio {
    /// Bind a single pin on this controller for embedded-hal digital traits.
    /// Configure direction first via `configure_output`/`configure_input`,
    /// or use `GpioPin::into_output`/`into_input`.
    pub fn pin(&self, pin: u32) -> GpioPin {
        GpioPin { gpio: *self, pin }
    }
}

impl GpioPin {
    /// Configure as output, builder-style.
    pub fn into_output(self, opts: OutputOpts) -> Result<Self> {
        self.gpio.configure_output(self.pin, opts)?;
        Ok(self)
    }
    /// Configure as input, builder-style.
    pub fn into_input(self, opts: InputOpts) -> Result<Self> {
        self.gpio.configure_input(self.pin, opts)?;
        Ok(self)
    }
    /// Read the current level.
    pub fn level(&self) -> Result<Level> {
        self.gpio.read(self.pin)
    }
}

#[cfg(feature = "embedded-hal")]
mod eh_digital {
    use super::{GpioPin, Level};
    use embedded_hal::digital::{ErrorType, InputPin, OutputPin, StatefulOutputPin};

    impl ErrorType for GpioPin {
        type Error = crate::Error;
    }

    impl OutputPin for GpioPin {
        fn set_low(&mut self) -> Result<(), Self::Error> {
            self.gpio.write(self.pin, Level::Low)
        }
        fn set_high(&mut self) -> Result<(), Self::Error> {
            self.gpio.write(self.pin, Level::High)
        }
    }

    impl StatefulOutputPin for GpioPin {
        fn is_set_high(&mut self) -> Result<bool, Self::Error> {
            Ok(self.gpio.read(self.pin)? == Level::High)
        }
        fn is_set_low(&mut self) -> Result<bool, Self::Error> {
            Ok(self.gpio.read(self.pin)? == Level::Low)
        }
    }

    impl InputPin for GpioPin {
        fn is_high(&mut self) -> Result<bool, Self::Error> {
            Ok(self.gpio.read(self.pin)? == Level::High)
        }
        fn is_low(&mut self) -> Result<bool, Self::Error> {
            Ok(self.gpio.read(self.pin)? == Level::Low)
        }
    }
}
