/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LSFS_VFS_H_
#define _LSFS_VFS_H_



#ifdef __cplusplus
extern "C" {
#endif

    int lsfs_register_vfs(const char *path);
    int lsfs_unregister_vfs(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* _LSFS_FS_INTERFACE_H_ */
