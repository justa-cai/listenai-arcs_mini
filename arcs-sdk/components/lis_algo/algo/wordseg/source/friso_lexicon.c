/*
 * friso lexicon functions implementation.
 * used to deal with the friso lexicon, like: load,remove,match...
 *
 * @author  lionsoul<chenxin619315@gmail.com>
 */
#include "wordseg_kernel.h"
#include "friso.h"
#include "wordseg_res.h"

#define __SPLIT_MAX_TOKENS__ 5
#define __LEX_FILE_DELIME__ '#'
#define __FRISO_LEX_IFILE__ "friso.lex.ini"

// create a new lexicon
FRISO_API int friso_dic_new(friso_hash_cdt_t dic, uint_t pnWordsNum[__FRISO_LEXICON_LENGTH__])
{
	register uint_t t;
	int ret = 0;
	if (NULL == dic)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	if (NULL != pnWordsNum)
	{
		for (t = 0; t < __FRISO_LEXICON_LENGTH__; t++)
		{
			ret = new_hash_table(dic + t, pnWordsNum[t]);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}
	else
	{
		for (t = 0; t < __FRISO_LEXICON_LENGTH__; t++)
		{
			ret = new_hash_table(dic + t, (uint_t)DEFAULT_LENGTH);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

/**
 * default callback function to invoke
 *     when free the friso dictionary .
 *
 * @date 2013-06-12
 */
__STATIC_API__ void default_fdic_callback(hash_entry_t e)
{
	// register uint_t i;
	// friso_array_t syn;
	lex_entry_cdt_t lex = (lex_entry_cdt_t)e->_val;
	// free the lex->word
	FRISO_FREE(lex->word);

	// free the e->_val
	//@date 2014-01-28 posted by mlemay@gmail.com
	FRISO_FREE(lex);
}

FRISO_API void friso_dic_free(friso_hash_cdt_t dic)
{
	register uint_t t;
	for (t = 0; t < __FRISO_LEXICON_LENGTH__; t++)
	{
		// free the hash table
		free_hash_table(dic + t, default_fdic_callback);
	}

	// FRISO_FREE( dic );
}

// create a new lexicon entry
FRISO_API lex_entry_cdt_t new_lex_entry(fstring word, fstring word_new, uint_t fre, uint_t length, uint_t type)
{
	lex_entry_cdt_t e = (lex_entry_cdt_t)FRISO_MALLOC(sizeof(lex_entry_cdt));
	if (NULL == e)
	{
		ivAssert(0);
		return NULL;
	}

	// initialize.
	e->word = word;
	e->word_new = word_new;
	// e->pos    = NULL;            //part of speech array list.
	// e->py    = NULL; //set to NULL first.
	e->fre = fre;
	e->length = (uchar_t)length; // length
	e->rlen = (uchar_t)length;	 // set to length by default.
	e->type = (uchar_t)type;	 // type
	e->ctrlMask = 0;			 // control mask.
	e->offset = -1;

	return e;
}

/**
 * free the given lexicon entry.
 * you have to do three thing maybe:
 * 1. free where its syn items points to. (not implemented)
 * 2. free its syn. (friso_array_t)
 * 3. free its pos. (friso_array_t)
 * 4. free the lex_entry_cdt_t.
 */
FRISO_API void free_lex_entry_full(lex_entry_cdt_t e)
{
	// register uint_t i;

	// free the lex->word
	FRISO_FREE(e->word);

	// free the e->_val
	//@date 2014-01-28 posted by mlemay@gmail.com
	FRISO_FREE(e);
}

FRISO_API void free_lex_entry(lex_entry_cdt_t e)
{
	// if ( e->syn != NULL ) {
	//     if ( flag == 1 ) free_array_list( e->syn);
	//     else free_array_list( e->syn );
	// }

	FRISO_FREE(e);
}

// add a new entry to the dictionary.
FRISO_API int friso_dic_add(friso_hash_cdt_t dic, friso_lex_t lex, fstring word, fstring word_new, uint_t fre)
{
	void *olex = NULL;
	if (__LEX_TY_EN_WORDS__ == lex || __LEX_TY_EN_WORD__ == lex) //英语词库，全部转为小写字母
	{
		convert_letter_upper_to_lower(word);
	}

	if (lex >= 0 && lex < __FRISO_LEXICON_LENGTH__)
	{
		int ret;
		lex_entry_cdt_t val = new_lex_entry(word, word_new, fre, (uint_t)strlen(word), (uint_t)lex);
		if (NULL == val)
		{
			return TC_WSErrID_OutOfMemory;
		}
		// printf("lex=%d, word=%s, syn=%s\n", lex, word, syn);
		ret = hash_put_mapping(dic + lex, word, (void *)val, &olex);
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
		if (olex != NULL) //重复的词条去掉
		{
			free_lex_entry_full((lex_entry_cdt_t)olex);
		}
	}

	return TC_WSErr_SUCCESS;
}

/*
 * read a line from a specified stream.
 *         the newline will be cleared.
 *
 * @date    2012-11-24
 */
FRISO_API fstring file_get_line(fstring __dst, FILE *_stream)
{
	register int c;
	fstring cs;

	cs = __dst;
	while ((c = fgetc(_stream)) != EOF)
	{
		if (c == '\n')
			break;
		*cs++ = c;
	}
	*cs = '\0';

	return (c == EOF && cs == __dst) ? NULL : __dst;
}

/*
 * static function to copy a string.
 */
/// instead of memcpy
__STATIC_API__ fstring string_copy(
	fstring _src,
	fstring __dst,
	uint_t blocks)
{

	register fstring __src = _src;
	register uint_t t;

	for (t = 0; t < blocks; t++)
	{
		if (*__src == '\0')
			break;
		__dst[t] = *__src++;
	}
	__dst[t] = '\0';

	return __dst;
}

/**
 * make a heap allocation, and copy the
 *     source fstring to the new allocation, and
 *     you should free it after use it .
 *
 * @param _src      source fstring
 * @param blocks    number of bytes to copy
 */
__STATIC_API__ fstring string_copy_heap(fstring _src, uint_t blocks)
{
	register uint_t t;

	fstring str = (fstring)FRISO_MALLOC(blocks + 1);
	if (NULL == str)
	{
		ivAssert(0);
		return NULL;
	}

	for (t = 0; t < blocks; t++)
	{
		// if ( *_src == '\0' ) break;
		str[t] = *_src++;
	}

	str[t] = '\0';
	return str;
}

/*
 * find the postion of the first appear of the given char.
 *    address of the char in the fstring will be return .
 *    if not found NULL will be return .
 */
__STATIC_API__ fstring indexOf(fstring __str, char delimiter)
{
	uint_t i, __length__;

	__length__ = strlen(__str);
	for (i = 0; i < __length__; i++)
	{
		if (__str[i] == delimiter)
		{
			return __str + i;
		}
	}

	return NULL;
}

/**
 * stat all the valid words num
 *
 * @param lex_file    the path of the lexicon file
 * @param pnWordsNum  output the num of the valid words num
 */
FRISO_API uint_t friso_get_dic_wordsnum(fstring lex_file, uint_t *pnWordsNum)
{
	FILE *_stream = NULL;
	char __char[1024];
	fstring _line;
	uint_t nWordsNum = 0;

	if (NULL == lex_file || NULL == pnWordsNum)
	{
		return 0;
	}
	*pnWordsNum = 0;

	_stream = fopen(lex_file, "rb");
	if (NULL == _stream)
	{
		printf("Error:load lex: %s failed!!!!!!!!\r\n", lex_file);
		return 0;
	}

	while ((_line = file_get_line(__char, _stream)) != NULL)
	{
		// clear up the notes
		// make sure the length of the line is greater than 1.
		// like the single '#' mark in stopwords dictionary.
		if (_line[0] == '#' && strlen(_line) > 1)
			continue;

		nWordsNum++;
	}

	*pnWordsNum = nWordsNum;

	fclose(_stream);

	return 0;
}

#if 0
static int friso_get_file_lines_num(FILE *fp)
{
	if (NULL == fp)
	{
		return 0;
	}
	fseek(fp, 0, SEEK_SET);
	char line[1024];
	int line_num = 0;
	while (fgets(line, 1024, fp))
	{
		line_num++;
	}
	fseek(fp, 0, SEEK_SET);
	return line_num;
}
#endif

/**
 * load all the valid wors from a specified lexicon file .
 *
 * @param dic        friso dictionary instance (A hash array)
 * @param lex        the lexicon type
 * @param lex_file    the path of the lexicon file
 * @param length    the maximum length of the word item
 */
FRISO_API int friso_dic_load(friso_entry_t friso, friso_config_t config, friso_lex_t lex, fstring lex_file, uint_t length, uint_t max_word_size_thr)
{
	FILE *_stream = NULL;
	char __char[1024] = {0}, __maxlenword[1024] = {0};
	fstring _line;
	//string_split_entry sse;

	fstring _word, _wordnew;
	//char _sbuffer[512];
	//fstring _syn;
	//friso_array_t sywords;
	//uint_t _fre;
	int ret = 0;
	uint_t wordmaxbytes = 0, wordminbytes=100000; //该词典里词最大/最小字节数(如一个汉字三个字节）
	int bFirstLine = 1;		 //针对第一行做去bom头特殊处理
	

	//统计该词典加载的有效词条数和词典文本总长度. qungao
	int nWordsNum = 0;
	int nWordsStrLen = 0;

	if (NULL == friso || NULL == config || NULL == lex_file)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
#ifdef WIN32
	printf("Lex:%s", lex_file);
#endif
	_stream = fopen(lex_file, "rb");
	if (NULL == _stream)
	{
		ivAssert(0);
		return TC_WSErrID_OpenFile;
	}
	int nLine = 0;
	// 	nLine = friso_get_file_lines_num(_stream);
	// 	rebuild_hash(friso->dic+lex, nLine);

	// printf("begin parser lex [%s]\r\n", lex_file);
	nLine = 0;
	while (NULL != (_line = file_get_line(__char, _stream)))
	{
		nLine++;
		int len = strlen(_line);
		// clear up the notes
		// make sure the length of the line is greater than 1.
		// like the single '#' mark in stopwords dictionary.
		if (bFirstLine)
		{
			// UTF8 bom头删除
			len = strlen(_line);
			if (len > 3 && 0xEF == (unsigned char)(_line[0]) && 0xBB == (unsigned char)(_line[1]) && 0xBF == (unsigned char)(_line[2]))
			{
				memcpy(_line, _line + 3, len - 3);
				_line[len - 3] = 0;
			}
			bFirstLine = 0;
		}
		while (_line[strlen(_line) - 1] == '\r' || _line[strlen(_line) - 1] == '\n') {
			_line[strlen(_line) - 1] = 0;
		}

		if (_line[0] == '#' && 0 != strcmp(_line, "#"))
			continue;

		if ((__LEX_PUNCTUATION__ == lex && 0 == strcmp(_line, "\t")))
		{
			strcpy(_line, "\t"); //将\t作为符号打包
		}
		else
		{
			myDelBegEndBlank(_line); //删除首尾的空格、换行符、tab等无效字符
		}
		len = strlen(_line);
		if (0 == len)
		{
			continue;
		}

		char *p = strstr(_line, "\t");
		if (NULL != p)
		{
			len = p - _line;
		}
		if (len > max_word_size_thr)
		{
			continue; //限制词条最大长度，否则影响效率
		}

		//对于字母 A-Z,a-z和纯数字不打包到分词资源里，这些会导致针对“d esk"->"desk" ”10.5“分到一起等失效 20201222
		if (lex < __LEX_CN_EN_END__) //保持中英文词典打包方式不变
		{
			if (string_is_numeric(_line) || ((1 == strlen(_line) && string_is_letter(_line))))
			{
				continue;
			}
		}

		// add them to the dictionary directly
		_wordnew = NULL;
		uint_t fre = 0;
		if (0 == strcmp(_line, "\t") || NULL == p)
		{
			_word = string_copy_heap(_line, strlen(_line));
		}
		else
		{
			p[0] = 0;
			_word = string_copy_heap(_line, strlen(_line));
			if (__LEX_CN_CHAR__ == lex)
			{
				fre = atoi(p + 1);
			}
			else
			{
				_wordnew = string_copy_heap(p + 1, strlen(p + 1));
			}
		}
		nWordsNum++;
		nWordsStrLen += strlen(_word);
		ret = friso_dic_add(friso->dic, lex, _word, _wordnew, fre);
		if (TC_WSErr_SUCCESS != ret)
		{
			fclose(_stream);
			return ret;
		}
		if (strlen(_word) > wordmaxbytes) //记录最长词条
		{
			wordmaxbytes = strlen(_word);
			strcpy(__maxlenword, _word);
		}
		wordminbytes = strlen(_word) < wordminbytes ? strlen(_word) : wordminbytes;

#ifdef LEX_PRINT
		printf("\rprocess %d----------", nWordsNum);
#endif
	}

#ifdef LEX_PRINT
	printf("Load lex: %s,  wordsNum=%d,  wordsStrLen=%d\r\n", lex_file, nWordsNum, nWordsStrLen);
#endif

	fclose(_stream);

#ifdef LEX_PRINT
	printf("\r\n");
#endif

	if (wordmaxbytes > WS_PER_TOKEN_MAX_LEN)
	{
		ivAssert(0);
		return TC_WSErrID_LexWordTooLong;
	}

	friso->dic[lex].wordmaxbytes = wordmaxbytes > friso->dic[lex].wordmaxbytes ? wordmaxbytes : friso->dic[lex].wordmaxbytes;
	if (nWordsNum>0 && (0 == friso->dic[lex].wordminbytes || wordminbytes < friso->dic[lex].wordminbytes)) {
		friso->dic[lex].wordminbytes = wordminbytes;
	}

	return TC_WSErr_SUCCESS;
}

/**
 * get the lexicon type index with the specified
 *     type keywords .
 *
 * @see        friso.h#friso_lex_t
 * @param     _key
 * @return     int
 */
__STATIC_API__ friso_lex_t get_lexicon_type_with_constant(fstring _key)
{
	if (0 == strcmp(_key, "__LEX_CJK_WORDS__"))
	{
		return __LEX_CJK_WORDS__;
	}
	if (0 == strcmp(_key, "__LEX_CN_CHAR__"))
	{
		return __LEX_CN_CHAR__;
	}	
	else if (0 == strcmp(_key, "__LEX_PINYIN_DICT__"))
	{
		return __LEX_PINYIN_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_PINYIN_SOFT_DICT__"))
	{
		return __LEX_PINYIN_SOFT_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_PINYIN_CHAIFEN_DICT__"))
	{
		return __LEX_PINYIN_CHAIFEN_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_TTS_USER_DICT__"))
	{
		return __LEX_TTS_USER_DICT__;
	}	
	else if (0 == strcmp(_key, "__LEX_TY_CN_WORDS__"))
	{
		return __LEX_TY_CN_WORDS__;
	}	
	else if (0 == strcmp(_key, "__LEX_TY_EN_WORDS__"))
	{
		return __LEX_TY_EN_WORDS__;
	}
	else if (0 == strcmp(_key, "__LEX_TY_EN_WORD__"))
	{
		return __LEX_TY_EN_WORD__;
	}
	else if (0 == strcmp(_key, "__LEX_TY_EN_ABBREVIATION__"))
	{
		return __LEX_TY_EN_ABBREVIATION__;
	}
	else if (0 == strcmp(_key, "__LEX_PUNCTUATION__"))
	{
		return __LEX_PUNCTUATION__;
	}
	else if (0 == strcmp(_key, "__LEX_KOREAN_WORDS__"))
	{
		return __LEX_KOREAN_WORDS__;
	}
	else if (0 == strcmp(_key, "__LEX_JAPANESR_CHAR__"))
	{
		return __LEX_JAPANESR_CHAR__;
	}
	else if (0 == strcmp(_key, "__LEX_JAPANESR_WORDS__"))
	{
		return __LEX_JAPANESR_WORDS__;
	}
	else if (0 == strcmp(_key, "__LEX_SPANISH_CHAR__"))
	{
		return __LEX_SPANISH_CHAR__;
	}
	else if (0 == strcmp(_key, "__LEX_SPANISH_WORDS__"))
	{
		return __LEX_SPANISH_WORDS__;
	}
	else if (0 == strcmp(_key, "__LEX_RUSSIAN_CHAR__"))
	{
		return __LEX_RUSSIAN_CHAR__;
	}
// 	else if (0 == strcmp(_key, "__LEX_RUSSIAN_WORDS__"))
// 	{
// 		return __LEX_RUSSIAN_WORDS__;
// 	}
	else if (0 == strcmp(_key, "__LEX_ISE_EN_USER_DICT__"))
	{
		return __LEX_ISE_EN_USER_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_ISE_CN_USER_DICT__"))
	{
		return __LEX_ISE_CN_USER_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_PINYIN_DICT__"))
	{
		return __LEX_PINYIN_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_PINYIN_SOFT_DICT__"))
	{
		return __LEX_PINYIN_SOFT_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_PINYIN_CHAIFEN_DICT__"))
	{
		return __LEX_PINYIN_CHAIFEN_DICT__;
	}
	else if (0 == strcmp(_key, "__LEX_CORNERMARK__"))
	{
		return __LEX_CORNERMARK__;
	}

	return -1;
}

/*
 * load the lexicon configuration file.
 *        and load all the valid lexicon from the configuration file.
 *
 * @param friso     friso instance
 * @param config    friso_config instance
 * @param _path     dictionary directory
 * @param _limitts  words length limit
 */

FRISO_API int friso_dic_load_from_ifile(friso_entry_t friso, friso_config_t config, fstring _path, uint_t _limits)
{
	// 1.parse the configuration file.
	FILE *__stream;
	char __chars__[1024], __key__[50], *__line__;
	uint_t __length__, i, t;
	friso_lex_t lex_t;
	string_buffer sb[1];
	int ret = 0;
	int nLine = 0;
	uint_t max_word_size_thr = SUPPORT_DICT_WORD_MAX_LEN;

	if (NULL == friso || NULL == config || NULL == _path)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	// get the lexicon configruation file path
	memset(sb, 0, sizeof(sb));
	string_buffer_append(sb, _path);
	string_buffer_append(sb, __FRISO_LEX_IFILE__);
	// printf("%s\n", sb->buffer);

	__stream = fopen(sb->buffer, "rb");
	if (NULL == __stream)
	{
		// fprintf(stderr, "Error: Fail to open the lexicon configuration file %s\n", sb->buffer);
		ivAssert(0);
		// free_string_buffer(sb);
		return TC_WSErrID_OpenFile;
	}

#ifdef LEX_PRINT
	printf("---Lex_init:%s---\r\n", sb->buffer);
#endif

	while (NULL != (__line__ = file_get_line(__chars__, __stream)))
	{
		nLine++;
		// comment filter.
		if (__line__[0] == '#')
			continue;
		if (__line__[0] == '\0')
			continue;

		const char *tag = "max_word_size:";
		if (0 == strncmp(__line__, tag, strlen(tag))) {
			max_word_size_thr = atoi(__line__ + strlen(tag));
			printf("max_word_size_thr=%d\r\n", max_word_size_thr);
		}

		__length__ = strlen(__line__);

		if (__length__ > 0 && __line__[__length__ - 1] == '\r')
		{
			__line__[__length__ - 1] = 0;
			__length__--;
		}

		// item start
		if (__line__[__length__ - 1] == '[')
		{
			// get the type key
			for (i = 0; i < __length__ && (__line__[i] == ' ' || __line__[i] == '\t'); i++)
				;
			for (t = 0; i < __length__; i++, t++)
			{
				if (__line__[i] == ' ' || __line__[i] == '\t' || __line__[i] == ':')
					break;
				__key__[t] = __line__[i];
			}
			__key__[t] = '\0'; // eg:__LEX_CJK_WORDS__

			// get the lexicon type
			lex_t = get_lexicon_type_with_constant(__key__);
			if (lex_t < 0)
			{
				ivAssert(0);
				fclose(__stream);
				return TC_WSErrID_LexType;
			}

			if (lex_t < __LEX_END)
			{
				if (strlen(__key__) >= LEX_NAME_SIZE) {
					ivAssert(0);
					return TC_WSErrID_LexNameTooLong;
				}
				strcpy(friso->dic[lex_t].szName, __key__); // qungao
			}
			// printf("key=%s, type=%d\n", __key__, lex_t );
			while ((__line__ = file_get_line(__chars__, __stream)) != NULL)
			{
				nLine++;
				// comments filter.
				if (__line__[0] == '#')
					continue;
				if (__line__[0] == '\0')
					continue;

				__length__ = strlen(__line__);
				if (__length__ > 0 && __line__[__length__ - 1] == '\r')
				{
					__line__[__length__ - 1] = 0;
					__length__--;
				}

				if (__line__[__length__ - 1] == ']')
					break;

				for (i = 0; i < __length__ && (__line__[i] == ' ' || __line__[i] == '\t'); i++)
					;
				for (t = 0; i < __length__; i++, t++)
				{
					if (__line__[i] == ' ' || __line__[i] == '\t' || __line__[i] == ';')
						break;
					__key__[t] = __line__[i];
				}
				__key__[t] = '\0';

				// load the lexicon item from the lexicon file.
				string_buffer_clear(sb);
				string_buffer_append(sb, _path);
				string_buffer_append(sb, __key__);
				// printf("key=%s, type=%d\n", __key__, lex_t);
				ret = friso_dic_load(friso, config, lex_t, sb->buffer, _limits, max_word_size_thr); // qungao 解析一个lex词典，构建hash值
				if (0 != ret)
				{
					fclose(__stream);
					return ret;
				}
			}
		}
	} // end while

	fclose(__stream);

	return TC_WSErr_SUCCESS;
}

// match the item.
FRISO_API int friso_dic_match(friso_hash_cdt_t dic, friso_lex_t lex_start, friso_lex_t lex_end, fstring word)
{
	int iLex;

	for (iLex = lex_start; iLex <= lex_end; iLex++)
	{
		int bExist = 0;
		if (iLex < 0 || iLex >= __FRISO_LEXICON_LENGTH__)
		{
			continue;
		}
		bExist = hash_exist_mapping(dic + iLex, word);
		if (bExist)
		{
			return 1;
		}
	}
	return 0;
}

// get the lex_entry_cdt_t associated with the word.
//从dic[lex_start, lex_end]查询word
FRISO_API lex_entry_cdt_t friso_dic_get(void *res, PWSCallBack func_read_data_res, friso_lex_t lex_start, friso_lex_t lex_end, fstring word)
{
	if (NULL == word || 0 == strlen(word))
		return NULL;

#if 0//#ifdef _DEBUG
	FILE *fp = fopen("xxxx.txt", "ab");
	fprintf(fp, "%s\tlex=[%d,%d]\r\n", word, lex_start, lex_end);
	fclose(fp);
#endif

#if 0
	static FILE *fpxx = NULL;
	if (NULL == fpxx) {
		fpxx = fopen("log.txt", "ab");
	}
	fprintf(fpxx, "lex[%2d=%2d]\t\tword=%s\r\n", lex_start, lex_end, word);
	fflush(fpxx);
#endif

	PWordsegDict dic = (PWordsegDict)res;
	PTmpRst rst = wordseg_dic_get(dic, (PWSCallBack)func_read_data_res, lex_start, lex_end, word);
	if (NULL == rst) {
		return NULL;
	}
	else {
		memset((void *)(dic->val), 0, sizeof(lex_entry_cdt));
		dic->val->type = rst->type;
		dic->val->length = strlen(word);
		dic->val->rlen = strlen(word);
		dic->val->word = rst->word;
		if (0 != rst->wordnew[0]) {
			dic->val->word_new = rst->wordnew;
		}
		return dic->val;
	}
}

// get the size of the specified type dictionary.
FRISO_API uint_t friso_spec_dic_size(friso_hash_cdt_t *dic, friso_lex_t lex)
{
	if (lex >= 0 && lex < __FRISO_LEXICON_LENGTH__)
	{
		return hash_get_size(dic[lex]);
	}
	return 0;
}

// get size of the whole dictionary.
FRISO_API uint_t friso_all_dic_size(
	friso_hash_cdt_t *dic)
{
	register uint_t size = 0, t;

	for (t = 0; t < __FRISO_LEXICON_LENGTH__; t++)
	{
		size += hash_get_size(dic[t]);
	}

	return size;
}

//将字符串都转为小写字母
void convert_letter_upper_to_lower(fstring szWords)
{
	size_t i;
	for (i = 0; i < strlen(szWords); i++)
	{
		if (szWords[i] >= 65 && szWords[i] <= 90)
		{
			szWords[i] += 32;
		}
	}
	return;
}

//将字符串都转为小写字母
void convert_letter_upper_to_lower_ex(fstring szWords, int nLen)
{
	int i;
	for (i = 0; i < nLen; i++)
	{
		if (szWords[i] >= 65 && szWords[i] <= 90)
		{
			szWords[i] += 32;
		}
	}
	return;
}

/* ************************
 *  mapping function area *
 **************************/
uint_t myHash(fstring str, uint_t length)
{
	// hash code
	uint_t h = 0;

	while (*str != '\0')
	{
		h = h * 1313131 + (unsigned char)(*str++);
	}

	return (h % length);
}

//删除首尾无效字符
void myDelBegEndBlank(char *pStr)
{
	int i, j, nTmp;

	nTmp = (int)strlen(pStr);
	if (0 == nTmp)
		return;

	while ((--nTmp >= 0) && (' ' == pStr[nTmp] || '\t' == pStr[nTmp] || '\r' == pStr[nTmp] || '\n' == pStr[nTmp]))
	{
		pStr[nTmp] = '\0';
	}
	if (nTmp < 0)
		return;

	i = 0;
	while (('\0' != pStr[i]) && (' ' == pStr[i] || '\t' == pStr[i] || '\r' == pStr[i] || '\n' == pStr[i]))
	{
		i++;
	}

	if (i > 0)
	{
		for (j = i; j <= nTmp; j++)
		{
			pStr[j - i] = pStr[j];
		}

		pStr[j - i] = '\0';
	}
}

//判断字符串是否都是大小写字母
int string_is_letter(const char *str)
{
	size_t i;
	for (i = 0; i < strlen(str); i++)
	{
		//[65,90]:大写字母  [97,122]:小写字母
		if (!(str[i] >= 65 && str[i] <= 90) &&
			!(str[i] >= 97 && str[i] <= 122))
		{
			return 0;
		}
	}
	return 1;
}

//判断字符串是否都是数字，是返回1，否返回0
int string_is_numeric(const char *str)
{
	size_t i;
	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] < 48 || str[i] > 57)
		{
			return 0;
		}
	}
	return 1;
}
