/**
 * Friso utf8 serial function implementation source file.
 * @package src/friso_UTF8.c .
 *
 * @author  lionsoul<chenxin619315@gmail.com>
 */

/****************************************************************************
unicode符号范围 | utf-8编码方式
(十六进制) | （二进制)
0000 0000-0000 007f:0xxxxxxx
0000 0080-0000 07ff:110xxxxx 10xxxxxx
0000 0800-0000 ffff:1110xxxx 10xxxxxx 10xxxxxx
0001 0000-001f ffff:11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
0020 0000-03ff ffff:111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
0400 0000-7fff ffff:1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
**************************************************************************/

#include "wordseg_kernel.h"
#include "friso_ctype.h"

#define UTF8_CHAR_MAX_LEN (4) // utf8字符所占字节长度
const char g_connector_symbol[] = { '/', '-', '&', /*'.',*/ '\'', '\0' }; //连接符 //':',
		
const char g_szSentEndPunc[][UTF8_CHAR_MAX_LEN + 1] = {
	{0xE3,0x80,0x82,0x00,0x00}, //。
	{ 0xEF,0xBC,0x81,0x00,0x00 },//！
	{ 0xEF,0xBC,0x9F,0x00,0x00 }, //？
	{ 0x2E,0x00,0x00,0x00,0x00 },//. 
	{ 0x21,0x00,0x00,0x00,0x00 },//! 
	{ 0x3F,0x00,0x00,0x00,0x00 }//? 
}; //表示一句话结束的标点符号

const char g_szCnPunctuation[][UTF8_CHAR_MAX_LEN + 1] = {	//。，！？--> .,!?
	{0xE3, 0x80, 0x82, 0x00, 0x00}, //。
	{ 0xEF, 0xBC, 0x8C, 0x00, 0x00 }, //，
	{ 0xEF, 0xBC, 0x81, 0x00, 0x00 }, //！
	{ 0xEF, 0xBC, 0x9F, 0x00, 0x00 }, //？
	{ 0xE2, 0X80, 0x98, 0x00, 0x00 }  //‘
};
const char g_szEnPunctuation[][UTF8_CHAR_MAX_LEN + 1] = {
	{ 0x2E, 0x00, 0x00, 0x00, 0x00 }, //.
	{ 0x2C, 0x00, 0x00, 0x00, 0x00}, //,
	{ 0x21, 0x00, 0x00, 0x00, 0x00}, //!
	{ 0x3F, 0x00, 0x00, 0x00, 0x00}, //?
	{ 0x27, 0x00, 0x00, 0x00, 0x00}  //'
};

const char g_szCountingUnit[][7] = {
	{ 0XE5, 0X8D, 0X81, 0X00, 0X00, 0X00, 0X00 }, //十
	{ 0XE7, 0X99, 0XBE, 0X00, 0X00, 0X00, 0X00 }, //百
	{ 0XE5, 0X8D, 0X83, 0X00, 0X00, 0X00, 0X00 }, //千
	{ 0XE4, 0XB8, 0X87, 0X00, 0X00, 0X00, 0X00 }, //万
	{ 0XE5, 0X8D, 0X81, 0XE4, 0XB8, 0X87, 0X00 }, //十万
	{ 0XE7, 0X99, 0XBE, 0XE4, 0XB8, 0X87, 0X00 }, //百万
	{ 0XE5, 0X8D, 0X83, 0XE4, 0XB8, 0X87, 0X00 }, //千万
	{ 0XE4, 0XBA, 0XBF, 0X00, 0X00, 0X00, 0X00 }, //亿
	{ 0XE5, 0X8D, 0X81, 0XE4, 0XBA, 0XBF, 0X00 }, //十亿
	{ 0XE7, 0X99, 0XBE, 0XE4, 0XBA, 0XBF, 0X00 }, //百亿
	{ 0XE5, 0X8D, 0X83, 0XE4, 0XBA, 0XBF, 0X00 }, //千亿
	{ 0XE4, 0XB8, 0X87, 0XE4, 0XBA, 0XBF, 0X00 } //万亿
};

const char g_szUnit[][10] = {
	{ 0xE5, 0x8D, 0x83, 0xE5, 0x85, 0x8B, 0x00, 0x00, 0x00, 0x00 }, //千克
	{ 0xE5, 0x8D, 0x83, 0xE7, 0xB1, 0xB3, 0x00, 0x00, 0x00, 0x00 }, //千米
	// 0xE5,0x8D,0x83,0xE6,0x96,0xA4,0x00,0x00,0x00,0x00,  //千斤
	{ 0xE4, 0xB8, 0x87, 0xE5, 0x85, 0x83, 0xE6, 0x88, 0xB7, 0x00 } //万元户
};


