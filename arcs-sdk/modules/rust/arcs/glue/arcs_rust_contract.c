/*
 * arcs_rust_contract.c — C-side compile-time anchors for hand-written Rust
 * FFI bindings (constant dual-anchoring; the Rust twins live in
 * src/sys/audio.rs and src/sys/fs.rs as `const _: () = assert!(...)`).
 *
 * Each section is self-guarded by the Kconfig that makes its header
 * available, so this file can be added to build graphs unconditionally.
 * Sections compile (and thus anchor) whenever the matching subsystem is
 * enabled in the firmware build.
 */
#include <stdint.h>

/* keep the TU non-empty even when every section is disabled */
typedef int arcs_rust_contract_unused_t;

#if defined(CONFIG_LISA_AUDIO_DEVICE)
#include <stddef.h>

#include "lisa_audio.h"

_Static_assert(sizeof(lisa_audio_format_t) == 12, "audio format size contract");
_Static_assert(sizeof(lisa_audio_gain_t) == 2, "audio gain size contract");
_Static_assert(sizeof(lisa_audio_record_channel_gain_t) == 4,
               "audio record channel gain size contract");
_Static_assert(offsetof(lisa_audio_record_channel_gain_t, right) == 2,
               "audio record channel gain right offset");
_Static_assert(sizeof(lisa_audio_play_config_t) == 20, "audio play config size contract");
_Static_assert(offsetof(lisa_audio_play_config_t, gain) == 12, "play config gain offset");
_Static_assert(offsetof(lisa_audio_play_config_t, buffer_samples) == 16,
               "play config buffer_samples offset");
_Static_assert(sizeof(lisa_audio_record_config_t) == 16, "audio record config size contract");
_Static_assert(offsetof(lisa_audio_record_config_t, enable_hpf) == 14,
               "record config enable_hpf offset");
_Static_assert(sizeof(lisa_audio_event_t) == 16, "audio event size contract");
_Static_assert(offsetof(lisa_audio_event_t, record_samples) == 8,
               "audio event record_samples offset");
_Static_assert(sizeof(lisa_audio_phase_compensation_t) == 4, "phase compensation size contract");

_Static_assert(LISA_AUDIO_IOCTL_RECORD_START == 0x00, "record start cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_RECORD_STOP == 0x01, "record stop cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_RECORD_PAUSE == 0x02, "record pause cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_RECORD_RESUME == 0x03, "record resume cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_RECORD_SET_GAIN == 0x05, "record set gain cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_RECORD_SET_CHANNEL_GAIN == 0x09,
               "record set channel gain cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_PLAY_SET_GAIN == 0x22, "play set gain cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_SET_PHASE_COMPENSATION == 0x40, "set phase cmd contract");
_Static_assert(LISA_AUDIO_IOCTL_GET_PHASE_COMPENSATION == 0x41, "get phase cmd contract");

/* callback signature anchor — mirrors sys/audio.rs AudioCallback */
typedef void (*arcs_rust_expected_audio_cb_t)(const lisa_audio_event_t *event, void *user_data);
_Static_assert(_Generic((lisa_audio_callback_t)0,
                        arcs_rust_expected_audio_cb_t: 1,
                        default: 0) == 1,
               "lisa_audio_callback_t signature contract");
#endif /* CONFIG_LISA_AUDIO_DEVICE */

#if defined(CONFIG_LSFS)
#include <errno.h>

#include "lsfs.h"

_Static_assert(LSFS_FATFS == 0, "LSFS_FATFS contract");
/* FatFS FR_NO_FILESYSTEM -> -ENODEV is the only mount error that
 * hal::fs::mount() reformats on; mirrored in sys/fs.rs. */
_Static_assert(ENODEV == 19, "ENODEV contract");
#endif /* CONFIG_LSFS */

#if defined(CONFIG_LVFS)
#include <fcntl.h>
#include <unistd.h>

/* newlib open flags — mirrored in sys/fs.rs */
_Static_assert(O_RDONLY == 0x0000, "O_RDONLY contract");
_Static_assert(O_WRONLY == 0x0001, "O_WRONLY contract");
_Static_assert(O_RDWR == 0x0002, "O_RDWR contract");
_Static_assert(O_APPEND == 0x0008, "O_APPEND contract");
_Static_assert(O_CREAT == 0x0200, "O_CREAT contract");
_Static_assert(O_TRUNC == 0x0400, "O_TRUNC contract");
_Static_assert(O_EXCL == 0x0800, "O_EXCL contract");
_Static_assert(SEEK_SET == 0 && SEEK_CUR == 1 && SEEK_END == 2, "SEEK_* contract");
#endif /* CONFIG_LVFS */
