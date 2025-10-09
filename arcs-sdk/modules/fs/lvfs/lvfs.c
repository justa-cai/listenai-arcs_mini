#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>
#include "fs_env/dlist.h"
#include "fs_env/fs_env.h"
#include "lvfs.h"

#if (CONFIG_LVFS_POSIX_RESERVE_FD >= CONFIG_LVFS_NFILE_DESC_PER_BLOCK)
#error "CONFIG_LVFS_NFILE_DESC_PER_BLOCK must be greater than CONFIG_LVFS_POSIX_RESERVE_FD"
#endif

static fs_env_mutex_handle_t fdtable_mutex_handle = {
	.mutex = NULL,
};

static fs_env_mutex_handle_t desc_list_mutex_handle = {
	.mutex = NULL,
};

typedef struct
{
    char path_prefix[CONFIG_LVFS_PREFIX_PATH_LENGTH_MAX + 1];
    lvfs_filesystem_type_e type;
    union lvfs_ops_u ops;

} lvfs_register_entry_t;

typedef struct
{
    lvfs_register_entry_t entry;
    sys_dnode_t node;

} lvfs_register_desc_t;

/* list of registered file systems */
static sys_dlist_t registered_desc_list;

typedef struct
{
    int refcount;
    lvfs_register_entry_t *pentry;
    lvfs_file_t vfile;

} lvfs_table_t;

typedef struct
{

    int num_of_files;

    lvfs_table_t *ftable;

} lvfs_fd_t;

lvfs_fd_t lvfs_fd;

static lvfs_register_desc_t *_find_register_desc(const char *path)
{
    lvfs_register_desc_t *desc;
    sys_dnode_t *node;
    bool is_found = false;

    /* Check if mount point already exists */
    SYS_DLIST_FOR_EACH_NODE(&registered_desc_list, node)
    {
        desc = CONTAINER_OF(node, lvfs_register_desc_t, node);

        if (strcmp(path, desc->entry.path_prefix) == 0)
        {
            is_found = true;
            break;
        }
    }

    if (is_found)
    {
        return desc;
    }

    return NULL;
}

static lvfs_register_desc_t *_find_register_desc_compare_mount(const char *path)
{
    lvfs_register_desc_t *desc;
    sys_dnode_t *node;
    bool is_found = false;

    fs_env_mutex_lock(&desc_list_mutex_handle,FS_ENV_MAX_DELAY);
    /* Check if mount point already exists */
    SYS_DLIST_FOR_EACH_NODE(&registered_desc_list, node)
    {
        desc = CONTAINER_OF(node, lvfs_register_desc_t, node);

        if (memcmp(path, desc->entry.path_prefix, strlen(desc->entry.path_prefix)) == 0)
        {
            is_found = true;
            break;
        }
    }

    fs_env_mutex_unlock(&desc_list_mutex_handle);

    if (is_found)
    {
        return desc;
    }

    return NULL;
}

int lvfs_register(const char *path, lvfs_file_ops_t *lvfs_ops)
{
    lvfs_register_desc_t *desc;
    int ret = 0;

    if ((path == NULL) || (lvfs_ops == NULL) || (strlen(path) > CONFIG_LVFS_PREFIX_PATH_LENGTH_MAX) || (path[0] != '/'))
    {
        return -EINVAL;
    }

    fs_env_mutex_lock(&desc_list_mutex_handle,FS_ENV_MAX_DELAY);
    if (NULL != _find_register_desc(path))
    {
        ret = -EBUSY;
        goto ERROR;
    }

    desc = FS_ENV_MEM_MALLOC(sizeof(lvfs_register_desc_t));
    if (desc == NULL)
    {
        ret = -ENOMEM;
        goto ERROR;
    }

    strcpy(desc->entry.path_prefix, path);
    memcpy(&desc->entry.ops, lvfs_ops, sizeof(lvfs_file_ops_t));

    sys_dlist_append(&registered_desc_list, &desc->node);
ERROR:

    fs_env_mutex_unlock(&desc_list_mutex_handle);
    return ret;
}

