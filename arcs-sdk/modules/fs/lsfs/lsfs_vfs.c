#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <lvfs.h>

#include "lsfs.h"
#include "lsfs_sys.h"
#include "fs_env/fs_env.h"

static int lsfs_vfs_open(lvfs_file_t *vfile, const char *relpath,
                         int oflags, mode_t mode);

static int lsfs_vfs_close(lvfs_file_t *vfile);
static ssize_t lsfs_vfs_read(lvfs_file_t *vfile, char *buffer, size_t bufsize);
static ssize_t lsfs_vfs_write(lvfs_file_t *vfile, const char *buffer, size_t len);
static off_t lsfs_vfs_lseek(lvfs_file_t *vfile, off_t offset, int whence);
static int lsfs_vfs_lsize(lvfs_file_t *vfile);
#if CONFIG_FS_LARGE64_FILES
static _off64_t lsfs_vfs_lseek64(lvfs_file_t *vfile, _off64_t offset, int whence);
static int64_t lsfs_vfs_lsize64(lvfs_file_t *vfile);
#endif
static int lsfs_vfs_truncate(lvfs_file_t *vfile, off_t length);
static int lsfs_vfs_sync(lvfs_file_t *vfile);
static int lsfs_vfs_rename(const char *oldpath, const char *newpath);
static int lsfs_vfs_unlink(const char *pathname);
static int lsfs_vfs_stat(const char *pathname, struct stat *buf);
static int lsfs_vfs_mkdir(const char *pathname, mode_t mode);
static int lsfs_vfs_getcwd(const char *mountpath, char *buf, size_t size);
static int lsfs_vfs_chdir(const char *pathname);
static int lsfs_vfs_chdrive(const char *pathname);

static lvfs_file_ops_t vfs_ops = {
    .open = lsfs_vfs_open,
    .close = lsfs_vfs_close,
    .read = lsfs_vfs_read,
    .write = lsfs_vfs_write,
    .lseek = lsfs_vfs_lseek,
    .lsize = lsfs_vfs_lsize,
#if CONFIG_FS_LARGE64_FILES
    .lseek64 = lsfs_vfs_lseek64,
    .lsize64 = lsfs_vfs_lsize64,
#endif
    .truncate = lsfs_vfs_truncate,
    .sync = lsfs_vfs_sync,
    .rename = lsfs_vfs_rename,
    .unlink = lsfs_vfs_unlink,
    .stat = lsfs_vfs_stat,
    .mkdir = lsfs_vfs_mkdir,
    .getcwd = lsfs_vfs_getcwd,
    .chdir = lsfs_vfs_chdir,
    .chdrive = lsfs_vfs_chdrive
};

static int translate_flags_posix2lsfs(int mf)
{
    int mode = 0;

    mode |= (mf & O_CREAT) ? LSFS_O_CREATE : 0;
    mode |= (mf & O_APPEND) ? LSFS_O_APPEND : 0;
    mode |= (mf & O_TRUNC) ? LSFS_O_TRUNC : 0;

    switch (mf & O_ACCMODE)
    {
    case O_RDONLY:
        mode |= LSFS_O_READ;
        break;
    case O_WRONLY:
        mode |= LSFS_O_WRITE;
        break;
    case O_RDWR:
        mode |= LSFS_O_RDWR;
        break;
    default:
        break;
    }

    return mode;
}
static int lsfs_vfs_open(lvfs_file_t *vfile, const char *relpath,
                         int oflags, mode_t mode)
{
    struct lsfs_file_t *fp;
    int ret;

    if ((!vfile) || (!relpath))
    {
        return -EINVAL;
    }
    fp = FS_ENV_MEM_MALLOC(sizeof(struct lsfs_file_t));
    if (!fp)
    {
        return -ENOMEM;
    }

    lsfs_file_t_init(fp);

    ret = lsfs_open(fp, relpath, translate_flags_posix2lsfs(oflags));
    if (ret != 0)
    {
        FS_ENV_MEM_FREE(fp);
        return ret;
    }

    vfile->fl_private = fp;

    return ret;
}
static int lsfs_vfs_close(lvfs_file_t *vfile)
{
    struct lsfs_file_t *fp;
    int ret;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_close(fp);
    FS_ENV_MEM_FREE(fp);
    return ret;
}

ssize_t lsfs_vfs_read(lvfs_file_t *vfile, char *buffer, size_t bufsize)
{
    struct lsfs_file_t *fp;
    ssize_t ret;

    if ((!vfile) || (!vfile->fl_private) || (!buffer) || (!bufsize))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_read(fp, buffer, bufsize);

    return ret;
}

ssize_t lsfs_vfs_write(lvfs_file_t *vfile, const char *buffer, size_t len)
{
    struct lsfs_file_t *fp;
    ssize_t ret;

    if ((!vfile) || (!vfile->fl_private) || (!buffer) || (!len))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_write(fp, buffer, len);

    return ret;
}

