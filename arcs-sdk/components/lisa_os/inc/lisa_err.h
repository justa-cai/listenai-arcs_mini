#ifndef __LISA_COMMON_ERR__
#define __LISA_COMMON_ERR__

#include <stdint.h>
#include "lisa_typedef.h"

typedef int32_t lisa_err_t;

#define LISA_OK   (int32_t)(0)
#define LISA_FAIL (int32_t)(-1)

#define LISA_APP_ASSERT(exp, fmt, ...)                                                                                                                                                                                                                                                                     \
	while (!(exp)) {                                                                                                                                                                                                                                                                                   \
		while (1) {                                                                                                                                                                                                                                                                                \
		};                                                                                                                                                                                                                                                                                         \
		break;                                                                                                                                                                                                                                                                                     \
	}
#define LISA_ISR_ASSERT(exp, fmt, ...)                                                                                                                                                                                                                                                                     \
	while (!(exp)) {                                                                                                                                                                                                                                                                                   \
		while (1) {                                                                                                                                                                                                                                                                                \
		};                                                                                                                                                                                                                                                                                         \
		break;                                                                                                                                                                                                                                                                                     \
	}

#endif // __LISA_COMMON_ERR__
