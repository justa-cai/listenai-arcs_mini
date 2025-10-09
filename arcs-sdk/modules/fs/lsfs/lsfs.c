/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "fs_env/dlist.h"
#include "fs_env/fs_env.h"
#include "lsfs.h"
#include "lsfs_sys.h"
#include "lsfs_vfs.h"

static fs_env_mutex_handle_t mount_list_mutex_handle = {
	.mutex = NULL,
};

/* list of mounted file systems */
static sys_dlist_t  lsfs_mnt_list;


#if CONFIG_LSFS_SUPPORT_FREEZE
typedef struct{
	bool is_inited;
	fs_env_mutex_handle_t mutex;
	sys_dlist_t  open_list;
	bool is_freeze;
}lsfs_freeze_t;

static lsfs_freeze_t lsfs_freeze = {
	.is_inited= false,
};

static int lsfs_freeze_init(void){

	memset(&lsfs_freeze,0,sizeof(lsfs_freeze_t));
	fs_env_mutex_create(&lsfs_freeze.mutex);
	sys_dlist_init(&lsfs_freeze.open_list);
	lsfs_freeze.is_freeze = false;
	lsfs_freeze.is_inited = true;
	return 0;
}

int lsfs_freeze_freeze(uint32_t flags){
	struct lsfs_file_t *fp;
	sys_dnode_t *node, *next_node;
	int rc = 0;
	int sync_count = 0;

	if(lsfs_freeze.is_inited == false){
		LS_ENV_LOG("lsfs freeze is not inited!");
		return -EPERM;
	}

	/* Lock the open files list to prevent concurrent access */
	fs_env_mutex_lock(&lsfs_freeze.mutex, FS_ENV_MAX_DELAY);
	lsfs_freeze.is_freeze = true;
	/* Iterate through all open files and sync them */
	SYS_DLIST_FOR_EACH_NODE_SAFE(&lsfs_freeze.open_list, node, next_node) {
		fp = CONTAINER_OF(node, struct lsfs_file_t, freeze_node);
		
		/* Remove from list first to avoid issues during sync */
		sys_dlist_remove(&fp->freeze_node);
		
		/* sync the file using the file system's sync function */
		if (fp->mp && fp->mp->fs && fp->mp->fs->sync) {
			int sync_rc = fp->mp->fs->sync(fp);
			if (sync_rc < 0) {
				LS_ENV_LOG("Failed to sync file during freeze: %d", sync_rc);
				rc = sync_rc; /* Keep track of last error */
			}
		}
		
		sync_count++;
	}
	
	fs_env_mutex_unlock(&lsfs_freeze.mutex);

	LS_ENV_LOG("File system freeze completed: %d files synced\n", sync_count);
	return rc;
}

static int lsfs_freeze_add_fp(struct lsfs_file_t *fp){
	if (fp == NULL) {
		return -EINVAL;
	}

	/* Lock the open files list */
	fs_env_mutex_lock(&lsfs_freeze.mutex, FS_ENV_MAX_DELAY);

	/* Initialize the node and add to the list */
	sys_dnode_init(&fp->freeze_node);
	sys_dlist_append(&lsfs_freeze.open_list, &fp->freeze_node);

	fs_env_mutex_unlock(&lsfs_freeze.mutex);

	return 0;
}

static int lsfs_freeze_remove_fp(struct lsfs_file_t *fp){
	if (fp == NULL) {
		return -EINVAL;
	}

	/* Lock the open files list */
	fs_env_mutex_lock(&lsfs_freeze.mutex, FS_ENV_MAX_DELAY);

	/* Remove from the list if it's in the list */
	if (sys_dnode_is_linked(&fp->freeze_node)) {
		sys_dlist_remove(&fp->freeze_node);
	}

	fs_env_mutex_unlock(&lsfs_freeze.mutex);

	return 0;
}
#endif

