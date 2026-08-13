//! Bridge between the `log` crate facade and the ARCS easylogger backend.

use crate::{sys, Error, Result};
use core::fmt::Write;
use log::{Level, LevelFilter, Metadata, Record};

struct ArcsLogger;

impl log::Log for ArcsLogger {
    fn enabled(&self, _: &Metadata) -> bool {
        true
    }

    fn log(&self, r: &Record) {
        let level: u8 = match r.level() {
            Level::Error => sys::log::LEVEL_ERROR,
            Level::Warn => sys::log::LEVEL_WARN,
            Level::Info => sys::log::LEVEL_INFO,
            Level::Debug => sys::log::LEVEL_DEBUG,
            Level::Trace => sys::log::LEVEL_VERBOSE,
        };
        // Stack buffer — no allocator involvement on the log path so we can
        // log from within allocator failure scenarios.
        let mut buf = heapless::String::<256>::new();
        // Ignore the truncation error from write!: heapless::String stops at
        // capacity. A truncated log line is better than a panic.
        let _ = write!(buf, "{}", r.args());
        unsafe {
            sys::log::arcs_rust_log(level, c"rust".as_ptr(), buf.as_ptr() as _, buf.len());
        }
    }

    fn flush(&self) {}
}

static LOGGER: ArcsLogger = ArcsLogger;

/// Install the global logger. Idempotent across calls; second call is a no-op.
pub fn init() -> Result<()> {
    match log::set_logger(&LOGGER) {
        Ok(()) => {
            log::set_max_level(LevelFilter::Trace);
            Ok(())
        }
        Err(_) => Err(Error::Busy), // already set — treat as benign-ish
    }
}
