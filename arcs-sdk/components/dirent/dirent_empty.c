/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dirent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Minimal DIR implementation using standard C FILE for demonstration
struct DIR {
    FILE *fp;
    struct dirent entry;
};

DIR *opendir(const char *name) {
    // This is a stub for demonstration. In a real embedded FS, use FS APIs.
    // Here, always return NULL (not implemented)
    (void)name;
    return NULL;
}

struct dirent *readdir(DIR *dirp) {
    // Stub: always return NULL
    (void)dirp;
    return NULL;
}

int closedir(DIR *dirp) {
    // Stub: always succeed
    (void)dirp;
    return 0;
}