const char g_szChineseNum[][4] = {
	{ 0xE4, 0xB8, 0x80, 0x00 }, //一
	{ 0xE4, 0xBA, 0x8C, 0x00 }, //二
	{ 0XE4, 0XB8, 0X89, 0X00 }, //三
	{ 0XE5, 0X9B, 0X9B, 0X00 }, //四
	{ 0XE4, 0XBA, 0X94, 0X00 }, //五
	{ 0XE5, 0X85, 0XAD, 0X00 }, //六
	{ 0XE4, 0XB8, 0X83, 0X00 }, //七
	{ 0XE5, 0X85, 0XAB, 0X00 }, //八
	{ 0XE4, 0XB9, 0X9D, 0X00 } //九
};
const char g_szChineseNum_Special[][4] = {
	{ 0xE4, 0xB8, 0xA4, 0x00 }, //两
	{ 0XE9, 0X9B, 0XB6, 0X00 } //零
};

const char g_szChineseNum_ext[][4] = {
	{ 0xE4, 0xB8, 0x80, 0x00 }, //一
	{ 0xE4, 0xBA, 0x8C, 0x00 }, //二
	{ 0XE4, 0XB8, 0X89, 0X00 }, //三
	{ 0XE5, 0X9B, 0X9B, 0X00 }, //四
	{ 0XE4, 0XBA, 0X94, 0X00 }, //五
	{ 0XE5, 0X85, 0XAD, 0X00 }, //六
	{ 0XE4, 0XB8, 0X83, 0X00 }, //七
	{ 0XE5, 0X85, 0XAB, 0X00 }, //八
	{ 0XE4, 0XB9, 0X9D, 0X00 }, //九
	{ 0XE9, 0X9B, 0XB6, 0X00 }, //零
};

static const char g_szPinyinWithTone[][3] = {
	//带调拼音
	{ 0xc4, 0x81, 0x00 }, //ā
	{ 0xc3, 0xa1, 0x00 }, //á
	{ 0xc7, 0x8e, 0x00 }, //ǎ
	{ 0xc3, 0xa0, 0x00 }, //à
	{ 0xc4, 0x93, 0x00 }, //ē
	{ 0xc3, 0xa9, 0x00 }, //é
	{ 0xc4, 0x9b, 0x00 }, //ě
	{ 0xc3, 0xa8, 0x00 }, //è
	{ 0xc4, 0xab, 0x00 }, //ī
	{ 0xc3, 0xad, 0x00 }, //í
	{ 0xc7, 0x90, 0x00 }, //ǐ
	{ 0xc3, 0xac, 0x00 }, //ì
	{ 0xc5, 0x8d, 0x00 }, //ō
	{ 0xc3, 0xb3, 0x00 }, //ó
	{ 0xc7, 0x92, 0x00 }, //ǒ
	{ 0xc3, 0xb2, 0x00 }, //ò
	{ 0xc5, 0xab, 0x00 }, //ū
	{ 0xc3, 0xba, 0x00 }, //ú
	{ 0xc7, 0x94, 0x00 }, //ǔ
	{ 0xc3, 0xb9, 0x00 }, //ù
	{ 0xc7, 0x96, 0x00 }, //ǖ
	{ 0xc7, 0x98, 0x00 }, //ǘ
	{ 0xc7, 0x9a, 0x00 }, //ǚ
	{ 0xc7, 0x9c, 0x00 }, //ǜ
	{ 0xc3, 0xbc, 0x00 }  //ü
};

/* read the next utf-8 word from the specified position.
 *
 * @return int    the bytes of the current readed word.
 */
FRISO_API int utf8_next_word(
    friso_task_t task,
    uint_t *idx,
    fstring __word)
{
    if (*idx >= task->length)
        return 0;

    // register uint_t t;
    task->bytes = get_utf8_bytes(task->text[*idx]);

    // for ( t = 0; t < task->bytes; t++ ) {
    //     __word[t] = task->text[ (*idx)++ ];
    // }

    // change the loop to memcpy.
    // it is more efficient.
    //@date 2013-09-04
    memcpy(__word, task->text + (*idx), task->bytes);
    (*idx) += task->bytes;
    __word[task->bytes] = '\0';

    // the unicode counter was moved here from version 1.6.0
    task->unicode = get_utf8_unicode(__word);

    return task->bytes;
}

/*
 * print a character in a binary style.
 *
 * @param int
 */
FRISO_API void print_char_binary(char value)
{
    register uint_t t;

    for (t = 0; t < __CHAR_BYTES__; t++)
    {
        if ((value & 0x80) == 0x80)
        {
            printf("1");
        }
        else
        {
            printf("0");
        }
        value <<= 1;
    }
}

/*
 * get the bytes of a utf-8 char.
 *         between 1 - 6.
 *
 * @param __char
 * @return int
 */
