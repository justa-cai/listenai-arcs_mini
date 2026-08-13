//! Safe wrapper over `sys::fs` — LVFS file I/O + LSFS mounting.
//!
//! Bring-up: `init()` (disk + lvfs + lsfs, once at boot) → `mount(...)`
//! (formats with `mkfs` + retries on first-boot mount failure when asked).
//! File I/O: `File::open/create` → `read`/`write`/`seek` → drop (auto close).
//!
//! Negative returns from the C layer are LVFS/LSFS errno-style codes; they
//! are surfaced as `Error::Io(code)` (not the lisa_device error space).

use crate::{sys, Error, Result};
use core::ffi::CStr;

/// Filesystem type for `mount`/`mkfs`.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum FsType {
    Fatfs,
}

impl FsType {
    fn to_c(self) -> core::ffi::c_int {
        match self {
            FsType::Fatfs => sys::fs::LSFS_FATFS,
        }
    }
}

/// `lseek` origin.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum SeekFrom {
    Start(u32),
    Current(i32),
    End(i32),
}

fn ret_to_result(code: core::ffi::c_int) -> Result<()> {
    if code == 0 {
        Ok(())
    } else {
        Err(Error::Io(code))
    }
}

/// Initialise the storage stack: disk driver + LVFS + LSFS. Call once at
/// boot before `mount`.
pub fn init() -> Result<()> {
    ret_to_result(unsafe { sys::fs::disk_init(core::ptr::null_mut()) })?;
    ret_to_result(unsafe { sys::fs::lvfs_init() })?;
    ret_to_result(unsafe { sys::fs::lsfs_init() })
}

/// Mount `device` (e.g. `c"NAND:"`) of type `fs` at `mnt_point`
/// (e.g. `c"/NAND:"`). With `format_on_fail`, a mount that fails *because
/// the volume has no filesystem* (`-ENODEV`, from FatFS `FR_NO_FILESYSTEM`)
/// triggers `mkfs` + one retry (first-boot flow). Every other error —
/// `-EBUSY` (already mounted), invalid arguments, slot exhaustion, I/O —
/// is returned unchanged and never reformats, so a populated volume is
/// never wiped.
///
/// `mnt_point` must be `'static`: the C glue stores the raw pointer in a
/// static `lsfs_mount_t` and LSFS dereferences it on every later path
/// lookup, for as long as the filesystem stays mounted (there is no
/// `unmount`, so that is effectively forever). Pass a `c"..."` literal or
/// other `'static` storage — a temporary or stack-backed `CStr` would be
/// dropped after this call returns, leaving LSFS with a dangling pointer.
/// `device` carries no such requirement: it is only borrowed for the
/// duration of this call.
pub fn mount(
    fs: FsType,
    device: &CStr,
    mnt_point: &'static CStr,
    format_on_fail: bool,
) -> Result<()> {
    let first = unsafe { sys::fs::arcs_rust_lsfs_mount(fs.to_c(), mnt_point.as_ptr()) };
    if first == 0 {
        return Ok(());
    }
    // Only reformat when the volume genuinely has no filesystem
    // (`-ENODEV`). Reformatting on any other failure — e.g. `-EBUSY` from a
    // remount, invalid args, or slot exhaustion — would destroy data on an
    // already-mounted or otherwise valid volume.
    if !format_on_fail || first != -sys::fs::ENODEV {
        return Err(Error::Io(first));
    }
    mkfs(fs, device)?;
    ret_to_result(unsafe { sys::fs::arcs_rust_lsfs_mount(fs.to_c(), mnt_point.as_ptr()) })
}

/// Format `device` (e.g. `c"NAND:"`) with filesystem `fs`.
pub fn mkfs(fs: FsType, device: &CStr) -> Result<()> {
    ret_to_result(unsafe {
        sys::fs::lsfs_mkfs(fs.to_c(), device.as_ptr(), core::ptr::null_mut(), 0)
    })
}

/// Remove a file.
pub fn unlink(path: &CStr) -> Result<()> {
    ret_to_result(unsafe { sys::fs::lvfs_unlink(path.as_ptr()) })
}

/// Create a directory.
pub fn mkdir(path: &CStr) -> Result<()> {
    ret_to_result(unsafe { sys::fs::lvfs_mkdir(path.as_ptr(), 0o777) })
}

