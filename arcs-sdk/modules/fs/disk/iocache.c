/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file iocache.c
 * @brief IO Cache module for disk read operations
 * 
 * This module provides a configurable cache layer to reduce physical disk reads
 * by caching frequently accessed sectors in memory using LRU replacement policy.
 */

#include "disk/iocache.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include "disk/disk.h"
#include "fs_env/fs_env.h"


/* Cache entry structure */
struct io_cache_entry {
    uint32_t start_sector;      /* Starting sector number */
    uint32_t sector_count;      /* Number of sectors cached */
    uint8_t *data;              /* Cached data buffer */
    uint32_t access_time;       /* LRU timestamp */
    bool valid;                 /* Entry validity flag */
    struct io_cache_entry *next;
    struct io_cache_entry *prev;
};

/* Cache management structure */
struct io_cache {
    struct io_cache_entry *entries;     /* Cache entry pool */
    struct io_cache_entry *lru_head;    /* LRU list head (most recent) */
    struct io_cache_entry *lru_tail;    /* LRU list tail (least recent) */
    uint8_t *data_pool;                 /* Data buffer pool */
    uint32_t entry_count;               /* Total cache entries */
    uint32_t max_block_size;            /* Maximum block size in bytes */
    uint32_t sector_size;               /* Sector size in bytes */
    uint32_t access_counter;            /* Global access counter for LRU */
    const struct disk_operations *orig_ops; /* Original disk operations */
    struct disk_info *disk;             /* Associated disk */
    fs_env_mutex_handle_t mutex;        /* Thread safety mutex */
    uint32_t cache_hits;                /* Cache hit counter */
    uint32_t cache_misses;              /* Cache miss counter */
};

/* Cache instance management */
struct io_cache_instance {
    struct io_cache cache;
    struct disk_operations cached_ops;  /* Modified operations for this instance */
    bool initialized;
};

/**
 * Initialize LRU list management
 */
static void lru_init(struct io_cache *cache)
{
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
}

/**
 * Move entry to head of LRU list (mark as most recently used)
 */
static void lru_move_to_head(struct io_cache *cache, struct io_cache_entry *entry)
{
    if (cache->lru_head == entry) {
        return; /* Already at head */
    }

    /* Remove from current position */
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (cache->lru_tail == entry) {
        cache->lru_tail = entry->prev;
    }

    /* Insert at head */
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    }
    cache->lru_head = entry;
    
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/**
 * Get least recently used entry for eviction
 */
static struct io_cache_entry *lru_get_victim(struct io_cache *cache)
{
    return cache->lru_tail;
}

/**
 * Search for cache entry that contains the requested sector range
 */
static struct io_cache_entry *cache_find_entry(struct io_cache *cache, 
                                              uint32_t start_sector, 
                                              uint32_t sector_count)
{
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        struct io_cache_entry *entry = &cache->entries[i];
        
        if (!entry->valid) {
            continue;
        }
        
        /* Check if requested range is fully contained in cached range */
        if (start_sector >= entry->start_sector && 
            (start_sector + sector_count) <= (entry->start_sector + entry->sector_count)) {
            return entry;
        }
    }
    
    return NULL;
}

/**
 * Find an available cache entry (invalid or LRU victim)
 */
static struct io_cache_entry *cache_get_free_entry(struct io_cache *cache)
{
    /* First try to find an invalid entry */
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        if (!cache->entries[i].valid) {
            return &cache->entries[i];
        }
    }
    
    /* No free entry, use LRU victim */
    struct io_cache_entry *victim = lru_get_victim(cache);
    
    return victim;
}

/**
 * Load data into cache entry from disk
 */
static int cache_load_entry(struct io_cache *cache, 
                           struct io_cache_entry *entry,
                           uint32_t start_sector, 
                           uint32_t sector_count)
{
    int ret;
    
    /* Calculate optimal read size (don't exceed max block size) */
    uint32_t max_sectors = cache->max_block_size / cache->sector_size;
    if (sector_count > max_sectors) {
        sector_count = max_sectors;
    }
    
    /* Perform actual disk read using original operations */
    ret = cache->orig_ops->read(cache->disk, entry->data, start_sector, sector_count);
    if (ret != 0) {
        return ret;
    }
    
    /* Update entry metadata */
    entry->start_sector = start_sector;
    entry->sector_count = sector_count;
    entry->access_time = ++cache->access_counter;
    entry->valid = true;
    
    /* Update LRU list */
    lru_move_to_head(cache, entry);
    
    return 0;
}


/**
 * Cached disk read operation
 */
