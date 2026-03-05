/* 定义 LOG_TAG (必须在包含 lisa_log.h 之前) */
#define LOG_TAG "logger_sample"

/* 日志头文件 */
#include <lisa_log.h>

#include "stdio.h"

int main(int argc, char **argv)
{
    LOGI("Hello, world! \n");

    return 0;
}
