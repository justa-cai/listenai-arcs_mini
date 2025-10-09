#if CONFIG_LOW_WIFI_CLI

#include "shell.h"
#include "cli_wifi.h"
#include "cli_main.h"
#include "lisa_log.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static char command_buffer[256] = {0};
static void shell_cli_print(const char *fmt, ...)
{
    Shell *_shell = shellGetCurrent();
    if(_shell) {
        char buffer[256];  /* 缓冲区用于格式化字符串 */
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        shellPrint(_shell, "%s", buffer);  /* 使用格式化后的字符串 */
    }
}


static int ls_wifi_command(int argc, char *argv[])
{
    int total_len = 0;
    uint32_t result;

    set_cli_print_func(shell_cli_print);
    
    LOGI("[DEBUG] ls_wifi_command called with argc=%d\n", argc);
    
    // 打印所有参数
    for (int i = 0; i < argc; i++) {
        LOGI("[DEBUG] argv[%d] = '%s'\n", i, argv[i]);
    }
    
    if (argc < 2) {
        LOGI("[DEBUG] argc < 2, returning -1\n");
        return -1;
    }
    
    // 清空命令缓冲区
    memset(command_buffer, 0, sizeof(command_buffer));
    
    // 重新组装命令字符串，跳过第一个参数（命令名 "wifi"）
    for (int i = 1; i < argc; i++) {
        int arg_len = strlen(argv[i]);
        
        LOGI("[DEBUG] Processing arg[%d]: '%s' (len=%d)\n", i, argv[i], arg_len);
        
        // 检查缓冲区是否足够
        if (total_len + arg_len + 1 >= sizeof(command_buffer)) {
            LOGI("[DEBUG] Buffer overflow protection, returning -1\n");
            return -1;
        }
        
        // 添加参数到命令缓冲区
        if (i > 1) {
            command_buffer[total_len++] = ' ';
        }
        strcpy(command_buffer + total_len, argv[i]);
        total_len += arg_len;
    }
    
    // 确保字符串以null结尾
    command_buffer[total_len] = '\0';
    
    LOGI("[DEBUG] Final command: '%s' (len=%d)\n", command_buffer, total_len);
    
    // 调用 wifi_cmd_handler，传入总长度+1以包含null字符
    LOGI("[DEBUG] Calling wifi_cmd_handler...\n");
    result = wifi_cmd_handler(command_buffer, total_len + 1);
    LOGI("[DEBUG] wifi_cmd_handler returned: %u\n", result);
    
    int return_value = (result == 0) ? 0 : -1;
    LOGI("[DEBUG] Returning: %d\n", return_value);
    
    return return_value;
}


extern uint32_t cli_cmd_handler(char* command, int len);

static int ls_cli_command(int argc, char *argv[])
{
    int total_len = 0;
    uint32_t result;

    set_cli_print_func(shell_cli_print);
    
    LOGI("[DEBUG] ls_cli_command called with argc=%d\n", argc);
    
    // 打印所有参数
    for (int i = 0; i < argc; i++) {
        LOGI("[DEBUG] argv[%d] = '%s'\n", i, argv[i]);
    }
    
    if (argc < 2) {
        LOGI("[DEBUG] argc < 2, returning -1\n");
        return -1;
    }
    
    // 清空命令缓冲区
    memset(command_buffer, 0, sizeof(command_buffer));
    
    // 重新组装命令字符串，跳过第一个参数（命令名 "cli"）
    for (int i = 1; i < argc; i++) {
        int arg_len = strlen(argv[i]);
        
        LOGI("[DEBUG] Processing arg[%d]: '%s' (len=%d)\n", i, argv[i], arg_len);
        
        // 检查缓冲区是否足够
        if (total_len + arg_len + 1 >= sizeof(command_buffer)) {
            LOGI("[DEBUG] Buffer overflow protection, returning -1\n");
            return -1;
        }
        
        // 添加参数到命令缓冲区
        if (i > 1) {
            command_buffer[total_len++] = ' ';
        }
        strcpy(command_buffer + total_len, argv[i]);
        total_len += arg_len;
    }
    
    // 确保字符串以null结尾
    command_buffer[total_len] = '\0';
    
    LOGI("[DEBUG] Final command: '%s' (len=%d)\n", command_buffer, total_len);
    
    // 调用 cli_cmd_handler，传入总长度+1以包含null字符
    LOGI("[DEBUG] Calling cli_cmd_handler...\n");
    result = cli_cmd_handler(command_buffer, total_len + 1);
    LOGI("[DEBUG] cli_cmd_handler returned: %u\n", result);
    
    int return_value = (result == 0) ? 0 : -1;
    LOGI("[DEBUG] Returning: %d\n", return_value);
    
    return return_value;
}

// Export the wifi command to the shell system
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), wf, ls_wifi_command,
                 wifi command);

// Export the cli command to the shell system
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), cli, ls_cli_command,
                 cli command);

#endif
