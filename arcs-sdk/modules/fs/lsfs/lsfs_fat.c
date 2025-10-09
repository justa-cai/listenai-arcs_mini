/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ff.h>
#include "lsfs.h"
#include "lsfs_sys.h"
#include "fs_env/fs_env.h"

static int translate_error(int error)
{
	switch (error) {
	case FR_OK:
		return 0;
	case FR_NO_FILE:
	case FR_NO_PATH:
	case FR_INVALID_NAME:
		return -ENOENT;
	case FR_DENIED:
		return -EACCES;
	case FR_EXIST:
		return -EEXIST;
	case FR_INVALID_OBJECT:
		return -EBADF;
	case FR_WRITE_PROTECTED:
		return -EROFS;
	case FR_INVALID_DRIVE:
	case FR_NOT_ENABLED:
	case FR_NO_FILESYSTEM:
		return -ENODEV;
	case FR_NOT_ENOUGH_CORE:
		return -ENOMEM;
	case FR_TOO_MANY_OPEN_FILES:
		return -EMFILE;
	case FR_INVALID_PARAMETER:
		return -EINVAL;
	case FR_LOCKED:
	case FR_TIMEOUT:
	case FR_MKFS_ABORTED:
	case FR_DISK_ERR:
	case FR_INT_ERR:
	case FR_NOT_READY:
		return -EIO;
	}

	return -EIO;
}


static const char *translate_path(const char *path)
{
	/* this is guaranteed by the lsfs subsystem */
	if(path[0] != '/'){
        return NULL;
    }

	return &path[1];
}

static void append_slash(char *path)
{
	if (path[0] != '/') {
		path[0] = '/';
	}
}

static uint8_t translate_flags(lsfs_mode_t flags)
{
	uint8_t fat_mode = 0;

	fat_mode |= (flags & LSFS_O_READ) ? FA_READ : 0;
	fat_mode |= (flags & LSFS_O_WRITE) ? FA_WRITE : 0;
	fat_mode |= (flags & LSFS_O_CREATE) ? FA_OPEN_ALWAYS : 0;
	fat_mode |= (flags & LSFS_O_TRUNC) ? FA_CREATE_ALWAYS : 0;

	return fat_mode;
}

static int fatfs_open(struct lsfs_file_t *fp, const char *file_name,
		      lsfs_mode_t mode)
{
	FRESULT res;
	uint8_t fs_mode;
	void *ptr;

    ptr = FS_ENV_MEM_MALLOC(sizeof(FIL));
	if (ptr != 0) {
		(void)memset(ptr, 0, sizeof(FIL));
		fp->filep = ptr;
	} else {
		return -ENOMEM;
	}

	fs_mode = translate_flags(mode);

	res = f_open(fp->filep, translate_path(file_name), fs_mode);

	if (res != FR_OK) {
		FS_ENV_MEM_FREE(ptr);
		fp->filep = NULL;
	}

	return translate_error(res);
}

static int fatfs_close(struct lsfs_file_t *fp)
{
	FRESULT res;

	res = f_close(fp->filep);

	/* Free file ptr memory */
	FS_ENV_MEM_FREE(fp->filep);
	fp->filep = NULL;

	return translate_error(res);
}

static int fatfs_unlink(struct lsfs_mount_t *mountp, const char *path)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	res = f_unlink(translate_path(path));

	res = translate_error(res);
#endif

	return res;
}

static int fatfs_rename(struct lsfs_mount_t *mountp, const char *from,
			const char *to)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	FILINFO fno;

	/* Check if 'to' path exists; remove it if it does */
	res = f_stat(translate_path(to), &fno);
	if (res == FR_OK) {
		res = f_unlink(translate_path(to));
		if (res != FR_OK) {
			return translate_error(res);
		}
	}

	res = f_rename(translate_path(from), translate_path(to));
	res = translate_error(res);
#endif

	return res;
}

