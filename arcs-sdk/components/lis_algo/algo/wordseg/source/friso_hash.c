/*
 * friso hash table functions implementation defined in header file "friso_API.h".

 * @author  lionsoul<chenxin619315@gmail.com>
 */
#include "wordseg_kernel.h"

#if PRINT_RES_READ
int g_nReadResSize = 0;
int g_nReadResCnt = 0, g_nReadResCntPerLex[50] = { 0 };
FILE *g_fpReadResLog = NULL;
int g_nCallLexCntMax = 0;
char g_pReadResLogFileName[256] = { 0 };
#endif

//-166411799L
// 31 131 1331 13331 133331 ..
// 31 131 1313 13131 131313 ..    the best
#define HASH_FACTOR 1313131
/* ************************
 *  mapping function area *
 ************关键字：字符串哈希算法 **************/
FRISO_API uint_t hash(fstring str, uint_t length)
{
    // hash code
    uint_t h = 0;

    while ('\0' != *str)
    {
        h = h * HASH_FACTOR + (unsigned char)(*str++);
    }

    return (h % length);
}

/*test if a integer is a prime.*/
__STATIC_API__ int is_prime(int n)
{
    int j;
    if (n == 2 || n == 3)
    {
        return 1;
    }

    if (n == 1 || n % 2 == 0)
    {
        return 0;
    }

    for (j = 3; j * j < n; j++)
    {
        if (n % j == 0)
        {
            return 0;
        }
    }

    return 1;
}

/*get the next prime just after the speicified integer.*/
__STATIC_API__ int next_prime(int n)
{
    if (n % 2 == 0)
        n++;
    for (; !is_prime(n); n = n + 2)
        ;

    return n;
}

// fstring copy, return the pointer of the new string.
// static fstring string_copy( fstring _src ) {
// int bytes = strlen( _src );
// fstring _dst = ( fstring ) FRISO_MALLOC( bytes + 1 );
// register int t = 0;

// do {
//_dst[t] = _src[t];
// t++;
// } while ( _src[t] != '\0' );
//_dst[t] = '\0';

// return _dst;
// }

/* *********************************
 * static hashtable function area. *
 ***********************************/
__STATIC_API__ hash_entry_t new_hash_entry(fstring key, void *value, hash_entry_t next)
{
    hash_entry_t e = (hash_entry_t)FRISO_MALLOC(sizeof(friso_hash_entry));
    if (NULL == e)
    {
        ivAssert(0);
        return NULL;
    }

    // e->_key = string_copy( key );
    e->_word = key;
    e->_val = value;
    e->_next = next;

    return e;
}

// create blocks copy of entries.
__STATIC_API__ hash_entry_t *create_hash_entries(uint_t blocks)
{
    register uint_t t;
    hash_entry_t *e = (hash_entry_t *)FRISO_CALLOC(sizeof(hash_entry_t), blocks);
    if (NULL == e)
    {
        ivAssert(0);
        return NULL;
    }

    for (t = 0; t < blocks; t++)
    {
        e[t] = NULL;
    }

    return e;
}

// a static function to do the re-hash work.
FRISO_API int rebuild_hash(friso_hash_cdt_t _hash, uint_t nLength)
{
    // printf("rehashed.\n");
    // find the next prime as the length of the hashtable.
    uint_t t, length;
    hash_entry_t e, next, *_src = _hash->table, *table = NULL;
    uint_t bucket;

    if (nLength > 0)
    {
        length = next_prime(nLength);
    }
    else
    {
        length = next_prime(_hash->length * 3/2 + 1);
    }
    table = create_hash_entries(length);
    if (NULL == table)
    {
        return TC_WSErrID_OutOfMemory;
    }
    // copy the nodes
    for (t = 0; t < _hash->length; t++)
    {
        e = *(_src + t);
        if (e != NULL)
        {
            do
            {
                next = e->_next;
                bucket = hash(e->_word, length);
                e->_next = table[bucket];
                table[bucket] = e;
                e = next;
            } while (e != NULL);
        }
    }

    _hash->table = table;
    _hash->length = length;
    _hash->threshold = (uint_t)(_hash->length * _hash->factor);

    // free the old hash_entry_t blocks allocations.
    FRISO_FREE(_src);

    return TC_WSErr_SUCCESS;
}

/* ********************************
 * hashtable interface functions. *
 * ********************************/

// create a new hash table.
FRISO_API int new_hash_table(friso_hash_cdt_t dict, uint_t nLength)
{
    // initialize the the hashtable
    dict->length = nLength; // DEFAULT_LENGTH;
    dict->size = 0;
    dict->factor = DEFAULT_FACTOR;
    dict->threshold = (uint_t)(dict->length * dict->factor);
    dict->table = create_hash_entries(dict->length);
    if (NULL == dict->table)
    {
        return TC_WSErrID_OutOfMemory;
    }

    return TC_WSErr_SUCCESS;
}