#if CONFIG_LSFS_SUPPORT_FREEZE
#define CHECK_IS_FREEZE() {if(lsfs_freeze.is_freeze){LS_ENV_LOG("File system is freezed!");return -EBUSY;}}
#else
#define CHECK_IS_FREEZE() 
#endif

struct registry_entry {
	uint8_t type;
	const struct lsfs_file_system_t *fstp;
};
static struct registry_entry registry[CONFIG_LSFS_MAX_NUMBER];

static inline void registry_clear_entry(struct registry_entry *ep)
{
	ep->fstp = NULL;
}

static int registry_add(int type,
			const struct lsfs_file_system_t *fstp)
{
	int rv = -ENOSPC;

	for (size_t i = 0; i < ARRAY_SIZE(registry); ++i) {
		struct registry_entry *ep = &registry[i];

		if (ep->fstp == NULL) {
			ep->type = type;
			ep->fstp = fstp;
			rv = 0;
			break;
		}
	}

	return rv;
}

static struct registry_entry *registry_find(int type)
{
	for (size_t i = 0; i < ARRAY_SIZE(registry); ++i) {
		struct registry_entry *ep = &registry[i];

		if ((ep->fstp != NULL) && (ep->type == type)) {
			return ep;
		}
	}
	return NULL;
}

static const struct lsfs_file_system_t *lsfs_type_get(int type)
{
	struct registry_entry *ep = registry_find(type);

	return (ep != NULL) ? ep->fstp : NULL;
}

static int lsfs_get_mnt_point(struct lsfs_mount_t **mnt_pntp,
			    const char *name, size_t *match_len)
{
	struct lsfs_mount_t *mnt_p = NULL, *itr;
	size_t longest_match = 0;
	size_t len, name_len = strlen(name);
	sys_dnode_t *node;

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);
	SYS_DLIST_FOR_EACH_NODE(&lsfs_mnt_list, node) {
		itr = CONTAINER_OF(node, struct lsfs_mount_t, node);
		len = itr->mountp_len;

		/*
		 * Move to next node if mount point length is
		 * shorter than longest_match match or if path
		 * name is shorter than the mount point name.
		 */
		if ((len < longest_match) || (len > name_len)) {
			continue;
		}

		/*
		 * Move to next node if name does not have a directory
		 * separator where mount point name ends.
		 */
		if ((len > 1) && (name[len] != '/') && (name[len] != '\0')) {
			continue;
		}

		/* Check for mount point match */
		if (strncmp(name, itr->mnt_point, len) == 0) {
			mnt_p = itr;
			longest_match = len;
		}
	}
	fs_env_mutex_unlock(&mount_list_mutex_handle);

	if (mnt_p == NULL) {
		return -ENOENT;
	}

	*mnt_pntp = mnt_p;
	if (match_len)
		*match_len = mnt_p->mountp_len;

	return 0;
}

/* File operations */
int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags)
{
	struct lsfs_mount_t *mp;
	int rc = -EINVAL;

	CHECK_IS_FREEZE();

	if ((file_name == NULL) ||
			(strlen(file_name) <= 1) || (file_name[0] != '/')) {
		LS_ENV_LOG("invalid file name!!");
		return -EINVAL;
	}

	if (fp->mp != NULL) {
		return -EBUSY;
	}

	rc = lsfs_get_mnt_point(&mp, file_name, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if (((mp->flags & LSFS_MOUNT_FLAG_READ_ONLY) != 0) &&
	    (flags & LSFS_O_CREATE || flags & LSFS_O_WRITE)) {
		return -EROFS;
	}

	if(mp->fs->open == NULL) {
		return -ENOTSUP;
	}

	fp->mp = mp;
	rc = mp->fs->open(fp, file_name, flags);
	if (rc < 0) {
		LS_ENV_LOG("file open error (%d)", rc);
		fp->mp = NULL;
		return rc;
	}

	/* Copy flags to fp for use with other lsfs_ API calls */
	fp->flags = flags;

#if CONFIG_LSFS_SUPPORT_FREEZE
	/* Add file to freeze list for power-down management */
	if((flags & LSFS_O_WRITE) != 0){
		lsfs_freeze_add_fp(fp);
	}
#endif

	return rc;
}

int lsfs_close(struct lsfs_file_t *fp)
{
	int rc = -EINVAL;

	if (fp->mp == NULL) {
		return 0;
	}

	if(fp->mp->fs->close == NULL) {
		return -ENOTSUP;
	}

#if CONFIG_LSFS_SUPPORT_FREEZE
	/* Remove file from freeze list */
	if((fp->flags & LSFS_O_WRITE) != 0){
		lsfs_freeze_remove_fp(fp);
	}
#endif

	rc = fp->mp->fs->close(fp);
	if (rc < 0) {
		LS_ENV_LOG("file close error (%d)", rc);
		return rc;
	}

	fp->mp = NULL;

	return rc;
}

ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size)
{
	int rc = -EINVAL;

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->read == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->read(fp, ptr, size);
	if (rc < 0) {
		LS_ENV_LOG("file read error (%d)", rc);
	}

	return rc;
}