int lvfs_unregister(const char *path)
{

    lvfs_register_desc_t *desc;
    int ret = 0;

    if ((path == NULL))
    {
        return -EINVAL;
    }

    fs_env_mutex_lock(&desc_list_mutex_handle,FS_ENV_MAX_DELAY);
    desc = _find_register_desc(path);
    if (NULL == desc)
    {
        ret = -ENODEV;
        goto ERROR;
    }

    sys_dlist_remove(&desc->node);
    FS_ENV_MEM_FREE(desc);

ERROR:

    fs_env_mutex_unlock(&desc_list_mutex_handle);
    return ret;
}

static int _find_fd_entry(void)
{
    int fd;

    for (fd = 0; fd < lvfs_fd.num_of_files; fd++)
    {
        if (!__atomic_load_n(&lvfs_fd.ftable[fd].refcount, __ATOMIC_SEQ_CST))
        {
            return fd;
        }
    }

    errno = ENFILE;
    return -1;
}

static int _reserve_fd(void)
{
    int fd;

    fs_env_mutex_lock(&fdtable_mutex_handle,FS_ENV_MAX_DELAY);
    fd = _find_fd_entry();
    if (fd >= 0)
    {
        __atomic_fetch_add(&lvfs_fd.ftable[fd].refcount, 1, __ATOMIC_SEQ_CST);
        lvfs_fd.ftable[fd].pentry = NULL;
    }

    fs_env_mutex_unlock(&fdtable_mutex_handle);

    return fd;
}

static int _fd_unref(int fd)
{
    int old_rc;

    do
    {
        old_rc = __atomic_load_n(&lvfs_fd.ftable[fd].refcount, __ATOMIC_SEQ_CST);
        if (!old_rc)
        {
            return 0;
        }
    } while (!__atomic_compare_exchange_n(&lvfs_fd.ftable[fd].refcount, &old_rc, old_rc - 1, 0,
                                          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

    if (old_rc != 1)
    {
        return old_rc - 1;
    }

    lvfs_fd.ftable[fd].pentry = NULL;

    return 0;
}
static void _free_fd(int fd)
{
    (void)_fd_unref(fd);
}

/*TODO*/
int _extend_fd(void)
{
    return -ENFILE;
}

int lvfs_open(const char *path, int flags, int mode)
{
    int fd;
    int ret = 0;
    lvfs_register_desc_t *desc;

    if ((path == NULL))
    {
        return -EINVAL;
    }

    desc = _find_register_desc_compare_mount(path);
    if (!desc)
    {
        return -ENODEV;
    }

    fd = _reserve_fd();
    if (fd < 0)
    {
        ret = _extend_fd();
        if (ret != 0)
        {
            return ret;
        }

        fd = _reserve_fd();
        if (fd < 0)
        {
            return fd;
        }
    }

    fs_env_mutex_lock(&fdtable_mutex_handle,FS_ENV_MAX_DELAY);
    lvfs_fd.ftable[fd].pentry = &desc->entry;

    fs_env_mutex_unlock(&fdtable_mutex_handle);

    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.open)
    {
        _free_fd(fd);
        return -EOPNOTSUPP;
    }

    ret = lvfs_fd.ftable[fd].pentry->ops.f_ops.open(&lvfs_fd.ftable[fd].vfile, path, flags, mode);
    if (ret < 0)
    {
        _free_fd(fd);
        return ret;
    }

    return fd;
}

int lvfs_close(int fd)
{
    int ret = 0;

    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.close)
    {
        _free_fd(fd);
        return -EOPNOTSUPP;
    }
    ret = lvfs_fd.ftable[fd].pentry->ops.f_ops.close(&lvfs_fd.ftable[fd].vfile);
    _free_fd(fd);
    return ret;
}

int lvfs_read(int fd, char *buffer, size_t buflen)
{
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.read)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.read(&lvfs_fd.ftable[fd].vfile, buffer, buflen);
}

int lvfs_write(int fd, const char *buffer, size_t buflen)
{
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.write)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.write(&lvfs_fd.ftable[fd].vfile, buffer, buflen);
}

int lvfs_lseek(int fd, off_t offset, int whence)
{
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.lseek)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.lseek(&lvfs_fd.ftable[fd].vfile, offset, whence);
}

int lvfs_lsize(int fd){
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.lsize)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.lsize(&lvfs_fd.ftable[fd].vfile);
}

