/**
 * UTF8字符串相关处理函数.
 */

#ifndef _friso_utf8_h
#define _friso_utf8_h

#include "friso.h"
#include "friso_API.h"

#define FRISO_CJK_CHK_C (1)
#define FRISO_CJK_CHK_J (2)
#define FRISO_CJK_CHK_K (3)

/** {{{ UTF8 interface*/

/* read the next utf-8 word from the specified position.
 *
 * @return int    the bytes of the current readed word.
 */
FRISO_API int utf8_next_word(friso_task_t, uint_t *, fstring);

// get the bytes of a utf-8 char.
FRISO_API int get_utf8_bytes(char);

// return the unicode serial number of a given string.
FRISO_API int get_utf8_unicode(const fstring);

// convert the unicode serial to a utf-8 string.
FRISO_API int unicode_to_utf8(uint_t, fstring);

// check if the given char is a CJK.
FRISO_API int unicode_cjk_string(uint_t);

/* 当前utf8字符是否是cjk字符 */
FRISO_API int utf8_char_is_cjk(char *str);

FRISO_API int utf8_string_is_cjk(char *str);

/*check the given char is a Basic Latin letter or not.
 *         include all the letters and english puntuations.*/
FRISO_API int utf8_halfwidth_en_char(uint_t);

/*
 * check the given char is a full-width latain or not.
 *    include the full-width arabic numeber, letters.
 *        but not the full-width puntuations.
 */
FRISO_API int utf8_fullwidth_en_char(uint_t);

// check the given char is a upper case letter or not.
//     included all the full-width and half-width letters.
FRISO_API int utf8_uppercase_letter(uint_t);

// check the given char is a lower case letter or not.
//     included all the full-width and half-width letters.
FRISO_API int utf8_lowercase_letter(uint_t);

// check the given char is a numeric.
//     included the full-width and half-width arabic numeric.
FRISO_API int utf8_numeric_letter(uint_t);

/*
 * check if the given fstring is make up with numeric chars.
 *     both full-width,half-width numeric is ok.
 */
FRISO_API int utf8_numeric_string(const fstring);

FRISO_API int utf8_decimal_string(const fstring);

// check the given char is a english char.
//(full-width and half-width)
// not the punctuation of course.
FRISO_API int utf8_en_letter(uint_t);

// check the given char is a whitespace or not.
FRISO_API int utf8_whitespace(uint_t);

/* check if the given char is a letter number like 'ⅠⅡ'
 */
FRISO_API int utf8_letter_number(uint_t);

/*
 * check if the given char is a other number like '①⑩⑽㈩'
 */
FRISO_API int utf8_other_number(uint_t);

// check if the given char is a english punctuation.
FRISO_API int utf8_en_punctuation(uint_t);

// check if the given char is a chinese punctuation.
FRISO_API int utf8_cn_punctuation(uint_t u);

FRISO_API int is_en_punctuation(friso_charset_t, char);
//#define is_en_punctuation( c ) utf8_en_punctuation((uint_t) c)

//@Deprecated
// FRISO_API int utf8_keep_punctuation( fstring );
/* }}} */

/* 获取utf8字符串字符数中字节数=3的个数，认为是汉字，如“中国abc”返回2 */
FRISO_API int get_utf8_hanzi_num(char *str);


int string_is_pinyin_with_tone(char *str);					//判断输入utf8字符是否是带调的拼音，是返回1，否返回0
int string_has_whitespace(char *str);						//utf8字符串是否含有空格
int string_has_cjk(char *str);								//utf8字符串是否含有cjk字符
int string_to_lowcase_letter(char *src, char *dst);			//将src字符串中大写字母转为小写
int string_is_en_words(const char *str, int has_whitespace_is_ok); //判断输入字符串是否是英文单词：由字母和数字组成
int string_is_letter_num_pinyin(char *str);
int string_has_pinyinwithtone(char *str);					//utf8字符串是否含有带调拼音
int string_is_ascii(const char *str);                       //判断UTF8字符串是否都是ascii char
int string_is_connector(const char *str);                   //判断字符串是否是由英文连接符 & - /
int string_is_percentage(const char *str);                  //检查字符串是否是百分或者千分比%
int string_is_currencysymbol(const char *str);              //检查字符是否是货币符合
int string_is_countingunit(const char *str);                //判断字符串是否是中文计数单位
int string_is_chinesenum(const char *str, int bHasSpecial); //判断字符串是否是中文单个数字
int string_is_chinesenums(const char *str);                 //判断字符串是否是中文数字串,零-九
int string_is_mathunit(const char *str);                    //判断字符串是否是中文计数单位
int string_is_chinese_zero(const char *str);                //判断字符串是否是中文数字“零”
int string_modify_cn_punctuation_to_en(char *str);
int string_is_cn_type(const char *szU8Text);				//不依赖分词简单判断文本类型：没有汉字和带调拼音，就是英文，否则中文；纯数字当中文
int string_is_symbol_pie(const char *str);					//判断字符串是否是中文/英文撇
int string_is_en_sentence_endpunctuation(char *str);		//判断是否是英文句子结束符
int string_has_letter(char *str);
int string_has_num(char *str);
int utf8_char_is_letter_num_pinyin(char *ch, int len);
#ifdef WIN32
int get_file_dir(char *szFile, char *szFileDir);
#endif
#endif /* end _friso_utf8_h */