ssize_t lsfs_write(struct lsfs_file_t *fp, const void *ptr, size_t size)
{
	int rc = -EINVAL;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->write == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->write(fp, ptr, size);
	if (rc < 0) {
		LS_ENV_LOG("file write error (%d)", rc);
	}

	return rc;
}

int lsfs_seek(struct lsfs_file_t * fp, off_t offset, int whence)
{
	int rc = -ENOTSUP;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->lseek == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->lseek(fp, offset, whence);
	if (rc < 0) {
		LS_ENV_LOG("file seek error (%d)", rc);
	}

	return rc;
}

int lsfs_fastseek_link(struct lsfs_file_t * fp, uint32_t size)
{
	int rc = -ENOTSUP;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->fastseek_link == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->fastseek_link(fp, size);
	if (rc < 0) {
		LS_ENV_LOG("file fastseek link error (%d)", rc);
	}

	return rc;
}

int lsfs_fastseek_unlink(struct lsfs_file_t * fp){
	int rc = -ENOTSUP;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->fastseek_unlink == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->fastseek_unlink(fp);
	if (rc < 0) {
		LS_ENV_LOG("file fastseek unlink error (%d)", rc);
	}

	return rc;
}


int lsfs_fastseek_relinkmap(struct lsfs_file_t * fp)
{
	int rc = -ENOTSUP;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->fastseek_relinkmap == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->fastseek_relinkmap(fp);
	if (rc < 0) {
		LS_ENV_LOG("file fastseek relinkmap error (%d)", rc);
	}

	return rc;
}

off_t lsfs_tell(struct lsfs_file_t * fp)
{
	off_t rc = -ENOTSUP;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->tell == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->tell(fp);
	if (rc < 0) {
		LS_ENV_LOG("file tell error (%d)", rc);
	}

	return rc;
}
off_t lsfs_lsize(struct lsfs_file_t *fp)
{
	off_t rc = -ENOTSUP;

	CHECK_IS_FREEZE();

	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->size == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->size(fp);
	if (rc < 0) {
		LS_ENV_LOG("file size error (%d)", rc);
	}

	return rc;
}

#if CONFIG_FS_LARGE64_FILES
int lsfs_seek64(struct lsfs_file_t * fp, _off64_t offset, int whence)
{
	int rc = -ENOTSUP;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->lseek64 == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->lseek64(fp, offset, whence);
	if (rc < 0) {
		LS_ENV_LOG("file seek error (%d)", rc);
	}

	return rc;
}

_off64_t lsfs_tell64(struct lsfs_file_t * fp)
{
	_off64_t rc = -ENOTSUP;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->tell64 == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->tell64(fp);
	if (rc < 0) {
		LS_ENV_LOG("file tell error (%d)", rc);
	}

	return rc;
}

