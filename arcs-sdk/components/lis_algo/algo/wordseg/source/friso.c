/*
 * friso main source file with the the friso main functions implemented.
 * starts with friso_ in the friso header file "friso.h";
 *
 * @author  lionsoul<chenxin619315@gmail.com>
 */

#include "wordseg_kernel.h"
#include "friso_ctype.h"
#include "friso.h"
#include "wordseg_res.h"

//-----------------------------------------------------------------
// friso instance about function
/* {{{ create a new friso configuration variable.
 */
FRISO_API friso_entry_t friso_new(void)
{
	friso_entry_t e = (friso_entry_t)FRISO_MALLOC(sizeof(friso_entry));
	if (NULL == e)
	{
		return NULL;
	}

	memset(e, 0, sizeof(friso_entry));

	// e->dic = NULL;
	e->charset = FRISO_UTF8; // set default charset UTF8.

	return e;
}
/* }}} */

/* {{{ creat a new friso with initialize item from a configuration file.
 *
 * @return 0 for successfully and -1 for failed.
 */
FRISO_API int friso_init_from_ifile(friso_entry_t friso, friso_config_t config, fstring __ifile)
{
	FILE *__stream;
	char __chars__[256], __key__[128], *__line__;
	char __lexi__[160], lexpath[160];
	uint_t i, t, __hit__ = 0, __length__;

	char *slimiter = NULL;
	uint_t flen = 0;

	if (NULL == friso || NULL == config || NULL == __ifile)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	// get the base part of the path of the __ifile
	if (NULL != (slimiter = strrchr(__ifile, '/')))
	{
		flen = slimiter - __ifile + 1;
	}

	// yat, start to parse the friso.ini configuration file
	__stream = fopen(__ifile, "rb");
	if (NULL == __stream)
	{
		// printf("Error:open %s failed.\r\n", __ifile);
		ivAssert(0);
		return TC_WSErrID_OpenFile;
	}

	// initialize the entry with the value from the ifile.
	while (NULL != (__line__ = file_get_line(__chars__, __stream)))
	{
		// comments filter.
		if ('#' == __line__[0])
			continue;
		if ('\t' == __line__[0])
			continue;
		if ((' ' == __line__[0]) || ('\0' == __line__[0]))
			continue;

		__length__ = strlen(__line__);
		for (i = 0; i < __length__; i++)
		{
			if ((' ' == __line__[i]) || ('\t' == __line__[i]) || ('=' == __line__[i]))
			{
				break;
			}
			__key__[i] = __line__[i];
		}
		__key__[i] = '\0';

		// position the euqals char '='.
		if (' ' == __line__[i] || '\t' == __line__[i])
		{
			for (i++; i < __length__; i++)
			{
				if ('=' == __line__[i])
				{
					break;
				}
			}
		}

		// clear the left whitespace of the value.
		for (i++; i < __length__ && (' ' == __line__[i] || '\t' == __line__[i]); i++)
			;
		for (t = 0; i < __length__; i++, t++)
		{
			if (' ' == __line__[i] || '\t' == __line__[i])
			{
				break;
			}
			__line__[t] = __line__[i];
		}
		__line__[t] = '\0';

		// printf("key=%s, value=%s\n", __key__, __line__ );
		if (0 == strcmp(__key__, "friso.lex_dir"))
		{
			/*
			 * here copy the value of the lex_dir. cause we need the value of friso.max_len to finish all
			 *    the work when we call function friso_dic_load_from_ifile to initiliaze the friso dictionary.
			 */
			if (0 == __hit__)
			{
				__hit__ = t;
				for (t = 0; t < __hit__; t++)
				{
					__lexi__[t] = __line__[t];
				}
				__lexi__[t] = '\0';
			}
		}
		else if (0 == strcmp(__key__, "friso.max_len"))
		{
			config->max_len = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.r_name"))
		{
			config->r_name = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.mix_len"))
		{
			config->mix_len = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.lna_len"))
		{
			config->lna_len = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.clr_stw"))
		{
			config->clr_stw = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.keep_urec"))
		{
			config->keep_urec = (uint_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.spx_out"))
		{
			config->spx_out = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.nthreshold"))
		{
			config->nthreshold = atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.mode"))
		{
			// config->mode = ( friso_mode_t ) atoi( __line__ );
			friso_set_mode(config, (friso_mode_t)atoi(__line__));
		}
		else if (0 == strcmp(__key__, "friso.charset"))
		{
			friso->charset = (friso_charset_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.en_sseg"))
		{
			config->en_sseg = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.st_minl"))
		{
			config->st_minl = (ushort_t)atoi(__line__);
		}
		else if (0 == strcmp(__key__, "friso.kpuncs"))
		{
			// t is the length of the __line__.
			memcpy(config->kpuncs, __line__, t);
			// printf("friso_init_from_ifile#kpuncs: %s\n", config->kpuncs);
		}
	}

	/*
	 * intialize the friso dictionary here. use the setting from the ifile parse above we copied the value in the __lexi__
	 */
	if (0 != __hit__)
	{
		// add relative path search support
		//@added: 2014-05-24
		// convert the relative path to absolute path base on the path of friso.ini
		// improved at @date: 2014-10-26
		int ret = 0;
		// #ifdef FRISO_WINNT
		// 		if (__lexi__[1] != ':' && flen != 0) {
		// #else
		// 		if (__lexi__[0] != '/' && flen != 0) {
		// #endif
		if (0)
		{
			if ((flen + __hit__) > sizeof(lexpath) - 1)
			{
				fprintf(stderr, "[Error]: Buffer is not long enough to hold the final lexicon path");
				fprintf(stderr, " with a length of {%d} at function friso.c#friso_init_from_ifile", flen + __hit__);
				fclose(__stream);
				return -1;
			}

			memcpy(lexpath, __ifile, flen);
			memcpy(lexpath + flen, __lexi__, __hit__ - 1);
			// count the new length
			flen = flen + __hit__ - 1;
			if ('/' != lexpath[flen - 1])
				lexpath[flen] = '/';
			lexpath[flen + 1] = '\0';
		}
		else
		{
			memcpy(lexpath, __lexi__, __hit__);
			lexpath[__hit__] = '\0';
			if ('/' != lexpath[__hit__ - 1])
			{
				lexpath[__hit__] = '/';
				lexpath[__hit__ + 1] = '\0';
			}
		}

		friso_dic_new(friso->dic, NULL);

		// add charset check for max word length counting
		ret = friso_dic_load_from_ifile(friso, config, lexpath, config->max_len * ((FRISO_UTF8 == friso->charset) ? 3 : 2));
		if (0 != ret)
		{
			fclose(__stream);
			return ret;
		}
	}
	else
	{
		fprintf(stderr, "[Error]: failed get lexicon path, check lex_dir in friso.ini \n");
		return -1;
	}

	fclose(__stream);
	return 0;
}
/* }}} */

/* {{{ friso free functions.
 * here we have to free its dictionary.
 */
FRISO_API void friso_free(friso_entry_t friso)
{
	// free the dictionary
	// 	if (friso->dic != NULL)
	// 	{
	// 		friso_dic_free(friso->dic);
	// 	}
	FRISO_FREE(friso);
}
/* }}} */

/* {{{ set the current split mode
 *    view the friso.h#friso_mode_t
 */
FRISO_API void friso_set_mode(friso_config_t config, friso_mode_t mode)
{
	config->mode = mode;

	switch (config->mode)
	{
	case __FRISO_SIMPLE_MODE__:
		config->next_token = next_mmseg_token;
		config->next_cjk = next_simple_cjk;
		break;
	case __FRISO_DETECT_MODE__:
		config->next_token = next_detect_token;
		break;
	default:
		config->next_token = next_mmseg_token;
		config->next_cjk = next_complex_cjk;
		break;
	}
}
/* }}} */

/* {{{ create a new friso configuration entry and initialize it with default value.*/
FRISO_API friso_config_t friso_new_config(void)
{
	friso_config_t cfg = (friso_config_t)FRISO_MALLOC(sizeof(friso_config_entry));
	if (NULL == cfg)
	{
		ivAssert(0);
		return NULL;
	}

	// initialize the configuration entry.
	friso_init_config(cfg);

	return cfg;
}
/* }}} */

/* {{{ initialize the specified friso config entry with default value.*/
FRISO_API void friso_init_config(friso_config_t cfg)
{
	cfg->max_len = DEFAULT_SEGMENT_LENGTH;
	cfg->r_name = 1;
	cfg->mix_len = DEFAULT_MIX_LENGTH;
	cfg->lna_len = DEFAULT_LNA_LENGTH;
	cfg->clr_stw = 0;
	cfg->keep_urec = 1;
	cfg->spx_out = 0;
	cfg->en_sseg = 1; // default start the secondary segmentaion.
	cfg->st_minl = 2; // min length for secondary split sub token.
	cfg->nthreshold = DEFAULT_NTHRESHOLD;
	cfg->mode = (friso_mode_t)DEFAULT_SEGMENT_MODE;

	friso_set_mode(cfg, cfg->mode);

	// Zero fill the kpuncs buffer.
	memset(cfg->kpuncs, 0x00, sizeof(cfg->kpuncs));
	strcpy(cfg->kpuncs, "@%.#&+");
}
/* }}} */

/* {{{ create a new segment task entry.
 */
FRISO_API friso_task_t friso_new_task()
{
	friso_task_t task = (friso_task_t)FRISO_MALLOC(sizeof(friso_task));
	if (NULL == task)
	{
		ivAssert(0);
		return NULL;
	}

	// initliaze the segment.
	task->text = NULL;
	task->idx = 0;
	task->length = 0;
	task->bytes = 0;
	task->unicode = 0;
	task->ctrlMask = 0;
	task->pool = new_link_list(); // no use
	task->sbuf = new_string_buffer();
	friso_new_token(task->token);

	return task;
}
/* }}} */

/* {{{ free the specified task*/
FRISO_API void friso_free_task(friso_task_t task)
{
	// free the allocation of the poll link list.
	if (NULL != task->pool)
	{
		free_link_list(task->pool);
	}

	// release the allocation of the sbuff string_buffer_t.
	if (NULL != task->sbuf)
	{
		free_string_buffer(task->sbuf);
	}

	FRISO_FREE(task);
}
/* }}} */

/* {{{ create a new friso token */
FRISO_API int friso_new_token(friso_token_t token)
{
	// initialize
	token->type = (uchar_t)__LEX_OTHER_WORDS__;
	token->length = 0;
	token->offset = -1;
	memset(token->word, 0x00, WS_PER_TOKEN_MAX_LEN);

	return TC_WSErr_SUCCESS;
}
/* }}} */

/* {{{ set the text of the current segmentation. that means we could re-use the segment.
 *    also we have to reset the idx and the length of the segmentation.
 * and the most important one - clear the poll link list.
 */
FRISO_API void friso_set_text(friso_task_t task, fstring text)
{
	task->text = text;
	task->idx = 0; // reset the index
	task->length = strlen(text);
	task->pool = link_list_clear(task->pool); // clear the word poll
	string_buffer_clear(task->sbuf);		  // crear the string buffer.
}
/* }}} */

//--------------------------------------------------------------------
// friso core part 1: simple mode tokenize handler functions
/* {{{ read the next word from the current position.
 *
 * @return    int the bytes of the readed word.
 */
__STATIC_API__ uint_t readNextWord(
	friso_entry_t friso, // friso instance
	friso_task_t task,	 // token task
	uint_t *idx,		 // current index.
	fstring __word)		 // work buffer.
{
	if (FRISO_UTF8 == friso->charset)
	{
		//@reader: task->unicode = get_utf8_unicode(task->buffer) is moved insite function utf8_next_word from friso 1.6.0 .
		return utf8_next_word(task, idx, __word);
	}
	else if (FRISO_GBK == friso->charset)
	{
		return gbk_next_word(task, idx, __word);
	}

	return 0; // unknow charset.
}
/* }}} */

/* {{{ get the next cjk word from the current position, with simple mode.
 */
FRISO_API lex_entry_cdt_t next_simple_cjk(
	friso_entry_t friso,
	friso_config_t config,
	friso_task_t task)
{
	uint_t t, idx = task->idx, __length__;
	string_buffer_t sb = new_string_buffer_with_string(task->buffer);
	lex_entry_cdt_t e = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, sb->buffer);

	/*
	 * here bak the e->length in the task->token->type. we will use it to count the task->idx.
	 * for the sake of use less variable.
	 */
	__length__ = e->length;

	for (t = 1; t < config->max_len && 0 != (task->bytes = readNextWord(friso, task, &idx, task->buffer)); t++)
	{
		lex_entry_cdt_t val = NULL;
		if (friso_whitespace(friso->charset, task))
			break;
		if (!friso_cn_string(friso->charset, task))
			break;

		string_buffer_append(sb, task->buffer);

		// check the existence of the word by search the dictionary.
		val = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, sb->buffer);
		if (NULL != val)
		{
			e = val;
		}
	}

	// correct the offset of the segment.
	task->idx += (e->length - __length__);
	free_string_buffer(sb); // free the buffer

	return e;
}
/* }}} */

//-------------------------------------------------------------------
// friso core part 2: basic latin handler functions
/* {{{ basic latin segmentation*/
/*convert full-width char  to half-width*/
#define convert_full_to_half(friso, task, convert)                  \
	do                                                              \
	{                                                               \
		if (friso_fullwidth_en_char(friso->charset, task))          \
		{                                                           \
			if (FRISO_UTF8 == friso->charset)                       \
				task->unicode -= 65248;                             \
			else if (FRISO_GBK == friso->charset)                   \
			{                                                       \
				task->buffer[0] = ((uchar_t)task->buffer[1]) - 128; \
				task->buffer[1] = '\0';                             \
			}                                                       \
			convert = 1;                                            \
		}                                                           \
	} while (0)

/*convert uppercase char to lowercase char*/
#define convert_upper_to_lower(friso, task, convert)      \
	do                                                    \
	{                                                     \
		if (friso_uppercase_letter(friso->charset, task)) \
		{                                                 \
			if (FRISO_UTF8 == friso->charset)             \
				task->unicode += 32;                      \
			/* With the above logic(full to half),        \
			 * here we just need to check half-width*/    \
			else if (FRISO_GBK == friso->charset)         \
				task->buffer[0] = task->buffer[0] + 32;   \
			convert = 1;                                  \
		}                                                 \
	} while (0)

/* convert the unicode to utf-8 bytes. (FRISO_UTF8) */
#define convert_work_apply(friso, task, convert)          \
	do                                                    \
	{                                                     \
		if (1 == convert && FRISO_UTF8 == friso->charset) \
		{                                                 \
			memset(task->buffer, 0x00, 7);                \
			unicode_to_utf8(task->unicode, task->buffer); \
			convert = 0;                                  \
		}                                                 \
	} while (0)

void xxxxxx()
{
	return;
}

#define EN_UPPER_TO_LOWER (0) //英文字母大写转为小写 qungao
// get the next latin word from the current position.
__STATIC_API__ lex_entry_cdt_t next_basic_latin(friso_entry_t friso, friso_config_t config, friso_task_t task)
{
#if EN_UPPER_TO_LOWER
	int __convert = 0;
#endif
	int t = 0, blen = 0;
	int chkecm = 0, chkunits = 1, wspace = 0;

	/* cause friso will convert full-width numeric and letters
	 *     (Not punctuations) to half-width ones. so, here we need
	 * wlen to record the real length of the lex_entry_cdt_t.
	 * */
	uint_t wlen = task->bytes;
	uint_t idx = task->idx;
	lex_entry_cdt_t e = NULL;
	fstring word = NULL;
	string_buffer sb[1] = {0};
	string_buffer tmp[1] = {0};

	// condition controller to start the secondary segmente.
	int ssseg = 0;
	int fdunits = 0;

	// secondray segmente.
	int tcount = 1; // number fo different type of char.
	friso_enchar_t _ctype, _TYPE;
	task_ssseg_close(task);

	/* full-half width and upper-lower case exchange. */
#if EN_UPPER_TO_LOWER
	convert_full_to_half(friso, task, __convert);
	convert_upper_to_lower(friso, task, __convert);
	convert_work_apply(friso, task, __convert);
#endif

	// creat a new fstring buffer and append the task->buffer insite.
	set_string_buffer_with_string(sb, task->buffer);
	_TYPE = friso_enchar_type(friso->charset, task);

	// segmentation.
	task->bytes = readNextWord(friso, task, &idx, task->buffer);
	while (0 != task->bytes)
	{
		// convert full-width to half-width.
#if EN_UPPER_TO_LOWER
		convert_full_to_half(friso, task, __convert);
#endif
		_ctype = friso_enchar_type(friso->charset, task);

		if (_ctype != _TYPE)
		{
			break; // qungao;
		}

		if (FRISO_EN_WHITESPACE == _ctype)
		{
			wspace = 1;
			break;
		}

		if (FRISO_EN_PUNCTUATION == _ctype)
		{
			// clear the full-width punctuations.
			if (task->bytes > 1)
				break;
			if (!friso_en_kpunc(config, task->buffer[0]))
				break;
		}

		/* check if is an FRISO_EN_NUMERIC, or FRISO_EN_LETTER.
		 *     here just need to make sure it is not FRISO_EN_UNKNOW.
		 * */
		if (FRISO_EN_UNKNOW == _ctype)
		{
			if (friso_cn_string(friso->charset, task))
				chkecm = 1;
			break;
		}

#if EN_UPPER_TO_LOWER
		// upper-lower case convert
		convert_upper_to_lower(friso, task, __convert);
		convert_work_apply(friso, task, __convert);
#endif

		// sound a little crazy, i did't limit the length of this
		//@Added: 2015-01-16 night
		if ((wlen + task->bytes) >= WS_PER_TOKEN_MAX_LEN)
		{
			break;
		}

		string_buffer_append(sb, task->buffer);
		wlen += task->bytes;
		task->idx += task->bytes;

		/* Char type counter.
		 *     make the condition to start the secondary segmentation.
		 *
		 * @TODO: 2013-12-22
		 * */
		if (_ctype != _TYPE)
		{
			tcount++;
			_TYPE = _ctype;
		}

		task->bytes = readNextWord(friso, task, &idx, task->buffer);
	} // while (0 != task->bytes)

	/*
	 * 1. clear the useless english punctuation from the end of the buffer.
	 * 2. check the english and punctuation mixed word.
	 *
	 * set _ctype to as the status for the existence of punctuation at the end of the sb cause we need to plus the tcount
	 *     to avoid the secondary check for work like 'c+', 'chenxin.'.
	 */
	_ctype = 0;
	for (; sb->length > 0 && '%' != sb->buffer[sb->length - 1] && is_en_punctuation(friso->charset, sb->buffer[sb->length - 1]);)
	{
		// lex_entry_cdt_t val = NULL;

		// mark the end of the buffer.
		sb->buffer[--sb->length] = '\0';
		wlen--;
		task->idx--;

		/*check and plus the tcount*/
		if (0 == _ctype)
		{
			tcount--;
			_ctype = 1;
		}
	}

	// check the condition to start the secondary segmentation.
	ssseg = (tcount > 1) && (1 == chkunits);

	// check the tokenize loop is break by whitespace.no need for all the following work if it is.
	//@added 2013-11-19
	if (1 == wspace || task->idx == task->length)
	{
		fstring word = NULL;
		blen = sb->length;
		word = (fstring)FRISO_MALLOC(sb->length + 1);
		memcpy(word, sb->buffer, sb->length);
		word[sb->length] = 0;
		e = new_lex_entry(word, NULL, 0, blen, __LEX_OTHER_WORDS__);
		e->rlen = (uchar_t)wlen;
		// set the secondary mask.
		if (ssseg)
			task_ssseg_open(task);
		return e;
	}

	if (1 != chkecm)
	{
		/*
		 * check the single words unit. not only the chinese word but also other kinds of word.
		 * so we can recongnize the complex unit like '℉,℃'' eg..
		 * @date 2013-10-14
		 */
		if (chkunits && (friso_numeric_string(friso->charset, sb->buffer) || friso_decimal_string(friso->charset, sb->buffer)))
		{
			idx = task->idx;
			if (0 != (task->bytes = readNextWord(friso, task, &idx, task->buffer)))
			{
			}
		}

		// set the START_SS_MASK
		if (1 != fdunits && ssseg)
		{
			task_ssseg_open(task);
		}

		// creat the lexicon entry and return it.
		blen = sb->length;
		word = (fstring)FRISO_MALLOC(sb->length + 1);
		memcpy(word, sb->buffer, sb->length);
		word[sb->length] = 0;
		e = new_lex_entry(word, NULL, 0, blen, __LEX_OTHER_WORDS__);
		e->rlen = (uchar_t)wlen;

		return e;
	}

	// Try to find a english chinese mixed word.
	set_string_buffer_with_string(tmp, sb->buffer);
	idx = task->idx;
	for (t = 0; (t < config->mix_len) && (0 != (task->bytes = readNextWord(friso, task, &idx, task->buffer))); t++)
	{
		// lex_entry_cdt_t val;
		// if ( ! friso_cn_string( friso->charset, task ) ) {
		//     task->idx -= task->bytes;
		//     break;
		// }
		// replace with the whitespace check.
		// more complex mixed words could be find here. (no only english and chinese mix word)
		//@date 2013-10-14
		if (friso_whitespace(friso->charset, task))
		{
			break;
		}

		string_buffer_append(tmp, task->buffer);
	}

	/* e is not NULL does't mean it must be EC mixed word. it could be an english and punctuation mixed word, like 'c++'
	 * But we don't need to check and set the START_SS_MASK mask here.
	 * */
	if (NULL != e)
	{
		task->idx += (e->length - sb->length);
		return e;
	}

	// no match for mix word, try to find a single unit.
	if (chkunits && (friso_numeric_string(friso->charset, sb->buffer) || friso_decimal_string(friso->charset, sb->buffer)))
	{
		idx = task->idx;
		if (0 != (task->bytes = readNextWord(friso, task, &idx, task->buffer)))
		{
		}
	}

	// set the START_SS_MASK.
	if (1 != fdunits && ssseg)
	{
		task_ssseg_open(task);
	}

	// create the lexicon entry and return it.
	blen = sb->length;
	word = (fstring)FRISO_MALLOC(sb->length + 1);
	memcpy(word, sb->buffer, sb->length);
	word[sb->length] = 0;
	e = new_lex_entry(word, NULL, 0, blen, __LEX_OTHER_WORDS__);
	e->rlen = (uchar_t)wlen;

	return e;
}
/* }}} */

//-------------------------------------------------------------------
// friso core part 3: mmseg tokenize implements functions
// mmseg algorithm implemented functions - start
/* {{{ get the next match from the current position, throught the dictionary this will return all the matchs.
 *
 * @return friso_array_t that contains all the matchs.
 * 找到task->buffer里的字在词典里的所有词+词组，放到match->item中返回
 */
__STATIC_API__ friso_array_t get_next_match(friso_entry_t friso, friso_config_t config, friso_task_t task, uint_t idx)
{
	register uint_t t;
	lex_entry_cdt_t val;

	string_buffer sb[1] = {0};
	set_string_buffer_with_string(sb, task->buffer);

	// create a match dynamic array.
	friso_array_t match = new_array_list_with_opacity(config->max_len);
	
	val = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->buffer);
	lex_entry_cdt_t val_cpy = FRISO_MALLOC(sizeof(lex_entry_cdt));
	memcpy(val_cpy, val, sizeof(lex_entry_cdt));
	array_list_add(match, (void *)val_cpy);

	// config->max_len 是friso.ini里设置的最多匹配的字符个数（汉字）
	task->bytes = readNextWord(friso, task, &idx, task->buffer);
	for (t = 1; t < config->max_len && (0 != task->bytes); t++)
	{
		if (friso_whitespace(friso->charset, task))
			break;
		if (!friso_cn_string(friso->charset, task))
			break;

		// append the task->buffer to the buffer.
		string_buffer_append(sb, task->buffer);

		// check the CJK dictionary.
		val = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, sb->buffer);
		if (NULL != val)
		{
			/*
			 * add the lex_entry_cdt_t insite.
			 * here is a key point:
			 *        we use friso_dic_get function to get the address of the lex_entry_cdt that store in the dictionary,
			 *        not create a new lex_entry_cdt.
			 * so :
			 *        1.we will not bother to the allocations of the newly created lex_entry_cdt.
			 *        2.more efficient of course.
			 */
#if SUPPORT_PERFECT_MATCH
			array_list_add(match, (void *)val);
#else
			if (strlen(sb->buffer) < task->length)
			{
				lex_entry_cdt_t val_cpy = FRISO_MALLOC(sizeof(lex_entry_cdt));
				memcpy(val_cpy, val, sizeof(lex_entry_cdt));
				array_list_add(match, (void *)val_cpy);
			}
#endif
		}
		task->bytes = readNextWord(friso, task, &idx, task->buffer);
	}

	return match;
}
/* }}} */

/* {{{ chunk for mmseg defines and functions to handle them.*/
typedef struct
{
	friso_array_t words;
	uint_t length;
	float average_word_length;
	float word_length_variance;
	float single_word_dmf;
} friso_chunk_entry;
typedef friso_chunk_entry *friso_chunk_t;
/* }}} */

/* {{{ create a new chunks*/
__STATIC_API__ friso_chunk_t new_chunk(
	friso_array_t words, uint_t length)
{
	friso_chunk_t chunk = (friso_chunk_t)FRISO_MALLOC(sizeof(friso_chunk_entry));
	if (NULL == chunk)
	{
		ivAssert(0);
		return NULL;
	}

	chunk->words = words;
	chunk->length = length;
	chunk->average_word_length = -1;
	chunk->word_length_variance = -1;
	chunk->single_word_dmf = -1;

	return chunk;
}
/* }}} */

/* {{{ free the specified chunk */
__STATIC_API__ void free_chunk(friso_chunk_t chunk)
{
	FRISO_FREE(chunk);
}
/* }}} */

/* {{{ a static function to count the average word length of the given chunk.*/
__STATIC_API__ float count_chunk_avl(friso_chunk_t chunk)
{
	chunk->average_word_length = ((float)chunk->length) / chunk->words->length;
	return chunk->average_word_length;
}
/* }}} */

/* {{{ a static function to count the word length variance of the given chunk. */
__STATIC_API__ float count_chunk_var(friso_chunk_t chunk)
{
	float var = 0, tmp = 0; // snapshot
	register uint_t t;
	lex_entry_cdt_t e;

	for (t = 0; t < chunk->words->length; t++)
	{
		e = (lex_entry_cdt_t)chunk->words->items[t];
		tmp = e->length - chunk->average_word_length;
		var += tmp * tmp;
	}

	chunk->word_length_variance = var / chunk->words->length;

	return chunk->word_length_variance;
}
/* }}} */

/* {{{ a static function to count the single word morpheme degree of freedom of the given chunk.*/
__STATIC_API__ float count_chunk_mdf(friso_chunk_t chunk)
{
	float __mdf__ = 0;
	register uint_t t;
	lex_entry_cdt_t e;

	for (t = 0; t < chunk->words->length; t++)
	{
		e = (lex_entry_cdt_t)chunk->words->items[t];
		// single CJK(UTF-8)/chinese(GBK) word.
		// better add a charset check here, but this will works find.
		// all CJK words will take 3 bytes with UTF-8 encoding.
		// all chinese words take 2 bytes with GBK encoding.
		if (3 == e->length || 2 == e->length)
		{
			__mdf__ += (float)log((float)e->fre);
		}
	}
	chunk->single_word_dmf = __mdf__;

	return chunk->single_word_dmf;
}
/* }}} */

/* {{{ chunk printer - use for for debug*/
#define ___CHUNK_PRINTER___(_chunks_)                                   \
	for (t = 0; t < _chunks_->length; t++)                              \
	{                                                                   \
		__tmp__ = ((friso_chunk_t)_chunks_->items[t])->words;           \
		for (j = 0; j < __tmp__->length; j++)                           \
		{                                                               \
			printf("%s/ ", ((lex_entry_cdt_t)__tmp__->items[j])->word); \
		}                                                               \
		putchar('\n');                                                  \
	}                                                                   \
	putchar('\n');                                                      \
/* }}} */

/* {{{ mmseg algorithm core invoke here,
 * we use four rules to filter all the chunks to get the best chunk.and this is the core of the mmseg alogrithm.
 * 1. maximum match word length.
 * 2. larget average word length.
 * 3. smallest word length variance.
 * 4. largest single word morpheme degrees of freedom.
 * 过滤规则：例句“南京市长江大桥”
 * 1、最大匹配：
 *    例：“南/京/市” = 6
 *    例：“南京市/长江大桥” = 14
 * 2、最大平均词汇长度
 *    例：“南/京/市” = 14/3
 *    例：“南京市/长江大桥” = 14/2
 * 3、最小词长方差。这个规则和我们平时用语有关，一般我们在组织语句时，为了朗朗上口，往往使用的词汇组成长度比較一致。
 *    比方，“幸福_快乐”，“人之初_性本善”，“君子一言，驷马难追”。
 *    例：“南京市/长江大桥” = ((6-7)^2+(8-7)^2)/2 (其中7是均值=(6+8)/2)
 * 4、最大单字自由度。所谓单字自由度，能够简单的理解为这个字作为单独出现的语境次数。比方“的”常常作为定语修饰字，常常出如今各种语境。可是“的”偶尔也会和其它字词组成成语。比方“目的”等，这样的组合会影响改字的自由度。
 *    计算公式=sum(log(单字词频）
 */
__STATIC_API__ friso_chunk_t mmseg_core_invoke(friso_array_t chunks)
{
	register uint_t t /*, j*/;
	float max;
	friso_chunk_t e;
	friso_array_t __res__, __tmp__;
	__res__ = new_array_list_with_opacity(chunks->length);

#if 0 // test 直接给最长串,不考虑消歧义规则  qungao
	for (t = 0; t < chunks->length-1; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		free_array_list(e->words);
		free_chunk(e);		
	}
	e = (friso_chunk_t)chunks->items[chunks->length - 1];
	return e;
#endif

	// 1.get the maximum matched chunks.
	// count the maximum length
	max = (float)((friso_chunk_t)chunks->items[0])->length;
	for (t = 1; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (e->length > max)
			max = (float)e->length;
	}
	// get the chunk items that owns the maximum length.
	for (t = 0; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (e->length >= max)
		{
			array_list_add(__res__, e);
		}
		else
		{
			free_array_list(e->words, 0);
			free_chunk(e);
		}
	}
	// check the left chunks
	if (1 == __res__->length)
	{
		e = (friso_chunk_t)__res__->items[0];
		free_array_list(__res__, 0);
		free_array_list(chunks, 0);
		return e;
	}
	else
	{
		__tmp__ = array_list_clear(chunks);
		chunks = __res__;
		__res__ = __tmp__;
	}

	// 2.get the largest average word length chunks.
	// count the maximum average word length.
	max = count_chunk_avl((friso_chunk_t)chunks->items[0]);
	for (t = 1; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (count_chunk_avl(e) > max)
		{
			max = e->average_word_length;
		}
	}
	// get the chunks items that own the largest average word length.
	for (t = 0; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (e->average_word_length >= max)
		{
			array_list_add(__res__, e);
		}
		else
		{
			free_array_list(e->words, 0);
			free_chunk(e);
		}
	}
	// check the left chunks
	if (1 == __res__->length)
	{
		e = (friso_chunk_t)__res__->items[0];
		free_array_list(__res__, 0);
		free_array_list(chunks, 0);
		return e;
	}
	else
	{
		__tmp__ = array_list_clear(chunks);
		chunks = __res__;
		__res__ = __tmp__;
	}

	// 3.get the smallest word length variance chunks
	// count the smallest word length variance
	max = count_chunk_var((friso_chunk_t)chunks->items[0]);
	for (t = 1; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (count_chunk_var(e) < max)
		{
			max = e->word_length_variance;
		}
	}
	// get the chunks that own the smallest word length variance.
	for (t = 0; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (e->word_length_variance <= max)
		{
			array_list_add(__res__, e);
		}
		else
		{
			free_array_list(e->words, 0);
			free_chunk(e);
		}
	}
	// check the left chunks
	if (1 == __res__->length)
	{
		e = (friso_chunk_t)__res__->items[0];
		free_array_list(chunks, 0);
		free_array_list(__res__, 0);
		return e;
	}
	else
	{
		__tmp__ = array_list_clear(chunks);
		chunks = __res__;
		__res__ = __tmp__;
	}

	// 4.get the largest single word morpheme degrees of freedom.
	// count the maximum single word morpheme degreees of freedom
	max = count_chunk_mdf((friso_chunk_t)chunks->items[0]);
	for (t = 1; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (count_chunk_mdf(e) > max)
		{
			max = e->single_word_dmf;
		}
	}
	// get the chunks that own the largest single word word morpheme degrees of freedom.
	for (t = 0; t < chunks->length; t++)
	{
		e = (friso_chunk_t)chunks->items[t];
		if (e->single_word_dmf >= max)
		{
			array_list_add(__res__, e);
		}
		else
		{
			free_array_list(e->words, 0);
			free_chunk(e);
		}
	}

	/*
	 * there is still more than one chunks? well, this rarely happen but still happens.
	 * here we simple return the first chunk as the final result, and we need to free the all the chunks that __res__ points to except the 1th one.
	 * you have to do two things to totaly free a chunk:
	 * 1. call free_array_list to free the allocations of a chunk's words.
	 * 2. call free_chunk to the free the allocations of a chunk.
	 */
	//(lex_entry_cdt_t)((((friso_chunk_t)__res__->items[0])->words)->items[0])
	//((lex_entry_cdt_t)((((friso_chunk_t)__res__->items[0])->words)->items[0]))->word,s8
	for (t = 1; t < __res__->length; t++)
	{
		e = (friso_chunk_t)__res__->items[t];
		free_array_list(e->words, 0);
		free_chunk(e);
	}

	e = (friso_chunk_t)__res__->items[0];
	free_array_list(chunks, 0);
	free_array_list(__res__, 0);

	return e;
}
/* }}} */

/* {{{ get the next cjk word from the current position with complex mode.
 *    this is the core of the mmseg chinese word segemetation algorithm.
 *    we use four rules to filter the matched chunks and get the best one as the final result.
 *
 * @see mmseg_core_invoke( chunks );
 */
#define LOG_PRINT (0) // print切分后文本，以便查看 qungao
FRISO_API lex_entry_cdt_t next_complex_cjk(
	friso_entry_t friso,
	friso_config_t config,
	friso_task_t task)
{
	register uint_t x, y, z;
	/*bakup the task->bytes here*/
	uint_t __idx__ = task->bytes;
	lex_entry_cdt_t fe, se, te;
	friso_chunk_t e;
	friso_array_t words, chunks;
	friso_array_t smatch, tmatch, fmatch;

	fmatch = get_next_match(friso, config, task, task->idx); //把词典里所有能匹配到的都放到fmatch->items里

	if (fmatch->length == 0) {
		ivAssert(0);
		return NULL;
	}
	/*
	 * here: if the length of the fmatch is 1, mean we don't have to
	 * continue the following work. ( no matter what we get the same result. )
	 */
#if GREEDY_TOKEN
	if (fmatch->length >= 1)
#else
	if (1 == fmatch->length)
#endif
	{
		fe = ((lex_entry_cdt_t)fmatch->items[fmatch->length - 1]);
		if (fmatch->length > 1)
		{
			task->idx += fe->length - __idx__;
		}
		memcpy(friso->lex, fe, sizeof(friso->lex));
		free_array_list(fmatch, 1);
		return friso->lex;
	} // if (1 == fmatch->length)

	ivAssert(0);   //下面的代码未维护，理论上跑不到

	chunks = new_array_list();
	task->idx -= __idx__;

	for (x = 0; x < fmatch->length; x++)
	{
		/*get the word and try the second layer match*/
		fe = (lex_entry_cdt_t)array_list_get(fmatch, x);
		__idx__ = task->idx + fe->length;
		readNextWord(friso, task, &__idx__, task->buffer);
#if LOG_PRINT
		printf("\r\n");
#endif
		if (0 != task->bytes && friso_cn_string(friso->charset, task) && NULL != friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->buffer))
		{
			// get the next matchs
			smatch = get_next_match(friso, config, task, __idx__);
			for (y = 0; y < smatch->length; y++)
			{
				/*get the word and try the third layer match*/
				se = (lex_entry_cdt_t)array_list_get(smatch, y);
				__idx__ = task->idx + fe->length + se->length;
				readNextWord(friso, task, &__idx__, task->buffer);

				if (0 != task->bytes && friso_cn_string(friso->charset, task) && NULL != friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->buffer))
				{
					// get the matchs.
					tmatch = get_next_match(friso, config, task, __idx__);
					for (z = 0; z < tmatch->length; z++)
					{
						te = (lex_entry_cdt_t)array_list_get(tmatch, z);
						words = new_array_list_with_opacity(3);
						array_list_add(words, fe);
						array_list_add(words, se);
						array_list_add(words, te);
						array_list_add(chunks, new_chunk(words, fe->length + se->length + te->length));
#if LOG_PRINT
						printf("%s/%s/%s\r\n", fe->word, se->word, te->word);
#endif
					}
					// free the third matched array list
					free_array_list(tmatch, 1);
				}
				else
				{
					words = new_array_list_with_opacity(2);
					array_list_add(words, fe);
					array_list_add(words, se);
					// add the chunk
					array_list_add(chunks, new_chunk(words, fe->length + se->length));
#if LOG_PRINT
					printf("%s/%s\r\n", fe->word, se->word);
#endif
				}
			}
			// free the second match array list
			free_array_list(smatch, 1);
		}
		else
		{
			words = new_array_list_with_opacity(1);
			array_list_add(words, fe);
			array_list_add(chunks, new_chunk(words, fe->length));
#if LOG_PRINT
			printf("%s\r\n", fe->word);
#endif
		}
	}
	// free the first match array list
	free_array_list(fmatch, 1);

	/*
	 * filter the chunks with the four rules of the mmseg algorithm and get best chunk as the final result.
	 *
	 * @see mmseg_core_invoke( chunks );
	 * @date 2012-12-13
	 */
	if (chunks->length > 1)
	{
		e = mmseg_core_invoke(chunks);
	}
	else
	{
		e = (friso_chunk_t)chunks->items[0];
	}

	fe = (lex_entry_cdt_t)e->words->items[0];
	task->idx += fe->length;   // reset the idx of the task.
	free_array_list(e->words, 0); // free the chunks words allocation
	free_chunk(e);

	return fe;
}
/* }}} */
//----------------end of mmseg core

//-------------------------------------------------------------------------------------
// mmseg core logic controller, output style controller and macro defines
/* {{{ A macro function to check and free
 *     the lex_entry_cdt_t with type of __LEX_OTHER_WORDS__.
 */
#define check_free_otlex_entry(lex)           \
	do                                        \
	{                                         \
		if (__LEX_OTHER_WORDS__ == lex->type) \
		{                                     \
			FRISO_FREE(lex->word);            \
			free_lex_entry(lex);              \
		}                                     \
	} while (0)
/* }}} */

/* {{{ sphinx style output synonyms words append.
 *
 * @param    task
 * @param    lex
 * */
__STATIC_API__ void token_sphinx_output(
	friso_task_t task,
	lex_entry_cdt_t lex)
{
	uint_t /*i, j,*/ len;
	// fstring _word;
	len = lex->length;

	ivAssert(0);

	// set the new end of the buffer.
	task->token->word[len] = '\0';
}
/* }}} */

/* {{{ normal style output synonyms words append.
 *
 * @param    task
 * @param    lex
 * @param    front    1 for add the synoyum words from the head and
 *                     0 for append from the tail.
 * */
__STATIC_API__ void token_normal_output(
	friso_task_t task,
	lex_entry_cdt_t lex,
	int front)
{
	// uint_t i;
	// fstring _word;
	// lex_entry_cdt_t e;

	ivAssert(0);
}
/* }}} */

/* {{{ do the secondary segmentation of the complex english token.
 *
 * @param    friso
 * @param    config
 * @param    task
 * @param    lex
 * @param    retfw    -Wether to return the first word.
 * @return    lex_entry_cdt_t(NULL or the first sub token of the lex)
 */
__STATIC_API__ lex_entry_cdt_t en_second_seg(
	friso_entry_t friso,
	friso_config_t config,
	friso_task_t task,
	lex_entry_cdt_t lex, int retfw)
{
	// printf("sseg: %d\n", (task->ctrlMask & START_SS_MASK));

	int j, p = 0, start = 0;
	fstring str = lex->word;

	lex_entry_cdt_t fword = NULL, sword = NULL;

	int _ctype, _TYPE = get_enchar_type(str[0]);
	string_buffer_clear(task->sbuf);
	string_buffer_append_char(task->sbuf, str[0]);

	for (j = 1; j < lex->length; j++)
	{
		// get the type of the char
		_ctype = get_enchar_type(str[j]);
		if (FRISO_EN_WHITESPACE == _ctype)
		{
			_TYPE = FRISO_EN_WHITESPACE;
			p++;
			continue;
		}

		if (_ctype == _TYPE)
		{
			string_buffer_append_char(task->sbuf, str[j]);
		}
		else
		{
			start = j - task->sbuf->length - p;

			/* If the number of chars of current type is larger than config->st_minl then we will
			 *     create a new lex_entry_cdt_t and append it to the task->wordPool.
			 * */
			if (task->sbuf->length >= config->st_minl)
			{
				/* the allocation of lex_entry_cdt_t and its word should be released and the type of the lex_entry_cdt_t must be __LEX_OTHER_WORDS__.
				 * */
				sword = new_lex_entry(strdup(task->sbuf->buffer), NULL, 0, task->sbuf->length, __LEX_OTHER_WORDS__);
				sword->offset = lex->offset + start;
				if (retfw && NULL == fword)
				{
					fword = sword;
				}
				else
				{
					link_list_add(task->pool, sword);
				}
			}

			string_buffer_clear(task->sbuf);
			string_buffer_append_char(task->sbuf, str[j]);
			p = 0;
			_TYPE = _ctype;
		}
	}

	// continue to check the last item.
	if (task->sbuf->length >= config->st_minl)
	{
		start = j - task->sbuf->length;
		sword = new_lex_entry(strdup(task->sbuf->buffer), NULL, 0, task->sbuf->length, __LEX_OTHER_WORDS__);
		sword->offset = j - task->sbuf->length;
		if (retfw && NULL == fword)
		{
			fword = sword;
		}
		else
		{
			link_list_add(task->pool, sword);
		}
	}

	return fword;
}
/*}}}*/

/* {{{ get the next segmentation.
 *     and also this is the friso enterface function.
 *
 * @param     friso.
 * @param    config.
 * @return    task.
 */
FRISO_API friso_token_t next_mmseg_token(friso_entry_t friso, friso_config_t config, friso_task_t task)
{
	uint_t len = 0;
	string_buffer sb[1];
	lex_entry_cdt_t lex = NULL, tmp = NULL, sword = NULL;

	memset(sb, 0, sizeof(sb));
	/* {{{ task word pool check */
	if (!link_list_empty(task->pool))
	{
		/*
		 * load word from the word poll if it is not empty.
		 *  this will make the next word more convenient and efficient.often synonyms, newly created word will be stored in the poll.
		 */
		lex = (lex_entry_cdt_t)link_list_remove_first(task->pool);
		memcpy(task->token->word, lex->word, lex->length);
		task->token->type = lex->type;
		task->token->length = lex->length;
		task->token->offset = (short)(lex->offset);
		task->token->word[lex->length] = '\0';

		/* check and handle the english synonyms words append mask.Also we have to close the mask after finish the operation.
		 *
		 * 1. we've check the config->add_syn before open the  _LEX_APPENSYN_MASK mask.
		 * 2. we should add the synonyms words of the curren lex_entry_cdt_t from the head.
		 *
		 * @since: 1.6.0
		 * */
		if (lex_appensyn_check(lex))
		{
			ivAssert(0);
			lex_appensyn_close(lex);
			// append_en_syn(lex, tmp, 1);
		}

		/*
		 * __LEX_NCSYN_WORDS__:
		 *  these lex_entry_cdt_t was created to store the the synonyums words.
		 *     and its word pointed to the lex_entry_cdt_t's synonyms word of friso->dic, so :free the lex_entry_cdt_t but not its word here.
		 *
		 * __LEX_OTHER_WORDS__:
		 *  newly created lexicon entry, like the chinese and english mixed word.
		 *     during the invoke of function next_basic_latin.
		 *
		 * other type: they must exist in the dictionary, so just pass them.
		 */
		switch (lex->type)
		{
		case __LEX_OTHER_WORDS__:
			FRISO_FREE(lex->word);
			if (NULL != lex->word_new)
			{
				FRISO_FREE(lex->word_new);
			}
			free_lex_entry(lex);
			break;
		}

		return task->token;
	}
	/* }}} */

	while (task->idx < task->length)
	{
		// read the next word from the current position.
		task->bytes = readNextWord(friso, task, &task->idx, task->buffer);
		if (0 == task->bytes)
			break;

		// clear up the whitespace.
		if (friso_whitespace(friso->charset, task)) //空格也作为一个分词输出
		{
			memcpy(task->token->word, task->buffer, task->bytes);
			task->token->type = __LEX_WHITESPACE__; //空格
			task->token->length = (uchar_t)task->bytes;			
			task->token->offset = (short)(task->idx - task->bytes);
			task->token->word[task->bytes] = '\0';
			return task->token;

			// continue;
		}

		/* {{{ CJK words recongnize block. */
		if (friso_cn_string(friso->charset, task))
		{
			/* check the dictionary.and return the unrecognized CJK char as a single word. */
			if (NULL == friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->buffer))
			{
				memcpy(task->token->word, task->buffer, task->bytes);
				task->token->word[(int)task->bytes] = '\0';
				task->token->type = __LEX_PUNCTUATION__;
				task->token->length = (uchar_t)task->bytes;				
				task->token->offset = (short)(task->idx - task->bytes);
				return task->token;
			}

			// specifield mode split.
			// if ( config->mode == __FRISO_COMPLEX_MODE__ )
			//     lex = next_complex_cjk( friso, config, task );
			// else lex = next_simple_cjk( friso, config, task );
			lex = config->next_cjk(friso, config, task);

			if (NULL == lex)
				continue; // find a stopwrod.
			lex->offset = task->idx - lex->rlen;

			/*
			 * try to find a chinese and english mixed words, like '卡拉ok'
			 *     keep in mind that is not english and chinese mixed words like 'x射线'.
			 *
			 * @reader:
			 * 1. only if the char after the current word is an english char.
			 * 2. if the first point meet, friso will call next_basic_latin() to get the next basic latin. (yeah, you have to handle it).
			 * 3. if match a CE word, set lex to the newly match CE word.
			 * 4. if no match a CE word, we will have to append the basic latin to the pool, and it should after the append of synonyms words.
			 * 5. do not use the task->buffer and task->unicode as the check condition for the CE word identify.
			 * 6. Add friso_numeric_letter check so can get work like '高3'
			 *
			 * @date 2013-09-02
			 */
			if ((task->idx < task->length) && ((int)task->text[task->idx]) > 0 && (friso_en_letter(friso->charset, task) || friso_numeric_letter(friso->charset, task)))
			{
				// create a string buffer
				set_string_buffer_with_string(sb, lex->word);

				// find the next basic latin.
				task->buffer[0] = task->text[task->idx++];
				task->buffer[1] = '\0';
				tmp = next_basic_latin(friso, config, task);
				tmp->offset = task->idx - tmp->length;
				string_buffer_append(sb, tmp->word);
			}

			/*
			 * copy the lex_entry to the result token
			 * @reader: (boodly lession, added 2013-08-31): don't bother to handle the task->token->offset problem. is has been sovled perfectly above.
			 */
			len = (int)lex->length;
			if (lex->length >= sizeof(task->token->word) - 1)
			{
				return NULL;
			}
			memcpy(task->token->word, lex->word, lex->length);
			task->token->word[len] = '\0';
			task->token->type = lex->type;
			task->token->length = lex->length;			
			task->token->offset = (short)(lex->offset);

			/* {{{ here: handle the newly found basic latin created when we try to find a CE word.
			 * @reader: * when tmp is not NULL and sb will not be NULL too except a CE word is found.
			 * @TODO: finished append the synonyms words on 2013-12-19.
			 */
			if (NULL != tmp && sb->length > 0)
			{
				// check the secondary split.
				if (1 == config->en_sseg && task_ssseg_check(task))
				{
					en_second_seg(friso, config, task, tmp, 0);
				}

				link_list_add(task->pool, tmp);
			}
			/* }}} */

			return task->token;
		}
		/* }}} */

		/* {{{ basic english/latin recongnize block. */
		else if (friso_halfwidth_en_char(friso->charset, task) || friso_fullwidth_en_char(friso->charset, task))
		{
			//是ascii可显示字符[32,126] 或者 fullwidth的英文大小写字母和数字
			/*
			 * handle the english punctuation.
			 *
			 * @todo:
			 * 1. commen all the code of the following if
			 *     and uncomment the continue to clear up the punctuation directly.
			 *
			 * @reader:
			 * 2. keep in mind that ALL the english punctuation will be handled here,
			 *  (when a english punctuation is found during the other process, we will
			 *      reset the task->idx back to it and then back here)
			 *     except the keep punctuation(define in file friso_string.c)
			 *     that will make up a word with the english chars around it.
			 */
			if (friso_en_punctuation(friso->charset, task)) // ascii标点符号
			{
				// count the punctuation in.
				task->token->word[0] = task->buffer[0];
				task->token->type = __LEX_PUNCTUATION__;
				task->token->length = (uchar_t)task->bytes;
				task->token->offset = (short)(task->idx - task->bytes);
				task->token->word[1] = '\0';
				return task->token;

				// continue
			}

			// get the next basic latin word.
			lex = next_basic_latin(friso, config, task);

#if 0
			while (1)
			{
				lex_entry_cdt_t lex_next = next_basic_latin(friso, config, task);
				break;
			}
#endif

			lex->offset = task->idx - lex->rlen;

			/* @added: 2013-12-22
			 * check and do the secondary segmentation work.
			 * this will split 'qq2013' to 'qq, 2013'
			 * */
			sword = NULL;
			if (1 == config->en_sseg && task_ssseg_check(task))
			{
				sword = en_second_seg(friso, config, task, lex, 1);
			}

			if (NULL != sword)
			{
				link_list_add(task->pool, lex);

				/* If the sub token is not NULL:
				 * add the lex to the task->pool if it is not NULL
				 * and return the sub token istead of lex so
				 *     the sub tokens will be output ahead of lex.
				 * */
				lex = sword;
			}

			// if the token is longer than __HITS_WORD_LENGTH__, drop it
			// copy the word to the task token buffer.
			// if ( lex->length >= __HITS_WORD_LENGTH__ ) continue;
			memcpy(task->token->word, lex->word, lex->length);
			task->token->word[lex->length] = '\0';
			task->token->type = lex->type;
			task->token->length = lex->length;
			task->token->offset = (short)(lex->offset);

			// free the newly create lex_entry_cdt_t
			check_free_otlex_entry(lex);

			return task->token;
		}
		/* }}} */

		/* {{{ Keep the chinese punctuation.
		 * @added 2013-08-31) */
		else if (friso_cn_punctuation(friso->charset, task))
		{
			// count the punctuation in.
			memcpy(task->token->word, task->buffer, task->bytes);
			task->token->word[task->bytes] = '\0';
			task->token->type = __LEX_PUNCTUATION__;
			task->token->length = (uchar_t)task->bytes;
			task->token->offset = (short)(task->idx - task->bytes);
			return task->token;
		}
		/* }}} */
		// else if ( friso_letter_number( friso->charset, task ) )
		//{
		// }
		// else if ( friso_other_number( friso->charset, task ) )
		//{
		// }

		/* {{{ keep the unrecognized words?
		//@date 2013-10-14 */
		else if (config->keep_urec)
		{
			memcpy(task->token->word, task->buffer, task->bytes);
			task->token->word[task->bytes] = '\0';
			task->token->type = __LEX_UNKNOW_WORDS__;
			task->token->length = (uchar_t)task->bytes;
			task->token->offset = (short)(task->idx - task->bytes);
			return task->token;
		}
		/* }}} */
	}

	return NULL;
}
/* }}} */

//----------------------------------------------------------------------
// detect core logic controller: detect tokenize mode handler functions
/** {{{ get the next splited token with detect mode
 *    detect mode will only return the words in the dictionary
 *        with simple forward maximum matching algorithm
 */
FRISO_API friso_token_t next_detect_token(
	friso_entry_t friso, friso_config_t config, friso_task_t task) // no use
{
	lex_entry_cdt_t lex = NULL;
	int i, __convert = 0, tbytes, wbytes;

	/* {{{ task word pool check */
	if (!link_list_empty(task->pool))
	{
		/*
		 * load word from the word poll if it is not empty.
		 *  this will make the next word more convenient and efficient.
		 *     often synonyms, newly created word will be stored in the poll.
		 */
		lex = (lex_entry_cdt_t)link_list_remove_first(task->pool); // no use
		memcpy(task->token->word, lex->word, lex->length);
		task->token->word[lex->length] = '\0';
		task->token->type = lex->type;
		task->token->length = lex->length;
		task->token->offset = (short)(lex->offset);

		return task->token;
	}
	/* }}} */

	while (task->idx < task->length)
	{
		lex_entry_cdt_t val = NULL;

		lex = NULL;

		// read the next word from the current position.
		task->bytes = readNextWord(friso, task, &task->idx, task->buffer);
		if (0 == task->bytes)
			break;

		// clear up the whitespace.
		if (friso_whitespace(friso->charset, task))
			continue;

		// convert full-width to half-width and uppercase to lowercase for english chars
		wbytes = 0;
		tbytes = task->bytes;
		convert_full_to_half(friso, task, __convert);
		convert_upper_to_lower(friso, task, __convert);
		convert_work_apply(friso, task, __convert);

		string_buffer_clear(task->sbuf);
		string_buffer_append(task->sbuf, task->buffer);

		val = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->sbuf->buffer);
		if (NULL != val)
		{

			lex = val;
			wbytes = tbytes;
		}

		for (i = 1; i < config->max_len; i++)
		{
			task->bytes = readNextWord(friso, task, &task->idx, task->buffer);
			if (0 == task->bytes)
				break;

			// convert full-width to half-width and uppercase to lowercase for english chars
			tbytes += task->bytes;
			convert_full_to_half(friso, task, __convert);
			convert_upper_to_lower(friso, task, __convert);
			convert_work_apply(friso, task, __convert);
			string_buffer_append(task->sbuf, task->buffer);

			val = friso_dic_get(friso->new_res, friso->func_read_data_res, __LEX_CN_CHAR__, __LEX_TY_CN_WORDS__, task->sbuf->buffer);
			if (NULL != val)
			{
				lex = val;
				wbytes = tbytes;
			}
		}

		/* matches no word in the dictionary reset the task->idx to the correct value */
		if (NULL == lex)
		{
			task->idx -= (tbytes - 1);
			continue;
		}

		// yat, matched a item and tanke it to initialize the returning token
		//     also we need to push back the none-matched part by reset the task->idx
		task->idx -= (tbytes - wbytes);

		memcpy(task->token->word, lex->word, lex->length);
		task->token->word[(int)lex->length] = '\0';
		task->token->type = __LEX_CN_CHAR__;
		task->token->length = lex->length;
		task->token->offset = (short)(task->idx - wbytes);

		return task->token;
	}

	return NULL;
}

FRISO_API int friso_init(friso_entry_t friso, friso_config_t config, friso_task_t task, const char *lex_path)
{
	int ret = 0;

	if ((NULL == friso))
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	memset(friso, 0, sizeof(friso_entry));	
	friso->charset = FRISO_UTF8; // set default charset UTF8.

	if (NULL != config) {
		memset(config, 0, sizeof(friso_config_entry));
		friso_init_config(config); // initialize the configuration entry.
	}

	// task	init
	if (NULL != task) {
		memset(task, 0, sizeof(friso_task));
		friso_new_token(task->token);
		task->pool = new_link_list();
		task->sbuf = new_string_buffer();
		if (NULL == task->pool || NULL == task->sbuf)
		{
			return TC_WSErrID_OutOfMemory;
		}
	}
	
	if (NULL != lex_path) {
		ret = friso_dic_new(friso->dic, NULL);
		if (TC_WSErr_SUCCESS != ret)
		{
			ivAssert(0);
			return ret;
		}

		ret = friso_dic_load_from_ifile(friso, config, (fstring)lex_path, config->max_len * (friso->charset == FRISO_UTF8 ? 3 : 2));
		if (0 != ret)
		{
			ivAssert(0);
			return ret;
		}
	}
	
	return ret;
}

FRISO_API int friso_uninit(friso_entry_t friso, friso_config_t config, friso_task_t task, uint_t has_dic)
{
	// free the allocation of the poll link list.
	if (NULL != task && task->pool != NULL)
	{
		free_link_list(task->pool);
	}

	// release the allocation of the sbuff string_buffer_t.
	if (NULL != task && task->sbuf != NULL)
	{
		free_string_buffer(task->sbuf);
	}

	// free the dictionary
	if (has_dic) {
		friso_dic_free(friso->dic);
	}

	return TC_WSErr_SUCCESS;
}
