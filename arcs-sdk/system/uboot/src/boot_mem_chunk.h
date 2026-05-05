#ifndef __BOOT_MEM_CHUNK__
#define __BOOT_MEM_CHUNK__

#include <stdint.h>

#include "sysheap.h"

#if defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB) && defined(CONFIG_ADB_MAX_PAYLOAD_SIZE) && \
    (CONFIG_ADB_MAX_PAYLOAD_SIZE > 0)
#define MEM_CHUNK_PAYLOAD_SIZE CONFIG_ADB_MAX_PAYLOAD_SIZE
#else
#define MEM_CHUNK_PAYLOAD_SIZE (64U * 1024U)
#endif

#if defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
#if (MEM_CHUNK_PAYLOAD_SIZE <= (32U * 1024U))
#define MEM_CHUNK_COUNT 4
#elif defined(CONFIG_MEM_SRAM_SIZE) && (CONFIG_MEM_SRAM_SIZE > 0x60000)
#define MEM_CHUNK_COUNT 6
#else
#define MEM_CHUNK_COUNT 3
#endif
#else
#define MEM_CHUNK_COUNT 2
#endif

#if defined(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE) && \
    (CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE > 32)
#define BOOT_MEM_CHUNK_ALIGN CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#else
#define BOOT_MEM_CHUNK_ALIGN 32U
#endif

#define MEM_CHUNK_SIZE (MEM_CHUNK_PAYLOAD_SIZE + 24U + 24U)

static inline int boot_mem_large_chunk_init(void)
{
    return 0;
}

static inline void *boot_mem_large_chunk_get(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return inram_malloc(BOOT_MEM_CHUNK_ALIGN, MEM_CHUNK_SIZE);
}

static inline void boot_mem_large_chunk_put(void *ptr)
{
    inram_free(ptr);
}

#endif