_off64_t lsfs_lsize64(struct lsfs_file_t *fp)
{
	_off64_t rc = -ENOTSUP;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->size64 == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->size64(fp);
	if (rc < 0) {
		LS_ENV_LOG("file size error (%d)", rc);
	}

	return rc;
}

int lsfs_truncate64(struct lsfs_file_t * fp, _off64_t length)
{
	int rc = -EINVAL;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->truncate64 == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->truncate64(fp, length);
	if (rc < 0) {
		LS_ENV_LOG("file truncate error (%d)", rc);
	}

	return rc;
}

#endif

int lsfs_truncate(struct lsfs_file_t * fp, off_t length)
{
	int rc = -EINVAL;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->truncate == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->truncate(fp, length);
	if (rc < 0) {
		LS_ENV_LOG("file truncate error (%d)", rc);
	}

	return rc;
}

int lsfs_sync(struct lsfs_file_t * fp)
{
	int rc = -EINVAL;

	CHECK_IS_FREEZE();
	if (fp->mp == NULL) {
		return -EBADF;
	}

	if(fp->mp->fs->sync == NULL) {
		return -ENOTSUP;
	}

	rc = fp->mp->fs->sync(fp);
	if (rc < 0) {
		LS_ENV_LOG("file sync error (%d)", rc);
	}

	return rc;
}

/* Directory operations */
int lsfs_opendir(struct lsfs_dir_t *dp, const char *abs_path)
{
	struct lsfs_mount_t *mp;
	int rc = -EINVAL;

	if ((abs_path == NULL) ||
			(strlen(abs_path) < 1) || (abs_path[0] != '/')) {
		LS_ENV_LOG("invalid directory name :%s\n", abs_path);
		return -EINVAL;
	}

	if (dp->mp != NULL || dp->dirp != NULL) {
		return -EBUSY;
	}


	if (strcmp(abs_path, "/") == 0) {
		/* Open VFS root dir, marked by dp->mp == NULL */
		fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

		dp->mp = NULL;
		dp->dirp = sys_dlist_peek_head(&lsfs_mnt_list);

		fs_env_mutex_unlock(&mount_list_mutex_handle);

		return 0;
	}

	rc = lsfs_get_mnt_point(&mp, abs_path, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->opendir == NULL) {
		return -ENOTSUP;
	}

	dp->mp = mp;
	rc = dp->mp->fs->opendir(dp, abs_path);
	if (rc < 0) {
		dp->mp = NULL;
		dp->dirp = NULL;
		LS_ENV_LOG("directory open error (%d)", rc);
	}

	return rc;
}

int lsfs_readdir(struct lsfs_dir_t *dp, struct lsfs_dirent *entry)
{
	if (dp->mp) {
		/* Delegate to mounted filesystem */
		int rc = -EINVAL;

		if(dp->mp->fs->readdir == NULL) {
			return  -ENOTSUP;
		}

		/* Loop until error or not special directory */
		while (true) {
			rc = dp->mp->fs->readdir(dp, entry);
			if (rc < 0) {
				break;
			}
			if (entry->name[0] == 0) {
				break;
			}
			if (entry->type != LSFS_DIR_ENTRY_DIR) {
				break;
			}
			if ((strcmp(entry->name, ".") != 0)
			    && (strcmp(entry->name, "..") != 0)) {
				break;
			}
		}
		if (rc < 0) {
			LS_ENV_LOG("directory read error (%d)", rc);
		}

		return rc;
	}

	/* VFS root dir */
	if (dp->dirp == NULL) {
		/* No more entries */
		entry->name[0] = 0;
		return 0;
	}

	/* Find the current and next entries in the mount point dlist */
	sys_dnode_t *node, *next = NULL;
	bool found = false;

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

	SYS_DLIST_FOR_EACH_NODE(&lsfs_mnt_list, node) {
		if (node == dp->dirp) {
			found = true;

			/* Pull info from current entry */
			struct lsfs_mount_t *mnt;

			mnt = CONTAINER_OF(node, struct lsfs_mount_t, node);

			entry->type = LSFS_DIR_ENTRY_DIR;
			strncpy(entry->name, mnt->mnt_point + 1,
				sizeof(entry->name) - 1);
			entry->name[sizeof(entry->name) - 1] = 0;
			entry->size = 0;

			/* Save pointer to the next one, for later */
			next = sys_dlist_peek_next(&lsfs_mnt_list, node);
			break;
		}
	}

	fs_env_mutex_unlock(&mount_list_mutex_handle);

	if (!found) {
		/* Current entry must have been removed before this
		 * call to readdir -- return an error
		 */
		return -ENOENT;
	}

	dp->dirp = next;
	return 0;
}