/// Rename/move a file.
pub fn rename(old: &CStr, new: &CStr) -> Result<()> {
    ret_to_result(unsafe { sys::fs::lvfs_rename(old.as_ptr(), new.as_ptr()) })
}

/// An open LVFS file descriptor (closed on drop).
///
/// `read`/`write`/`seek` take `&mut self`: they advance the shared file
/// offset and mutate LVFS/LSFS state that has no per-fd lock, so the `&mut`
/// borrow is what stops two callers from corrupting the offset concurrently
/// (the struct is otherwise `Send`/`Sync`, holding only a raw `c_int`).
pub struct File {
    fd: core::ffi::c_int,
}

impl File {
    /// Open an existing file read-only.
    pub fn open(path: &CStr) -> Result<Self> {
        Self::open_with(path, sys::fs::O_RDONLY)
    }

    /// Create (or truncate) a file for writing.
    pub fn create(path: &CStr) -> Result<Self> {
        Self::open_with(
            path,
            sys::fs::O_WRONLY | sys::fs::O_CREAT | sys::fs::O_TRUNC,
        )
    }

    /// Open with explicit `O_*` flags from `sys::fs`.
    pub fn open_with(path: &CStr, flags: core::ffi::c_int) -> Result<Self> {
        let fd = unsafe { sys::fs::lvfs_open(path.as_ptr(), flags, 0o666) };
        if fd < 0 {
            Err(Error::Io(fd))
        } else {
            Ok(Self { fd })
        }
    }

    /// Read into `buf`; returns the number of bytes read (0 = EOF).
    pub fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        // LVFS/LSFS reject a zero-length request with `-EINVAL`; mirror the
        // standard I/O contract and report `Ok(0)` without entering C so
        // generic read loops behave correctly.
        if buf.is_empty() {
            return Ok(0);
        }
        let n = unsafe { sys::fs::lvfs_read(self.fd, buf.as_mut_ptr() as *mut _, buf.len()) };
        if n < 0 {
            Err(Error::Io(n))
        } else {
            Ok(n as usize)
        }
    }

    /// Write `buf`; returns the number of bytes written.
    pub fn write(&mut self, buf: &[u8]) -> Result<usize> {
        // LVFS/LSFS reject a zero-length request with `-EINVAL`; mirror the
        // standard I/O contract and report `Ok(0)` without entering C so
        // generic write loops behave correctly.
        if buf.is_empty() {
            return Ok(0);
        }
        let n = unsafe { sys::fs::lvfs_write(self.fd, buf.as_ptr() as *const _, buf.len()) };
        if n < 0 {
            Err(Error::Io(n))
        } else {
            Ok(n as usize)
        }
    }

    /// Reposition the file offset; returns the new absolute offset.
    ///
    /// `SeekFrom::Start` offsets above `i32::MAX` are rejected with
    /// `Error::InvalidArg` rather than silently wrapping to a negative
    /// `lvfs_lseek` offset.
    pub fn seek(&mut self, pos: SeekFrom) -> Result<u32> {
        let (offset, whence) = match pos {
            SeekFrom::Start(o) => (
                i32::try_from(o).map_err(|_| Error::InvalidArg)?,
                sys::fs::SEEK_SET,
            ),
            SeekFrom::Current(o) => (o, sys::fs::SEEK_CUR),
            SeekFrom::End(o) => (o, sys::fs::SEEK_END),
        };
        let r = unsafe { sys::fs::lvfs_lseek(self.fd, offset, whence) };
        if r < 0 {
            Err(Error::Io(r))
        } else {
            Ok(r as u32)
        }
    }

    /// Total file size in bytes.
    pub fn size(&self) -> Result<u32> {
        let r = unsafe { sys::fs::lvfs_lsize(self.fd) };
        if r < 0 {
            Err(Error::Io(r))
        } else {
            Ok(r as u32)
        }
    }

    /// Flush file data to storage.
    pub fn sync(&self) -> Result<()> {
        ret_to_result(unsafe { sys::fs::lvfs_sync(self.fd) })
    }

    /// Raw fd for interop with C code.
    pub fn as_raw_fd(&self) -> core::ffi::c_int {
        self.fd
    }
}

impl Drop for File {
    fn drop(&mut self) {
        unsafe { sys::fs::lvfs_close(self.fd) };
    }
}
