/*
 * friso ADT application interface header source file.
 * 1. string bufffer interface.
 * 2. hashmap interface.
 * 3. dynamaic array interface.
 * 4. double link list interface.
 *
 * @author chenxin <chenxin619315@gmail.com>
 */

#ifndef _friso_api_h
#define _friso_api_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WIN32
#include <crtdbg.h>
#endif
#include "wordseg_cfg.h"

#if PRINT_RES_READ
extern int g_nReadResSize;
extern int g_nReadResCnt;
extern int g_nReadResCntPerLex[50];
extern FILE *g_fpReadResLog;
extern int g_nCallLexCntMax;
extern char g_pReadResLogFileName[256];
#endif

#ifdef WIN32
#define ivAssert(exp) _ASSERT(exp) /* 断言操作(可以为空) */
#else
#define ivAssert(exp) /* 断言操作(可以为空) */
#endif

// yat, just take it as this way, 99 percent you will find no problem
#if (defined(_WIN32) || defined(_WINDOWS_) || defined(__WINDOWS_))
#define FRISO_WINNT
#else
#define FRISO_LINUX
#endif

#ifdef FRISO_WINNT
#define FRISO_API extern __declspec(dllexport)
#define __STATIC_API__ static
#else
/*platform shared library statement :: unix*/
#define FRISO_API extern
#define __STATIC_API__ static inline
#endif

#define print(str) printf("%s", str)
#define println(str) printf("%s\n", str)

/*
 * memory allocation macro definition which make it more more convenient
 * to change to use your favorite or a better memory manage library.
 */
#if TEST_MEMORY_LEAK
void *FRISO_CALLOC(int _bytes, int _blocks);
void *FRISO_MALLOC(int _bytes);
void FRISO_FREE(void *_ptr);
void AnalyseMemoryLeak();
#else
#define FRISO_CALLOC(_bytes, _blocks) calloc(_bytes, _blocks)
#define FRISO_MALLOC(_bytes) malloc(_bytes)
#define FRISO_FREE(_ptr) free(_ptr)
#endif

typedef unsigned short ushort_t;
typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef char *fstring;

/* {{{ fstring handle interface define::start. */
#define __CHAR_BYTES__ 8

typedef struct
{
    char buffer[WS_INPUT_TEXT_MAX_LEN + 10];
    // fstring buffer;
    uint_t length;
    uint_t allocs;
} string_buffer_entry;

typedef string_buffer_entry string_buffer;
typedef string_buffer_entry *string_buffer_t;

// FRISO_API string_buffer_t new_string_buffer( void );
FRISO_API string_buffer_t new_string_buffer();
FRISO_API string_buffer_t new_string_buffer_with_string(fstring str);
FRISO_API void set_string_buffer_with_string(string_buffer_t, fstring);

/*
 * this function will copy the chars that the fstring pointed.
 *        to the buffer.
 * this may cause the resize action of the buffer.
 */
FRISO_API void string_buffer_append(string_buffer_t, fstring);
FRISO_API void string_buffer_append_char(string_buffer_t, char);

// insert the given fstring from the specified position.
FRISO_API void string_buffer_insert(string_buffer_t, uint_t idx, fstring);

// remove the char in the specified position.
FRISO_API fstring string_buffer_remove(string_buffer_t, uint_t idx, uint_t);

/*
 * turn the string_buffer to a string.
 *        or return the buffer of the string_buffer.
 */
FRISO_API string_buffer_t string_buffer_trim(string_buffer_t);

/*
 * free the given fstring buffer.
 *        and this function will not free the allocations of the
 *        the string_buffer_t->buffer, we return it to you, if there is
 *     a necessary you could free it youself by calling free();
 */
FRISO_API fstring string_buffer_devote(string_buffer_t);

/*
 * clear the given fstring buffer.
 *        reset its buffer with 0 and reset its length to 0.
 */
FRISO_API void string_buffer_clear(string_buffer_t);

// free the fstring buffer include the buffer.
FRISO_API void free_string_buffer(string_buffer_t);

/**
 * fstring specified chars tokenizer functions
 *
 * @date 2013-06-08
 */
typedef struct
{
    fstring source;
    uint_t srcLen;
    fstring delimiter;
    uint_t delLen;
    uint_t idx;
} string_split_entry;
typedef string_split_entry *string_split_t;

/**
 * create a new string_split_entry.
 *
 * @param    source
 * @return    string_split_t;
 */
// FRISO_API string_split_t new_string_split( fstring, fstring );

FRISO_API void string_split_reset(string_split_t, fstring, fstring);