static ssize_t fatfs_read(struct lsfs_file_t *fp, void *ptr, size_t size)
{
	FRESULT res;
	unsigned int br;

	res = f_read(fp->filep, ptr, size, &br);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return br;
}

static ssize_t fatfs_write(struct lsfs_file_t *fp, const void *ptr, size_t size)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	unsigned int bw;
	off_t pos = f_size((FIL *)fp->filep);
	res = FR_OK;

	/* FA_APPEND flag means that file has been opened for append.
	 * The FAT FS write does not support the POSIX append semantics,
	 * to always write at the end of file, so set file position
	 * at the end before each write if FA_APPEND is set.
	 */
	if (fp->flags & LSFS_O_APPEND) {
		res = f_lseek(fp->filep, pos);
	}

	if (res == FR_OK) {
		res = f_write(fp->filep, ptr, size, &bw);
	}

	if (res != FR_OK) {
		res = translate_error(res);
	} else {
		res = bw;
	}
#endif

	return res;
}

static int fatfs_seek(struct lsfs_file_t *fp, off_t offset, int whence)
{
	FRESULT res = FR_OK;
	off_t pos;

	switch (whence) {
	case LSFS_SEEK_SET:
		pos = offset;
		break;
	case LSFS_SEEK_CUR:
		pos = f_tell((FIL *)fp->filep) + offset;
		break;
	case LSFS_SEEK_END:
		pos = f_size((FIL *)fp->filep) + offset;
		break;
	default:
		return -EINVAL;
	}

	if ((pos < 0) || (pos > f_size((FIL *)fp->filep))) {
		return -EINVAL;
	}

	res = f_lseek(fp->filep, pos);

	return translate_error(res);
}


static ssize_t fatfs_size(struct lsfs_file_t *fp)
{
	return f_size((FIL *)fp->filep);
}


#if CONFIG_FS_LARGE64_FILES
static int fatfs_seek64(struct lsfs_file_t *fp, _off64_t offset, int whence)
{
	FRESULT res = FR_OK;
	_off64_t pos;

	switch (whence) {
	case LSFS_SEEK_SET:
		pos = offset;
		break;
	case LSFS_SEEK_CUR:
		pos = f_tell((FIL *)fp->filep) + offset;
		break;
	case LSFS_SEEK_END:
		pos = f_size((FIL *)fp->filep) + offset;
		break;
	default:
		return -EINVAL;
	}
	if ((pos < 0) || (pos > f_size((FIL *)fp->filep))) {
		return -EINVAL;
	}

	res = f_lseek(fp->filep, pos);


	return translate_error(res);
	
}

static _off64_t fatfs_tell64(struct lsfs_file_t *fp)
{
	return f_tell((FIL *)fp->filep);
}


static int64_t fatfs_size64(struct lsfs_file_t *fp)
{
	return f_size((FIL *)fp->filep);
}


static int fatfs_truncate64(struct lsfs_file_t *fp, _off64_t length)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	_off64_t cur_length = f_size((FIL *)fp->filep);

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(fp->filep, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(fp->filep);
	} else {
		/*
		 * Get actual length after expansion. This could be
		 * less if there was not enough space in the volume
		 * to expand to the requested length
		 */
		length = f_tell((FIL *)fp->filep);

		res = f_lseek(fp->filep, cur_length);
		if (res != FR_OK) {
			return translate_error(res);
		}

	
#if (CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE > 0)
		unsigned int bw;
		uint8_t* _buf;
		_off64_t remian_len;
		_off64_t _u64len;

		remian_len = length - cur_length;
		if(remian_len > 0){
			_buf = FS_ENV_MEM_MALLOC(CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE);
			
			if(_buf){
				memset(_buf,0,CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE);
				do{
					uint32_t _len;
					_u64len = remian_len > CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE ? CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE : remian_len;
					remian_len -= _u64len;
					_len = (uint32_t)_u64len;
					res = f_write(fp->filep, _buf, _len, &bw);
					if(res != FR_OK){
						break;
					}
				}while(remian_len > 0);
				FS_ENV_MEM_FREE(_buf);
			}
			else{
				uint8_t c = 0U;

				for (_off64_t i = cur_length; i < length; i++) {
					res = f_write(fp->filep, &c, 1, &bw);
					if (res != FR_OK) {
						break;
					}
				}
			}

		}
#else
		/*
		 * The FS module does caching and optimization of
		 * writes. Here we write 1 byte at a time to avoid
		 * using additional code and memory for doing any
		 * optimization.
		 */
		unsigned int bw;
		uint8_t c = 0U;

		for (_off64_t i = cur_length; i < length; i++) {
			res = f_write(fp->filep, &c, 1, &bw);
			if (res != FR_OK) {
				break;
			}
		}

#endif
		
	}

	res = translate_error(res);
