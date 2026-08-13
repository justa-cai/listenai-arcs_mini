#include "lisa_log.h"
#include "ipc_shared.h"
#include "sys_init.h"
#include "sysutils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef CONFIG_LOG_BACKEND_IPC_NAME
#define CONFIG_LOG_BACKEND_IPC_NAME "ipc.log"
#endif

#ifndef CONFIG_LOG_BACKEND_IPC_PREFIX
#define CONFIG_LOG_BACKEND_IPC_PREFIX "[REMOTE] "
#endif

#if defined(CFG_AMP_IPC) && defined(CFG_IPC_PRINT_WRITER)
extern struct ipc_shared_env_tag ipc_shared_env;
extern int32_t ipc_dbg_output(char *string, int32_t len);

#define IPC_LOG_PREFIX     CONFIG_LOG_BACKEND_IPC_PREFIX
#define IPC_LOG_PREFIX_LEN (sizeof(IPC_LOG_PREFIX) - 1)

static __psram_bss__ uint8_t ipc_log_cache[CONFIG_LOG_BACKEND_IPC_CACHE_SIZE];
static uint32_t ipc_log_cache_len;
static bool ipc_log_local_ready;

static uint32_t ipc_log_buffer_size(void)
{
    volatile struct ipc_dbg_tag *dbg_buffer = &ipc_shared_env.dbg_buffer;

    if (dbg_buffer->pattern != IPC_PATTERN1 || dbg_buffer->buffer_start == 0 ||
        dbg_buffer->buffer_size == 0) {
        return 0;
    }

    return dbg_buffer->buffer_size;
}

static bool ipc_log_ready(void)
{
    return ipc_log_local_ready && ipc_log_buffer_size() != 0;
}

static void ipc_log_cache_drop(uint32_t len)
{
    if (len >= ipc_log_cache_len) {
        ipc_log_cache_len = 0;
        return;
    }

    memmove(ipc_log_cache, ipc_log_cache + len, ipc_log_cache_len - len);
    ipc_log_cache_len -= len;
}

static void ipc_log_cache_append(const uint8_t *log, uint32_t len)
{
    if (len >= sizeof(ipc_log_cache)) {
        memcpy(ipc_log_cache, log + len - sizeof(ipc_log_cache), sizeof(ipc_log_cache));
        ipc_log_cache_len = sizeof(ipc_log_cache);
        return;
    }

    if (len > sizeof(ipc_log_cache) - ipc_log_cache_len) {
        ipc_log_cache_drop(len - (sizeof(ipc_log_cache) - ipc_log_cache_len));
    }

    memcpy(ipc_log_cache + ipc_log_cache_len, log, len);
    ipc_log_cache_len += len;
}

static void ipc_log_cache_append_prefixed(const uint8_t *log, uint32_t len)
{
    if (IPC_LOG_PREFIX_LEN != 0) {
        ipc_log_cache_append((const uint8_t *)IPC_LOG_PREFIX, IPC_LOG_PREFIX_LEN);
    }
    ipc_log_cache_append(log, len);
}

static int ipc_log_output_raw(const uint8_t *log, uint32_t len)
{
    uint32_t buffer_size = ipc_log_buffer_size();

    if (buffer_size == 0) {
        return -1;
    }

    while (len != 0) {
        uint32_t chunk_len = len > buffer_size ? buffer_size : len;

        if (ipc_dbg_output((char *)log, (int32_t)chunk_len) != 0) {
            return -1;
        }

        log += chunk_len;
        len -= chunk_len;
    }

    return 0;
}

static void ipc_log_cache_flush(void)
{
    if (ipc_log_cache_len == 0 || !ipc_log_ready()) {
        return;
    }

    if (ipc_log_output_raw(ipc_log_cache, ipc_log_cache_len) == 0) {
        ipc_log_cache_len = 0;
    }
}

static int ipc_log_output_prefixed(const uint8_t *log, uint32_t len)
{
    if (IPC_LOG_PREFIX_LEN != 0 && ipc_log_output_raw((const uint8_t *)IPC_LOG_PREFIX,
                                                      IPC_LOG_PREFIX_LEN) != 0) {
        return -1;
    }

    return ipc_log_output_raw(log, len);
}

static void log_ipc_output(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

    if (log == NULL || len == 0) {
        return;
    }

    ipc_log_cache_flush();
    if (ipc_log_cache_len != 0) {
        ipc_log_cache_append_prefixed(log, len);
        return;
    }

    if (!ipc_log_ready() || ipc_log_output_prefixed(log, len) != 0) {
        ipc_log_cache_append_prefixed(log, len);
    }
}

static void log_ipc_panic_output(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

    if (log == NULL || len == 0 || !ipc_log_ready()) {
        return;
    }

    ipc_log_output_prefixed(log, len);
}

int lisa_log_backend_ipc_init(void)
{
    return lisa_log_backend_add_v2(CONFIG_LOG_BACKEND_IPC_NAME, log_ipc_output,
                                   log_ipc_panic_output, NULL);
}

void lisa_log_backend_ipc_set_ready(void)
{
    ipc_log_local_ready = true;
    ipc_log_cache_flush();
}

static int lisa_log_backend_ipc_ready_init(void)
{
    lisa_log_backend_ipc_set_ready();
    return 0;
}

SYS_INIT(lisa_log_backend_ipc_ready_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 5);
#else
int lisa_log_backend_ipc_init(void)
{
    return 0;
}

void lisa_log_backend_ipc_set_ready(void)
{
}
#endif
