#include "shell.h"
#include "shell_passthrough.h"


// 透传模式处理函数(在shell_task中运行,快速返回)
static int at_passthrough_handler(char *data, unsigned short len)
{
    atcmd_handler(data, len);

    return 0;  // 立即返回,不阻塞shell_task
}

// 初始化AT命令处理系统
void at_cmd_init(void)
{
    // shell任务同步执行AT命令,无需额外队列
}

// 注册透传模式命令
// 使用方式:
//   交互模式: AT (进入后可输入AT+?,不需要空格)
//   单行模式: AT AT+? (一次性执行)
SHELL_EXPORT_PASSTROUGH(SHELL_CMD_PERMISSION(0), AT, AT>>, at_passthrough_handler, AT command passthrough mode);