#if CONFIG_FS_LARGE64_FILES
_off64_t lvfs_lseek64(int fd, _off64_t offset, int whence)
{
    _off64_t seek;
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.lseek64)
    {
        return -EOPNOTSUPP;
    }
    seek= lvfs_fd.ftable[fd].pentry->ops.f_ops.lseek64(&lvfs_fd.ftable[fd].vfile, offset, whence);
    return seek;
}

int64_t lvfs_lsize64(int fd){
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.lsize64)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.lsize64(&lvfs_fd.ftable[fd].vfile);
}
#endif

int lvfs_truncate(int fd, off_t length)
{
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.truncate)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.truncate(&lvfs_fd.ftable[fd].vfile, length);
}

int lvfs_sync(int fd)
{
    if (!lvfs_fd.ftable[fd].pentry->ops.f_ops.sync)
    {
        return -EOPNOTSUPP;
    }
    return lvfs_fd.ftable[fd].pentry->ops.f_ops.sync(&lvfs_fd.ftable[fd].vfile);
}

int lvfs_rename(const char *oldpath, const char *newpath)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!oldpath || !newpath)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(oldpath);
    if (!desc)
    {
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.rename)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.rename(oldpath, newpath);
    return ret;
}

int lvfs_unlink(const char *pathname)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!pathname)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(pathname);
    if (!desc)
    {
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.unlink)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.unlink(pathname);
    return ret;
}

int lvfs_stat(const char *pathname, struct stat *buf)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!pathname || !buf) {
        printf("[ERROR] lvfs_stat: invalid arguments\n");
        return -EINVAL;
    }

    desc = _find_register_desc_compare_mount(pathname);
    if (!desc)
    {
        printf("[ERROR] lvfs_stat: desc not found\n");
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.stat){
        printf("[ERROR] lvfs_stat: stat not supported\n");
        return -EOPNOTSUPP;
    }

    ret = desc->entry.ops.f_ops.stat(pathname, buf);
    return ret;
}

int lvfs_mkdir(const char *pathname, mode_t mode)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!pathname)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(pathname);
    if (!desc)
    {
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.mkdir)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.mkdir(pathname, mode);
    return ret;
}

int lvfs_getcwd(const char *mount_path, char *buf, size_t size)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;
    
    if (!mount_path || !buf)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(mount_path);
    if (!desc)
    {
        return -ENODEV ;
    }

    if (!desc->entry.ops.f_ops.getcwd)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.getcwd(mount_path, buf, size);
    return ret;
}

int lvfs_chdir(const char *pathname)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!pathname)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(pathname);
    if (!desc)
    {
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.chdir)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.chdir(pathname);
    return ret;
}

int lvfs_chdrive(const char *pathname)
{
    lvfs_register_desc_t *desc = NULL;
    int ret = 0;

    if (!pathname)
        return -EINVAL;

    desc = _find_register_desc_compare_mount(pathname);
    if (!desc)
    {
        return -ENODEV;
    }
    if (!desc || !desc->entry.ops.f_ops.chdrive)
        return -EOPNOTSUPP;

    ret = desc->entry.ops.f_ops.chdrive(pathname);
    return ret;
}

int lvfs_init(void)
{

    sys_dlist_init(&registered_desc_list);
    lvfs_fd.num_of_files = CONFIG_LVFS_NFILE_DESC_PER_BLOCK;
    lvfs_fd.ftable = FS_ENV_MEM_MALLOC(sizeof(lvfs_table_t) * CONFIG_LVFS_NFILE_DESC_PER_BLOCK);

    if (lvfs_fd.ftable == NULL)
    {
        lvfs_fd.num_of_files = 0;
        return -ENOMEM;
    }
    memset(lvfs_fd.ftable, 0, sizeof(lvfs_table_t) * CONFIG_LVFS_NFILE_DESC_PER_BLOCK);
    fs_env_mutex_create(&desc_list_mutex_handle);
    fs_env_mutex_create(&fdtable_mutex_handle);

#if (CONFIG_LVFS_POSIX_RESERVE_FD > 0)
    for(int i=0;i<CONFIG_LVFS_POSIX_RESERVE_FD;i++){
        _reserve_fd();
    }
#endif
    return 0;
}