FRISO_API int get_utf8_bytes(char value)
{
    register uint_t t = 0;

    // one byte ascii char.
    if ((value & 0x80) == 0)
        return 1;
    for (; (value & 0x80) != 0; value <<= 1)
    {
        t++;
    }

    return t;
}

/*
 * get the unicode serial of a utf-8 char.
 *
 * @param  ch
 * @return int.
 */
FRISO_API int get_utf8_unicode(const fstring ch)
{
    int code = 0, bytes = get_utf8_bytes(*ch);
    register uchar_t *bit = (uchar_t *)&code;
    register char b1, b2, b3;

    switch (bytes)
    {
    case 1:
        *bit = *ch;
        break;
    case 2:
        b1 = *ch;
        b2 = *(ch + 1);

        *bit = (b1 << 6) + (b2 & 0x3F);
        *(bit + 1) = (b1 >> 2) & 0x07;
        break;
    case 3:
        b1 = *ch;
        b2 = *(ch + 1);
        b3 = *(ch + 2);

        *bit = (b2 << 6) + (b3 & 0x3F);
        *(bit + 1) = (b1 << 4) + ((b2 >> 2) & 0x0F);
        break;
        // ignore the ones that are larger than 3 bytes;
    }

    return code;
}

// turn the unicode serial to a utf-8 string.
FRISO_API int unicode_to_utf8(uint_t u, fstring __word)
{
    if (u <= 0x0000007F)
    {
        // U-00000000 - U-0000007F
        // 0xxxxxxx
        *__word = (u & 0x7F);
        return 1;
    }
    else if (u >= 0x00000080 && u <= 0x000007FF)
    {
        // U-00000080 - U-000007FF
        // 110xxxxx 10xxxxxx
        *(__word + 1) = (u & 0x3F) | 0x80;
        *__word = ((u >> 6) & 0x1F) | 0xC0;
        return 2;
    }
    else if (u >= 0x00000800 && u <= 0x0000FFFF)
    {
        // U-00000800 - U-0000FFFF
        // 1110xxxx 10xxxxxx 10xxxxxx
        *(__word + 2) = (u & 0x3F) | 0x80;
        *(__word + 1) = ((u >> 6) & 0x3F) | 0x80;
        *__word = ((u >> 12) & 0x0F) | 0xE0;
        return 3;
    }
    else if (u >= 0x00010000 && u <= 0x001FFFFF)
    {
        // U-00010000 - U-001FFFFF
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(__word + 3) = (u & 0x3F) | 0x80;
        *(__word + 2) = ((u >> 6) & 0x3F) | 0x80;
        *(__word + 1) = ((u >> 12) & 0x3F) | 0x80;
        *__word = ((u >> 18) & 0x07) | 0xF0;
        return 4;
    }
    else if (u >= 0x00200000 && u <= 0x03FFFFFF)
    {
        // U-00200000 - U-03FFFFFF
        // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(__word + 4) = (u & 0x3F) | 0x80;
        *(__word + 3) = ((u >> 6) & 0x3F) | 0x80;
        *(__word + 2) = ((u >> 12) & 0x3F) | 0x80;
        *(__word + 1) = ((u >> 18) & 0x3F) | 0x80;
        *__word = ((u >> 24) & 0x03) | 0xF8;
        return 5;
    }
    else if (u >= 0x04000000 && u <= 0x7FFFFFFF)
    {
        // U-04000000 - U-7FFFFFFF
        // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(__word + 5) = (u & 0x3F) | 0x80;
        *(__word + 4) = ((u >> 6) & 0x3F) | 0x80;
        *(__word + 3) = ((u >> 12) & 0x3F) | 0x80;
        *(__word + 2) = ((u >> 18) & 0x3F) | 0x80;
        *(__word + 1) = ((u >> 24) & 0x3F) | 0x80;
        *__word = ((u >> 30) & 0x01) | 0xFC;
        return 6;
    }

    return 0;
}

/*
 * check the given char is a CJK char or not.
 *     2E80-2EFF CJK 部首补充
 *     2F00-2FDF 康熙字典部首
 *     3000-303F CJK 符号和标点                 --ignore
 *     31C0-31EF CJK 笔画
 *     3200-32FF 封闭式 CJK 文字和月份             --ignore.
 *     3300-33FF CJK 兼容
 *     3400-4DBF CJK 统一表意符号扩展 A
 *     4DC0-4DFF 易经六十四卦符号
 *     4E00-9FBF CJK 统一表意符号
 *     F900-FAFF CJK 兼容象形文字
 *     FE30-FE4F CJK 兼容形式
 *     FF00-FFEF 全角ASCII、全角标点            --ignore (as basic latin)
 *
 * Japanese:
 *     3040-309F 日本平假名
 *     30A0-30FF 日本片假名
 *     31F0-31FF 日本片假名拼音扩展
 *
 * Korean:
 *     AC00-D7AF 韩文拼音
 *     1100-11FF 韩文字母
 *     3130-318F 韩文兼容字母
 *
 * @param ch :pointer to the char
 * @return int : 1 for yes and 0 for not.
 */

