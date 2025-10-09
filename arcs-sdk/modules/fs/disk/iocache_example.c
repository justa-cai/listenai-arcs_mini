/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file iocache_example.c
 * @brief Example usage of IO Cache module
 * 
 * This file demonstrates how to integrate and use the IO cache module
 * with existing disk operations to improve read performance.
 */

#include <stdio.h>
#include <string.h>
#include "iocache.h"

/* Example disk read implementation (simulates EMMC read) */
static int example_disk_read(struct disk_info *disk, uint8_t *data_buf,
                            uint32_t start_sector, uint32_t num_sector)
{
    LS_ENV_LOG("Physical disk read: sector %u, count %u\n", start_sector, num_sector);
    
    /* Simulate disk read delay */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* Fill buffer with test data */
    for (uint32_t i = 0; i < num_sector * 512; i++) {
        data_buf[i] = (uint8_t)(start_sector + i);
    }
    
    return 0;
}

/* Example disk operations */
static const struct disk_operations example_disk_ops = {
    .read = example_disk_read,
    /* Other operations can be NULL for this example */
};

/* Example disk info */
static struct disk_info example_disk = {
    .name = "emmc0",
    .ops = &example_disk_ops,
};

/**
 * Example function showing how to initialize and use IO cache
 */
int iocache_usage_example(void)
{
    int ret;
    uint8_t read_buffer[512];
    struct io_cache_config config;
    struct io_cache_stats stats;
    io_cache_handle_t cache_handle;
    
    LS_ENV_LOG("=== IO Cache Usage Example ===\n");
    
    /* Configure cache parameters */
    config.entry_count = 8;        /* 8 cache entries */
    config.max_block_size = 2048;  /* 4 sectors max per entry */
    config.sector_size = 512;      /* Standard sector size */
    
    /* Create cache instance with custom configuration */
    cache_handle = io_cache_create(&example_disk, &config);
    if (!cache_handle) {
        LS_ENV_LOG("Failed to create cache instance\n");
        return -1;
    }
    
    /* Enable cache for the disk */
    ret = io_cache_enable(cache_handle, &example_disk);
    if (ret != 0) {
        LS_ENV_LOG("Failed to enable cache: %d\n", ret);
        io_cache_destroy(cache_handle);
        return ret;
    }
    
    LS_ENV_LOG("Cache initialized and enabled\n");
    
    /* Demonstrate cache behavior */
    LS_ENV_LOG("\n--- First read (cache miss) ---\n");
    ret = example_disk.ops->read(&example_disk, read_buffer, 100, 1);
    if (ret != 0) {
        LS_ENV_LOG("Read failed: %d\n", ret);
        return ret;
    }
    
    LS_ENV_LOG("\n--- Second read (cache hit) ---\n");
    ret = example_disk.ops->read(&example_disk, read_buffer, 100, 1);
    if (ret != 0) {
        LS_ENV_LOG("Read failed: %d\n", ret);
        return ret;
    }
    
    LS_ENV_LOG("\n--- Write to cached sector (creates dirty data) ---\n");
    uint8_t write_data[512];
    memset(write_data, 0xAA, sizeof(write_data));
    ret = example_disk.ops->write(&example_disk, write_data, 100, 1);
    if (ret != 0) {
        LS_ENV_LOG("Write failed: %d\n", ret);
        return ret;
    }
    
    LS_ENV_LOG("\n--- Read modified data (cache hit, dirty data) ---\n");
    ret = example_disk.ops->read(&example_disk, read_buffer, 100, 1);
    if (ret != 0) {
        LS_ENV_LOG("Read failed: %d\n", ret);
        return ret;
    }
    LS_ENV_LOG("First byte of read data: 0x%02X (should be 0xAA)\n", read_buffer[0]);
    
    LS_ENV_LOG("\n--- Third read (different sector, cache miss) ---\n");
    ret = example_disk.ops->read(&example_disk, read_buffer, 200, 1);
    if (ret != 0) {
        LS_ENV_LOG("Read failed: %d\n", ret);
        return ret;
    }
    
    /* Get cache statistics */
    ret = io_cache_get_stats(cache_handle, &stats);
    if (ret == 0) {
        LS_ENV_LOG("\n--- Cache Statistics ---\n");
        LS_ENV_LOG("Total entries: %u\n", stats.total_entries);
        LS_ENV_LOG("Cached entries: %u\n", stats.cached_entries);
        LS_ENV_LOG("Max block size: %u bytes\n", stats.max_block_size);
        LS_ENV_LOG("Sector size: %u bytes\n", stats.sector_size);
        LS_ENV_LOG("Cache hits: %u\n", stats.cache_hits);
        LS_ENV_LOG("Cache misses: %u\n", stats.cache_misses);
        if (stats.cache_hits + stats.cache_misses > 0) {
            uint32_t hit_rate = (stats.cache_hits * 100) / (stats.cache_hits + stats.cache_misses);
            LS_ENV_LOG("Hit rate: %u%%\n", hit_rate);
        }
    }
    
    /* Flush cache */
    LS_ENV_LOG("\n--- Flushing cache ---\n");
    io_cache_flush(cache_handle);
    
    /* Disable cache */
    ret = io_cache_disable(cache_handle, &example_disk);
    if (ret != 0) {
        LS_ENV_LOG("Failed to disable cache: %d\n", ret);
        io_cache_destroy(cache_handle);
        return ret;
    }
    
    LS_ENV_LOG("\n--- Read after cache disabled (direct disk access) ---\n");
    ret = example_disk.ops->read(&example_disk, read_buffer, 100, 1);
    if (ret != 0) {
        LS_ENV_LOG("Read failed: %d\n", ret);
        io_cache_destroy(cache_handle);
        return ret;
    }
    
    /* Cleanup */
    io_cache_destroy(cache_handle);
    LS_ENV_LOG("\nCache cleanup completed\n");
    
    return 0;
}

