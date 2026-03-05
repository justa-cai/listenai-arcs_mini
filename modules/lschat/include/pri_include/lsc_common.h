/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#define CHECK_COND_GOTO(cond, label, format, ...) 								  	\
	do {                                                                          	\
		if (!(cond)) {                                                            	\
			LISA_NLOGE("[%s:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);   \
			goto label;                                                           	\
		}                                                                         	\
	} while (0)

#define CHECK_COND_RETURN_VAL(cond, ret_val, format, ...) 						  	\
	do {                                                                          	\
		if (!(cond)) {                                                            	\
			LISA_NLOGE("[%s:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);   \
			return (ret_val);                                                     	\
		}                                                                         	\
	} while (0)

#define CHECK_COND_RETURN(cond, format, ...) 						              	\
	do {                                                                          	\
		if (!(cond)) {                                                            	\
			LISA_NLOGE("[%s:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);   \
			return;                                                               	\
		}                                                                         	\
	} while (0)

/* clang-format on */

#ifdef __cplusplus
}
#endif