#endif

	return res;
}
#endif


static int fatfs_fastseek_link(struct lsfs_file_t * fp, uint32_t size)
{

	DWORD* cltbl;
	FIL* ff_fp;
	FRESULT res = FR_OK;

	cltbl = FS_ENV_MEM_MALLOC(sizeof(DWORD)*size);
	memset(cltbl,0,sizeof(DWORD)*size);
	cltbl[0] = size;
	ff_fp = (FIL*)fp->filep;
	ff_fp->cltbl = cltbl;
	res = f_lseek(ff_fp, CREATE_LINKMAP); // 创建簇映射表
	if(res != FR_OK){
		ff_fp->cltbl = NULL;
		FS_ENV_MEM_FREE(cltbl);
	}

	return translate_error(res);
}

static int fatfs_fastseek_unlink(struct lsfs_file_t * fp){
	
	FIL* ff_fp;
	FRESULT res = FR_OK;

	ff_fp = (FIL*)fp->filep;
	if(ff_fp->cltbl){

		FS_ENV_MEM_FREE(ff_fp->cltbl);
		ff_fp->cltbl = NULL;
	}
	return translate_error(res);
}
static int fatfs_fastseek_relinkmap(struct lsfs_file_t * fp){
	FIL* ff_fp;
	FRESULT res = FR_OK;

	ff_fp = (FIL*)fp->filep;
	res = f_lseek(ff_fp, CREATE_LINKMAP); // 创建簇映射表

	return translate_error(res);	
}

static off_t fatfs_tell(struct lsfs_file_t *fp)
{
	return f_tell((FIL *)fp->filep);
}

static int fatfs_truncate(struct lsfs_file_t *fp, off_t length)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	off_t cur_length = f_size((FIL *)fp->filep);

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(fp->filep, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(fp->filep);
	} else {
		/*
		 * Get actual length after expansion. This could be
		 * less if there was not enough space in the volume
		 * to expand to the requested length
		 */
		length = f_tell((FIL *)fp->filep);

		res = f_lseek(fp->filep, cur_length);
		if (res != FR_OK) {
			return translate_error(res);
		}


#if (CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE > 0)
		unsigned int bw;
		uint8_t* _buf;
		uint32_t remian_len;

		remian_len = length - cur_length;
		if(remian_len > 0){
			_buf = FS_ENV_MEM_MALLOC(CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE);
			memset(_buf,0,CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE);
			if(_buf){

				do{
					uint32_t _len;
					_len = remian_len > CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE ? CONFIG_LSFS_TRUNCATE_ALLOC_UNIT_SIZE : remian_len;
					remian_len -= _len;
					res = f_write(fp->filep, _buf, _len, &bw);
					if(res != FR_OK){
						break;
					}
				}while(remian_len > 0);
				FS_ENV_MEM_FREE(_buf);
			}
			else{
				uint8_t c = 0U;

				for (_off64_t i = cur_length; i < length; i++) {
					res = f_write(fp->filep, &c, 1, &bw);
					if (res != FR_OK) {
						break;
					}
				}
			}

		}
#else
		/*
		 * The FS module does caching and optimization of
		 * writes. Here we write 1 byte at a time to avoid
		 * using additional code and memory for doing any
		 * optimization.
		 */
		unsigned int bw;
		uint8_t c = 0U;

		for (_off64_t i = cur_length; i < length; i++) {
			res = f_write(fp->filep, &c, 1, &bw);
			if (res != FR_OK) {
				break;
			}
		}

#endif
	}

	res = translate_error(res);