FRISO_API void free_hash_table(friso_hash_cdt_t _hash, fhash_callback_fn_t fentry_func)
{
    register uint_t j;
    hash_entry_t e, n;

    for (j = 0; j < _hash->length; j++)
    {
        e = *(_hash->table + j);
        for (; e != NULL;)
        {
            n = e->_next;
            if (fentry_func != NULL)
                fentry_func(e);
            FRISO_FREE(e);
            e = n;
        }
    }

    // free the pointer array block ( 4 * htable->length continuous bytes ).
    FRISO_FREE(_hash->table);
}

// put a new mapping insite.
// the value cannot be NULL.
FRISO_API int hash_put_mapping(friso_hash_cdt_t _hash, fstring key, void *value, void **oldvalue)
{
    uint_t bucket = (key == NULL) ? 0 : hash(key, _hash->length);
    hash_entry_t e = *(_hash->table + bucket);
    void *oval = NULL;

    // check the given key is already exists or not.
    for (; e != NULL; e = e->_next)
    {
        if (key == e->_word || (key != NULL && e->_word != NULL && strcmp(key, e->_word) == 0))
        {
            oval = e->_val; // bak the old value
            e->_word = key;
            e->_val = value;
            *oldvalue = oval;
            return TC_WSErr_SUCCESS;
        }
    }

    // put a new mapping into the hashtable.
    _hash->table[bucket] = new_hash_entry(key, value, _hash->table[bucket]);
    if (NULL == _hash->table[bucket])
    {
        return TC_WSErrID_OutOfMemory;
    }
    _hash->size++;

    // check the condition to rebuild the hashtable.
    if (_hash->size >= _hash->threshold)
    {
        int ret = rebuild_hash(_hash, 0);
        if (TC_WSErr_SUCCESS != ret)
        {
            return TC_WSErrID_RebuildHash;
        }
        // static int g_nRebuildHashNum = 0;   //qungao
        // printf("g_nRebuildHashNum=%d   %d->%d\r\n", ++g_nRebuildHashNum, _hash->length, _hash->length * 2 + 1);
    }

    *oldvalue = oval;
    return TC_WSErr_SUCCESS;
}

// check the existence of the mapping associated with the given key.
FRISO_API int hash_exist_mapping(friso_hash_cdt_t _hash, fstring key)
{
    uint_t bucket = (key == NULL) ? 0 : hash(key, _hash->length);
    hash_entry_t e;

    for (e = *(_hash->table + bucket); e != NULL; e = e->_next)
    {
#if PRINT_RES_READ
        // printf("read res:func=%s\tsize=%d\r\n", __FUNCTION__, sizeof(friso_hash_entry)+strlen(e->_word));
        g_nReadResSize += sizeof(friso_hash_entry) + strlen(e->_word);
        g_nReadResCnt++;
#endif
        if (key == e->_word || (key != NULL && e->_word != NULL && strcmp(key, e->_word) == 0))
        {
            return 1;
        }
    }

    return 0;
}

// get the value associated with the given key.
FRISO_API void *hash_get_value(friso_hash_cdt_t _hash, fstring key)
{
    uint_t bucket = (key == NULL) ? 0 : hash(key, _hash->length);
    hash_entry_t e;

    for (e = *(_hash->table + bucket); e != NULL; e = e->_next)
    {
#if PRINT_RES_READ
        // printf("read res:func=%s\tsize=%d\r\n", __FUNCTION__, sizeof(friso_hash_entry) + strlen(e->_word));
        g_nReadResSize += sizeof(friso_hash_entry) + strlen(e->_word);
        g_nReadResCnt++;
#endif
        if (key == e->_word || (key != NULL && e->_word != NULL && strcmp(key, e->_word) == 0))
        {
            return e->_val; // lex_entry_cdt *
        }
    }

    return NULL;
}

// remove the mapping associated with the given key.
FRISO_API hash_entry_t hash_remove_mapping(friso_hash_cdt_t _hash, fstring key)
{
    uint_t bucket = (key == NULL) ? 0 : hash(key, _hash->length);
    hash_entry_t e, prev = NULL;
    hash_entry_t b;

    for (e = *(_hash->table + bucket); e != NULL; prev = e, e = e->_next)
    {
        if (key == e->_word || (key != NULL && e->_word != NULL && strcmp(key, e->_word) == 0))
        {
            b = e;
            // the node located at *( htable->table + bucket )
            if (prev == NULL)
            {
                _hash->table[bucket] = e->_next;
            }
            else
            {
                prev->_next = e->_next;
            }
            // printf("%s was removed\n", b->_key);
            _hash->size--;
            // FRISO_FREE( b );
            return b;
        }
    }

    return NULL;
}

// count the size.(A macro define has replace this.)
// FRISO_API uint_t hash_get_size( friso_hash_cdt_t _hash ) {
//     return _hash->size;
// }