// Comment one of the following macro define
// to clear the check of the specified language.
FRISO_API int unicode_cjk_string(uint_t u)
{
    int c = 0, j = 0, k = 0;
    // Chinese.
    c = ((u >= 0x4E00 && u <= 0x9FBF) || (u >= 0x2E80 && u <= 0x2EFF) || (u >= 0x2F00 && u <= 0x2FDF) || (u >= 0x31C0 && u <= 0x31EF) //|| ( u >= 0x3200 && u <= 0x32FF )
         || (u >= 0x3300 && u <= 0x33FF)                                                                                              //|| ( u >= 0x3400 && u <= 0x4DBF )
         || (u >= 0x4DC0 && u <= 0x4DFF) || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFE30 && u <= 0xFE4F));
    if (c)
    {
        return FRISO_CJK_CHK_C;
    }

    // Japanese.
    j = ((u >= 0x3040 && u <= 0x309F) || (u >= 0x30A0 && u <= 0x30FF) || (u >= 0x31F0 && u <= 0x31FF));
    if (j)
    {
        return FRISO_CJK_CHK_J;
    }

    // Korean
    k = ((u >= 0xAC00 && u <= 0xD7AF) || (u >= 0x1100 && u <= 0x11FF) || (u >= 0x3130 && u <= 0x318F));
    if (k)
    {
        return FRISO_CJK_CHK_K;
    }

    return 0;
}

/*
 * check the given char is a Basic Latin letter or not.
 *    include all the letters and english punctuations.
 *
 * @param c
 * @return int 1 for yes and 0 for not.
 */
FRISO_API int utf8_halfwidth_en_char(uint_t u)
{
    return (u >= 32 && u <= 126); // ASCII可显示字符：数字、大小写字母、符号
}

/*
 * check the given char is a full-width latain or not.
 *    include the full-width arabic numeber, letters.
 *    but not the full-width punctuations.
 *
 * @param c
 * @return int
 */
FRISO_API int utf8_fullwidth_en_char(uint_t u)
{
    return ((u >= 65296 && u <= 65305)      // arabic number
            || (u >= 65313 && u <= 65338)   // upper case letters
            || (u >= 65345 && u <= 65370)); // lower case letters
}

// check the given char is a upper case letters or not.
//     included the full-width and half-width letters.
FRISO_API int utf8_uppercase_letter(uint_t u)
{
    if (u > 65280)
        u -= 65248;
    return (u >= 65 && u <= 90);
}

// check the given char is a upper case letters or not.
//     included the full-width and half-width letters.
FRISO_API int utf8_lowercase_letter(uint_t u)
{
    if (u > 65280)
        u -= 65248;
    return (u >= 97 && u <= 122);
}

// check the given char is a numeric
//     included the full-width and half-width arabic numeric.
FRISO_API int utf8_numeric_letter(uint_t u)
{
    if (u > 65280)
        u -= 65248; // make full-width half-width.
    return ((u >= 48 && u <= 57));
}

// check the given char is a english letter.(included the full-width)
//     not the punctuation of course.
FRISO_API int utf8_en_letter(uint_t u)
{
    if (u > 65280)
        u -= 65248;
    return ((u >= 65 && u <= 90) || (u >= 97 && u <= 122));
}

/*
 * check if the given fstring is make up with numeric.
 *    both full-width,half-width numeric is ok.
 *
 * @param str
 * @return int
 * 65296, ０
 * 65297, １
 * 65298, ２
 * 65299, ３
 * 65300, ４
 * 65301, ５
 * 65302, ６
 * 65303, ７
 * 65304, ８
 * 65305, ９
 */
FRISO_API int utf8_numeric_string(const fstring str)
{
    fstring s = str;
    int bytes, u;

    while (*s != '\0')
    {
        // if ( ! utf8_numeric_letter( get_utf8_unicode( s++ ) ) ) {
        //     return 0;
        // }

        // new implemention.
        //@date 2013-10-14
        bytes = 1;
        if (*s < 0)
        { // full-width chars.
            u = get_utf8_unicode(s);
            bytes = get_utf8_bytes(*s);
            if (u < 65296 || u > 65305)
                return 0;
        }
        else if (*s < 48 || *s > 57)
        {
            return 0;
        }

        s += bytes;
    }

    return 1;
}

