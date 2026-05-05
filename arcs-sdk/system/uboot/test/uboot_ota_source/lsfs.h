#ifndef LSFS_H
#define LSFS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define LSFS_FATFS 0
#define LSFS_O_READ 0x01
#define LSFS_SEEK_SET 0
#define LSFS_SEEK_CUR 1
#define LSFS_SEEK_END 2

typedef uint32_t lsfs_mode_t;

struct lsfs_mount_t {
    int type;
    const char *mnt_point;
    void *fs_data;
};

struct lsfs_file_t {
    const uint8_t *data;
    size_t size;
    size_t pos;
    int open;
};

static inline void lsfs_file_t_init(struct lsfs_file_t *fp)
{
    fp->data = NULL;
    fp->size = 0;
    fp->pos = 0;
    fp->open = 0;
}

int lsfs_mount(struct lsfs_mount_t *mnt);
int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags);
int lsfs_close(struct lsfs_file_t *fp);
ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size);
int lsfs_seek(struct lsfs_file_t *fp, off_t offset, int whence);
off_t lsfs_tell(struct lsfs_file_t *fp);

#endif
