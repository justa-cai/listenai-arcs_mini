#ifndef TEST_ADB_SYNC_METADATA_LSFS_H
#define TEST_ADB_SYNC_METADATA_LSFS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define LSFS_O_READ 0x01u
#define LSFS_O_WRITE 0x02u
#define LSFS_O_CREATE 0x04u
#define LSFS_O_TRUNC 0x08u

typedef uint32_t lsfs_mode_t;

struct lsfs_file_t {
    char path[128];
    size_t pos;
    lsfs_mode_t flags;
    uint8_t open;
};

struct lsfs_dirent {
    char name[64];
    uint32_t size;
    uint8_t type;
};

static inline void lsfs_file_t_init(struct lsfs_file_t *fp)
{
    if (fp != NULL) {
        fp->path[0] = '\0';
        fp->pos = 0u;
        fp->flags = 0u;
        fp->open = 0u;
    }
}

int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags);
int lsfs_close(struct lsfs_file_t *fp);
ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size);
ssize_t lsfs_write(struct lsfs_file_t *fp, const void *ptr, size_t size);
int lsfs_stat(const char *path, struct lsfs_dirent *entry);

#endif
