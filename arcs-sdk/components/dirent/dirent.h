/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIR DIR;
struct dirent {
    ino_t d_ino;             // inode number
    off_t d_off;             // offset to the next dirent
    unsigned short d_reclen; // length of this record
    unsigned char d_type;    // type of file
    char d_name[256];        // filename (null-terminated)
};

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#ifdef __cplusplus
}
#endif

#endif /*_DIRENT_H_*/