FRISO_API void string_split_set_source(string_split_t, fstring);

FRISO_API void string_split_set_delimiter(string_split_t, fstring);

// FRISO_API void free_string_split( string_split_t );

/**
 * get the next split fstring, and copy the
 *     splited fstring into the __dst buffer .
 *
 * @param    string_split_t
 * @param    __dst
 * @return    fstring (NULL if reach the end of the source
 *         or there is no more segmentation)
 */
FRISO_API fstring string_split_next(string_split_t, fstring);
/* }}} */

/* {{{ dynamaic array interface define::start*/

/*friso array list entry struct*/
typedef struct
{
    void **items;
    uint_t allocs;
    uint_t length;
} friso_array_entry;

typedef friso_array_entry *friso_array_t;

// create a new friso dynamic array.
// FRISO_API friso_array_t new_array_list( void );
#define new_array_list() new_array_list_with_opacity(WS_INPUT_TEXT_MAX_LEN)

// create a new friso dynamic array with the given opacity
FRISO_API friso_array_t new_array_list_with_opacity(uint_t);

/*
 * free the given friso array.
 *     and its items, but never where the items's item to pointed to .
 */
FRISO_API void free_array_list(friso_array_t, int);

// add a new item to the array.
FRISO_API void array_list_add(friso_array_t, void *);

// insert a new item at a specifed position.
FRISO_API void array_list_insert(friso_array_t, uint_t, void *);

// get a item at a specified position.
FRISO_API void *array_list_get(friso_array_t, uint_t);

/*
 * set the item at a specified position.
 *     this will return the old value.
 */
FRISO_API void *array_list_set(friso_array_t, uint_t, void *);

/*
 * remove the given item at a specified position.
 *    this will return the value of the removed item.
 */
FRISO_API void *array_list_remove(friso_array_t, uint_t);

/*trim the array list for final use.*/
FRISO_API friso_array_t array_list_trim(friso_array_t);

/*
 * clear the array list.
 *     this function will free all the allocations that the pointer pointed.
 *        but will not free the point array allocations,
 *        and will reset the length of it.
 */
FRISO_API friso_array_t array_list_clear(friso_array_t);

// return the size of the array.
// FRISO_API uint_t array_list_size( friso_array_t );
#define array_list_size(array) array->length

// return the allocations of the array.
// FRISO_API uint_t array_list_allocs( friso_array_t );
#define array_list_allocs(array) array->allocs

// check if the array is empty.
// FRISO_API int array_list_empty( friso_array_t );
#define array_list_empty(array) (array->length == 0)
/* }}} dynamaic array interface define::end*/

/* {{{ link list interface define::start*/
struct friso_link_node
{
    void *value;
    struct friso_link_node *prev;
    struct friso_link_node *next;
};
typedef struct friso_link_node link_node_entry;
typedef link_node_entry *link_node_t;

/*
 * link list adt
 */
typedef struct
{
    link_node_t head;
    link_node_t tail;
    uint_t size;
} friso_link_entry;

typedef friso_link_entry *friso_link_t;

// create a new link list
FRISO_API friso_link_t new_link_list(void);

// free the specified link list
FRISO_API void free_link_list(friso_link_t);

// return the size of the current link list.
// FRISO_API uint_t link_list_size( friso_link_t );
#define link_list_size(link) link->size

// check the given link is empty or not.
// FRISO_API int link_list_empty( friso_link_t );
#define link_list_empty(link) (link->size == 0)

// clear all the nodes in the link list( except the head and the tail ).
FRISO_API friso_link_t link_list_clear(friso_link_t link);

// add a new node to the link list.(append from the tail)
FRISO_API void link_list_add(friso_link_t, void *);

// add a new node before the specified node
FRISO_API void link_list_insert_before(friso_link_t, uint_t, void *);

// get the node in the current index.
FRISO_API void *link_list_get(friso_link_t, uint_t);

// modify the node in the current index.
FRISO_API void *link_list_set(friso_link_t, uint_t, void *);

// remove the specified link node
FRISO_API void *link_list_remove(friso_link_t, uint_t);

// remove the given node
FRISO_API void *link_list_remove_node(friso_link_t, link_node_t);

// remove the node from the frist.
FRISO_API void *link_list_remove_first(friso_link_t);

// remove the last node from the link list
FRISO_API void *link_list_remove_last(friso_link_t);

// append a node from the end.
FRISO_API void link_list_add_last(friso_link_t, void *);

