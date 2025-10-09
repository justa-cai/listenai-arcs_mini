/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LSFS_FS_INTERFACE_H_
#define _LSFS_FS_INTERFACE_H_

#include <stdint.h>
#include "fs_env/dlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MAX_FILE_NAME) 
#define MAX_FILE_NAME 256
#endif 



typedef uint8_t lsfs_mode_t;
struct lsfs_mount_t;

struct lsfs_file_t {
    void *filep;
    const struct lsfs_mount_t *mp;
    lsfs_mode_t flags;
    /** Node for freeze management list */
    sys_dnode_t freeze_node;
};


struct lsfs_dir_t {
    void *dirp;
    const struct lsfs_mount_t *mp;
};


#ifdef __cplusplus
}
#endif

#endif /* _LSFS_FS_INTERFACE_H_ */
