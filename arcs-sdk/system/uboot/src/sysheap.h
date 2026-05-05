#ifndef __BOOT_SYSHEAP_WRAPPER_H__
#define __BOOT_SYSHEAP_WRAPPER_H__

#if defined(__has_include_next) && __has_include_next("sysheap.h")
#include_next "sysheap.h"
#else
#include "../../heap/sysheap.h"
#endif

#endif
