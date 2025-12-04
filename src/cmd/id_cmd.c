#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lisa_kv.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "shell.h"
#include "stdint.h"
#include "stdio.h"
#include "../kv/kv_user.h"
#include "../cloud/config/aiui_cfg.h"

#define TAG "id_cmd"
extern int adb_printf(const char *format, ...);
extern void update_device_id(void);

/**
 * @brief Set product ID
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success, -1 on failure
 */
static int device_cmd_set_pid(int argc, char **argv)
{
    if (argc < 1) {
        shellPrint(shellGetCurrent(), "Usage: device set_pid [product_id]\n");
        return -1;
    }

    char *pid = argv[0];
    
    // 检查产品ID长度是否符合要求 (36字符，包含连字符)
    if (strlen(pid) != 36) {
        shellPrint(shellGetCurrent(), "Invalid product ID length: %zu, expected 36 characters\n", strlen(pid));
        return -1;
    }
    
    if (lisa_kv_set_string(KV_KEY_USER_PID, pid) != 0) {
        shellPrint(shellGetCurrent(), "Set product ID failed: %s\n", pid);
        return -1;
    } else {
        shellPrint(shellGetCurrent(), "Set product ID success: %s\n", pid);
        return 0;
    }
}

/**
 * @brief Set secret ID
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success, -1 on failure
 */
static int device_cmd_set_sid(int argc, char **argv)
{
    if (argc < 1) {
        printf("Usage: device set_sid [secret_id]\n");
        shellPrint(shellGetCurrent(), "Usage: device set_sid [secret_id]\n");
        return -1;
    }

    char *sid = argv[0];
    
    // 检查密钥ID长度是否符合要求 (36字符，包含连字符)
    if (strlen(sid) != 36) {
        shellPrint(shellGetCurrent(), "Invalid secret ID length: %zu, expected 36 characters\n", strlen(sid));
        return -1;
    }
    
    if (lisa_kv_set_string(KV_KEY_USER_SID, sid) != 0) {
        shellPrint(shellGetCurrent(), "Set secret ID failed: %s\n", sid);
        return -1;
    } else {
        shellPrint(shellGetCurrent(), "Set secret ID success: %s\n", sid);
        return 0;
    }
}


/**
 * @brief Get product ID
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success, -1 on failure
 */
static int device_cmd_get_pid(int argc, char **argv)
{
    char *pid = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_PID, &pid);
    
    if (ret != 0 || pid == NULL) {
        // 从默认宏获取产品ID
        shellPrint(shellGetCurrent(), "Product ID from default macro: %s\n", PRODUCT_ID);
        
        // 检查默认宏长度
        if (strlen(PRODUCT_ID) != 36) {
            shellPrint(shellGetCurrent(), "Warning: Default Product ID has invalid length: %zu, expected 36 characters\n", strlen(PRODUCT_ID));
        }
        
        return 0;
    } else {
        // 检查KV中存储的ID长度
        if (strlen(pid) != 36) {
            shellPrint(shellGetCurrent(), "Warning: Stored Product ID has invalid length: %zu, expected 36 characters\n", strlen(pid));
        }
        
        shellPrint(shellGetCurrent(), "Product ID from KV: %s\n", pid);
        lisa_kv_free(pid);
        return 0;
    }
}

/**
 * @brief Get secret ID
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success, -1 on failure
 */
static int device_cmd_get_sid(int argc, char **argv)
{
    char *sid = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_SID, &sid);
    
    if (ret != 0 || sid == NULL) {
        // 从默认宏获取密钥ID
        shellPrint(shellGetCurrent(), "Secret ID from default macro: %s\n", SECRET_ID);
        
        // 检查默认宏长度
        if (strlen(SECRET_ID) != 36) {
            shellPrint(shellGetCurrent(), "Warning: Default Secret ID has invalid length: %zu, expected 36 characters\n", strlen(SECRET_ID));
        }
        
        return 0;
    } else {
        // 检查KV中存储的ID长度
        if (strlen(sid) != 36) {
            shellPrint(shellGetCurrent(), "Warning: Stored Secret ID has invalid length: %zu, expected 36 characters\n", strlen(sid));
        }
        
        shellPrint(shellGetCurrent(), "Secret ID from KV: %s\n", sid);
        lisa_kv_free(sid);
        return 0;
    }
}