// add a node at the begining of the link list.
FRISO_API void link_list_add_first(friso_link_t, void *);
/* }}} link list interface define::end*/

/* {{{ hashtable interface define :: start*/
struct hash_entry
{
    fstring _word;            //文本字符串指针
    void *_val;               // the node value  lex_entry_t
    struct hash_entry *_next; //指向下一个hash值相同的词
};
typedef struct hash_entry friso_hash_entry;
typedef struct hash_entry *hash_entry_t;
typedef void (*fhash_callback_fn_t)(hash_entry_t);

#define LEX_NAME_SIZE		(32)
typedef struct
{
    char szName[LEX_NAME_SIZE];       //词典名称
	uint_t wordminbytes;   //该词典里词最少字节数
	uint_t wordmaxbytes;   //该词典里词最多字节数(如一个汉字三个字节）
    //char longestword[128]; //最长的单词，仅用于查看
    uint_t length;         //分配的table个数
    uint_t size;           // length中实际使用的个数
    float factor;          //用于分配length的控制参数
    uint_t threshold;      //用于分配length的控制参数
    hash_entry_t *table;

    uint_t table_offset;
	uint_t table_data_offset; //记录该hash表文本在data.bin中起始偏移
	uint_t table_data_size;  //记录该hansh表文本总字节数，以便后续一把导出导入ram
} friso_hash_cdt;

typedef friso_hash_cdt *friso_hash_cdt_t;

// default value for friso_hash_cdt
#define DEFAULT_LENGTH 31
#define DEFAULT_FACTOR 0.85f

/*
 * Function: new_hash_table
 * Usage: table = new_hash_table();
 * --------------------------------
 * this function allocates a new symbol table with no entries.
 */
FRISO_API int new_hash_table(friso_hash_cdt_t, uint_t nLength);

/*
 * Function: free_hash_table
 * Usage: free_hash_table( table );
 * --------------------------------------
 * this function will free all the allocation for memory.
 */
FRISO_API void free_hash_table(friso_hash_cdt_t, fhash_callback_fn_t);

/*
 * Function: put_new_mapping
 * Usage: put_mapping( table, key, value );
 * ----------------------------------------
 * the function associates the specified key with the given value.
 */
FRISO_API int hash_put_mapping(friso_hash_cdt_t, fstring, void *, void **);

/*
 * Function: is_mapping_exists
 * Usage: bool = is_mapping_exists( table, key );
 * ----------------------------------------------
 * this function check the given key mapping is exists or not.
 */
FRISO_API int hash_exist_mapping(friso_hash_cdt_t, fstring);

/*
 * Function: get_mapping_value
 * Usage: value = get_mapping_value( table, key );
 * -----------------------------------------------
 * this function return the value associated with the given key.
 *         UNDEFINED will be return if the mapping is not exists.
 */
FRISO_API void *hash_get_value(friso_hash_cdt_t, fstring);

FRISO_API uint_t hash(fstring str, uint_t length);


/*
 * Function: remove_mapping
 * Usage: remove_mapping( table, key );
 * ------------------------------------
 * This function is used to remove the mapping associated with the given key.
 */
FRISO_API hash_entry_t hash_remove_mapping(friso_hash_cdt_t, fstring);

FRISO_API int rebuild_hash(friso_hash_cdt_t _hash, uint_t nLength);

/*
 * Function: get_table_size
 * Usage: size = get_table_size( table );
 * --------------------------------------
 * This function is used to count the size of the specified table.
 */
// FRISO_API uint_t hash_get_size( friso_hash_cdt_t );
#define hash_get_size(hash) hash->size
/* }}} hashtable interface define :: end*/

/* {{{ utf8 string interface define :: start*/

/*
 * Function: get_utf8_bytes
 *
 * */
FRISO_API int get_utf8_bytes(char);

/*
 * Function: get_utf8_unicode
 *
 * */
FRISO_API int get_utf8_unicode(const fstring);

/*
 * Function: unicode_to_utf8
 *
 * */
FRISO_API int unicode_to_utf8(uint_t, fstring);

/* }}} utf8 string interface define :: start*/

//将字符串都转为小写字母
void convert_letter_upper_to_lower(fstring szWords);
void convert_letter_upper_to_lower_ex(fstring szWords, int nLen);

uint_t myHash(fstring str, uint_t length);
void myDelBegEndBlank(char *pStr); //删除首尾无效字符

int string_is_letter(const char *str);  //判断字符串是否都是大小写字母
int string_is_numeric(const char *str); //判断字符串是否都是数字，是返回1，否返回0

#endif /*end ifndef*/