FRISO_API int utf8_decimal_string(const fstring str)
{
    int len = strlen(str), i, p = 0;
    int bytes = 0, u;

    if (str[0] == '.' || str[len - 1] == '.')
        return 0;

    for (i = 0; i < len; bytes = 1)
    {
        // count the number of char '.'
        if (str[i] == '.')
        {
            i++;
            p++;
            continue;
        }
        else if (str[i] < 0)
        {
            // full-width numeric.
            u = get_utf8_unicode(str + i);
            bytes = get_utf8_bytes(str[i]);
            if (u < 65296 || u > 65305)
                return 0;
        }
        else if (str[i] < 48 || str[i] > 57)
        {
            return 0;
        }

        i += bytes;
    }

    return (p == 1);
}

/*
 * check the given char is a whitespace or not.
 *
 * @param ch
 * @return int 1 for yes and 0 for not.
 */
FRISO_API int utf8_whitespace(uint_t u)
{
    if (u == 32 || u == 12288)
    {
        return 1;
    }
    return 0;
}

/*
 * check the given char is a english punctuation.
 *
 * @param ch
 * @return int
 */
FRISO_API int utf8_en_punctuation(uint_t u)
{
    // if ( u > 65280 ) u = u - 65248;        //make full-width half-width
    return ((u > 32 && u < 48) || (u > 57 && u < 65) || (u > 90 && u < 97) // added @2013-08-31
            || (u > 122 && u < 127));
}

/*
 * check the given char is a chinese punctuation.
 * @date    2013-08-31 added.
 *
 * @param ch
 * @return int
 */
FRISO_API int utf8_cn_punctuation(uint_t u)
{
    return ((u > 65280 && u < 65296) || (u > 65305 && u < 65312) || (u > 65338 && u < 65345) || (u > 65370 && u < 65382)
            // cjk symbol and punctuation.(added 2013-09-06)
            // from http://www.unicode.org/charts/PDF/U3000.pdf
            || (u >= 12289 && u <= 12319));
}

/*
 * check if the given char is a letter number in unicode.
 *        like 'ⅠⅡ'.
 * @param ch
 * @return int
 */
FRISO_API int utf8_letter_number(uint_t u)
{
	if (u > 65280)
		u -= 65248;
	return ((u >= 65 && u <= 90) || (u >= 97 && u <= 122) || (u >= 48 && u <= 57));
}

/*
 * check if the given char is a other number in unicode.
 *        like '①⑩⑽㈩'.
 * @param ch
 * @return int
 */
FRISO_API int utf8_other_number(uint_t u)
{
    return 0;
}

FRISO_API int utf8_char_is_pinyin_with_tone(char *str)
{
	int i;

	//int nLen = get_utf8_bytes(str[0]);
	for (i = 0; i < sizeof(g_szPinyinWithTone) / sizeof(g_szPinyinWithTone[0]); i++)
	{
		//int j = 0;		
		ivAssert(strlen(g_szPinyinWithTone[i]) == 2);
		//if (0 == strncmp(g_szPinyinWithTone[i], str, strlen(g_szPinyinWithTone[0]))) 
		if(str[0]!=0 && str[1]!=0 && g_szPinyinWithTone[i][0]==str[0] && g_szPinyinWithTone[i][1] == str[1] && g_szPinyinWithTone[i][2] == str[2])
		{
			return 1;
		}		
	}
	return 0;
}

int string_is_letter_num_pinyin(char *str)
{
	int offset = 0;
	while (0 != str[offset])
	{
		if (!utf8_en_letter((uint_t)str[offset]) && !utf8_numeric_letter((uint_t)str[offset]) &&
			!utf8_char_is_pinyin_with_tone(str+offset))
		{
			return 0;
		}				
		offset += get_utf8_bytes(str[offset]);
	}

	return 1;
}

int utf8_char_is_letter_num_pinyin(char *ch, int len)
{	
	//int offset = 0;
	if (len > 2)	return 0;
	if (1 == len && !utf8_letter_number(ch[0])) return 0;
	if (2 == len && !utf8_char_is_pinyin_with_tone(ch)) return 0;
	return 1;
}

int string_has_letter(char *str)
{
	int offset = 0;
	while (0 != str[offset])
	{
		if (utf8_en_letter((uint_t)str[offset]))
		{
			return 1;
		}
		offset += get_utf8_bytes(str[offset]);
	}

	return 0;
}

int string_has_num(char *str)
{
	int offset = 0;
	while (0 != str[offset])
	{
		if (utf8_numeric_letter((uint_t)str[offset]))
		{
			return 1;
		}
		offset += get_utf8_bytes(str[offset]);
	}

	return 0;
}

/*
    判断输入utf8字符是否是带调的拼音，是返回1，否返回0
*/
int string_is_pinyin_with_tone(char *str)
{
    int i, j;

    int nLen = get_utf8_bytes(str[0]);
    for (i = 0; i < sizeof(g_szPinyinWithTone) / sizeof(g_szPinyinWithTone[0]); i++)
    {
        for (j = 0; j < nLen; j++)
        {
            if (g_szPinyinWithTone[i][j] != str[j])
            {
                break;
            }
        }
        if (j >= nLen)
        {
            return 1;
        }
    }
    return 0;
}