#endif

	return res;
}

static int fatfs_sync(struct lsfs_file_t *fp)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	res = f_sync(fp->filep);
	res = translate_error(res);
#endif
	return res;
}

static int fatfs_mkdir(struct lsfs_mount_t *mountp, const char *path)
{
	int res = -ENOTSUP;

#if !FF_FS_READONLY
	res = f_mkdir(translate_path(path));
	res = translate_error(res);
#endif

	return res;
}

static int fatfs_opendir(struct lsfs_dir_t *dp, const char *path)
{
	FRESULT res;
	void *ptr;

    ptr = FS_ENV_MEM_MALLOC(sizeof(DIR));
	if (ptr != 0) {
		(void)memset(ptr, 0, sizeof(DIR));
		dp->dirp = ptr;
	} else {
		return -ENOMEM;
	}

	res = f_opendir(dp->dirp, translate_path(path));

	if (res != FR_OK) {
		FS_ENV_MEM_FREE(ptr);
		dp->dirp = NULL;
	}

	return translate_error(res);
}

static int fatfs_readdir(struct lsfs_dir_t *dp, struct lsfs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_readdir(dp->dirp, &fno);
	if (res == FR_OK) {
		snprintf(entry->name ,sizeof(entry->name) , "%s", fno.fname);
		if (entry->name[0] != 0) {
			entry->type = ((fno.fattrib & AM_DIR) ?
			       LSFS_DIR_ENTRY_DIR : LSFS_DIR_ENTRY_FILE);
			entry->size = fno.fsize;
		}
	}

	return translate_error(res);
}

static int fatfs_closedir(struct lsfs_dir_t *dp)
{
	FRESULT res;

	res = f_closedir(dp->dirp);

	/* Free file ptr memory */
	FS_ENV_MEM_FREE(dp->dirp);

	return translate_error(res);
}

static int fatfs_stat(struct lsfs_mount_t *mountp,
		      const char *path, struct lsfs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_stat(translate_path(path), &fno);
	if (res == FR_OK) {
		entry->type = ((fno.fattrib & AM_DIR) ?
			       LSFS_DIR_ENTRY_DIR : LSFS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno.fname);
		entry->size = fno.fsize;
	}

	return translate_error(res);
}

static int fatfs_statvfs(struct lsfs_mount_t *mountp,
			 const char *path, struct lsfs_statvfs *stat)
{
	int res = -ENOTSUP;
#if !FF_FS_READONLY
	FATFS *fs;
	DWORD f_bfree = 0;

	res = f_getfree(translate_path(mountp->mnt_point), &f_bfree, &fs);
	if (res != FR_OK) {
		return -EIO;
	}

	stat->f_bfree = f_bfree;

	/*
	 * If FF_MIN_SS and FF_MAX_SS differ, variable sector size support is
	 * enabled and the file system object structure contains the actual sector
	 * size, otherwise it is configured to a fixed value give by FF_MIN_SS.
	 */
#if FF_MAX_SS != FF_MIN_SS
	stat->f_bsize = lsfs->ssize;
#else
	stat->f_bsize = FF_MIN_SS;
#endif
	stat->f_frsize = fs->csize * stat->f_bsize;
	stat->f_blocks = (fs->n_fatent - 2);

	res = translate_error(res);
#endif
	return res;
}

