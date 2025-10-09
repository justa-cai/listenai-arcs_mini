/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dirent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lsfs.h"
#include "sysheap.h"

// DIR 结构体，内部持有 lsfs_dir_t 和 struct dirent 缓存
struct DIR {
    struct lsfs_dir_t lsfs_dir;
    struct dirent entry;
    int end; // 标记目录是否读取完毕
};

DIR *opendir(const char *name) {
    DIR *dir = (DIR *)exram_malloc(4, sizeof(DIR));
    if (!dir) return NULL;
    memset(dir, 0, sizeof(DIR));
    int ret = lsfs_opendir(&dir->lsfs_dir, name);
    if (ret != 0) {
        exram_free(dir);
        return NULL;
    }
    dir->end = 0;
    return dir;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp || dirp->end) return NULL;
    struct lsfs_dirent lsfs_entry;
    int ret = lsfs_readdir(&dirp->lsfs_dir, &lsfs_entry);
    if (ret < 0) {
        dirp->end = 1;
        return NULL;
    }
    if (lsfs_entry.name[0] == 0) { // 目录尾
        dirp->end = 1;
        return NULL;
    }
    // 填充 struct dirent
    memset(&dirp->entry, 0, sizeof(struct dirent));
    dirp->entry.d_type = (lsfs_entry.type == LSFS_DIR_ENTRY_DIR) ? 4 : 8; // DT_DIR=4, DT_REG=8
    strncpy(dirp->entry.d_name, lsfs_entry.name, sizeof(dirp->entry.d_name) - 1);
    dirp->entry.d_name[sizeof(dirp->entry.d_name)-1] = '\0';
    return &dirp->entry;
}

int closedir(DIR *dirp) {
    if (!dirp) return -1;
    lsfs_closedir(&dirp->lsfs_dir);
    exram_free(dirp);
    return 0;
}
