/* Unity配置文件 */
#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#include <stdio.h>

/* 自定义配置 */
#define UNITY_OUTPUT_CHAR(a) putchar(a)
#define UNITY_OUTPUT_FLUSH() fflush(stdout)
#define UNITY_OUTPUT_START() printf("\n\n")
#define UNITY_OUTPUT_COMPLETE() printf("\n")

#endif /* UNITY_CONFIG_H */
