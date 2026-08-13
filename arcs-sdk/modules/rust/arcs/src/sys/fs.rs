//! FFI binding for modules/fs (lvfs.h / lsfs.h / disk_access.h subset).
//!
//! File I/O goes through the LVFS fd-based API (`lvfs_*` raw symbols — not
//! the optional POSIX alias layer, so the binding works regardless of
//! `CONFIG_LVFS_POSIX_API`). Mounting goes through the C glue helper
//! `arcs_rust_lsfs_mount` (glue/arcs_rust_fs.c) because `struct lsfs_mount_t`
//! embeds a kernel dlist node and must stay alive while mounted — the glue
//! owns static mount slots instead of mirroring that struct in Rust.
//!
//! Requires `CONFIG_LSFS` / `CONFIG_LVFS` (+ a disk backend such as
//! `CONFIG_DISK_DRIVER_FLASH`); link errors on these symbols mean the
//! configs are missing from prj.conf.
//!
//! Constants are dual-anchored in glue/arcs_rust_contract.c.

use core::ffi::{c_char, c_int, c_void};

// ── lsfs.h: filesystem type for mount/mkfs ───────────────────────────
pub const LSFS_FATFS: c_int = 0;

// ── newlib <fcntl.h> open flags (riscv32 newlib values) ──────────────
pub const O_RDONLY: c_int = 0x0000;
pub const O_WRONLY: c_int = 0x0001;
pub const O_RDWR: c_int = 0x0002;
pub const O_APPEND: c_int = 0x0008;
pub const O_CREAT: c_int = 0x0200;
pub const O_TRUNC: c_int = 0x0400;
pub const O_EXCL: c_int = 0x0800;

// ── <unistd.h> whence for lseek ───────────────────────────────────────
pub const SEEK_SET: c_int = 0;
pub const SEEK_CUR: c_int = 1;
pub const SEEK_END: c_int = 2;

// ── newlib <errno.h>: "no filesystem present" ─────────────────────────
// FatFS `f_mount` reports FR_NO_FILESYSTEM on a blank/foreign volume,
// which lsfs_fat's translate_error() maps to `-ENODEV`. `hal::fs::mount`
// treats this — and only this — as the first-boot case that `mkfs` fixes.
pub const ENODEV: c_int = 19;

extern "C" {
    // disk_access.h
    pub fn disk_init(dev: *mut c_void) -> c_int;

    // lvfs.h
    pub fn lvfs_init() -> c_int;
    pub fn lvfs_open(path: *const c_char, flags: c_int, mode: c_int) -> c_int;
    pub fn lvfs_close(fd: c_int) -> c_int;
    pub fn lvfs_read(fd: c_int, buffer: *mut c_char, buflen: usize) -> c_int;
    pub fn lvfs_write(fd: c_int, buffer: *const c_char, buflen: usize) -> c_int;
    pub fn lvfs_lseek(fd: c_int, offset: i32, whence: c_int) -> c_int;
    pub fn lvfs_lsize(fd: c_int) -> c_int;
    pub fn lvfs_truncate(fd: c_int, length: i32) -> c_int;
    pub fn lvfs_sync(fd: c_int) -> c_int;
    pub fn lvfs_rename(oldpath: *const c_char, newpath: *const c_char) -> c_int;
    pub fn lvfs_unlink(pathname: *const c_char) -> c_int;
    pub fn lvfs_mkdir(pathname: *const c_char, mode: u32) -> c_int;

    // lsfs.h
    pub fn lsfs_init() -> c_int;
    pub fn lsfs_mkfs(lsfs_type: c_int, dev: *const c_char, cfg: *mut c_void, flags: c_int)
        -> c_int;

    // glue/arcs_rust_fs.c — static-slot mount helper
    pub fn arcs_rust_lsfs_mount(lsfs_type: c_int, mnt_point: *const c_char) -> c_int;
}