int lsfs_closedir(struct lsfs_dir_t *dp)
{
	int rc = -EINVAL;

	if (dp->mp == NULL) {
		/* VFS root dir */
		dp->dirp = NULL;
		return 0;
	}

	if(dp->mp->fs->closedir == NULL) {
		return -ENOTSUP;
	}

	rc = dp->mp->fs->closedir(dp);
	if (rc < 0) {
		LS_ENV_LOG("directory close error (%d)", rc);
		return rc;
	}

	dp->mp = NULL;
	dp->dirp = NULL;
	return rc;
}

/* Filesystem operations */
int lsfs_mkdir(const char *abs_path)
{
	struct lsfs_mount_t *mp;
	int rc = -EINVAL;

	CHECK_IS_FREEZE();

	if ((abs_path == NULL) ||
			(strlen(abs_path) <= 1) || (abs_path[0] != '/')) {
		LS_ENV_LOG("invalid directory name!!");
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, abs_path, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if (mp->flags & LSFS_MOUNT_FLAG_READ_ONLY) {
		return -EROFS;
	}

	if(mp->fs->mkdir == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->mkdir(mp, abs_path);
	if (rc < 0) {
		LS_ENV_LOG("failed to create directory (%d)", rc);
	}

	return rc;
}

int lsfs_unlink(const char *abs_path)
{
	struct lsfs_mount_t *mp;
	int rc = -EINVAL;

	CHECK_IS_FREEZE();
	if ((abs_path == NULL) ||
			(strlen(abs_path) <= 1) || (abs_path[0] != '/')) {
		LS_ENV_LOG("invalid file name!!");
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, abs_path, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if (mp->flags & LSFS_MOUNT_FLAG_READ_ONLY) {
		return -EROFS;
	}

	if(mp->fs->unlink == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->unlink(mp, abs_path);
	if (rc < 0) {
		LS_ENV_LOG("failed to unlink path (%d)", rc);
	}

	return rc;
}

int lsfs_rename(const char *from, const char *to)
{
	struct lsfs_mount_t *mp;
	size_t match_len;
	int rc = -EINVAL;

	CHECK_IS_FREEZE();

	if ((from == NULL) || (strlen(from) <= 1) || (from[0] != '/') ||
			(to == NULL) || (strlen(to) <= 1) || (to[0] != '/')) {
		LS_ENV_LOG("invalid file name!!");
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, from, &match_len);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if (mp->flags & LSFS_MOUNT_FLAG_READ_ONLY) {
		return -EROFS;
	}

	if (strncmp(from, to, match_len) != 0) {
		LS_ENV_LOG("mount point not same!!");
		return -EINVAL;
	}

	if(mp->fs->rename == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->rename(mp, from, to);
	if (rc < 0) {
		LS_ENV_LOG("failed to rename file or dir (%d)", rc);
	}

	return rc;
}

int lsfs_stat(const char *abs_path, struct lsfs_dirent *entry)
{
	struct lsfs_mount_t *mp;
	int rc = -EINVAL;

	if ((abs_path == NULL) ||
			(strlen(abs_path) <= 1) || (abs_path[0] != '/')) {
		LS_ENV_LOG("invalid file or dir name!!");
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, abs_path, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->stat == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->stat(mp, abs_path, entry);
	if (rc == -ENOENT) {
		/* File doesn't exist, which is a valid stat response */
	} else if (rc < 0) {
		LS_ENV_LOG("failed get file or dir stat (%d)", rc);
	}
	return rc;
}


int lsfs_getcwd(const char *mountpath, char *buf, size_t size)
{
	struct lsfs_mount_t *mp;
	int rc;

	if (!mountpath || !buf) {
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, mountpath, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->getcwd == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->getcwd(mp, buf, size);
	if (rc < 0) {
		LS_ENV_LOG("failed to get current working directory (%d)", rc);
	}

	return rc;

}

int lsfs_chdir(const char *pathname)
{
	struct lsfs_mount_t *mp;
	int rc;

	CHECK_IS_FREEZE();

	if (!pathname) {
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, pathname, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->chdir == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->chdir(mp, pathname);
	if (rc < 0) {
		LS_ENV_LOG("failed to change directory (%d)", rc);
	}

	return rc;

}

int lsfs_chdrive(const char *pathname)
{
	struct lsfs_mount_t *mp;
	int rc;

	CHECK_IS_FREEZE();

	if (!pathname) {
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, pathname, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->chdrive == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->chdrive(pathname);
	if (rc < 0) {
		LS_ENV_LOG("failed to set target drive (%d)", rc);
	}

	return rc;
}

int lsfs_statvfs(const char *abs_path, struct lsfs_statvfs *stat)
{
	struct lsfs_mount_t *mp;
	int rc;

	if ((abs_path == NULL) ||
			(strlen(abs_path) <= 1) || (abs_path[0] != '/')) {
		LS_ENV_LOG("invalid file or dir name!!");
		return -EINVAL;
	}

	rc = lsfs_get_mnt_point(&mp, abs_path, NULL);
	if (rc < 0) {
		LS_ENV_LOG("mount point not found!!");
		return rc;
	}

	if(mp->fs->statvfs == NULL) {
		return -ENOTSUP;
	}

	rc = mp->fs->statvfs(mp, abs_path, stat);
	if (rc < 0) {
		LS_ENV_LOG("failed get file or dir stat (%d)", rc);
	}

	return rc;
}

int lsfs_mount(struct lsfs_mount_t *mp)
{
	struct lsfs_mount_t *itr;
	const struct lsfs_file_system_t *fs;
	sys_dnode_t *node;
	int rc = -EINVAL;
	size_t len = 0;

	CHECK_IS_FREEZE();

	if ((mp == NULL) || (mp->mnt_point == NULL)) {
		LS_ENV_LOG("mount point not initialized!!");
		return -EINVAL;
	}

	len = strlen(mp->mnt_point);

	if ((len <= 1) || (mp->mnt_point[0] != '/')) {
		LS_ENV_LOG("invalid mount point!!");
		return -EINVAL;
	}

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

	/* Check if mount point already exists */
	SYS_DLIST_FOR_EACH_NODE(&lsfs_mnt_list, node) {
		itr = CONTAINER_OF(node, struct lsfs_mount_t, node);
		/* continue if length does not match */
		if (len != itr->mountp_len) {
			continue;
		}

		if (strncmp(mp->mnt_point, itr->mnt_point, len) == 0) {
			LS_ENV_LOG("mount point already exists!!");
			rc = -EBUSY;
			goto mount_err;
		}
	}

	/* Get file system information */
	fs = lsfs_type_get(mp->type);
	if (fs == NULL) {
		LS_ENV_LOG("requested file system type not registered!!");
		rc = -ENOENT;
		goto mount_err;
	}

	if(fs->mount == NULL) {
		LS_ENV_LOG("lsfs type %d does not support mounting", mp->type);
		rc = -ENOTSUP;
		goto mount_err;
	}

	rc = fs->mount(mp);
	if (rc < 0) {
		LS_ENV_LOG("lsfs mount error (%d)", rc);
		goto mount_err;
	}

	/* Update mount point data and append it to the list */
	mp->mountp_len = len;
	mp->fs = fs;

	
	sys_dlist_append(&lsfs_mnt_list, &mp->node);
	LS_ENV_LOG("fs mounted at %s", mp->mnt_point);
#if (CONFIG_LSFS_REGISTER_VFS)
	lsfs_register_vfs(mp->mnt_point);
#endif

mount_err:
	fs_env_mutex_unlock(&mount_list_mutex_handle);
	return rc;
}


int lsfs_unmount(struct lsfs_mount_t *mp)
{
	int rc = -EINVAL;

	CHECK_IS_FREEZE();

	if ((mp == NULL) || (mp->fs == NULL)||(mp->fs->unmount == NULL)) {
		return EINVAL;
	}

	rc = mp->fs->unmount(mp);
	if (rc < 0) {
		LS_ENV_LOG("lsfs unmount error (%d)", rc);
		goto unmount_err;
	}

	/* clear file system interface */
	mp->fs = NULL;

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);
	/* remove mount node from the list */
	sys_dlist_remove(&mp->node);

#if (CONFIG_LSFS_REGISTER_VFS)
	lsfs_unregister_vfs(mp->mnt_point);
#endif
	fs_env_mutex_unlock(&mount_list_mutex_handle);

	LS_ENV_LOG("lsfs unmounted from %s",mp->mnt_point);
unmount_err:
	
	return rc;
}

int lsfs_mkfs(int lsfs_type, const char* dev, void *cfg, int flags)
{
	int rc = -EINVAL;
	const struct lsfs_file_system_t *fs;

	CHECK_IS_FREEZE();

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

	/* Get file system information */
	fs = lsfs_type_get(lsfs_type);
	if (fs == NULL) {
		LS_ENV_LOG("lsfs type %d not registered!!",
				lsfs_type);
		rc = -ENOENT;
		goto mount_err;
	}

	if(fs->mkfs == NULL) {
		LS_ENV_LOG("lsfs type %d does not support mkfs", lsfs_type);
		rc = -ENOTSUP;
		goto mount_err;
	}

	rc = fs->mkfs(dev, cfg, flags);
	if (rc < 0) {
		LS_ENV_LOG("mkfs error (%d)", rc);
		goto mount_err;
	}

mount_err:
	fs_env_mutex_unlock(&mount_list_mutex_handle);
	return rc;
}

/* Register File system */
int lsfs_register(int type, const struct lsfs_file_system_t *fs)
{
	int rc = 0;

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

	if (lsfs_type_get(type) != NULL) {
		rc = -EALREADY;
	} else {
		rc = registry_add(type, fs);
	}

	fs_env_mutex_unlock(&mount_list_mutex_handle);

	LS_ENV_LOG("lsfs register %d: %d", type, rc);

	return rc;
}

/* Unregister File system */
int lsfs_unregister(int type, const struct lsfs_file_system_t *fs)
{
	int rc = 0;
	struct registry_entry *ep;

	fs_env_mutex_lock(&mount_list_mutex_handle,FS_ENV_MAX_DELAY);

	ep = registry_find(type);
	if ((ep == NULL) || (ep->fstp != fs)) {
		rc = -EINVAL;
	} else {
		registry_clear_entry(ep);
	}

	fs_env_mutex_unlock(&mount_list_mutex_handle);

	LS_ENV_LOG("lsfs unregister %d: %d", type, rc);
	return rc;
}

int lsfs_init(void)
{
	fs_env_mutex_create(&mount_list_mutex_handle);
	sys_dlist_init(& lsfs_mnt_list);

#if (CONFIG_LSFS_FAT)
	extern int lsfs_fat_init(void);
	lsfs_fat_init();
#endif

#if CONFIG_LSFS_SUPPORT_FREEZE
	/* Initialize freeze functionality */
	lsfs_freeze_init();
#endif

	return 0;
}