static int fatfs_mount(struct lsfs_mount_t *mountp)
{
	FRESULT res;
	if((mountp == NULL)||((mountp->fs_data != NULL))){
		return -EINVAL;
	}

	mountp->fs_data = FS_ENV_MEM_MALLOC(sizeof(FATFS));
	if(mountp->fs_data == NULL){
		return -ENOMEM;
	}

	res = f_mount((FATFS *)mountp->fs_data, translate_path(mountp->mnt_point), 1);
	if(res != FR_OK){
		/* Unmount the fs, even if mount failed */
		f_mount(NULL, translate_path(mountp->mnt_point), 0);
		FS_ENV_MEM_FREE(mountp->fs_data);
		mountp->fs_data = 0;
		return translate_error(res);
	}

	return 0;

}

static int fatfs_unmount(struct lsfs_mount_t *mountp)
{
	FRESULT res;
	if((mountp == NULL)||(mountp->fs_data == NULL)){
		return -EINVAL;
	}

	res = f_mount(NULL, translate_path(mountp->mnt_point), 0);
	
	FS_ENV_MEM_FREE(mountp->fs_data);
	mountp->fs_data = NULL;

	if (res != FR_OK) {
		return translate_error(res);
	}

	return 0;
}

static MKFS_PARM def_cfg = {
	.fmt = FM_ANY | FM_SFD,	/* Any suitable FAT */
	.n_fat = 1,		/* One FAT lsfs table */
	.align = 0,		/* Get sector size via diskio query */
	.n_root = 512,
	.au_size = 0		/* Auto calculate cluster size */
};

static int fatfs_mkfs(const char* dev, void *cfg, int flags)
{
	FRESULT res;
	uint8_t work[FF_MAX_SS];
	MKFS_PARM *mkfs_opt = &def_cfg;

	if (cfg != NULL) {
		mkfs_opt = (MKFS_PARM *)cfg;
	}

	res = f_mkfs((char *)dev, mkfs_opt, work, sizeof(work));

	return translate_error(res);
}

static int fatfs_getcwd(struct lsfs_mount_t *mountp, char *buf, size_t size)
{
	FRESULT res;
	
	
	if (size <= 1) {
		return -EINVAL;
	}
	
	// Offset by 1 to make space for the '/'
	res = f_getcwd(buf + 1, size - 1);
	
	if (res == FR_OK) {
		append_slash(buf);
	}
	
	return translate_error(res);
}

static int fatfs_chdir(struct lsfs_mount_t *mountp, const char *pathname)
{
	FRESULT res;
	
	res = f_chdir(translate_path(pathname));
	
	return translate_error(res);
}

static int fatfs_chdrive(const char *pathname)
{
	FRESULT res;
	
	res = f_chdrive(translate_path(pathname));
	
	return translate_error(res);
}


/* File system interface */
static const struct lsfs_file_system_t fatfs_fs = {
	.open = fatfs_open,
	.close = fatfs_close,
	.read = fatfs_read,
	.write = fatfs_write,
	.fastseek_link = fatfs_fastseek_link,
	.fastseek_unlink = fatfs_fastseek_unlink,
	.fastseek_relinkmap = fatfs_fastseek_relinkmap,
	.lseek = fatfs_seek,
	.tell = fatfs_tell,
	.size = fatfs_size,
#if CONFIG_FS_LARGE64_FILES
	.lseek64 = fatfs_seek64,
	.tell64 = fatfs_tell64,
	.size64 = fatfs_size64,
	.truncate64 = fatfs_truncate64,
#endif
	.truncate = fatfs_truncate,
	.sync = fatfs_sync,
	.opendir = fatfs_opendir,
	.readdir = fatfs_readdir,
	.closedir = fatfs_closedir,
	.mount = fatfs_mount,
	.unmount = fatfs_unmount,
	.unlink = fatfs_unlink,
	.rename = fatfs_rename,
	.mkdir = fatfs_mkdir,
	.stat = fatfs_stat,
	.statvfs = fatfs_statvfs,
	.mkfs = fatfs_mkfs,
	.getcwd = fatfs_getcwd,
	.chdir = fatfs_chdir,
	.chdrive = fatfs_chdrive,
};

int lsfs_fat_init(void)
{

	return lsfs_register(LSFS_FATFS, &fatfs_fs);
}