/**
 * Integration example with existing SDMMC disk
 */
int integrate_with_sdmmc_example(void)
{
    /* This function shows how to integrate with existing SDMMC implementation */
    
    LS_ENV_LOG("\n=== SDMMC Integration Example ===\n");
    LS_ENV_LOG("To integrate with existing SDMMC disk:\n\n");
    
    LS_ENV_LOG("1. In sdmmc initialization code:\n");
    LS_ENV_LOG("   struct io_cache_config cache_config = {\n");
    LS_ENV_LOG("       .entry_count = 16,\n");
    LS_ENV_LOG("       .max_block_size = 4096,  // 8 sectors\n");
    LS_ENV_LOG("       .sector_size = 512\n");
    LS_ENV_LOG("   };\n");
    LS_ENV_LOG("   io_cache_init(&sdmmc_disk, &cache_config);\n");
    LS_ENV_LOG("   io_cache_enable(&sdmmc_disk);\n\n");
    
    LS_ENV_LOG("2. The cache will automatically intercept disk reads\n");
    LS_ENV_LOG("3. File system operations will benefit from caching\n");
    LS_ENV_LOG("4. No changes needed to existing file system code\n\n");
    
    LS_ENV_LOG("Configuration recommendations:\n");
    LS_ENV_LOG("- entry_count: 8-32 (depends on available memory)\n");
    LS_ENV_LOG("- max_block_size: 2048-8192 bytes (4-16 sectors)\n");
    LS_ENV_LOG("- sector_size: 512 bytes (standard)\n\n");
    
    return 0;
}

/* Main function for testing */
#ifdef IOCACHE_STANDALONE_TEST
int main(void)
{
    int ret;
    
    ret = iocache_usage_example();
    if (ret != 0) {
        return ret;
    }
    
    ret = integrate_with_sdmmc_example();
    return ret;
}
#endif
