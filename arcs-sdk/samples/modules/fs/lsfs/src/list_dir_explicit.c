#include <string.h>
#include <stdio.h>

#include "lsfs.h"
#include "disk/disk_access.h"
#include "log_print.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * 格式化文件大小为人类可读格式
 */
static const char* format_file_size(uint64_t size, char *buffer, size_t buffer_size)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size_d = (double)size;

    while (size_d >= 1024.0 && unit_index < 4) {
        size_d /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0) {
        snprintf(buffer, buffer_size, "%llu %s", (unsigned long long)size, units[unit_index]);
    } else {
        snprintf(buffer, buffer_size, "%.1f %s", size_d, units[unit_index]);
    }

    return buffer;
}

/**
 * 递归计算目录大小 (简化版本，用于统计)
 */
static uint64_t calculate_dir_size(const char *path)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;
    uint64_t total_size = 0;
    char sub_path[256];

    lsfs_dir_t_init(&dir);
    ret = lsfs_opendir(&dir, path);
    if (ret != 0) {
        return 0;
    }

    while (1) {
        ret = lsfs_readdir(&dir, &entry);
        if (ret != 0 || entry.name[0] == 0) {
            break;
        }

        /* 跳过当前目录和父目录 */
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }

        /* 构造完整路径 */
        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry.name);

        if (entry.type == LSFS_DIR_ENTRY_DIR) {
            /* 递归计算子目录大小 */
            total_size += calculate_dir_size(sub_path);
        } else {
            /* 累加文件大小 */
            total_size += entry.size;
        }
    }

    lsfs_closedir(&dir);
    return total_size;
}

/**
 * 树状递归遍历目录
 * @param path 目录路径
 * @param prefix 前缀字符串（用于绘制树状结构）
 * @param is_last 是否是最后一个项目
 * @param depth 递归深度（防止无限递归）
 * @param file_count 文件计数指针
 * @param dir_count 目录计数指针
 * @param total_size 总大小指针
 */
static void list_dir_tree_recursive(const char *path, const char *prefix, int is_last, int depth, 
                                   int *file_count, int *dir_count, uint64_t *total_size)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;
    char sub_path[256];
    char new_prefix[512];
    char size_str[32];
    int item_count = 0;
    int current_item = 0;
    
    /* 防止递归过深 */
    if (depth > 10) {
        CLOG("%s├── [MAX DEPTH REACHED]", prefix);
        return;
    }

    /* 初始化目录对象 */
    lsfs_dir_t_init(&dir);
    ret = lsfs_opendir(&dir, path);
    if (ret != 0) {
        CLOG("%s├── [ERROR: Cannot open directory]", prefix);
        return;
    }

    /* 首先遍历一遍，统计项目数量 */
    while (1) {
        ret = lsfs_readdir(&dir, &entry);
        if (ret != 0 || entry.name[0] == 0) {
            break;
        }
        if (strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0) {
            item_count++;
        }
    }
    lsfs_closedir(&dir);

    /* 重新打开目录进行实际遍历 */
    lsfs_dir_t_init(&dir);
    ret = lsfs_opendir(&dir, path);
    if (ret != 0) {
        return;
    }

    while (1) {
        ret = lsfs_readdir(&dir, &entry);
        if (ret != 0 || entry.name[0] == 0) {
            break;
        }

        /* 跳过当前目录和父目录 */
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }

        current_item++;
        int is_last_item = (current_item == item_count);
        
        /* 构造完整路径 */
        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry.name);

        if (entry.type == LSFS_DIR_ENTRY_DIR) {
            /* 目录 */
            TickType_t start_time = xTaskGetTickCount();
            uint64_t dir_size = calculate_dir_size(sub_path);
            TickType_t end_time = xTaskGetTickCount();
            uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;
            
            format_file_size(dir_size, size_str, sizeof(size_str));
            
            if (is_last_item) {
                CLOG("%s└── 📁 %s/ [%s] (计算耗时: %ums)", prefix, entry.name, size_str, elapsed_ms);
                snprintf(new_prefix, sizeof(new_prefix), "%s    ", prefix);
            } else {
                CLOG("%s├── 📁 %s/ [%s] (计算耗时: %ums)", prefix, entry.name, size_str, elapsed_ms);
                snprintf(new_prefix, sizeof(new_prefix), "%s│   ", prefix);
            }
            
            (*dir_count)++;
            *total_size += dir_size;
            
            /* 递归遍历子目录 */
            list_dir_tree_recursive(sub_path, new_prefix, is_last_item, depth + 1, 
                                   file_count, dir_count, total_size);
        } else {
            /* 文件 */
            format_file_size(entry.size, size_str, sizeof(size_str));
            
            if (is_last_item) {
                CLOG("%s└── 📄 %s [%s]", prefix, entry.name, size_str);
            } else {
                CLOG("%s├── 📄 %s [%s]", prefix, entry.name, size_str);
            }
            
            (*file_count)++;
            *total_size += entry.size;
        }
    }

    lsfs_closedir(&dir);
}

int list_dir_explicit(const char *path)
{
    int file_count = 0, dir_count = 0;
    uint64_t total_size = 0;
    char size_str[32];
    
    CLOG("📂 Directory Tree: %s", path);
    CLOG("═══════════════════════════════════════════════════");
    
    /* 显示根目录本身 */
    TickType_t root_start_time = xTaskGetTickCount();
    uint64_t root_size = calculate_dir_size(path);
    TickType_t root_end_time = xTaskGetTickCount();
    uint32_t root_elapsed_ms = (root_end_time - root_start_time) * portTICK_PERIOD_MS;
    
    format_file_size(root_size, size_str, sizeof(size_str));
    CLOG("📁 %s/ [%s] (根目录计算耗时: %ums)", strrchr(path, '/') ? strrchr(path, '/') + 1 : path, size_str, root_elapsed_ms);
    
    /* 开始树状遍历 */
    TickType_t tree_start_time = xTaskGetTickCount();
    list_dir_tree_recursive(path, "", 1, 0, &file_count, &dir_count, &total_size);
    TickType_t tree_end_time = xTaskGetTickCount();
    uint32_t tree_elapsed_ms = (tree_end_time - tree_start_time) * portTICK_PERIOD_MS;
    
    CLOG("═══════════════════════════════════════════════════");
    CLOG("📊 Summary:");
    CLOG("   📁 Directories: %d", dir_count);
    CLOG("   📄 Files: %d", file_count);
    CLOG("   📦 Total items: %d", dir_count + file_count);
    
    format_file_size(total_size, size_str, sizeof(size_str));
    CLOG("   💾 Total size: %s (%llu bytes)", size_str, (unsigned long long)total_size);
    CLOG("   ⏱️ 树状遍历总耗时: %ums", tree_elapsed_ms);
    
    return 0;
}
