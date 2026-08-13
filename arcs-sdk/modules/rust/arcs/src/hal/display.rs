//! Safe wrapper over `sys::display` + a PSRAM-backed framebuffer that
//! implements `embedded_graphics_core::DrawTarget` (RGB565).
//!
//! The bus/panel must already be attached (done in C via
//! `lisa_display_attach_bus` — a board-specific config struct); this layer
//! opens the ready `"display"` device and drives drawing/blitting. All ops are
//! blocking.

use crate::heap::{self, Zone};
use crate::{sys, Error, Result};
use core::alloc::Layout;
use core::ffi::{c_void, CStr};
use core::ptr::NonNull;

/// Handle to a display panel (e.g. `"display"`), with its dimensions cached.
pub struct Display {
    dev: NonNull<sys::device::LisaDevice>,
    width: u16,
    height: u16,
}

unsafe impl Send for Display {}
unsafe impl Sync for Display {}

impl Display {
    /// Open a display device whose bus/panel is already attached (e.g. by C
    /// `lisa_display_attach_bus`). Caches the panel size from its capabilities.
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        let dev = NonNull::new(p).ok_or(Error::NotFound)?;
        let mut caps = sys::display::DisplayCapabilities {
            width: 0,
            height: 0,
            pixel_format: 0,
            orientation: 0,
            supported_pixel_formats: 0,
        };
        Error::from_c(unsafe { sys::display::get_capabilities(dev.as_ptr(), &mut caps) })?;
        Ok(Self {
            dev,
            width: caps.width,
            height: caps.height,
        })
    }

    /// Panel width in pixels.
    pub fn width(&self) -> u16 {
        self.width
    }

    /// Panel height in pixels.
    pub fn height(&self) -> u16 {
        self.height
    }

    /// Turn the panel output off.
    pub fn blanking_on(&self) -> Result<()> {
        Error::from_c(unsafe { sys::display::blanking_on(self.dev.as_ptr()) })
    }

    /// Turn the panel output on.
    pub fn blanking_off(&self) -> Result<()> {
        Error::from_c(unsafe { sys::display::blanking_off(self.dev.as_ptr()) })
    }

    /// Set backlight brightness (0..=100).
    pub fn set_brightness(&self, brightness: u8) -> Result<()> {
        Error::from_c(unsafe { sys::display::set_brightness(self.dev.as_ptr(), brightness) })
    }

    /// Blit an RGB565 pixel buffer (`w * h` pixels) to the panel at `(x, y)`.
    pub fn write(&self, x: u16, y: u16, w: u16, h: u16, pixels: &[u16]) -> Result<()> {
        let desc = sys::display::DisplayBufferDesc {
            width: w,
            height: h,
            pitch: 0,
            buf_size: (pixels.len() * 2) as u32,
        };
        Error::from_c(unsafe {
            sys::display::write(
                self.dev.as_ptr(),
                x,
                y,
                &desc,
                pixels.as_ptr() as *const c_void,
            )
        })
    }
}

/// A full-screen RGB565 framebuffer in PSRAM (allocated via the dual allocator)
/// that implements `embedded_graphics_core::DrawTarget`. Render with
/// `embedded-graphics`, then [`FrameBuffer::flush`] to push it to the panel.
#[cfg(feature = "embedded-hal")]
pub struct FrameBuffer<'d> {
    display: &'d Display,
    buf: NonNull<u16>,
    len: usize, // pixel count
    w: u16,
    h: u16,
}

#[cfg(feature = "embedded-hal")]
impl<'d> FrameBuffer<'d> {
    /// Allocate a full-screen (display-sized) framebuffer in PSRAM, cleared to
    /// black.
    pub fn new(display: &'d Display) -> Result<Self> {
        let (w, h) = (display.width(), display.height());
        let len = w as usize * h as usize;
        let layout = Layout::from_size_align(len * 2, 4).map_err(|_| Error::InvalidArg)?;
        let p = unsafe { heap::alloc_in(Zone::Psram, layout) } as *mut u16;
        let buf = NonNull::new(p).ok_or(Error::NoMemory)?;
        unsafe { core::ptr::write_bytes(buf.as_ptr(), 0, len) };
        Ok(Self {
            display,
            buf,
            len,
            w,
            h,
        })
    }

    fn as_slice(&self) -> &[u16] {
        unsafe { core::slice::from_raw_parts(self.buf.as_ptr(), self.len) }
    }

    /// Push the whole framebuffer to the panel.
    pub fn flush(&self) -> Result<()> {
        self.display.write(0, 0, self.w, self.h, self.as_slice())
    }
}

#[cfg(feature = "embedded-hal")]
impl Drop for FrameBuffer<'_> {
    fn drop(&mut self) {
        let layout = Layout::from_size_align(self.len * 2, 4).unwrap();
        unsafe { heap::dealloc_in(Zone::Psram, self.buf.as_ptr() as *mut u8, layout) };
    }
}

#[cfg(feature = "embedded-hal")]
mod eg_impls {
    use super::FrameBuffer;
    use embedded_graphics_core::pixelcolor::{IntoStorage, Rgb565};
    use embedded_graphics_core::prelude::*;

    impl OriginDimensions for FrameBuffer<'_> {
        fn size(&self) -> Size {
            Size::new(self.w as u32, self.h as u32)
        }
    }

    impl DrawTarget for FrameBuffer<'_> {
        type Color = Rgb565;
        type Error = crate::Error;

        fn draw_iter<I>(&mut self, pixels: I) -> Result<(), Self::Error>
        where
            I: IntoIterator<Item = Pixel<Self::Color>>,
        {
            let w = self.w as i32;
            let h = self.h as i32;
            for Pixel(p, color) in pixels {
                if p.x >= 0 && p.x < w && p.y >= 0 && p.y < h {
                    let idx = p.y as usize * self.w as usize + p.x as usize;
                    unsafe { *self.buf.as_ptr().add(idx) = color.into_storage() };
                }
            }
            Ok(())
        }
    }
}