static int io_cache_read(struct disk_info *disk, uint8_t *data_buf,
                        uint32_t start_sector, uint32_t sector_count)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)disk->cache_instance;
    struct io_cache *cache;
    struct io_cache_entry *entry;
    int ret;

    if (!instance || !instance->initialized || (sector_count > instance->cache.max_block_size / instance->cache.sector_size)) {
        /* Cache not initialized, fall back to original operation */
        return instance ? instance->cache.orig_ops->read(disk, data_buf, start_sector, sector_count) : -EINVAL;
    }

    cache = &instance->cache;
    /* Lock for thread safety */
    fs_env_mutex_lock(&cache->mutex, FS_ENV_MAX_DELAY);
    /* Try to find data in cache */
    entry = cache_find_entry(cache, start_sector, sector_count);
    if (entry) {
        /* Cache hit - copy data from cache */
        uint32_t offset = (start_sector - entry->start_sector) * cache->sector_size;
        memcpy(data_buf, entry->data + offset, sector_count * cache->sector_size);
        
        /* Update LRU */
        entry->access_time = ++cache->access_counter;
        lru_move_to_head(cache, entry);
        
        /* Update hit statistics */
        cache->cache_hits++;
        
        /* Unlock and return */
        fs_env_mutex_unlock(&cache->mutex);
        return 0;
    }
    
    /* Cache miss - need to load from disk */
    cache->cache_misses++;
    
    entry = cache_get_free_entry(cache);
    if (!entry) {
        /* Should not happen, but fall back to direct read */
        fs_env_mutex_unlock(&cache->mutex);
        return cache->orig_ops->read(disk, data_buf, start_sector, sector_count);
    }
    
    /* Load data into cache */
    ret = cache_load_entry(cache, entry, start_sector, sector_count);
    if (ret != 0) {
        fs_env_mutex_unlock(&cache->mutex);
        return ret;
    }
    
    /* Copy requested data to output buffer */
    memcpy(data_buf, entry->data, sector_count * cache->sector_size);
    
    /* Unlock */
    fs_env_mutex_unlock(&cache->mutex);
    
    return 0;
}

/**
 * Cached disk write operation
 * Write operations are performed directly to disk, but we need to handle
 * cache coherency by invalidating overlapping cache entries.
 */
static int io_cache_write(struct disk_info *disk, const uint8_t *data_buf,
                         uint32_t start_sector, uint32_t sector_count)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)disk->cache_instance;
    struct io_cache *cache;
    int ret;
    
    if (!instance || !instance->initialized) {
        /* Cache not initialized, fall back to original operation */
        return instance ? instance->cache.orig_ops->write(disk, data_buf, start_sector, sector_count) : -EINVAL;
    }
    
    cache = &instance->cache;
    
    /* First, perform the actual write to disk */
    ret = cache->orig_ops->write(disk, data_buf, start_sector, sector_count);
    if (ret != 0) {
        return ret;
    }
    
    /* Lock for thread safety before modifying cache */
    fs_env_mutex_lock(&cache->mutex, FS_ENV_MAX_DELAY);

    /* After successful write, update any overlapping cache entries */
    uint32_t write_end = start_sector + sector_count;
    
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        struct io_cache_entry *entry = &cache->entries[i];
        
        if (!entry->valid) {
            continue;
        }
        
        uint32_t entry_end = entry->start_sector + entry->sector_count;
        
        /* Check if write range overlaps with cache entry */
        if (start_sector < entry_end && write_end > entry->start_sector) {
            /* Check if the write completely covers the cache entry */
            if (start_sector <= entry->start_sector && write_end >= entry_end) {
                /* Write completely covers cache entry - update entire entry */
                uint32_t offset_in_write = (entry->start_sector - start_sector) * cache->sector_size;
                uint32_t copy_size = entry->sector_count * cache->sector_size;
                memcpy(entry->data, data_buf + offset_in_write, copy_size);
                
                /* Update LRU - move to head since it was just written */
                entry->access_time = ++cache->access_counter;
                lru_move_to_head(cache, entry);
            }
            /* Check if the write is completely within the cache entry */
            else if (start_sector >= entry->start_sector && write_end <= entry_end) {
                /* Write is completely within cache entry - update partial data */
                uint32_t offset_in_entry = (start_sector - entry->start_sector) * cache->sector_size;
                uint32_t copy_size = sector_count * cache->sector_size;
                memcpy(entry->data + offset_in_entry, data_buf, copy_size);
                
                /* Update LRU - move to head since it was just written */
                entry->access_time = ++cache->access_counter;
                lru_move_to_head(cache, entry);
            }
            else {
                /* Partial overlap - invalidate the cache entry to avoid inconsistency */
                entry->valid = false;
                
                /* Remove from LRU list */
                if (cache->lru_head == entry) {
                    cache->lru_head = entry->next;
                    if (cache->lru_head) {
                        cache->lru_head->prev = NULL;
                    }
                }
                if (cache->lru_tail == entry) {
                    cache->lru_tail = entry->prev;
                    if (cache->lru_tail) {
                        cache->lru_tail->next = NULL;
                    }
                }
                if (entry->prev) {
                    entry->prev->next = entry->next;
                }
                if (entry->next) {
                    entry->next->prev = entry->prev;
                }
                
                entry->next = NULL;
                entry->prev = NULL;
            }
        }
    }
    
    /* Unlock */
    fs_env_mutex_unlock(&cache->mutex);
    return 0;
}

/**
 * Create and initialize IO cache instance
 */
