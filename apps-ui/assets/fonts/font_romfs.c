#include "font_romfs.h"

#include <stddef.h>

#include "arcs_ap_base.h"
#include "lisa_log.h"
#include "romfs/romfs.h"

#define RESPAK_BIN_ADDR (CMN_FLASH_REGION + 0x00440000)
#define RESPAK_BIN_SIZE (1792 * 1024)

static struct romfs *s_romfs = NULL;
static bool s_init_attempted = false;

static struct romfs *get_romfs(void)
{
    if (s_romfs) {
        return s_romfs;
    }

    if (s_init_attempted) {
        return NULL;
    }
    s_init_attempted = true;

    if (romfs_init(&s_romfs, (const void *)(uintptr_t)RESPAK_BIN_ADDR, RESPAK_BIN_SIZE) != 0 ||
        !s_romfs) {
        LOGE("font romfs_init failed");
        return NULL;
    }

    return s_romfs;
}

bool font_romfs_get(const char *path, uint8_t **data, uint32_t *size)
{
    struct romfs *fs = get_romfs();

    if (data) {
        *data = NULL;
    }
    if (size) {
        *size = 0U;
    }

    if (!fs || !path || !data || !size) {
        return false;
    }

    if (romfs_info_get(fs, path, data, size) == 0 && *data && *size > 0U) {
        return true;
    }

    return false;
}
