#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#include <stdarg.h>
#include <stdint.h>

int syslog_init(int dbg, uint32_t baudrate);
int syslog_write(const char *data, int len);
int syslog_hook_set(void (*hook)(const char *, va_list));
void syslog_raw_output_v(const char *format, va_list args);
int printk(const char *format, ...);

#endif