/* 获取utf8字符串字符数中字节数=3的个数，认为是汉字，如“中国abc”返回2 */
FRISO_API int get_utf8_hanzi_num(char *str)
{
    int nOffset = 0;
    int nCharNum = 0;
    while (0 != str[nOffset])
    {
        int nBytes = get_utf8_bytes(str[nOffset]);
        if (3 == nBytes)
        {
            nCharNum++;
        }
        nOffset += nBytes;
    }

    return nCharNum;
}

/* utf8字符串是否含有空格 */
int string_has_whitespace(char *str)
{
    int nOffset = 0;
    while (0 != str[nOffset])
    {
        int nBytes = get_utf8_bytes(str[nOffset]);
        if (utf8_whitespace((uint_t)(str[nOffset])))
        {
            return 1;
        }
        nOffset += nBytes;
    }

    return 0;
}

/* utf8字符串是否含有带调拼音 */
int string_has_pinyinwithtone(char *str)
{
    int nOffset = 0;
    while (0 != str[nOffset])
    {
        int nBytes = get_utf8_bytes(str[nOffset]);
        if (string_is_pinyin_with_tone(str + nOffset))
        {
            return 1;
        }
        nOffset += nBytes;
    }

    return 0;
}

/* 当前utf8字符是否是cjk字符 */
FRISO_API int utf8_char_is_cjk(char *str)
{
    uint_t unicode = 0;
    char szWord[10] = {0};
    int nBytes = get_utf8_bytes(str[0]);
    memcpy(szWord, str, nBytes);
    szWord[nBytes] = 0;
    unicode = get_utf8_unicode(szWord);
    return unicode_cjk_string(unicode);
}

/* utf8字符串是否全是cjk字符 */
FRISO_API int utf8_string_is_cjk(char *str)
{
	int nOffset = 0;
	while (0 != str[nOffset])
	{
		uint_t unicode = 0;
		char szWord[10] = { 0 };
		int nBytes = get_utf8_bytes(str[nOffset]);
		memcpy(szWord, str + nOffset, nBytes);
		szWord[nBytes] = 0;
		unicode = get_utf8_unicode(szWord);
		if (!unicode_cjk_string(unicode))
		{
			return 0;
		}
		nOffset += nBytes;
	}

	return 1;
}

/* utf8字符串是否含有cjk字符 */
int string_has_cjk(char *str)
{
    int nOffset = 0;
    while (0 != str[nOffset])
    {
        uint_t unicode = 0;
        char szWord[10] = {0};
        int nBytes = get_utf8_bytes(str[nOffset]);
        memcpy(szWord, str + nOffset, nBytes);
        szWord[nBytes] = 0;
        unicode = get_utf8_unicode(szWord);
        if (unicode_cjk_string(unicode))
        {
            return 1;
        }
        nOffset += nBytes;
    }

    return 0;
}

/* 将src字符串中大写字母转为小写 */
int string_to_lowcase_letter(char *src, char *dst)
{
    int nOffset = 0;

	if (dst != src) {
		strcpy(dst, src);
	}

    while (0 != dst[nOffset])
    {
        int nBytes = get_utf8_bytes(dst[nOffset]);
        if (1 == nBytes && dst[nOffset] >= 65 && dst[nOffset] <= 90)
        {
            dst[nOffset] += 32;
        }
        nOffset += nBytes;
    }

    return 0;
}

//判断输入字符串是否是英文单词：由字母和数字组成，且至少有一个字母
int string_is_en_words(const char *str, int has_whitespace_is_ok)
{
    int offset = 0;
    while (0 != str[offset])	
    {
        int len = get_utf8_bytes(str[offset]);
		if (!(utf8_en_letter((uint_t)str[offset]) || utf8_numeric_letter((uint_t)str[offset]) || (has_whitespace_is_ok && len == 1 && ' '== str[offset]))) {
			return 0;
		}        
        offset += len;
    }

    return 1;
}

// A macro define has replace this.
// FRISO_API int is_en_punctuation( char c )
//{
//     return utf8_en_punctuation( (uint_t) c );
// }

/* {{{
   '@', '$','%', '^', '&', '-', ':', '.', '/', '\'', '#', '+'
   */
// static friso_hash_cdt_t __keep_punctuations_hash__ = NULL;

/* @Deprecated
 * check the given char is an english keep punctuation.*/