io_cache_handle_t io_cache_create(struct disk_info *disk, const struct io_cache_config *config)
{
    struct io_cache_instance *instance;
    struct io_cache *cache;
    uint32_t total_data_size;
    
    if (!disk || !disk->ops || !disk->ops->read) {
        return NULL;
    }
    
    /* Allocate cache instance */
    instance = (struct io_cache_instance *)FS_ENV_MEM_MALLOC(sizeof(struct io_cache_instance));
    if (!instance) {
        return NULL;
    }
    
    cache = &instance->cache;
    
    /* Initialize mutex for thread safety */
    if (fs_env_mutex_create(&cache->mutex) != 0) {
        FS_ENV_MEM_FREE(instance);
        return NULL;
    }
    
    /* Use provided config or defaults */
    cache->entry_count = config->entry_count;
    cache->max_block_size = config->max_block_size;
    cache->sector_size = config->sector_size;
    
    /* Allocate cache entries */
    cache->entries = (struct io_cache_entry *)FS_ENV_MEM_MALLOC(cache->entry_count * sizeof(struct io_cache_entry));
    if (!cache->entries) {
        fs_env_mutex_destroy(&cache->mutex);
        FS_ENV_MEM_FREE(instance);
        return NULL;
    }
    
    /* Allocate data pool */
    total_data_size = cache->entry_count * cache->max_block_size;
    cache->data_pool = (uint8_t *)FS_ENV_MEM_MALLOC(total_data_size);
    if (!cache->data_pool) {
        FS_ENV_MEM_FREE(cache->entries);
        fs_env_mutex_destroy(&cache->mutex);
        FS_ENV_MEM_FREE(instance);
        return NULL;
    }
    
    /* Initialize cache entries */
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        struct io_cache_entry *entry = &cache->entries[i];
        entry->data = cache->data_pool + (i * cache->max_block_size);
        entry->valid = false;
        entry->access_time = 0;
        entry->next = NULL;
        entry->prev = NULL;
    }
    
    /* Initialize LRU list */
    lru_init(cache);
    
    /* Store original operations and disk reference */
    cache->orig_ops = disk->ops;
    cache->disk = disk;
    cache->access_counter = 0;
    
    /* Initialize statistics */
    cache->cache_hits = 0;
    cache->cache_misses = 0;
    
    instance->initialized = true;
    
    return instance;
}

/**
 * Enable cache for a disk by replacing its read operation
 */
int io_cache_enable(io_cache_handle_t handle, struct disk_info *disk)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)handle;
    
    if (!instance || !instance->initialized || !disk || !disk->ops) {
        return -EINVAL;
    }
    
    /* Create new operations structure with cached read and write */
    instance->cached_ops = *disk->ops;
    instance->cached_ops.read = io_cache_read;
    instance->cached_ops.write = io_cache_write;
    
    /* Store cache instance in disk and replace operations */
    disk->cache_instance = instance;
    disk->ops = &instance->cached_ops;
    
    return 0;
}

/**
 * Disable cache and restore original operations
 */
int io_cache_disable(io_cache_handle_t handle, struct disk_info *disk)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)handle;
    
    if (!instance || !instance->initialized || !disk) {
        return -EINVAL;
    }
    
    /* Restore original operations */
    disk->ops = instance->cache.orig_ops;
    disk->cache_instance = NULL;
    
    return 0;
}

/**
 * Get cache statistics
 */
int io_cache_get_stats(io_cache_handle_t handle, struct io_cache_stats *stats)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)handle;
    struct io_cache *cache;
    
    if (!instance || !instance->initialized || !stats) {
        return -EINVAL;
    }
    
    cache = &instance->cache;
    memset(stats, 0, sizeof(*stats));
    
    /* Lock for thread safety */
    fs_env_mutex_lock(&cache->mutex, FS_ENV_MAX_DELAY);
    
    /* Count valid entries */
    for (uint32_t i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].valid) {
            stats->cached_entries++;
        }
    }
    
    stats->total_entries = cache->entry_count;
    stats->max_block_size = cache->max_block_size;
    stats->sector_size = cache->sector_size;
    stats->cache_hits = cache->cache_hits;
    stats->cache_misses = cache->cache_misses;
    
    /* Unlock */
    fs_env_mutex_unlock(&cache->mutex);
    
    return 0;
}

/**
 * Destroy cache instance and cleanup resources
 */
int io_cache_destroy(io_cache_handle_t handle)
{
    struct io_cache_instance *instance = (struct io_cache_instance *)handle;
    struct io_cache *cache;
    
    if (!instance) {
        return 0;
    }
    
    cache = &instance->cache;
    
    /* Free allocated memory */
    if (cache->data_pool) {
        FS_ENV_MEM_FREE(cache->data_pool);
    }
    
    if (cache->entries) {
        FS_ENV_MEM_FREE(cache->entries);
    }
    
    /* Destroy mutex */
    if (instance->initialized) {
        fs_env_mutex_destroy(&cache->mutex);
    }
    
    FS_ENV_MEM_FREE(instance);
    
    return 0;
}