/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file iocache.h
 * @brief IO Cache module header for disk read operations
 * 
 * This module provides a configurable cache layer to reduce physical disk reads
 * by caching frequently accessed sectors in memory using LRU replacement policy.
 */

#ifndef _IO_CACHE_H_
#define _IO_CACHE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct disk_info;

/* Cache handle type for multi-instance support */
typedef struct io_cache_instance* io_cache_handle_t;

/**
 * @brief IO Cache configuration structure
 */
struct io_cache_config {
    uint32_t entry_count;       /* Number of cache entries */
    uint32_t max_block_size;    /* Maximum block size in bytes */
    uint32_t sector_size;       /* Sector size in bytes */
};

/**
 * @brief IO Cache statistics structure
 */
struct io_cache_stats {
    uint32_t total_entries;     /* Total cache entries */
    uint32_t cached_entries;    /* Currently cached entries */
    uint32_t max_block_size;    /* Maximum block size in bytes */
    uint32_t sector_size;       /* Sector size in bytes */
    uint32_t cache_hits;        /* Number of cache hits */
    uint32_t cache_misses;      /* Number of cache misses */
};

/**
 * @brief Create and initialize IO cache instance
 * 
 * @param disk Pointer to disk info structure
 * @param config Cache configuration (NULL for defaults)
 * @return Cache handle on success, NULL on failure
 */
io_cache_handle_t io_cache_create(struct disk_info *disk, const struct io_cache_config *config);

/**
 * @brief Enable cache for a disk
 * 
 * @param handle Cache handle
 * @param disk Pointer to disk info structure
 * @return 0 on success, negative errno on failure
 */
int io_cache_enable(io_cache_handle_t handle, struct disk_info *disk);

/**
 * @brief Disable cache for a disk
 * 
 * @param handle Cache handle
 * @param disk Pointer to disk info structure
 * @return 0 on success, negative errno on failure
 */
int io_cache_disable(io_cache_handle_t handle, struct disk_info *disk);

/**
 * @brief Get cache statistics
 * 
 * @param handle Cache handle
 * @param stats Pointer to statistics structure
 * @return 0 on success, negative errno on failure
 */
int io_cache_get_stats(io_cache_handle_t handle, struct io_cache_stats *stats);

/**
 * @brief Flush all cache entries (mark as invalid)
 * 
 * @param handle Cache handle
 * @return 0 on success, negative errno on failure
 */
int io_cache_flush(io_cache_handle_t handle);

/**
 * @brief Destroy cache instance and cleanup resources
 * 
 * @param handle Cache handle
 * @return 0 on success, negative errno on failure
 */
int io_cache_destroy(io_cache_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __IOCACHE_H_ */