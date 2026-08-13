//! FFI binding for the log shim.
//!
//! Source of truth: modules/rust/arcs/glue/arcs_rust_log.h
//!                  system/log/lisa_log.h (for lisa_log_init level constants)
//! Last synced against C header at commit: 1637394a (arcs_rust_log.h, A3 fix)

use core::ffi::{c_char, c_int};

// elog log levels — mirrors ELOG_LVL_* in modules/easylogger/easylogger/inc/elog.h
pub const LEVEL_ASSERT: u8 = 0;
pub const LEVEL_ERROR: u8 = 1;
pub const LEVEL_WARN: u8 = 2;
pub const LEVEL_INFO: u8 = 3;
pub const LEVEL_DEBUG: u8 = 4;
pub const LEVEL_VERBOSE: u8 = 5;

extern "C" {
    /// Forward a Rust-formatted message (non-NUL-terminated, explicit length)
    /// to easylogger via the arcs_rust_log C shim.
    pub fn arcs_rust_log(level: u8, tag: *const c_char, msg: *const c_char, msg_len: usize);

    /// One-time log subsystem init. Usually already called by SDK SYS_INIT
    /// before `main()`; call from Rust only for completeness.
    pub fn lisa_log_init() -> c_int;
}
