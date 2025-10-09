/**
 * @file vt_dict_init.h
 *
 */

#ifndef  VT_DICT_INIT_H
#define VT_DICT_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "cJSON.h"
#include "sqlite3.h"
/*********************
 *      DEFINES
 *********************/
#define VT_DICT_DB_FILE_NAME		"test-duobiao.db"
#define VT_DICT_DB_FILE_NAME_REALES		"offline_dictionary.db"
/**********************
 *      TYPEDEFS
 **********************/
 //memory
typedef void *(*vt_malloc)(unsigned long size);
typedef void (*vt_free)(void *ptr);
typedef void (*vt_memset)(void *ptr, int val ,unsigned long size);
typedef struct {
    vt_malloc 	vt_plat_malloc;
    vt_free 	vt_plat_free;
    vt_memset	 vt_plat_memset;
} vt_os_ops_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
 extern cJSON* wordListArry ; 
 extern cJSON* baseListArry; 
 extern sqlite3 *db_vt;
 extern char basePath[32];
 extern char baseResPath[32];
/**********************
 *      MACROS
 **********************/
void *vt_os_malloc(unsigned long  size) ;
void vt_os_free(void *ptr) ;
void vt_os_memset(void *ptr, int val, unsigned long count) ;
int  vt_dict_read_init(const char*  path, const char*  res_path, const vt_os_ops_t *ops);
int  vt_dict_read_uninst(void);
int  vt_dict_read_open(const char* dict_db_name);
int  vt_dict_read_close(void );
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VT_DICT_INIT_H*/
