#ifndef LOG_PRINT_H
#define LOG_PRINT_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_ERR(cond) do { if (!(cond)) { abort(); } } while (0)
#define CLOG(...) do { (void)printf(__VA_ARGS__); } while (0)
#define CLOGD(...) do { (void)printf(__VA_ARGS__); } while (0)
#define CLOGV(...) do { (void)printf(__VA_ARGS__); } while (0)

#endif
