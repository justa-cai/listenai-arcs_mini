#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdarg.h>

#include "lvfs.h"

int open(const char *name, int flags, ...)
{
	int mode = 0;
	va_list args;

	if ((flags & O_CREAT) != 0)
	{
		va_start(args, flags);
		mode = va_arg(args, int);
		va_end(args);
	}

	return lvfs_open(name, flags, mode);
}

int close(int fd)
{
	return lvfs_close(fd);
}

ssize_t read(int fd, void *buf, size_t sz)
{
	ssize_t ret;

	ret = lvfs_read(fd, buf, sz);

	return ret;
}

ssize_t write(int fd, const void *buf, size_t sz)
{
	ssize_t ret;

	ret = lvfs_write(fd, buf, sz);

	return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
	off_t ret;

	ret = lvfs_lseek(fd, offset, whence);

	return ret;
}

#ifdef CONFIG_FS_LARGE64_FILES
_off64_t lseek64(int fd, _off64_t offset, int whence)
{
	_off64_t ret;
	ret = lvfs_lseek64(fd, offset, whence);
	return ret;
}
#endif

int ftruncate(int fd, off_t length)
{
	int ret;

	ret = lvfs_truncate(fd, length);

	return ret;
}

int fsync(int fd)
{
	int ret;

	ret = lvfs_sync(fd);

	return ret;
}

int link(const char *oldpath, const char *newpath)
{
	int ret;

	ret = lvfs_rename(oldpath, newpath);
	return ret;
}

int unlink(const char *pathname)
{
	int ret;

	ret = lvfs_unlink(pathname);
	return ret;
}

int stat(const char *pathname, struct stat *buf)
{
	int ret;

	ret = lvfs_stat(pathname, buf);

	return ret;
}

int mkdir(const char *pathname, mode_t mode)
{
	int ret;

	ret = lvfs_mkdir(pathname, mode);

	return ret;
}

int chdir(const char *pathname)
{
	int ret;

	ret = lvfs_chdir(pathname);

	return ret;
}

char* getcwd(char *buf, size_t size)
{
	int ret;
	ret = lvfs_getcwd(CONFIG_LVFS_POSIX_API_DEFAULT_MOUNT_PATH, buf, size);

	if (ret != 0)
	{
		return NULL;
	}

	return buf;
}

#if (CONFIG_LVFS_POSIX_API_ALIAS)

int _open() __attribute__((alias("open")));
int _close() __attribute__((alias("close")));
ssize_t _read() __attribute__((alias("read")));
ssize_t _write() __attribute__((alias("write")));
off_t _lseek() __attribute__((alias("lseek")));
#ifdef __LARGE64_FILES
_off64_t _lseek64() __attribute__((alias("lseek64")));
#endif
int _ftruncate() __attribute__((alias("ftruncate")));
int _fsync() __attribute__((alias("fsync")));
int _link() __attribute__((alias("link")));
int _unlink() __attribute__((alias("unlink")));
int _stat() __attribute__((alias("stat")));
int _mkdir() __attribute__((alias("mkdir")));
int _chdir() __attribute__((alias("chdir")));
int _getcwd() __attribute__((alias("getcwd")));

#endif