/**
 * @brief Get device ID
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success, -1 on failure
 */
static int device_cmd_get_device_id(int argc, char **argv)
{
    // 从KV存储中获取设备ID
    char *device_id = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &device_id);
    
    // 直接从芯片读取ID
    uint32_t *id_1 = (uint32_t *)0x48600208;
    uint32_t *id_2 = (uint32_t *)0x4860020c;
    uint8_t id_buffer[8];
    char chip_id_str[17] = {0}; // 16个字符 + 结束符
    
    // 读取芯片ID
    if (*id_1 == 0 && *id_2 == 0) {
        shellPrint(shellGetCurrent(), "Chip ID is all zero\n");
    } else {
        memcpy(id_buffer, id_1, sizeof(uint32_t));
        memcpy(id_buffer + 4, id_2, sizeof(uint32_t));
        
        sprintf(chip_id_str, "%02x%02x%02x%02x%02x%02x%02x%02x", 
                id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3],
                id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7]);
                
        shellPrint(shellGetCurrent(), "Chip ID: %s\n", chip_id_str);
    }
    
    // 显示KV存储中的设备ID
    if (ret != 0 || device_id == NULL) {
        shellPrint(shellGetCurrent(), "KV Device ID not set or failed to get\n");
    } else {
        shellPrint(shellGetCurrent(), "KV Device ID: %s\n", device_id);
        lisa_kv_free(device_id);
    }
    
    return 0;
}

/**
 * @brief Reset IDs to default (delete from KV storage)
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0 on success
 */
static int device_cmd_reset_ids(int argc, char **argv)
{
    lisa_kv_del(KV_KEY_USER_PID);
    lisa_kv_del(KV_KEY_USER_SID);
    
    shellPrint(shellGetCurrent(), "All IDs have been reset to default\n");
    update_device_id();  // Update device ID immediately
    
    return 0;
}

/**
 * @brief Show help message
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0
 */
static int device_cmd_help(int argc, char **argv);

static const struct listen_cmd_t g_device_cmds[] = {
    {"set_pid", device_cmd_set_pid, "Set product ID, ex: device set_pid [product_id]"},
    {"set_sid", device_cmd_set_sid, "Set secret ID, ex: device set_sid [secret_id]"},
    {"get_pid", device_cmd_get_pid, "Get product ID, ex: device get_pid"},
    {"get_sid", device_cmd_get_sid, "Get secret ID, ex: device get_sid"},
    {"get_device_id", device_cmd_get_device_id, "Get device ID, ex: device get_device_id"},
    {"reset", device_cmd_reset_ids, "Reset all IDs to default, ex: device reset"},
    {"help", device_cmd_help, "Show help information"},
};

/**
 * @brief Show help message for device commands
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int 0
 */
static int device_cmd_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_device_cmds) / sizeof(g_device_cmds[0]);
    shellPrint(shellGetCurrent(), "Device ID management commands:\n");
    for (int i = 0; i < cmd_len; i++) {
        if (g_device_cmds[i].help != NULL) {
            shellPrint(shellGetCurrent(), "%-17s\t:\t%s\n", g_device_cmds[i].name, g_device_cmds[i].help);
        }
    }

    return 0;
}

/**
 * @brief Main handler for device commands
 * 
 * @param argc Argument count
 * @param argv Arguments
 * @return int Command execution result
 */
static int device_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        device_cmd_help(argc, argv);
        return 0;
    }
    
    int i;
    for (i = 0; i < sizeof(g_device_cmds) / sizeof(g_device_cmds[0]); i++) {
        if (strcmp(g_device_cmds[i].name, argv[1]) == 0) {
            return g_device_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    device_cmd_help(argc, argv);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, device,
                 device_cmd_handler, device ID management commands);
