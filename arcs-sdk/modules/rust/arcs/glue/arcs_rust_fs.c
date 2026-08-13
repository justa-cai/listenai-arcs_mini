/*
 * arcs_rust_fs.c — LSFS mount helper for the Rust `arcs` crate.
 *
 * `struct lsfs_mount_t` embeds a kernel dlist node and must stay alive while
 * mounted, so the mount records are owned here as static slots instead of
 * mirroring the struct layout in Rust (sys/fs.rs declares the helper).
 *
 * Self-guarded: compiles to an empty TU unless CONFIG_LSFS is enabled, so it
 * can be added to build graphs unconditionally.
 */
#if defined(CONFIG_LSFS)

#include <stddef.h>
#include <string.h>

#include "lsfs.h"

#ifndef ARCS_RUST_FS_MOUNT_SLOTS
#define ARCS_RUST_FS_MOUNT_SLOTS 2
#endif

static struct lsfs_mount_t arcs_rust_fs_mounts[ARCS_RUST_FS_MOUNT_SLOTS];
static int arcs_rust_fs_mount_used[ARCS_RUST_FS_MOUNT_SLOTS];

/*
 * Mount `mnt_point` (e.g. "/NAND:") with filesystem `lsfs_type`.
 * Returns 0 on success, the negative lsfs_mount() error, or -1 when no
 * static slot is free. Remounting an already-used mount point returns the
 * lsfs_mount() result unchanged (lsfs handles duplicates).
 */
int arcs_rust_lsfs_mount(int lsfs_type, const char *mnt_point)
{
    if (mnt_point == NULL) {
        return -1;
    }
    for (int i = 0; i < ARCS_RUST_FS_MOUNT_SLOTS; i++) {
        /*
         * Atomically claim the slot (0 -> 1) before touching it. lsfs_mount()
         * serialises the global mount list with its own mutex, so the only
         * race we must close here is two callers selecting and populating the
         * same slot/dlist node: one could memset() a node the other has
         * already linked into the list. A CAS reserves the slot up front and
         * we release it again if the mount fails. (rv32 has the 'A' extension,
         * so this lowers to a native lock-free lr/sc — no RTOS lock or
         * init needed, and it is safe against task preemption.)
         */
        int expected = 0;
        if (!__atomic_compare_exchange_n(&arcs_rust_fs_mount_used[i], &expected, 1,
                                         0 /* strong */,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            continue; /* slot already taken */
        }
        memset(&arcs_rust_fs_mounts[i], 0, sizeof(arcs_rust_fs_mounts[i]));
        arcs_rust_fs_mounts[i].type = lsfs_type;
        arcs_rust_fs_mounts[i].mnt_point = mnt_point;
        arcs_rust_fs_mounts[i].fs_data = NULL;
        int ret = lsfs_mount(&arcs_rust_fs_mounts[i]);
        if (ret != 0) {
            /* mount failed: release the reservation so the slot is reusable */
            __atomic_store_n(&arcs_rust_fs_mount_used[i], 0, __ATOMIC_RELEASE);
        }
        return ret;
    }
    return -1; /* no free slot */
}

#else /* !CONFIG_LSFS */

/* keep the TU non-empty for pedantic toolchains */
typedef int arcs_rust_fs_unused_t;

#endif /* CONFIG_LSFS */