// FRISO_API int utf8_keep_punctuation( fstring str )
//{
//     if ( __keep_punctuations_hash__ == NULL )
//     {
//     __keep_punctuations_hash__ = new_hash_table();
//     hash_put_mapping( __keep_punctuations_hash__, "@", NULL );
//     //hash_put_mapping( __keep_punctuations_hash__, "$", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, "%", NULL );
//     //hash_put_mapping( __keep_punctuations_hash__, "^", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, "&", NULL );
//     //hash_put_mapping( __keep_punctuations_hash__, "-", NULL );
//     //hash_put_mapping( __keep_punctuations_hash__, ":", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, ".", NULL );
//     //hash_put_mapping( __keep_punctuations_hash__, "/", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, "'", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, "#", NULL );
//     hash_put_mapping( __keep_punctuations_hash__, "+", NULL );
//     }
//     //check the hash.
//     return hash_exist_mapping( __keep_punctuations_hash__, str );
// }
/* }}} */

/*
 * check the given english char is a full-width char or not.
 *
 * @param ch
 * @return 1 for yes and 0 for not.
 */
// FRISO_API int utf8_fullwidth_char( uint_t u )
//{
//     if ( u == 12288 )
//     return 1;                    //full-width space
//     //(32 - 126) ascii code
//     return (u > 65280 && u <= 65406);
// }


//判断UTF8字符串是否都是ascii char
int string_is_ascii(const char *str)
{
	int i = 0;
	//for (i = 0; 0 != str[i]; i++)
	while(0 != str[i])
	{
		// one byte ascii char.
		if (0 != (str[i] & 0x80))
		{
			return 0;
		}
		i++;
	}
	return 1;
}

int string_is_percentage(const char *str) //检查字符串是否是百分或者千分比%
{
	if (1 == strlen(str) && 0x25 == (unsigned char)(str[0]))
	{
		return 1;
	}

	if (3 == strlen(str) && 0xe2 == (unsigned char)(str[0]) && 0x80 == (unsigned char)(str[1]) && 0xb0 == (unsigned char)(str[2]))
	{
		return 1;
	}

	return 0;
}

int string_is_currencysymbol(const char *str) //检查字符是否是货币符合
{
	if (1 == strlen(str) && 0x24 == (unsigned char)(str[0])) //$
	{
		return 1;
	}

	if (3 == strlen(str) && 0xef == (unsigned char)(str[0]) && 0xbf == (unsigned char)(str[1]) && 0xa5 == (unsigned char)(str[2])) //￥
	{
		return 1;
	}

	return 0;
}


//不依赖分词简单判断文本类型：没有汉字和带调拼音，就是英文，否则中文；纯数字当中文
int string_is_cn_type(const char *szU8Text)
{
	if (NULL == szU8Text)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	//修改文本类型判断方式（中/英/拼音）：没有汉字和带调拼音，就是英文 20210301
	char *pText = (char *)szU8Text;
	int nLen = (int)(strlen(pText));
	int nOffset = 0;
	int bHasLetter = 0; //记录是否有英文字母

						//整数和小数类型=中文
	if (utf8_numeric_string(pText) || utf8_decimal_string(pText))
	{
		return 1;
	}

	while (nOffset < nLen)
	{
		char szTmp[10] = { 0 };
		int nBytes = get_utf8_bytes(pText[nOffset]);
		memcpy(szTmp, pText + nOffset, nBytes);
		if (string_is_letter(szTmp))
		{
			bHasLetter = 1;
		}
		else if (string_is_pinyin_with_tone(pText + nOffset))
		{
			return 1;
		}
		else
		{
			int nUnicode = get_utf8_unicode(pText + nOffset);
			if (nUnicode >= 0x4E00 && nUnicode <= 0x9FEF) //中文的Unicode编码范围
			{
				return 1;
			}
		}

		nOffset += nBytes;
	}

	//没有英文字母且没有汉字的情况改为“中文”，感觉更合理，录入“123 123%”
	if (0 == bHasLetter)
	{
		return 1;
	}

	return 0;
}

//判断字符串是否是由英文连接符 & - /
int string_is_connector(const char *str)
{
	size_t i;

	if (3 == strlen(str) && 0xE2 == (unsigned char)str[0] && 0x80 == (unsigned char)str[1] && 0x99 == (unsigned char)str[2])
	{
		return 1;
	}

	if (1 != strlen(str))
	{
		return 0;
	}
	for (i = 0; i < strlen(g_connector_symbol); i++)
	{
		if (g_connector_symbol[i] == str[0])
		{
			return 1;
		}
	}

	return 0;
}

//判断字符串是否是中文/英文撇
int string_is_symbol_pie(const char *str)
{
	// size_t i;
	if (1 == strlen(str) && 0x27 == (unsigned char)str[0])
	{
		return 1;
	}
	if (3 == strlen(str) && 0xE2 == (unsigned char)str[0] && 0x80 == (unsigned char)str[1] && 0x99 == (unsigned char)str[2])
	{
		return 1;
	}
	return 0;
}