static off_t lsfs_vfs_lseek(lvfs_file_t *vfile, off_t offset, int whence)
{
    struct lsfs_file_t *fp;
    ssize_t ret;
    int lsfs_whence;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    switch (whence)
    {
    case SEEK_SET:
        lsfs_whence = LSFS_SEEK_SET;
        break;
    case SEEK_CUR:
        lsfs_whence = LSFS_SEEK_CUR;
        break;
    case SEEK_END:
        lsfs_whence = LSFS_SEEK_END;
        break;
    default:
        return -EINVAL;
    }

    ret = lsfs_seek(fp, offset, lsfs_whence);
    if(ret == 0){
        ret = lsfs_tell(fp);
    }

    return ret;
}


static int lsfs_vfs_lsize(lvfs_file_t *vfile)
{
    struct lsfs_file_t *fp;
    int ret;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_lsize(fp);

    return ret;
}


#if CONFIG_FS_LARGE64_FILES
static _off64_t lsfs_vfs_lseek64(lvfs_file_t *vfile, _off64_t offset, int whence)
{
    struct lsfs_file_t *fp;
    _off64_t ret;
    int lsfs_whence;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    switch (whence)
    {
    case SEEK_SET:
        lsfs_whence = LSFS_SEEK_SET;
        break;
    case SEEK_CUR:
        lsfs_whence = LSFS_SEEK_CUR;
        break;
    case SEEK_END:
        lsfs_whence = LSFS_SEEK_END;
        break;
    default:
        return -EINVAL;
    }

    ret = lsfs_seek64(fp, offset, lsfs_whence);
    if(ret == 0){
        ret = lsfs_tell64(fp);
    }
    return ret;
}


static int64_t lsfs_vfs_lsize64(lvfs_file_t *vfile)
{
    struct lsfs_file_t *fp;
    int64_t ret;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_lsize64(fp);

    return ret;
}
#endif

static int lsfs_vfs_truncate(lvfs_file_t *vfile, off_t length)
{
    struct lsfs_file_t *fp;
    int ret;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_truncate(fp, length);

    return ret;
}

static int lsfs_vfs_sync(lvfs_file_t *vfile)
{
    struct lsfs_file_t *fp;
    int ret;

    if ((!vfile) || (!vfile->fl_private))
    {
        return -EINVAL;
    }

    fp = vfile->fl_private;
    ret = lsfs_sync(fp);

    return ret;
}

static int lsfs_vfs_rename(const char *oldpath, const char *newpath)
{
    int ret;

    ret = lsfs_rename(oldpath, newpath);

    return ret;
}

static int lsfs_vfs_unlink(const char *pathname)
{
    int ret;

    ret = lsfs_unlink(pathname);

    return ret;
}

static int lsfs_vfs_stat(const char *pathname, struct stat *buf)
{
    int ret;
    struct lsfs_dirent entry;

    ret = lsfs_stat(pathname, &entry);
    if (ret == 0) {
        // 类型映射
        if (entry.type == LSFS_DIR_ENTRY_FILE) {
            buf->st_mode = S_IFREG | 0444; // 普通文件，读权限
            buf->st_nlink = 1;
        } else if (entry.type == LSFS_DIR_ENTRY_DIR) {
            buf->st_mode = S_IFDIR | 0555; // 目录，读执行权限
            buf->st_nlink = 2;
        } else {
            buf->st_mode = 0;
            buf->st_nlink = 0;
        }
        buf->st_size = entry.size;
        // 其他字段可根据需要初始化
        buf->st_uid = 0;
        buf->st_gid = 0;
        buf->st_atime = 0;
        buf->st_mtime = 0;
        buf->st_ctime = 0;
        buf->st_rdev = 0;
        buf->st_blocks = 0;
        buf->st_spare4[0] = 0;
        buf->st_spare4[1] = 0;
    }

    return ret;
}

static int lsfs_vfs_mkdir(const char *pathname, mode_t mode)
{
    int ret;

    ret = lsfs_mkdir(pathname);

    return ret;
}

static int lsfs_vfs_getcwd(const char *mountpath, char *buf, size_t size)
{
    int ret;

    ret = lsfs_getcwd(mountpath, buf, size);

    return ret;
}

static int lsfs_vfs_chdir(const char *pathname)
{
    int ret;

    ret = lsfs_chdir(pathname);

    return ret;
}

static int lsfs_vfs_chdrive(const char *pathname)
{
    int ret;

    ret = lsfs_chdrive(pathname);

    return ret;
}

int lsfs_register_vfs(const char *path)
{

    int ret;

    ret = lvfs_register(path, &vfs_ops);

    return ret;
}

int lsfs_unregister_vfs(const char *path)
{
    int ret;

    ret = lvfs_unregister(path);

    return ret;
}
