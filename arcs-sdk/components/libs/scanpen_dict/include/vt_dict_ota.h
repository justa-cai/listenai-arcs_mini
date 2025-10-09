/**
 * @file vt_dict_ota.h
 *
 */

#ifndef VT_DICT_OTA_H
#define VT_DICT_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/
#define VT_UPDATE_FILE_NAME			"ota-db-sql.txt"
#define VT_SOFT_VER_NAME			   "V1.0.6"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
 char * vt_get_version_info(void);
int vt_update_dict_info(const char *path);
/********************** 
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VT_DICT_OTA_H*/