//判断字符串是否是中文计数单位
int string_is_countingunit(const char *str)
{
	size_t i;

	for (i = 0; i < sizeof(g_szCountingUnit) / sizeof(g_szCountingUnit[0]); i++)
	{
		if ((strlen(str) == strlen(g_szCountingUnit[i])) &&
			(0 == strcmp(str, g_szCountingUnit[i])))
		{
			return 1;
		}
	}
	return 0;
}

//判断字符串是否是中文计数单位
int string_is_mathunit(const char *str)
{
	size_t i;	

	for (i = 0; i < sizeof(g_szUnit) / sizeof(g_szUnit[0]); i++)
	{
		if ((0 == strcmp(str, g_szUnit[i])))
		{
			return 1;
		}
	}
	return 0;
}

//判断字符串是否是中文数字“零”
int string_is_chinese_zero(const char *str)
{
	size_t i;
	char szChineseNum[][4] = {
		{0XE9, 0X9B, 0XB6, 0X00} //零
	};

	for (i = 0; i < sizeof(szChineseNum) / sizeof(szChineseNum[0]); i++)
	{
		if ((0 == strcmp(str, szChineseNum[i])))
		{
			return 1;
		}
	}

	return 0;
}

//判断字符串是否是中文数字
int string_is_chinesenum(const char *str, int bHasSpecial)
{
	size_t i;	

	for (i = 0; i < sizeof(g_szChineseNum) / sizeof(g_szChineseNum[0]); i++)
	{
		if ((0 == strcmp(str, g_szChineseNum[i])))
		{
			return 1;
		}
	}

	if (bHasSpecial)
	{
		for (i = 0; i < sizeof(g_szChineseNum_Special) / sizeof(g_szChineseNum_Special[0]); i++)
		{
			if ((0 == strcmp(str, g_szChineseNum_Special[i])))
			{
				return 1;
			}
		}
	}

	return 0;
}

//判断字符串是否是中文数字串
int string_is_chinesenums(const char *str)
{
	int i, j;
	int len, num;
	
	len = strlen(str);
	if (0 != len % 3)
	{
		return 0;
	}

	num = sizeof(g_szChineseNum_ext) / sizeof(g_szChineseNum_ext[0]);
	for (j = 0; j < len; j += 3)
	{
		for (i = 0; i < num; i++)
		{
			if (str[j] == g_szChineseNum_ext[i][0] && str[j + 1] == g_szChineseNum_ext[i][1] && str[j + 2] == g_szChineseNum_ext[i][2])
			{
				break;
			}
		}
		if (i >= num)
		{
			return 0;
		}
	}

	return 1;
}

int string_modify_cn_punctuation_to_en(char *str)
{
	size_t i;

	for (i = 0; i < sizeof(g_szCnPunctuation) / sizeof(g_szCnPunctuation[0]); i++)
	{
		if ((0 == strcmp(str, g_szCnPunctuation[i])))
		{
			strcpy(str, (const char *)(g_szEnPunctuation[i]));
			return 0;
		}
	}

	return -1;
}

/* 将字符串中的中文标点符号转为英文符号 */
FRISO_API int utf8_string_ch2en_punctuation(char *str)
{
	int nOffset = 0;
	while (0 != str[nOffset])
	{
		char tmpstr[10] = { 0 };
		int nBytes = get_utf8_bytes(str[nOffset]);
		strncpy(tmpstr, str + nOffset, nBytes);
		tmpstr[nBytes] = 0;

		for (int i = 0; i < sizeof(g_szCnPunctuation) / sizeof(g_szCnPunctuation[0]); i++)
		{
			if ((0 == strcmp(tmpstr, g_szCnPunctuation[i])))
			{
				strcpy(str + nOffset, g_szEnPunctuation[i]);
				strcpy(str + nOffset + strlen(g_szEnPunctuation[i]), str + nOffset + nBytes);
				nBytes = strlen(g_szEnPunctuation[i]);
				break;
			}
		}

		nOffset += nBytes;
	}

	return 0;
}

int string_is_en_sentence_endpunctuation(char *str)
{
	for (int i = 0; i < sizeof(g_szSentEndPunc) / sizeof(g_szSentEndPunc[0]); i++)
	{
		if (0 == strcmp(str, g_szSentEndPunc[i]))
		{
			return 1;
		}
	}

	return 0;
}


#ifdef WIN32
int get_file_dir(char *szFile, char *szFileDir)
{
	strcpy(szFileDir, szFile);
	int len = strlen(szFileDir) - 1;
	while (len >= 0)
	{
		if ('\\' == szFileDir[len] || '/' == szFileDir[len])
		{
			szFileDir[len] = 0;
			return 0;
		}
		len--;
	}

	return 0;
}
#endif

