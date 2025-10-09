/**
 * @file vt_dict_read.h
 *
 */

#ifndef  VT_DICT_READ_H
#define VT_DICT_READ_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
struct dict_base_t {
	char *id;
	char *father_id;
    char *text;
	char  *type;
	char  *audio_type;
};

typedef enum {
    //查词错误码
    E_VTIDIC_FIND_ERROR_10000 = 10000,    	   //调用查词接口时未初始化
    E_VTIDIC_FIND_ERROR_10001 = 10001,       //查词文本为空或长度为0
    E_VTIDIC_FIND_ERROR_10002 = 10002,       //查词文本过长大于255
    E_VTIDIC_FIND_ERROR_10003 = 10003,       //查询文本的基础信息未返回
    E_VTIDIC_FIND_ERROR_10004 = 10004,       //查询到的信息有误
    E_VTIDIC_FIND_ERROR_10005 = 10005,       //查询文本的具体信息时未返回
	//打开查词基础信息表失败>11000
    E_VTIDIC_FIND_ERROR_11001 = 11001,      
	//打开查询具体信息表失败>12000
    E_VTIDIC_FIND_ERROR_12001 = 12001,       
} E_VTIDIC_FIND_ERROR;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/
char* vt_get_dict_info(char* text);
void vt_dict_stop(void);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VT_DICT_READ_H*/
