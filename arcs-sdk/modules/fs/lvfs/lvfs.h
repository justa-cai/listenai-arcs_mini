/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LVFS_H_
#define _LVFS_H_

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LVFS_FILESYSTEM_TYPE_UNKNOWN = 0,
        LVFS_FILESYSTEM_TYPE_FILE,

    } lvfs_filesystem_type_e;

    typedef struct
    {
        void *fl_private;
    } lvfs_file_t;

    typedef struct
    {
        int (*open)(lvfs_file_t *vfile, const char *relpath,
                    int oflags, mode_t mode);

        int (*close)(lvfs_file_t *vfile);
        ssize_t (*read)(lvfs_file_t *vfile, char *buffer,
                        size_t buflen);
        ssize_t (*write)(lvfs_file_t *vfile, const char *buffer,
                         size_t buflen);
        off_t (*lseek)(lvfs_file_t *vfile, off_t offset, int whence);
#if CONFIG_FS_LARGE64_FILES
        _off64_t (*lseek64)(lvfs_file_t *vfile, _off64_t offset, int whence);
        int64_t (*lsize64)(lvfs_file_t *vfile);
#endif
        int (*lsize)(lvfs_file_t *vfile);
        int (*truncate)(lvfs_file_t *vfile, off_t length);
        int (*sync)(lvfs_file_t *vfile);
        int (*rename)(const char *oldpath, const char *newpath);
        int (*unlink)(const char *pathname);
        int (*stat)(const char *pathname, struct stat *buf);
        int (*mkdir)(const char *pathname, mode_t mode);
        int (*getcwd)(const char *mountpath, char *buf, size_t size);
        int (*chdir)(const char *pathname);
        int (*chdrive)(const char *pathname);

    } lvfs_file_ops_t;

    union lvfs_ops_u
    {
        lvfs_file_ops_t f_ops; /* Driver operations for inode */
    };

    int lvfs_init(void);
    int lvfs_register(const char *path, lvfs_file_ops_t *lvfs_ops);
    int lvfs_unregister(const char *path);
    int lvfs_open(const char *path, int flags, int mode);
    int lvfs_close(int fd);
    int lvfs_read(int fd, char *buffer, size_t buflen);
    int lvfs_write(int fd, const char *buffer, size_t buflen);
    int lvfs_lseek(int fd, off_t offset, int whence);
    int lvfs_lsize(int fd);
#if CONFIG_FS_LARGE64_FILES
    _off64_t lvfs_lseek64(int fd, _off64_t offset, int whence);
    int64_t lvfs_lsize64(int fd);
#endif
    int lvfs_truncate(int fd, off_t length);
    int lvfs_sync(int fd);
    int lvfs_rename(const char *oldpath, const char *newpath);
    int lvfs_unlink(const char *pathname);
    int lvfs_stat(const char *pathname, struct stat *buf);
    int lvfs_mkdir(const char *pathname, mode_t mode);
    int lvfs_getcwd(const char *mount_path, char *buf, size_t size);
    int lvfs_chdir(const char *pathname);
    int lvfs_chdrive(const char *pathname);

#ifdef __cplusplus
}
#endif

#endif /* _LVFS_H_ */