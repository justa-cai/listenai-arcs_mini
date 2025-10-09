/**
 * 将对外接口调用的一些函数放在这里

 *
 * @author  qungao
 * @date    2021.7.14
 */

#include <time.h>
#include "wordseg_kernel.h"
#include "friso_ctype.h"
#include "friso.h"
#include "wordseg_engine.h"
#include "wordseg_res.h"

extern FILE *g_fpCostTime;

//功能：针对输入的pStr字符串，若全是符号表里的字符则返回1，否则返回0
//static int string_is_all_punctuation(PWordSegEgn pEngine, const char *pStr);
/* 功能：判断当前utf8字符串的所有字符是否都在指定词典里 */
static int utf8str_char_is_all_in_dict(PWordSegEgn pEngine, char *pStr, int nDicID);
static int ise_cn_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize);
static int ise_en_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize);
static int tts_en_words_to_lower(PWordSegEgn pEngine, char *pOutputText, int nOutputTextSize);
static int myRstWordIsEqual(friso_token_t pResult, char *str2);
static int wordseg_merge_result(PWordSegEgn pEngine, uint_t iCur, uint_t nNum, friso_lex_t type, int bSupportWholeMerge, int bMergeDelPuncWord); //对pEngine->pResult进行合并，icur是下标(从0开始）, nNum表示从iCur开始合并的个数
static int wordseg_getmerge_max_num(PWordSegEgn pEngine, uint_t iCur, uint_t nNum, uint_t *pnNewNum);
static int wordseg_get_min_unit_word(PWordSegEgn pEngine, const char *szU8Text); //进行最小单元分词

//对分词结果二次处理，如果出了符号外，只有一个有效分词，则设置valid_wordseg_id为有效分词
int ws_engine_set_valid_wordseg_id(PWordSegEgn pEngine, text_attr_t * pTextAttr)
{
	int i;
	friso_token_t pResult = NULL;	
	int nValidCnt = 0, nPunctuationCnt = 0; //分词结果中有效分词个数，符号个数
	int iValidWordsegId = -1;

	if (pEngine->nResult < 2 || NULL == pTextAttr || pTextAttr->valid_wordseg_id >= 0)
	{
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_WHITESPACE__ == pResult[i].type || __LEX_PUNCTUATION__ == pResult[i].type)
		{
			nPunctuationCnt++;
		}
		if (pResult[i].type >= __LEX_CN_CHAR__ && pResult[i].type <= __LEX_TY_EN_WORDS__)
		{
			nValidCnt++;
			iValidWordsegId = i;
		}
	}
	if (1 == nValidCnt && (nPunctuationCnt + nValidCnt == pEngine->nResult))
	{
		pTextAttr->valid_wordseg_id = iValidWordsegId;
	}

	return TC_WSErr_SUCCESS;
}

int ws_engine_process_whitespace(PWordSegEgn pEngine)
{
	int i, j;
	friso_token_t pResult = NULL;	
	char *szWords;
	int bProcessWhitespace = 1;
	int nWhiteSpaceNum = 0;
	int nRet = 0;

	pResult = pEngine->pResult;
	if (pEngine->nResult < 3)
	{
		return TC_WSErr_SUCCESS;
	}

	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_WHITESPACE__ == pResult[i].type)
		{
			bProcessWhitespace = 1;
			break;
		}
	}
	if (0 == bProcessWhitespace) //没有空格无需处理
	{
		return TC_WSErr_SUCCESS;
	}

	szWords = pEngine->pTmpBuf->szOrgWords;
	pResult = pEngine->pResult;

	if (pEngine->nTextType != TEXT_TYPE_EN)
	{
		i = 0;
		while (i < pEngine->nResult)
		{
			int nNum = 0;
			if (pResult[i].type > __LEX_TY_CN_WORDS__) //非汉字不处理
			{
				i++;
				continue;
			}

			nWhiteSpaceNum = 0;
			//memset(szWords, 0, sizeof(pEngine->pTmpBuf->szOrgWords));
			//strcat(szWords, pResult[i].word);
			//strcpy(szWords, pResult[i].word);
			nNum++;
			int jj = i;
			for (j = i + 1; j < pEngine->nResult - 1; j++)
			{
				if (__LEX_WHITESPACE__ == pResult[j].type)
				{
					nNum++;
					nWhiteSpaceNum++;
					continue;
				}
				if (pResult[j].type > __LEX_TY_CN_WORDS__)
				{
					break;
				}
				jj = j;
				//strcat(szWords, pResult[j].word);
				nNum++;
			}
			//(nNum - nWhiteSpaceNum > 1):除空格外合并了二个分词才会
			if (nWhiteSpaceNum > 0 && nNum > 2 && nNum - nWhiteSpaceNum > 1)
			{
				int word_len = 0;
				for (int k = i; k <= jj; k++) {
					if (__LEX_WHITESPACE__ == pResult[k].type)	continue;
					memcpy(szWords + word_len, pResult[k].word, pResult[k].length);
					word_len += pResult[k].length;
				}
				szWords[word_len] = 0;
				lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TY_CN_WORDS__, __LEX_TY_CN_WORDS__, szWords); //先在中文词库中查找
				if (NULL != lex)
				{
					nRet = wordseg_merge_result(pEngine, i, nNum, (friso_lex_t)(lex->type), 1, 0);
					if (TC_WSErr_SUCCESS != nRet)
					{
						return nRet;
					}
					strcpy(pResult[i].word_delpunc, szWords);
					strcpy(pResult[i].word_tts, szWords);
				}
			} // if (nWhiteSpaceNum > 0)
			i++;
		}
	}

	//针对英语单词有空格导致的无法分词到一起进行处理,如“d e  sk”
#if 1
	if (pEngine->nTextType == TEXT_TYPE_EN) 
	{
		strcpy(szWords, pResult[i].word);
		bProcessWhitespace = 0;		
		for (i = 0; i < pEngine->nResult; i++)
		{
			if (__LEX_WHITESPACE__ == pResult[i].type)
			{
				bProcessWhitespace = 1;
				continue;
			}
			if (__LEX_PUNCTUATION__ == pResult[i].type || __LEX_TY_EN_WORDS__ == pResult[i].type)
			{
				bProcessWhitespace = 0;
				break;
			}
			strcat(szWords, pResult[i].word);
		}
		if (bProcessWhitespace)
		{
			lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, szWords);
			if (NULL != lex)
			{
				nRet = wordseg_merge_result(pEngine, 0, pEngine->nResult, (friso_lex_t)(lex->type), 1, 0);
				if (TC_WSErr_SUCCESS != nRet)
				{
					return nRet;
				}
				strcpy(pResult[0].word_delpunc, szWords);
				strcpy(pResult[0].word_tts, szWords);
			}
		}
	}
	
#endif

	return TC_WSErr_SUCCESS;
}

//处理结尾符号
int ws_engine_process_endpunc(PWordSegEgn pEngine)
{
	int i;
	friso_token_t pResult = NULL;
	
	pResult = pEngine->pResult;

	if (pEngine->nResult < 2)
	{
		return TC_WSErr_SUCCESS;
	}

#if 0 //只针对指定词典里存在的分词，才进行结尾符号处理
	if (!(pResult[0].type >= __LEX_TY_POEM__ && pResult[0].type <= __LEX_TY_ALLEGORICAL_SAYINGS__) &&
		__LEX_TY_EN_WORDS__ != pResult[0].type)
	{
		return  TC_WSErr_SUCCESS;
	}
#else //只要出了第一个分词外，后面的都是符号的话，全部处理掉
	if (__LEX_PUNCTUATION__ == pResult[0].type || __LEX_WHITESPACE__ == pResult[0].type || __LEX_DECIMAL__ == pResult[0].type || __LEX_INTEGER__ == pResult[0].type)
	{
		return TC_WSErr_SUCCESS;
	}
#endif

	if (utf8_numeric_string(pResult[0].word))
	{
		return TC_WSErr_SUCCESS;
	}

	for (i = 1; i < pEngine->nResult; i++)
	{
		if (__LEX_PUNCTUATION__ != pResult[i].type && __LEX_WHITESPACE__ != pResult[i].type)
		{
			return TC_WSErr_SUCCESS;
		}
	}

	if (NULL == friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TY_CN_WORDS__, __LEX_TY_EN_WORDS__, pEngine->szText) &&
		strlen(pEngine->szText) < sizeof(pResult[0].word))
	{
		strcpy(pResult[0].word, pEngine->szText);
		pResult[0].length = (uchar_t)(strlen(pResult[0].word));
		ivAssert(pResult[0].length < sizeof(pResult[0].word));
		pEngine->nResult = 1;
	}

	return TC_WSErr_SUCCESS;
}

//处理拼音拼读拆分可以继续分词，如 "b-à-bà" -> b/-/à/-/bà/
int ws_engine_process_pinduchaifen(PWordSegEgn pEngine)
{
	friso_token_t pResult = NULL;

	if (NULL == pEngine)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pResult = pEngine->pResult;

	// TTS发音标注
	for (int i = 0; i < pEngine->nResult; i++)
	{		
		if (__LEX_PINYIN_CHAIFEN_DICT__ != pResult[i].type && __LEX_OTHER_WORDS__ != pResult[i].type)
		{
			continue;
		}

		lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_CHAIFEN_DICT__, __LEX_PINYIN_CHAIFEN_DICT__, pResult[i].word_delpunc);
		if (NULL != lex)
		{
			ivAssert(strlen(lex->word_new) < sizeof(pResult[i].word_tts));
			strcpy(pResult[i].word_tts, lex->word_new);
		}
	}

	//针对整句是拼音拆分，需要支持分词 如 "b-à-bà" -> b/-/à/-/bà/
	lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_CHAIFEN_DICT__, __LEX_PINYIN_CHAIFEN_DICT__, pEngine->szText);
	if (NULL != lex)
	{
		//直接字符串处理暴力分词，如 "b-à-bà" -> b/-/à/-/bà/
		int nNewResultNum = 0, nOffset = 0;
		char *pStrTmp = pEngine->pTmpBuf->szLowerWords; //复用内存

		string_split_entry pStrSplit[1] = {0};
		string_split_reset(pStrSplit, "-", pEngine->szText);
		while (NULL != string_split_next(pStrSplit, pStrTmp))
		{
			ivAssert(strlen(pStrTmp) < sizeof(pResult[nNewResultNum].word));
			strcpy(pResult[nNewResultNum].word, pStrTmp);
			strcpy(pResult[nNewResultNum].word_delpunc, pStrTmp);
			strcpy(pResult[nNewResultNum].word_tts, pStrTmp);
			pResult[nNewResultNum].is_modified = 0;
			pResult[nNewResultNum].type = __LEX_PINYIN__;
			pResult[nNewResultNum].length = (uchar_t)(strlen(pStrTmp));
			ivAssert(pResult[nNewResultNum].length < sizeof(pResult[0].word));
			pResult[nNewResultNum].offset = nOffset;
			nOffset += pResult[nNewResultNum].length;
			nNewResultNum++;

			strcpy(pStrTmp, "-");
			strcpy(pResult[nNewResultNum].word, pStrTmp);
			strcpy(pResult[nNewResultNum].word_delpunc, pStrTmp);
			strcpy(pResult[nNewResultNum].word_tts, pStrTmp);
			pResult[nNewResultNum].is_modified = 0;
			pResult[nNewResultNum].type = __LEX_PUNCTUATION__;
			pResult[nNewResultNum].length = (uchar_t)(strlen(pStrTmp));
			ivAssert(pResult[nNewResultNum].length < sizeof(pResult[0].word));
			pResult[nNewResultNum].offset = nOffset;
			nOffset += pResult[nNewResultNum].length;
			nNewResultNum++;
		}
		if (nNewResultNum > 1)
		{
			nNewResultNum--; //去掉最后多加的一个“-”
		}

		pEngine->nResult = nNewResultNum;
	}

	return TC_WSErr_SUCCESS;
}

//针对用户想扫描单独的单词，但是可能会扫到前后其他词的一部分情况进行处理
int ws_engine_process_enword_beg_end_invalidstr(PWordSegEgn pEngine, text_attr_t * pTextAttr)
{
	int i /*, err*/;
	friso_token_t pResult = NULL;	

	if (NULL == pEngine || NULL == pTextAttr)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pTextAttr->valid_wordseg_id = -1;
	if (0 == pTextAttr->b_chk_valid_wordseg)
	{
		return TC_WSErr_SUCCESS;
	}
		
	pResult = pEngine->pResult;

	if (pEngine->nResult < 2 || pEngine->nResult > 6) //避免误伤第一波:分词太多的不处理
	{
		return TC_WSErr_SUCCESS;
	}

	//避免误伤，对于开头是“A/a "不处理，例如”A sheep”
	if (pEngine->nResult >= 3 && 1 == strlen(pResult[0].word_delpunc) &&
		('A' == pResult[0].word_delpunc[0] || 'a' == pResult[0].word_delpunc[0]) &&
		__LEX_WHITESPACE__ == pResult[1].type)
	{
		return TC_WSErr_SUCCESS;
	}

	//先判断是否处理
	int nWordsNum = 0, iWordsID = -1;
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_TY_EN_WORDS__ == pResult[i].type || __LEX_PINYIN_CHAIFEN_DICT__ == pResult[i].type ||
			0 == strcmp(pResult[i].word_delpunc, "I"))
		{
			nWordsNum++;
			iWordsID = i;
			continue;
		}
		if (pResult[i].length >= 5) //避免误伤第三波：需要被删除的分词字符数较多，不处理,例如一些人名不在lex里引起的误伤 “Christian Jolibois”->“Christian”
		{
			//考虑实际前后多扫的部分不会太多，做个长度限制，防止误伤
			break;
		}
		if (__LEX_WHITESPACE__ != pResult[i].type && __LEX_PUNCTUATION__ != pResult[i].type && __LEX_OTHER_WORDS__ != pResult[i].type)
		{
			break;
		}
		if (__LEX_OTHER_WORDS__ == pResult[i].type && !string_is_letter(pResult[i].word_delpunc))
		{
			break;
		}
	}

	//如果所有分词不是 "一个有效单词 + 若干符号 +若干无意义英文字母串"组合，则不处理
	if (i < pEngine->nResult || 1 != nWordsNum)
	{
		return TC_WSErr_SUCCESS;
	}

	//避免误伤第二波：整词在lex里存在不处理。例如把“a lot”误处理成“lot"
	strcpy(pEngine->pTmpBuf->szLowerWords, pEngine->szText);
	convert_letter_upper_to_lower(pEngine->pTmpBuf->szLowerWords); //都转为小写字母，英文词库也都是小写字母
	lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, pEngine->pTmpBuf->szLowerWords);
	if (NULL != lex) //如果全文本本身就是一个分词，就不继续了
	{
		return TC_WSErr_SUCCESS;
	}

	pTextAttr->valid_wordseg_id = iWordsID;

	return TC_WSErr_SUCCESS;
}

//每条分词的翻译和tts结果针对拼音单独处理
int ws_engine_process_tts_trans_text(PWordSegEgn pEngine)
{
	int i;
	int bHasPinyin = 0;
	friso_token_t pResult = NULL;
	
	pResult = pEngine->pResult;
	for (i = 0; i < pEngine->nResult; i++) //默认以去角标处理的文本为准
	{
		ivAssert(strlen(pResult[i].word_delpunc) < sizeof(pResult[i].word_tts));		
		if (0 == strlen(pResult[i].word_delpunc))
		{
			strcpy(pResult[i].word_delpunc, pResult[i].word);
		}
		strcpy(pResult[i].word_tts, pResult[i].word_delpunc);
	}

	for (i = 0; i < pEngine->nResult; i++)
	{
		if (!(__LEX_PINYIN_SOFT_DICT__ ==pResult[i].type || __LEX_PINYIN_DICT__ == pResult[i].type || __LEX_PINYIN_CHAIFEN_DICT__ == pResult[i].type ||
			__LEX_UNKNOW_WORDS__ == pResult[i].type || __LEX_OTHER_WORDS__ == pResult[i].type || __LEX_PINYIN__ == pResult[i].type))
		{
			continue; //效率优化，拼音只会是这几种类型
		}

		char *pBuffer = pEngine->pTmpBuf->szOrgWords; //复用
		strcpy(pBuffer, pResult[i].word_tts);
		string_to_lowcase_letter(pBuffer, pBuffer);
		lex_entry_cdt_t pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer);
		if (NULL != pTag)
		{
			//是拼音串
			ivAssert(strlen(pTag->word_new) < sizeof(pResult[i].word_tts));
			strcpy(pResult[i].word_tts, pTag->word_new); // TTS: zuō -> 哈[=zuo1]
			string_to_lowcase_letter(pResult[i].word_delpunc, pResult[i].word_delpunc);
			pResult[i].type = __LEX_PINYIN__;
			bHasPinyin = 1;
		}
	}

	if (bHasPinyin) //如果有带调拼音，则继续查看是否有轻声拼音，都按照TTS要求进行标注
	{
		for (i = 0; i < pEngine->nResult; i++)
		{
			if (__LEX_TY_EN_WORDS__ != pResult[i].type &&
				__LEX_OTHER_WORDS__ != pResult[i].type &&
				__LEX_PINYIN_CHAIFEN_DICT__ != pResult[i].type &&
				__LEX_PINYIN__ != pResult[i].type &&
				__LEX_PINYIN_SOFT_DICT__ != pResult[i].type)
			{
				continue; //效率优化，无调拼音只会是这二种类型
			}

			char *pBuffer = pEngine->pTmpBuf->szOrgWords; //复用
			strcpy(pBuffer, pResult[i].word_tts);
			string_to_lowcase_letter(pBuffer, pBuffer); //兼容大写字母
			lex_entry_cdt_t pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_SOFT_DICT__, __LEX_PINYIN_SOFT_DICT__, pBuffer);
			if (NULL != pTag)
			{
				//是轻声拼音串
				ivAssert(strlen(pTag->word_new) < sizeof(pResult[i].word_tts));
				strcpy(pResult[i].word_tts, pTag->word_new); // TTS: zuō -> 哈[=zuo1]
				string_to_lowcase_letter(pResult[i].word_delpunc, pResult[i].word_delpunc);
				pResult[i].type = __LEX_PINYIN__;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

#if SUPPORT_PROCESS_CORNER_MARKER
int myTokenCpy(friso_token_t pDst, friso_token_t pSrc)
{
	if (NULL == pDst || NULL == pSrc)
	{
		return TC_WSErrID_InvArg;
	}

	if ((unsigned int)pDst == (unsigned int)pSrc)
	{
		return TC_WSErr_SUCCESS;
	}

	memcpy(pDst, pSrc, sizeof(friso_token));

	return TC_WSErr_SUCCESS;
}

/*
 -------------------------------需求------------------------
 处理教材中古诗词有很多角标，导致分词和内容查询都不对的问题。
1、角标处理涉及的符号在g_szCornerMarkTab中，有：①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳@©㊟㊣㊤㊥㊦㊧㊨°{}^｛｝
2、针对出现的^{①-⑳}等各种组合(如 ^{①}、^①}、{①}、^①}进行处理，在句首只保留一个①-⑳；非句首直接去除
*/
int ws_engine_process_cornermarker(PWordSegEgn pEngine)
{
	int i, j;
	friso_token_t pResult = NULL;
	PWord pDelPuncWord = NULL;
	int bMalloc = 0;	
	int nMaxBytes = 0;
	char *pDelPuncText = NULL, *pOrigText;
	int nNewResultNum = 0;
	uint_t iCur = 0, nCornerMarkerCnt = 0;
	int nCornerMarkerID = -1;
		
	if (pEngine->nResult * sizeof(TWord) >= sizeof(pEngine->pTmpBuf->pCornerBuf))
	{
		return TC_WSErr_SUCCESS; //分词个数太多就暂时不处理角标，避免内存分配，后续待优化 ！！！
	}

	//耗时函数，分词太多，感觉乱扫的可能性更大，就不再处理
	pResult = pEngine->pResult;

	//考虑大部分分词个数不超过50个，就不分配内存，避免每次都malloc/free导致的内存碎片啥滴
	if (pEngine->nResult * sizeof(TWord) <= sizeof(pEngine->pTmpBuf->pCornerBuf))
	{
		pDelPuncWord = pEngine->pTmpBuf->pCornerBuf;
		bMalloc = 0;
	}
	else
	{
		pDelPuncWord = (PWord)FRISO_MALLOC(pEngine->nResult * sizeof(TWord));
		if (NULL == pDelPuncWord)
		{
			return TC_WSErrID_OutOfMemory;
		}
		bMalloc = 1;
	}
	memset(pDelPuncWord, 0, pEngine->nResult * sizeof(TWord));

	pDelPuncText = pEngine->pTmpBuf->szOrgWords; //内存复用
	pOrigText = pEngine->pTmpBuf->szLowerWords;	//内存复用

	nMaxBytes = (int)(pEngine->nLexMaxBytes);

	//先对原有分词结果进行去g_szCornerMarkTab符号处理
	int hasCornermark = 0;
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (NULL == friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CORNERMARK__, __LEX_CORNERMARK__, pResult[i].word))
		{
			strcpy(pDelPuncWord[i].word, pResult[i].word);			
		}
		else {
			hasCornermark = 1;
		}

		strcpy(pResult[i].word_delpunc, pResult[i].word);
		pDelPuncWord[i].length = strlen(pDelPuncWord[i].word);
	}

	if (!hasCornermark) {
		return TC_WSErr_SUCCESS;
	}

	//对去符号后分词进行拼接后，和词库进行匹配
	i = 0;
	while (i < pEngine->nResult)
	{
		int iMergeIdx = 0;
		int nWordBytes = 0, nOrgWordBytes = 0;

		//memset(pDelPuncText, 0, sizeof(pEngine->pTmpBuf->szOrgWords));
		memcpy(pDelPuncText, pDelPuncWord[i].word, pDelPuncWord[i].length);
		nWordBytes = pDelPuncWord[i].length;

		//memset(pOrigText, 0, sizeof(pEngine->pTmpBuf->szOrgWords));
		memcpy(pOrigText, pResult[i].word, pResult[i].length);
		nOrgWordBytes = pResult[i].length; //记录原分词文本长度

		if (0 == pDelPuncWord[i].length) //该条分词是单独的符号或者数字,则保持原样
		{
			myTokenCpy(pResult + nNewResultNum, pResult + i);
			nNewResultNum++;

			i++;
			continue;
		}

		for (j = i + 1; j < pEngine->nResult; j++)
		{
			lex_entry_cdt_t lex = NULL;
			if (0 == pDelPuncWord[j].length)
			{
				memcpy(pOrigText + nOrgWordBytes, pResult[j].word, pResult[j].length);
				nOrgWordBytes += pResult[j].length; //记录原分词文本长度
				continue;
			}
			if (nWordBytes + pDelPuncWord[i].length > nMaxBytes)
			{
				break; //拼接文本已经大于词典中最长词条数
			}

			memcpy(pDelPuncText + nWordBytes, pDelPuncWord[j].word, pDelPuncWord[j].length);
			nWordBytes += pDelPuncWord[j].length; //记录删除某些符号后的文本长度
			pDelPuncText[nWordBytes] = 0;

			memcpy(pOrigText + nOrgWordBytes, pResult[j].word, pResult[j].length);
			nOrgWordBytes += pResult[j].length; //记录原分词文本长度
			pOrigText[nOrgWordBytes] = 0;

			lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_CN_WORDS__, __LEX_TY_CN_WORDS__, pDelPuncText);
			if (NULL != lex)
			{
#if !SUPPORT_PERFECT_MATCH
				if (nOrgWordBytes < sizeof(pResult[0].word) && nOrgWordBytes < (int)(pEngine->nTextLen))
				{
#endif
					//若词典里能找到，就修改分词结果，分到一起
					memcpy(pResult[nNewResultNum].word, pOrigText, nOrgWordBytes);
					pResult[nNewResultNum].word[nOrgWordBytes] = '\0';
					memcpy(pResult[nNewResultNum].word_delpunc, pDelPuncText, nWordBytes);
					pResult[nNewResultNum].word_delpunc[nWordBytes] = '\0';

					pResult[nNewResultNum].length = (uchar_t)nOrgWordBytes;
					ivAssert(pResult[nNewResultNum].length < sizeof(pResult[0].word));
					pResult[nNewResultNum].type = __LEX_TY_CN_WORDS__;
					pResult[nNewResultNum].is_modified = 1;
					iMergeIdx = j;
					// break; //不需要break，继续匹配到可能的最长串
#if !SUPPORT_PERFECT_MATCH
				}
#endif
			}
		}

		if (iMergeIdx > 0)
		{
			i = iMergeIdx + 1;
		}
		else
		{
			if (nNewResultNum < i)
			{
				memcpy(pResult + nNewResultNum, pResult + i, sizeof(friso_token));
			}
			strcpy(pResult[nNewResultNum].word_delpunc, pResult[i].word);
			pResult[nNewResultNum].is_modified = 0;

			i++;
		}
		nNewResultNum++;
	}

	//结果有改变，则做其他后处理
	if (nNewResultNum < pEngine->nResult)
	{
		int nOffset = 0;
		pEngine->nResult = nNewResultNum;
		for (i = 0; i < nNewResultNum; i++)
		{
			pResult[i].offset = nOffset;
			nOffset += pResult[i].length;
		}
	}

	//修改角标处理规则，之前是影响分词才处理，改为出现的^{①-⑳}等各种组合(如 ^{①}、^①}、{①}、^①}进行处理，在句首只保留一个①-⑳；非句首直接去除 20201104
	iCur = 0;
	nCornerMarkerCnt = 0;
	nCornerMarkerID = -1;
	i = 1;
	while (i < pEngine->nResult)
	{
		if (NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CORNERMARK__, __LEX_CORNERMARK__, pResult[i].word))
		{
			if (pResult[i].length == 3 && ((unsigned char)pResult[i].word[0] == 0xe2) && ((unsigned char)pResult[i].word[1] == 0x91))			
			{
				nCornerMarkerID = i; //记录①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳对应的分词下标，用于句首的角标保留使用
			}
			nCornerMarkerCnt++;
			i++;
		}
		else
		{
			//
			if (nCornerMarkerCnt > 1 || (1 == nCornerMarkerCnt && nCornerMarkerID > 0))
			{
				if (nCornerMarkerCnt + 1 == (uint_t)(i) && nCornerMarkerID > 0 && NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CORNERMARK__, __LEX_CORNERMARK__, pResult[0].word))
				{
					//句首的角标串特殊处理 如"^{⑤,像细丝,密密地" ->"⑤,像细丝,密密地"
					pResult[iCur].is_modified = 1;
					strcpy(pResult[0].word_delpunc, pResult[nCornerMarkerID].word);
					wordseg_merge_result(pEngine, 0, nCornerMarkerCnt + 1, pResult[nCornerMarkerID].type, 1, 0);
					iCur = 0;
					i = 1;
				}
				else
				{
					pResult[iCur].is_modified = 1;
					wordseg_merge_result(pEngine, iCur, nCornerMarkerCnt + 1, pResult[iCur].type, 1, 0);
					i = iCur + 1;
					iCur = i;
				}
			}
			else
			{
				iCur = i;
				i++;
			}
			nCornerMarkerCnt = 0;
			nCornerMarkerID = -1;
		}
	}
	if (nCornerMarkerCnt > 1 || (1 == nCornerMarkerCnt && nCornerMarkerID > 0))
	{
		pResult[iCur].is_modified = 1;
		wordseg_merge_result(pEngine, iCur, nCornerMarkerCnt + 1, pResult[iCur].type, 1, 0);
	}

	if (bMalloc && NULL != pDelPuncWord)
	{
		FRISO_FREE(pDelPuncWord);
	}

	return TC_WSErr_SUCCESS;
}

#endif

/*
*  对输入文本进行中文/英文/拼音 判断 （依赖分词结果）
*  判断规则：排除所有符号外，全部是英文字母/字母数字组合是英文，否则是中文
* @param handle     [in] wordseg_init返回的工作句柄
* @param szU8Text   [in] 待分词文本(要求UTF-8文本，以\0结束）
* @param bHasResult [in] 是否有分词结果,区分是内部分词后调用（判断中文/英文/拼音）， 还是外部API调用（不做分词仅判断中文/英文）
* @param pTextAttr   [out] 返回文本类型和修正后文本
注意事项：wordseg_init之后才能调用。
*/
int wordseg_get_texttype_by_wordsegrst(void *handle, const char *szU8Text, text_attr_t * pTextAttr)
{
	PWordSegEgn pEngine = NULL;
	int i = 0, nType = 0;
	int nLetter = 0, nNumber = 0, nPinYin = 0; //统计数字/字母/拼音的个数
	int nOffset1 = 0, nOffset2 = 0;

	if (NULL == handle || NULL == szU8Text || NULL == pTextAttr)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	if (strlen(szU8Text) > WS_INPUT_TEXT_MAX_LEN)
	{
		ivAssert(0);
		return TC_WSErrID_TextTooLong;
	}

	memset(pTextAttr, 0, sizeof(text_attr_t));
	//拷贝修正后文本
	for (i = 0; i < pEngine->nResult; i++)
	{
		strcpy(pTextAttr->word_delpunc + nOffset1, pEngine->pResult[i].word_delpunc);
		//nOffset1 += strlen(pEngine->pResult[i].word_delpunc);
		strcpy(pTextAttr->word_tts + nOffset2, pEngine->pResult[i].word_tts);
		//nOffset2 += strlen(pEngine->pResult[i].word_tts);
	}

	//先根据分词结果判断是否是拼音类型
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_PINYIN__ == pEngine->pResult[i].type)
		{
			nPinYin++;
		}
		else if (__LEX_WHITESPACE__ != pEngine->pResult[i].type && __LEX_PUNCTUATION__ != pEngine->pResult[i].type)
		{
			break;
		}
	}

	if (i >= pEngine->nResult && nPinYin > 0) //排除符号和空格后，都是拼音，则认为是拼音串
	{
		nType = TEXT_TYPE_PY;
	}
	else //非拼音类型，则进行中英文的判断
	{
		//	判断依据：排除标点符号后，排除标点符号后判断， 纯数字当做中文. "123,"中文，”123ABC,"英文
		i = 0;
		nType = TEXT_TYPE_EN;
		nLetter = 0, nNumber = 0; //统计数字和字母的个数
		while (i < (int)(strlen(szU8Text)))
		{
			lex_entry_cdt_t lex = NULL;
			char szOneChar[10];
			int nBytes = get_utf8_bytes(szU8Text[i]);
			memcpy(szOneChar, szU8Text + i, nBytes);
			szOneChar[nBytes] = '\0';

			lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, szOneChar);
			if (NULL != lex)
			{
				//标点符号：不做判断依据
				i += nBytes;
				continue;
			}

			if (1 != nBytes)
			{
				nType = TEXT_TYPE_CN;
				break;
			}

			if (string_is_letter(szOneChar))
			{
				nLetter++;
			}
			else if (string_is_numeric(szOneChar))
			{
				nNumber++;
			}

			i += nBytes;
		}

		if (0 == TEXT_TYPE_EN && nNumber > 0 && 0 == nLetter)
		{
			nType = TEXT_TYPE_CN; //数字当做中文，数字英文混合当做英文
		}
	}

	pTextAttr->text_type = nType;

	return TC_WSErr_SUCCESS;
}

int ws_engine_texttype_get(const char *szU8Text, int *pnType)
{
	int nType = -1;
	int bHasLetter = 0; //记录是否有英文字母	

	//修改文本类型判断方式（中/英/拼音）：没有汉字和带调拼音，就是英文 20210301
	char *pText = (char *)szU8Text;
	int nLen = (int)(strlen(pText));
	int nOffset = 0;
	nType = TEXT_TYPE_EN;

	//整数和小数类型=中文
	if (utf8_numeric_string(pText) || utf8_decimal_string(pText))
	{
		*pnType = TEXT_TYPE_CN;
		return TC_WSErr_SUCCESS;
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
			nType = TEXT_TYPE_CN;
			break;
		}
		else
		{
			int nUnicode = get_utf8_unicode(pText + nOffset);
			if (nUnicode >= 0x4E00 && nUnicode <= 0x9FEF) //中文的Unicode编码范围
			{
				nType = TEXT_TYPE_CN;
				break;
			}
		}

		nOffset += nBytes;
	}

	//没有英文字母且没有汉字的情况改为“中文”，感觉更合理，录入“123 123%”
	if (0 == bHasLetter && TEXT_TYPE_EN == nType)
	{
		nType = TEXT_TYPE_CN;
	}

	*pnType = nType;

	return TC_WSErr_SUCCESS;
}


/*
*  对输入文本进行中文/英文判断
*  判断规则1：当没有汉字和带调拼音，且至少含有一个字母就认为是英文，否则中文
			如”uē üé üě“类型=中文；“13 23424,234%"类型=中文； ”我是a good boy！“类型=中文；”我真的是 a boy！“类型=中文
   判断规则2：仅在bHasResult=1(wordseg_do)时起效. 可以判断拼音串、中英混合根据数量判断是中/英文.
			如”uē üé üě“类型=拼音；“13 23424,234%"类型=中文； ”我是a good boy！“类型=英文；”我真的是 a boy！“类型=中文
* @param handle     [in] wordseg_init返回的工作句柄
* @param szU8Text   [in] 待分词文本(要求UTF-8文本，以\0结束）
* @param bHasResult [in] 是否有分词结果,区分是内部分词后调用（判断中文/英文/拼音）， 还是外部API调用（不做分词仅判断中文/英文）
* @param pTextAttr   [out] 返回文本类型和修正后文本
注意事项：wordseg_init之后才能调用。
*/
/*FRISO_API*/ int ws_engine_get_texttype_new(PWordSegEgn pEngine, const char *szU8Text, int bHasResult, text_attr_t * pTextAttr)
{	
	int i = 0, nType = -1;
	int nPinYin = 0; //统计数字/字母/拼音的个数
	int nOffset1 = 0, nOffset2 = 0;
	friso_token *pResult = NULL;
	int bPuncAndWhiteSpace = 1;
#if LOG_MODULE_COST_TIME
	int t1, t2, time_begin, time_end;
#endif
	char *text = pEngine->szText;

	if (NULL == pEngine || NULL == szU8Text || NULL == pTextAttr)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	memset(pTextAttr, 0, sizeof(text_attr_t));
	pTextAttr->text_type = TEXT_TYPE_CN; //设置默认值

	if (strlen(szU8Text) > WS_INPUT_TEXT_MAX_LEN)
	{
		//考虑调用该接口文本可能很长，做下小处理，按照前512字节判断文本类型 23.6.2
		int offset = 0;
		while (offset < strlen(szU8Text))
		{
			int bytes = get_utf8_bytes(szU8Text[offset]);
			if (offset + bytes > WS_INPUT_TEXT_MAX_LEN) {
				memcpy(text, szU8Text, offset);
				text[offset] = 0;
				break;
			}
			offset += bytes;
		}
		ivAssert(0);
		//return TC_WSErrID_TextTooLong;
	}
	else
	{
		text = (char *)szU8Text;
	}

	pTextAttr->bUseLocalOcr = 1;

	/* 增加多语种 日韩西俄分词需求
	因为云端OCR扫描是语种无关的，所以传入的文本可能是多个语种字符混合，需要先统计不同语种的字/单词个数来判断语种，
	如果上述语种判断是日韩西俄，则只进行基础分词，中/日/韩每个字是一个分词,英/西/俄以单词为准，碰到空格就作为一个单词，其他符号空格等都作为单独一个分词；
	如果上述语种判断是中英，则还是按原来的分词逻辑执行
	*/
	if ((WS_LANGUAGE_JAPANESE == pEngine->nPriorLanguage || WS_LANGUAGE_KOREAN == pEngine->nPriorLanguage ||
		 WS_LANGUAGE_SPANISH == pEngine->nPriorLanguage || WS_LANGUAGE_RUSSIAN == pEngine->nPriorLanguage) &&
		!(bHasResult && pEngine->nTextType <= TEXT_TYPE_PUNC))
	{
#if LOG_MODULE_COST_TIME
		int t1 = GetCostTimeMs();
#endif
		int nRet = ws_engine_do_otherlanguage(pEngine, text, pTextAttr);
		if (nRet != 0)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		int t2 = GetCostTimeMs();
		fprintf(g_fpCostTime, " language=%d\r\n", pEngine->nPriorLanguage);
		fprintf(g_fpCostTime, " 025 strlen=%d [wordseg_get_texttype_new wordseg_do_otherlanguage] cost time = %d ms\r\n", strlen(text), t2 - t1);
		if (strlen(text) > 200)
		{
			printf(" 025 strlen=%d [wordseg_get_texttype_new wordseg_do_otherlanguage] cost time = %d ms\r\n", strlen(text), t2 - t1);
		}
#endif

		if (TEXT_TYPE_JAPANESE == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_KOREAN == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_SPANISH == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_RUSSIAN == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_PUNC == pEngine->pTextAttr->text_type)
		{
			return nRet;
		}
	}

	pResult = pEngine->pResult;

	memset(pTextAttr->word_delpunc, 0, sizeof(pTextAttr->word_delpunc));
	memset(pTextAttr->word_tts, 0, sizeof(pTextAttr->word_tts));
	pTextAttr->text_type = 0;

	if (bHasResult) //有分词结果,判断中文/英文/拼音/符号, 否则只判断中文/英文
	{
		//拷贝修正后文本
		for (i = 0; i < pEngine->nResult; i++)
		{
			strcpy(pTextAttr->word_delpunc + nOffset1, pResult[i].word_delpunc);
			nOffset1 += strlen(pResult[i].word_delpunc);
			if (!(__LEX_WHITESPACE__ == pResult[i].type && i > 0 && __LEX_PINYIN__ == pResult[i - 1].type))
			{ //拼音后面的空格处理掉，否则TTS拼音读起来有停顿的感觉 2021.3.26
				strcpy(pTextAttr->word_tts + nOffset2, pResult[i].word_tts);
				nOffset2 += strlen(pResult[i].word_tts);
			}

			if (__LEX_PUNCTUATION__ != pResult[i].type && __LEX_WHITESPACE__ != pResult[i].type)
			{
				bPuncAndWhiteSpace = 0;
			}
			ivAssert(pEngine->pTextAttr->text_type >= 0);
		}
		// pTextAttr的结果后处理，针对是单独的一个拼音拆分，如"b-à-bà",需要把word_tts发音修改
		{
			lex_entry_cdt_t pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_CHAIFEN_DICT__, __LEX_PINYIN_CHAIFEN_DICT__, pTextAttr->word_tts);
			if (NULL != pTag)
			{
				strcpy(pTextAttr->word_tts, pTag->word_new);
			}
		}

		//整数和小数类型=中文
		if (1 == pEngine->nResult && (__LEX_NUMERIC__ == pResult->type || __LEX_DECIMAL__ == pResult->type))
		{
			pTextAttr->text_type = TEXT_TYPE_CN;
			return TC_WSErr_SUCCESS;
		}

		if (bPuncAndWhiteSpace) //全部由空格和符号组成，返回该类型，刘雨开发需要 20210312
		{
			pTextAttr->text_type = TEXT_TYPE_PUNC;
			return TC_WSErr_SUCCESS;
		}

		//先根据分词结果判断是否是拼音类型
		for (i = 0; i < pEngine->nResult; i++)
		{
			if (__LEX_PINYIN__ == pResult[i].type)
			{
				nPinYin++;
			}
			else if (__LEX_WHITESPACE__ != pResult[i].type && __LEX_PUNCTUATION__ != pResult[i].type)
			{
				break;
			}
		}
		if (i >= pEngine->nResult && nPinYin > 0) //排除符号和空格后，都是拼音，则认为是拼音串
		{
			pTextAttr->text_type = TEXT_TYPE_PY;
			return TC_WSErr_SUCCESS;
		}

		//新需求处理：中英混合翻译需求：汉字数量<=英语单词数量？文本类型=英文（英翻中）：文本类型=中文（中翻英）
#if 1
		int nHanziNum = 0, nEnglishWordNum = 0;
		nPinYin = 0;
		for (i = 0; i < pEngine->nResult; i++)
		{
			if (__LEX_PINYIN__ == pResult[i].type)
			{
				nPinYin++;
			}
			else if (__LEX_CN_CHAR__ == pResult[i].type) //基础汉字表
			{
				nHanziNum++;
			}
			else if (__LEX_TY_CN_WORDS__ == pResult[i].type || __LEX_CHINESE_NUM__ == pResult[i].type)
			{
				//中文词典里，统计汉字个数
				nHanziNum += get_utf8_hanzi_num(pResult[i].word_delpunc);
			}
			else if (__LEX_TY_EN_WORDS__ == pResult[i].type)
			{
				//英文词典，根据空格统计英文单词数
				char *pTmp = pResult[i].word_delpunc;
				nEnglishWordNum++;
				while (1)
				{
					char *p = strstr(pTmp, " ");
					if (NULL == p)
					{
						break;
					}
					nEnglishWordNum++;
					pTmp = p + 1;
				}
			}
			else if (__LEX_OTHER_WORDS__ == pResult[i].type)
			{
				//一些英文词典里不包含的英文单词类型是__LEX_OTHER_WORDS__
				if (string_is_letter(pResult[i].word_delpunc))
				{
					nEnglishWordNum++;
				}
			}
		}
		if (nHanziNum > 0 || nPinYin > 0 || nEnglishWordNum > 0)
		{
			if (nHanziNum > nEnglishWordNum || nPinYin > nEnglishWordNum)
			{
				// 1、如果汉字个数/或者拼音个数 比英文单词个数多，就认为是中文
				pTextAttr->text_type = TEXT_TYPE_CN;
			}
			else
			{
				pTextAttr->text_type = TEXT_TYPE_EN;
			}
			return TC_WSErr_SUCCESS;
		}
#endif
	}

	//修改文本类型判断方式（中/英/拼音）：没有汉字和带调拼音，就是英文 20210301
	int bHasLetter = 0; //记录是否有英文字母	
#if 1
	ws_engine_texttype_get(text, &nType);
	pTextAttr->text_type = nType;	
	pEngine->nTextType = nType;
	if (TEXT_TYPE_CN == nType && string_has_letter((char *)text)) {
		pEngine->nTextType = TEXT_TYPE_CN_EN;
	}
#else
	{
		char *pText = (char *)text;
		int nLen = (int)(strlen(pText));
		int nOffset = 0;
		nType = TEXT_TYPE_EN;

		//整数和小数类型=中文
		if (utf8_numeric_string(pText) || utf8_decimal_string(pText))
		{
			pTextAttr->text_type = TEXT_TYPE_CN;
			return TC_WSErr_SUCCESS;
		}

		while (nOffset < nLen)
		{
			char szTmp[10] = {0};
			int nBytes = get_utf8_bytes(pText[nOffset]);
			memcpy(szTmp, pText + nOffset, nBytes);
			if (string_is_letter(szTmp))
			{
				bHasLetter = 1;
			}
			else if (string_is_pinyin_with_tone(pText + nOffset))
			{
				nType = TEXT_TYPE_CN;
				break;
			}
			else
			{
				int nUnicode = get_utf8_unicode(pText + nOffset);
				if (nUnicode >= 0x4E00 && nUnicode <= 0x9FEF) //中文的Unicode编码范围
				{
					nType = TEXT_TYPE_CN;
					break;
				}
			}

			nOffset += nBytes;
		}

		//没有英文字母且没有汉字的情况改为“中文”，感觉更合理，录入“123 123%”
		if (0 == bHasLetter && TEXT_TYPE_EN == nType)
		{
			nType = TEXT_TYPE_CN;
		}
	}

	pTextAttr->text_type = nType;
	pEngine->nTextType = nType;
	if (TEXT_TYPE_CN == nType && string_has_letter((char *)text)) {
		pEngine->nTextType = TEXT_TYPE_CN_EN;
	}
#endif

	return TC_WSErr_SUCCESS;
}


//一些特殊处理，如"1940s"
int ws_engine_process_other(PWordSegEgn pEngine)
{
	int i;
	friso_token_t pResult = NULL;
	int ret = 0;
	
	if (!string_has_num(pEngine->szText)) {
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	for (i = 0; i < pEngine->nResult - 1; i++)
	{

#if !SUPPORT_PERFECT_MATCH
		if (2 == pEngine->nResult)
		{
			break;
		}
#endif

		//将 1945s 分到一起
		if (string_is_numeric(pResult[i].word) && (1 == pResult[i + 1].length && 0x73 == (unsigned char)(pResult[i + 1].word[0]))) // 0x73 = 's'
		{
#if !SUPPORT_PERFECT_MATCH
			if ((uint_t)(pResult[i].length + pResult[i + 1].length) == pEngine->nTextLen)
			{
				break;
			}
#endif
			ret = wordseg_merge_result(pEngine, i, 2, __LEX_TY_EN_WORDS__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	//将 数字:数字 分到一起, 如“12:12”“12:25:45”
	for (i = 0; pEngine->nResult > 2 && i < pEngine->nResult; i++)
	{
		int j;
		uint_t nLen = 0;
		if (!string_is_numeric(pResult[i].word))
		{
			continue;
		}
		nLen = pResult[i].length;
		for (j = i + 1; j < pEngine->nResult - 1; j += 2)
		{
			if (1 != pResult[j].length || 0x3a != (unsigned char)(pResult[j].word[0]) || !string_is_numeric(pResult[j + 1].word)) // 0x3a=':'
			{
				break;
			}
			nLen += pResult[j].length + pResult[j + 1].length;
		}
		if (j > i + 1)
		{
#if !SUPPORT_PERFECT_MATCH
			if (nLen == pEngine->nTextLen)
			{
				break;
			}
#endif
			ret = wordseg_merge_result(pEngine, i, j - i, __LEX_OTHER_WORDS__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

//补丁：处理一些特例
int ws_engine_process_specialcase(PWordSegEgn pEngine)
{
	int i, j;
	friso_token_t pResult = NULL;	
	char *pBuffer = NULL;
	int nPinyinNum = 0;
	PWsTmpBuf pTmpBuf = NULL;

	if (!string_has_pinyinwithtone(pEngine->szText)){
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	pTmpBuf = pEngine->pTmpBuf;

	//处理拼音中间无空格被分到一起的情况
	pBuffer = pTmpBuf->szLowerWords; //复用下
	memset(pTmpBuf->szLowerWords, 0, sizeof(pTmpBuf->szLowerWords));
#if 1
	for (i = 0; i < pEngine->nResult; i++)
	{
		char *pStr = pTmpBuf->szOrgWords; //复用
		int nOffset = 0, nLen = 0;

		if (__LEX_OTHER_WORDS__ != pResult[i].type && __LEX_UNKNOW_WORDS__ != pResult[i].type)
		{
			continue;
		}

		string_to_lowcase_letter(pResult[i].word, pStr); //转为小写字母
		if (!string_has_pinyinwithtone(pStr))
		{
			continue;
		}

		nPinyinNum = 0;		

		char *pOrgWord = pResult[i].word;
		int nOrgWordOffset = 0;
		while (1)
		{
			nOffset = 0;
			nLen = 0;
			memset(pTmpBuf->szLowerWords, 0, sizeof(pTmpBuf->szLowerWords));
			while (nOffset < (int)(strlen(pStr)))
			{
				int nBytes = get_utf8_bytes(pStr[nOffset]);
				memcpy(pBuffer, pStr, nOffset + nBytes);
				if (NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer))
				{
					nLen = nOffset + nBytes;
				}
				nOffset += nBytes;
			}

			if ((0 == nLen || nPinyinNum >= sizeof(pTmpBuf->szPinyinLst) / sizeof(pTmpBuf->szPinyinLst[0]) - 1) && strlen(pStr) < sizeof(pTmpBuf->szPinyinLst[0]))
			{
				ivAssert(strlen(pStr) < sizeof(pTmpBuf->szPinyinLst[nPinyinNum]));
				strcpy(pTmpBuf->szPinyinLst[nPinyinNum], pOrgWord + nOrgWordOffset);
				pTmpBuf->pbPinyin[nPinyinNum] = 0;
				pStr[0] = 0;
			}
			else if (nLen > 0 && nLen < sizeof(pTmpBuf->szPinyinLst[0]))
			{
				ivAssert(nLen < (pTmpBuf->szPinyinLst[nPinyinNum]));
				strncpy(pTmpBuf->szPinyinLst[nPinyinNum], pOrgWord + nOrgWordOffset, nLen);
				pTmpBuf->pbPinyin[nPinyinNum] = 1;
				strcpy(pStr, pStr + nLen);
				nOrgWordOffset += nLen;
			}
			else
			{
				//长度越界，不再处理
				nPinyinNum = 0;
				break;
			}
			nPinyinNum++;

			if (0 == strlen(pStr))
				break;
		}
		if (nPinyinNum > 1 && pEngine->nResult + nPinyinNum - 1 < WS_TOKEN_MAX_NUM)
		{
			for (j = pEngine->nResult - 1; j > i; j--)
			{
				memcpy(pResult + j + nPinyinNum - 1, pResult + j, sizeof(pEngine->pResult[0]));
			}
			nOffset = pResult[i].offset;
			for (j = 0; j < nPinyinNum; j++)
			{
				ivAssert(strlen(pTmpBuf->szPinyinLst[j]) < sizeof(pResult[0].word));
				strcpy(pResult[j + i].word, pTmpBuf->szPinyinLst[j]);
				string_to_lowcase_letter(pTmpBuf->szPinyinLst[j], pTmpBuf->szPinyinLst[j]); //转为小写字母
				strcpy(pResult[j + i].word_delpunc, pTmpBuf->szPinyinLst[j]);
				strcpy(pResult[j + i].word_tts, pTmpBuf->szPinyinLst[j]);
				pResult[j + i].type = (1 == pTmpBuf->pbPinyin[j] ? __LEX_PINYIN__ : __LEX_OTHER_WORDS__);
				pResult[j + i].offset = nOffset;
				pResult[j + i].length = (uchar_t)(strlen(pResult[j + i].word));

				pResult[j + i].is_modified = 0;
				nOffset += pResult[j + i].length;
			}
			pEngine->nResult += (nPinyinNum - 1);
			i += (nPinyinNum - 1);
		}
	}

#endif

	return TC_WSErr_SUCCESS;
}

//处理中文年份，如“一九四五年”-> 一九四五/年/
int ws_engine_process_chineseyear(PWordSegEgn pEngine)
{
	int i;
	friso_token_t pResult = NULL;	
	int ret = 0;
	char szYearWord[] = {0xE5, 0xB9, 0xB4, 00}; //"年"

	//不存在“年”这个字符，直接返回
	if (NULL == strstr(pEngine->szText, szYearWord))
	{
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	//将中文年份分到一起  如“一九四五年”-> 一九四五/年/
	for (i = 0; pEngine->nResult >= 5 && i <= pEngine->nResult - 5; i += 2)
	{

#if !SUPPORT_PERFECT_MATCH
		if (5 == pEngine->nResult)
		{
			break;
		}
#endif

		if (string_is_chinesenum(pResult[i].word, 0) && string_is_chinesenum(pResult[i + 1].word, 0) &&
			string_is_chinesenum(pResult[i + 2].word, 0) && string_is_chinesenum(pResult[i + 3].word, 0) &&
			(3 == pResult[i + 4].length && 0xE5 == (unsigned char)(pResult[i + 4].word[0]) && 0xB9 == (unsigned char)(pResult[i + 4].word[1]) && 0xB4 == (unsigned char)(pResult[i + 4].word[2]))) // 0xE5B9B4 "年"
		{
			ret = wordseg_merge_result(pEngine, i, 4, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	//有些数字可能lex里有，如“三九”等，特殊处理
	for (i = 0; pEngine->nResult >= 4 && i <= pEngine->nResult - 4; i += 2)
	{

#if !SUPPORT_PERFECT_MATCH
		if (4 == pEngine->nResult)
		{
			break;
		}
#endif

		if (string_is_chinesenums(pResult[i].word) && string_is_chinesenums(pResult[i + 1].word) &&
			string_is_chinesenums(pResult[i + 2].word) &&
			(3 == pResult[i + 3].length && 0xE5 == (unsigned char)(pResult[i + 3].word[0]) && 0xB9 == (unsigned char)(pResult[i + 3].word[1]) && 0xB4 == (unsigned char)(pResult[i + 3].word[2]))) // 0xE5B9B4 "年"
		{
			ret = wordseg_merge_result(pEngine, i, 3, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	for (i = 0; pEngine->nResult >= 3 && i <= pEngine->nResult - 3; i += 2)
	{

#if !SUPPORT_PERFECT_MATCH
		if (3 == pEngine->nResult)
		{
			break;
		}
#endif

		if (string_is_chinesenums(pResult[i].word) && string_is_chinesenums(pResult[i + 1].word) &&
			(3 == pResult[i + 2].length && 0xE5 == (unsigned char)(pResult[i + 2].word[0]) && 0xB9 == (unsigned char)(pResult[i + 2].word[1]) && 0xB4 == (unsigned char)(pResult[i + 2].word[2]))) // 0xE5B9B4 "年"
		{
			ret = wordseg_merge_result(pEngine, i, 2, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

//处理英文连接符"/-&"前后是英文则合并,连接符前后是字母串或者数字串都进行合并
int ws_engine_process_connector(PWordSegEgn pEngine)
{
	int i, j;
	friso_token_t pResult = NULL;

	pResult = pEngine->pResult;
	for (i = 0; (pEngine->nResult > 2) && (i < pEngine->nResult - 2); i++)
	{
		int ret = 0;
		int bLetter = 0, bNumeric = 0;
		char szTag[10] = {0x2F, 0x00}; // "/"的UTF编码

		bLetter = string_is_letter(pResult[i].word);
		bNumeric = string_is_numeric(pResult[i].word);
		if (!bLetter && !bNumeric) //开头非字母串或者数字串
		{
			continue;
		}

		//规避将单词音标分到一起的问题，如“good/gʊd/     20210223
		//实现策略：原来 / 是连接符，会将其前后单词都合并成一个分词，改为针对 “xxx / ”的数据，若"xxx"都是英文字母，转为小写后在英文词典中存在，则不将xxx和 / 合并
		if (1 == strlen(pResult[i + 1].word) && 0 == strcmp(szTag, pResult[i + 1].word))
		{
			char *pLowerWord = pEngine->pTmpBuf->szLowerWords;
			lex_entry_cdt_t lex = NULL;
			strcpy(pLowerWord, pResult[i].word);
			convert_letter_upper_to_lower(pLowerWord);																	//都转为小写字母，英文词库也都是小写字母
			lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, pLowerWord); //查看当前词条是否在英文词典中
			if (NULL != lex)
			{
				continue; //[i].word分词在英文词典中，[i+1].word是连接符“/"，则不进行合并，防止把单词音标标错
			}
		}

		for (j = i + 1; pEngine->nResult > 1 && j < pEngine->nResult - 1; j += 2)
		{
			if (!string_is_connector(pResult[j].word) || (!string_is_letter(pResult[j + 1].word) && !string_is_numeric(pResult[j + 1].word)))
			{
				break;
			}
		}
		if (j == i + 1)
		{
			continue;
		}

		ret = wordseg_merge_result(pEngine, i, j - i, pResult[i].type, bLetter, 0); //若是字母则支持整词分到一起
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
	}

#if 0
	//处理 the Kangaroos' goal -> /the/ /Kangaroos'/ /goal/
	for (i = 0; (pEngine->nResult > 2) && (i < pEngine->nResult - 1); i++)
	{
		int ret = 0;

		//将 英文单词+'分到一起（'后面不是单词)
		if (!string_is_letter(pResult[i].word))
		{
			continue;
		}
		if (1 != strlen(pResult[i + 1].word) || '\'' != pResult[i + 1].word[0])
		{
			continue;
		}

		ret = wordseg_merge_result(pEngine, i, 2, pResult[i].type, 1, 0);
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
	}
#endif

	return TC_WSErr_SUCCESS;
}

//处理中文数字，如“一千五百”，需要合并成一个分词
int ws_engine_process_chinesenum(PWordSegEgn pEngine)
{
	int i;
	friso_token_t pResult = NULL;

	if (pEngine->nTextType == TEXT_TYPE_EN) {
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;

	//解决因为词典里存在一些类似“一天”“第二”导致中文数字无法分到一起的问题
	for (i = 0; (pEngine->nResult > 1) && (i < pEngine->nResult - 1); i += 1)
	{
		char szTmp[4] = {0};
		memcpy(szTmp, pResult[i + 1].word, 3);
		if (pResult[i].length > 3 && string_is_chinesenum(pResult[i].word + pResult[i].length - 3, 0) && string_is_countingunit(pResult[i + 1].word))
		{
			char word[WS_PER_TOKEN_MAX_LEN + 4] = {0};
			//将“第二/十”改成“第/二十”
			memcpy(word, pResult[i].word + pResult[i].length - 3, 3);
			memcpy(word + 3, pResult[i + 1].word, pResult[i + 1].length);
			pResult[i].word[pResult[i].length - 3] = 0x00;
			pResult[i].length -= 3;
			//memset(pResult[i + 1].word, 0, sizeof(pResult[i + 1].word));
			pResult[i + 1].length += 3;
			pResult[i + 1].offset -= 3;
			memcpy(pResult[i + 1].word, word, pResult[i + 1].length);
			pResult[i + 1].word[pResult[i + 1].length] = 0;
			pResult[i + 1].type = __LEX_CHINESE_NUM__;
			i++;
		}
		else if ((string_is_countingunit(pResult[i].word) && pResult[i + 1].length > 3 && string_is_chinesenum(szTmp, 0)) || //将“十/一天”改成“十一/天”
				 (string_is_chinesenum(pResult[i].word, 1) && pResult[i + 1].length > 3 && string_is_countingunit(szTmp) &&
				  (!string_is_countingunit(pResult[i + 1].word)) && (!string_is_mathunit(pResult[i + 1].word))) ||		  //将“八/百年”改成“八百/年”
				 (string_is_chinese_zero(pResult[i].word) && pResult[i + 1].length > 3 && string_is_chinesenum(szTmp, 0)) //一百零一双鞋*/
		)
		{
			memcpy(pResult[i].word + pResult[i].length, szTmp, 3);
			pResult[i].length += 3;
			pResult[i].word[pResult[i].length] = 0;
			pResult[i].type = __LEX_CHINESE_NUM__;

			memcpy(pResult[i + 1].word, pResult[i + 1].word + 3, pResult[i + 1].length - 3);
			pResult[i + 1].length -= 3;
			pResult[i + 1].offset += 3;
			pResult[i + 1].word[pResult[i + 1].length] = 0x00;
			i++;
		}
	}

	for (i = 0; (pEngine->nResult >= 2) && (i <= pEngine->nResult - 2); i++)
	{
		int j;
		int ret = 0;
		uchar_t length = 0;

		//实现将如“一”“百”“五”“十”合并成“一百五十”
		for (j = i; (pEngine->nResult >= 2) && (j <= (int)(pEngine->nResult) - 2); j += 2)
		{
			char szTmp[4] = {0};
			memcpy(szTmp, pResult[j + 1].word, 3);
			if (!(string_is_chinesenum(pResult[j].word, 1) && string_is_countingunit(pResult[j + 1].word)) &&
				!(string_is_chinesenum(pResult[j].word, 1) && pResult[j + 1].length > 3 && string_is_countingunit(szTmp) && !string_is_mathunit(pResult[j + 1].word))) // 二/十一/天/ ->二十一/天/
			{
				break;
			}
#if !SUPPORT_PERFECT_MATCH
			length += pResult[j].length + pResult[j + 1].length;
			if (length == pEngine->nTextLen)
			{
				break;
			}
#endif
		}

		if (j > i)
		{
			ret = wordseg_merge_result(pEngine, i, j - i, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	//实现将“一百五十”“三”合并成“一百五十三” 或者 “十”“三”合并成“十三”
	for (i = 0; (pEngine->nResult >= 2) && (i <= pEngine->nResult - 2); i++)
	{
		char szTen[4] = {0XE5, 0X8D, 0X81, 0X00}; //“十	”

		if (((__LEX_CHINESE_NUM__ == pResult[i].type) || (0 == strcmp(pResult[i].word, szTen))) && string_is_chinesenum(pResult[i + 1].word, 0))
		{
			int ret = 0;
			ret = wordseg_merge_result(pEngine, i, 2, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
		else if (__LEX_CHINESE_NUM__ == pResult[i].type && __LEX_CHINESE_NUM__ == pResult[i + 1].type)
		{
			int ret = 0;
			ret = wordseg_merge_result(pEngine, i, 2, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
		else if (__LEX_CHINESE_NUM__ == pResult[i].type && string_is_chinesenum(pResult[i + 1].word, 0)) //如“一百一”
		{
			int ret = 0;
			ret = wordseg_merge_result(pEngine, i, 2, __LEX_CHINESE_NUM__, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

//对friso结果进行二次处理，将小数进行合并、%‰合并，比如"在/3/./25/亿/年/"->"在/3.25/亿/年/"
int ws_engine_process_decimal(PWordSegEgn pEngine)
{
	int i, j;
	int ret = 0;
	friso_token_t pResult = NULL;

	if (!string_has_num(pEngine->szText)) {
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	for (i = 0; i < pEngine->nResult; i++)
	{
		friso_lex_t type = pResult[i].type;
		int bNumeric = 0, bDecimal = 0;
		int nMergeNum = 0;
		int bWholeMerge = 0;
		if (__LEX_OTHER_WORDS__ != pResult[i].type) //数字类别是otherwords
		{
			continue;
		}

		bNumeric = string_is_numeric(pResult[i].word);
		if (!bNumeric)
		{
			bDecimal = friso_decimal_string(FRISO_UTF8, pResult[i].word);
		}

		if (!bNumeric && !bDecimal) //[i]不是数字串，就continue
		{
			continue;
		}
		if (bNumeric)
		{
			pResult[i].type = __LEX_INTEGER__;
		}
		else if (bDecimal)
		{
			pResult[i].type = __LEX_DECIMAL__;
		}

		if (i + 4 <= pEngine->nResult && bNumeric &&
			(1 == pResult[i + 1].length && '.' == pResult[i + 1].word[0]) &&
			string_is_numeric(pResult[i + 2].word) &&
			string_is_percentage(pResult[i + 3].word))
		{
			// 32 . 25 %
			nMergeNum = 4;
			type = __LEX_DECIMAL__;
		}
		else if (i + 4 <= pEngine->nResult && bNumeric &&
				 (1 == pResult[i + 1].length && '.' == pResult[i + 1].word[0]) &&
				 string_is_numeric(pResult[i + 2].word) &&
				 !string_is_percentage(pResult[i + 3].word))
		{
			// 32 . 25
			nMergeNum = 3;
			type = __LEX_DECIMAL__;
			bWholeMerge = 1;
		}
		else if (i + 3 <= pEngine->nResult && bNumeric &&
				 (1 == pResult[i + 1].length && '.' == pResult[i + 1].word[0]) && string_is_numeric(pResult[i + 2].word))
		{
			// 32 . 25
			nMergeNum = 3;
			type = __LEX_DECIMAL__;
			bWholeMerge = 1;
		}
		else if (i + 2 <= pEngine->nResult && bNumeric && string_is_percentage(pResult[i + 1].word))
		{
			// 32 %
			nMergeNum = 2;
			type = __LEX_INTEGER__;
		}
		else if (i + 2 <= pEngine->nResult && bDecimal && string_is_percentage(pResult[i + 1].word))
		{
			// 32.25 %
			nMergeNum = 2;
			type = __LEX_INTEGER__;
		}
		else
		{
			continue;
		}

		ret = wordseg_merge_result(pEngine, i, nMergeNum, type, bWholeMerge, 0);
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
	}

	for (i = 0; pEngine->nResult > 1 && i < pEngine->nResult - 1; i++) //处理数学正负号
	{
		int nMergeNum = 0;

		if (!(1 == pResult[i].length && ('-' == pResult[i].word[0] || '+' == pResult[i].word[0])))
		{
			continue;
		}
		if (i > 0 && 1 == pResult[i - 1].length && pResult[i - 1].word[0] == pResult[i].word[0]) //存在连续的--或++,则不合并
		{
			continue;
		}

		if (__LEX_INTEGER__ != pResult[i + 1].type && __LEX_DECIMAL__ != pResult[i + 1].type) //[i]不是整数/小数串，则不合并
		{
			continue;
		}

		nMergeNum = 2;

		ret = wordseg_merge_result(pEngine, i, nMergeNum, pResult[i + 1].type, 0, 0);
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
	}

	for (i = 0; pEngine->nResult > 2 && i < pEngine->nResult - 2; i++) //处理千分分隔符，如12,133   12,133.12
	{
		int nMergeNum = 0;

		if (__LEX_INTEGER__ != pResult[i].type && __LEX_DECIMAL__ != pResult[i].type) //[i]不是整数/小数串，则不合并
		{
			continue;
		}

		for (j = i + 1; j < pEngine->nResult - 1; j += 2)
		{
			if (1 != pResult[j].length || ',' != pResult[j].word[0])
			{
				break;
			}
			if (__LEX_INTEGER__ != pResult[j + 1].type && __LEX_DECIMAL__ != pResult[j + 1].type)
			{
				break;
			}
			if (__LEX_INTEGER__ == pResult[j + 1].type && 3 != pResult[j + 1].length) //不是3位数
			{
				break;
			}
			if (__LEX_DECIMAL__ == pResult[j + 1].type && (pResult[j + 1].length < 5 || '.' != pResult[j + 1].word[3])) //小数整数部分不是3位
			{
				break;
			}
			if (__LEX_DECIMAL__ == pResult[j + 1].type)
			{
				j += 2;
				break;
			}
		}

		if (j > i + 1)
		{
			nMergeNum = j - i;

			ret = wordseg_merge_result(pEngine, i, nMergeNum, pResult[i].type, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	for (i = 0; pEngine->nResult > 1 && i < pEngine->nResult - 1; i++) //处理货币符号：￥ $
	{
		int nMergeNum = 0;

		if (string_is_currencysymbol(pResult[i].word) && (__LEX_DECIMAL__ == pResult[i + 1].type || __LEX_INTEGER__ == pResult[i + 1].type))
		{
			nMergeNum = 2;

			ret = wordseg_merge_result(pEngine, i, nMergeNum, pResult[i + 1].type, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	for (i = 0; pEngine->nResult > 2 && i < pEngine->nResult - 2; i++) //处理科学计数法 1.23E+06
	{
		int nMergeNum = 0;

		if ((__LEX_DECIMAL__ == pResult[i].type || __LEX_INTEGER__ == pResult[i].type) &&
			(1 == pResult[i + 1].length && 'E' == pResult[i + 1].word[0]) &&
			(__LEX_INTEGER__ == pResult[i + 2].type && '+' == pResult[i + 2].word[0]))
		{
			nMergeNum = 3;

			ret = wordseg_merge_result(pEngine, i, nMergeNum, pResult[i].type, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	return TC_WSErr_SUCCESS;
}

static int wordseg_getmerge_max_num(PWordSegEgn pEngine, uint_t iCur, uint_t nNum, uint_t *pnNewNum)
{
	uint_t i;
	friso_token_t pResult = NULL;
	
	*pnNewNum = nNum;

	if ((int)iCur >= pEngine->nResult || (int)nNum > pEngine->nResult - iCur)
	{
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}
	if (nNum < 2)
	{
		// ivAssert(0);
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;

	//由于每个分词token支持的最长文本串长度限制，做下异常处理
	int nLen1 = 0, nLen2 = 0, nLen4 = 0, nNewNum = 0;
	for (i = iCur; i < iCur + nNum; i++)
	{
		nLen1 += strlen(pResult[i].word);
		nLen2 += strlen(pResult[i].word_delpunc);
		nLen4 += strlen(pResult[i].word_tts);
		if (nLen1 >= WS_PER_TOKEN_MAX_LEN || nLen2 >= WS_PER_TOKEN_MAX_LEN || nLen4 >= WS_PER_TOKEN_MAX_LEN ||
			nLen1 >= 255 || nLen2 >= 255 ||nLen4 >= 255)
		{
			*pnNewNum = nNewNum;
			break;
		}
		nNewNum++;
	}

	return TC_WSErr_SUCCESS;
}

//对pEngine->pResult进行合并，icur是下标(从0开始）, nNum表示从iCur开始合并的个数
static int wordseg_merge_result(PWordSegEgn pEngine, uint_t iCur, uint_t nNum, friso_lex_t type, int bSupportWholeMerge, int bMergeDelPuncWord)
{
	uint_t i;
	friso_token_t pResult = NULL;
	
	if ((int)iCur >= pEngine->nResult || (int)nNum > pEngine->nResult - iCur)
	{
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}
	if (nNum < 2)
	{
		// ivAssert(0);
		return TC_WSErr_SUCCESS;
	}

	if (0 == bSupportWholeMerge && (int)nNum == pEngine->nResult)
	{
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;
	for (i = iCur + 1; i < iCur + nNum; i++)
	{
		// ivAssert(pResult[iCur].length + pResult[i].length < sizeof(pResult[0].word));
		//增加数组可能越界的异常处理，实际设置的每个分词长度128字节够大，不会出现，以防万一
		if (pResult[iCur].length + pResult[i].length >= sizeof(pResult[0].word))
		{
			break;
		}

		int nLen = pResult[iCur].length + pResult[i].length < sizeof(pResult[0].word) ? pResult[i].length : sizeof(pResult[0].word) - pResult[iCur].length - 1;
		memcpy(pResult[iCur].word + pResult[iCur].length, pResult[i].word, nLen);
		pResult[iCur].length += (uchar_t)(nLen);
		ivAssert(pResult[iCur].length < sizeof(pResult[0].word));

		if (bMergeDelPuncWord)
		{
			if (strlen(pResult[iCur].word_delpunc) + strlen(pResult[i].word_delpunc) < sizeof(pResult[iCur].word_delpunc))
			{
				strcat(pResult[iCur].word_delpunc, pResult[i].word_delpunc);
			}
			if (strlen(pResult[iCur].word_tts) + strlen(pResult[i].word_tts) < sizeof(pResult[iCur].word_tts))
			{
				strcat(pResult[iCur].word_tts, pResult[i].word_tts);
			}			
			ivAssert(strlen(pResult[iCur].word_delpunc) < sizeof(pResult[iCur].word_delpunc));
			ivAssert(strlen(pResult[iCur].word_tts) < sizeof(pResult[iCur].word_tts));
		}
	}
	pResult[iCur].word[pResult[iCur].length] = '\0';
	pResult[iCur].type = type;

	memset(pResult + iCur + 1, 0, sizeof(friso_token) * (nNum - 1));
	memcpy(pResult + iCur + 1, pResult + iCur + nNum, sizeof(friso_token) * (pEngine->nResult - iCur - nNum));
	pEngine->nResult -= (nNum - 1);

	return TC_WSErr_SUCCESS;
}

//对friso结果进行二次处理，将数字和%/‰和之前的数字分到一起
int wordseg_process_mathsymbol(void *handle)
{
	int i;
	friso_token_t pResult = NULL;
	PWordSegEgn pEngine = NULL;
	if (NULL == handle)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	pResult = pEngine->pResult;
	for (i = 0; (pEngine->nResult > 2) && (i < pEngine->nResult - 2); i++)
	{
		int ret = 0;
		if (__LEX_OTHER_WORDS__ != pResult[i].type) //数字类别是otherwords
		{
			continue;
		}

		//如果 [i]是数字串，[i+1]='.', [i+2]是数字串，则进行合并
		if (!(string_is_numeric(pResult[i].word) &&
			  (1 == pResult[i + 1].length && '.' == pResult[i + 1].word[0]) &&
			  string_is_numeric(pResult[i + 2].word)))
		{
			continue;
		}

		ret = wordseg_merge_result(pEngine, i, 3, pResult[i].type, 0, 0);
		if (TC_WSErr_SUCCESS != ret)
		{
			return ret;
		}
	}

	return TC_WSErr_SUCCESS;
}

//对friso结果进行二次处理，和词典进行全匹配，以便实现一些特殊分词，如[清平乐·宫怨]
int ws_engine_dict_wholematch(PWordSegEgn pEngine)
{
	//分词原则，对于分词结果里的每项进行是否英文单词进行判断，如果是，则到英文词典匹配 （都转为小写进行匹配）
	char *szWords = NULL;
	int i, j, nWordsLen = 0;
	friso_token_t pResult = NULL;
	
	//耗时函数，分词太多，感觉乱扫的可能性更大，就不再处理
	if (pEngine->nResult > WORDSEG_POSTPROCESS_CNT_LIMIT)
	{
		return TC_WSErr_SUCCESS;
	}

	szWords = pEngine->pTmpBuf->szOrgWords;	
	pResult = pEngine->pResult;	
	for (i = 0; i < pEngine->nResult - 1; i++)
	{		
		int nPuncNum = 0;
		int bWholeWord = 0;
		lex_entry_cdt_t val = NULL;
		uint_t iMerge = 0;

		if (' ' == pResult[i].word[0] || __LEX_PUNCTUATION__ == pResult[i].type)
		{
			continue; //开头是空格或者符号的,认为词库中不存在，不进行查询
		}
				
		nWordsLen = 0;
		memcpy(szWords + nWordsLen, pResult[i].word, pResult[i].length);
		nWordsLen = pResult[i].length;
				
		for (j = i + 1; j < pEngine->nResult; j++)
		{
			int bAsciiStr = 0;
			lex_entry_cdt_t lex = NULL;
			memcpy(szWords + nWordsLen, pResult[j].word, pResult[j].length);
			nWordsLen += pResult[j].length;
			szWords[nWordsLen] = 0;

			if (__LEX_PUNCTUATION__ == pResult[j].type)
			{				
				nPuncNum++;
				continue; //结尾是符号的不查词典
			}

			if (' ' == szWords[nWordsLen - 1])
			{
				continue; //结尾是空格,认为词库中不存在，不进行查询
			}

			//效率优化  除了拼音/数字1th这种/带符号的，已经在friso中被分词到一起了，没必要再处理
			if (nWordsLen > (int)(pEngine->nWholeMatchMaxBytes) && 0 == nPuncNum)
			{
				break;
			}

			//效率优化：超过词典中最长词条，不需要查找了
			if (nWordsLen > (int)(pEngine->nLexMaxBytes))
			{
				break;
			}

			//效率优化：词条中符号太多，理论上不在词典中，不处理
			if (nPuncNum > 3) {
				break;
			}

			bAsciiStr = string_is_ascii(szWords);

#if !SUPPORT_PERFECT_MATCH
			//控制不可再分词的整词条：拼音、特殊分词1th-100th
			if (nWordsLen == (int)(pEngine->nTextLen))
			{
				if ((bAsciiStr && 0 == string_has_whitespace(szWords)) ||
					NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, szWords))
				{
					bWholeWord = 1; //复用改字段描述是否支持整词分到一起
				}
			}
#endif
			if (0 == i && j == pEngine->nResult - 1 && 0 == bWholeWord)
			{
				break;
			}

			if (utf8_string_is_cjk(szWords)) {
				continue;
			}
			//else if(string_is_ascii(szWords) /*|| string_is_en_words(szWords, 1)*/){
			//	lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_CHAIFEN_DICT__, __LEX_PINYIN_CHAIFEN_DICT__, szWords);
			//}
			else if(pEngine->nTextType != TEXT_TYPE_EN){
				if (string_has_cjk(szWords)) {
					lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TY_CN_WORDS__, __LEX_TY_CN_WORDS__, szWords);
				}
				//if (NULL == lex && string_has_pinyinwithtone(szWords))
				else
				{
					lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_CHAIFEN_DICT__, szWords);
				}
			}
			if (NULL != lex)
			{
				iMerge = j;
				val = lex;
			}
		}				 // for (j = i + 1; j < pEngine->nResult; j++)
		if (NULL != val) //词典中找到分词
		{
			int ret = 0;
			ret = wordseg_merge_result(pEngine, i, iMerge - i + 1, (friso_lex_t)(val->type), bWholeWord, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}

	} // for (i = 0; i < pEngine->nResult - 1; i++)

	return TC_WSErr_SUCCESS;
}

//设置分词结果里标点符号类型
int ws_engine_set_punc_type(PWordSegEgn pEngine)
{
	int i;
	friso_token_t pResult = NULL;
	
	pResult = pEngine->pResult;
	for (i = 0; i < pEngine->nResult; i++)
	{
		lex_entry_cdt_t val = NULL;

		if (pResult[i].type < __LEX_END || __LEX_DECIMAL__ == pResult[i].type || __LEX_INTEGER__ == pResult[i].type)
		{
			continue;
		}

		val = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pResult[i].word);
		if (NULL != val)
		{
			pResult[i].type = __LEX_PUNCTUATION__;
		}
	} // for (i = 0; i < pEngine->nResult; i++)

	return TC_WSErr_SUCCESS;
}

int ws_engine_set_result_offset(PWordSegEgn pEngine)
{
	short offset = 0;
	friso_token_t pResult = pEngine->pResult;
	for (int i = 0; i < pEngine->nResult; i++)
	{
		pResult[i].offset = offset;
		offset += pResult[i].length;
	}

	return TC_WSErr_SUCCESS;
}

//对friso结果进行二次处理，实现英文分词
int ws_engine_process_en(PWordSegEgn pEngine)
{
	//分词原则，对于分词结果里的每项进行是否英文单词进行判断，如果是，则到英文词典匹配 （都转为小写进行匹配）
	char *szWords = NULL, *szLowerWords = NULL;
	int i, nWordsLen = 0;
	friso_token_t pResult = NULL;
	
	if (pEngine->nTextType == TEXT_TYPE_CN ) {
		return TC_WSErr_SUCCESS;
	}

	szWords = pEngine->pTmpBuf->szOrgWords;
	szLowerWords = pEngine->pTmpBuf->szLowerWords;
	pResult = pEngine->pResult;
	
	int nLexWordMaxBytes = pEngine->pResHdr->hash_res_hdr->dic[__LEX_TY_EN_WORDS__].wordmaxbytes;
	
	ws_engine_set_result_offset(pEngine);

	for (i = 0; i < pEngine->nResult - 2; i++)
	{
		// int bHasWhiteSpace = 0;
		int nMergeNum = 0;		
		friso_lex_t type;

		uint_t j = 0;
				
		//if (__LEX_LETTER_OR_DIGIT__ != pResult[i].type)
		if (__LEX_OTHER_WORDS__ != pResult[i].type)
		{
			continue;
		}

#if TEST_FILTER_LEX_CHAR
		if (!string_is_in_enlex(pResult[i].word)) {
			continue;
		}
#endif

		//英文单词都归为__LEX_OTHER_WORDS__
		nWordsLen = pResult[i].length;

		j = i + 1;
		while ((int)j <= pEngine->nResult - 2 && 
			((__LEX_WHITESPACE__ == pResult[j].type && __LEX_OTHER_WORDS__ == pResult[j + 1].type) || (pResult[j].word[0]=='.' && pResult[j].word[1]==0)))
		{
#if TEST_FILTER_LEX_CHAR
			if (!string_is_in_enlex(pResult[j].word) || !string_is_in_enlex(pResult[j+1].word)) {
				break;
			}
#endif

			int str_len = nWordsLen + pResult[j].length + pResult[j+1].length;
#if !SUPPORT_PERFECT_MATCH
			if (str_len == (int)(pEngine->nTextLen))
			{
				break;
			}
#endif
			if (str_len > nLexWordMaxBytes)
			{
				break;
			}

			//当前可能是：“单词 + 空格 + 单词”，则到英文词典中搜索			
			nWordsLen = pResult[j+1].offset - pResult[i].offset + pResult[j+1].length;
			memcpy(szWords, pEngine->szText + pResult[i].offset, nWordsLen);
			szWords[nWordsLen] = 0;

			convert_letter_upper_to_lower(szWords); //都转为小写字母，英文词库也都是小写字母
			lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, szWords);
			if (NULL != lex)
			{				
				type = (friso_lex_t)(lex->type);
				nMergeNum = j + 1 - i + 1;
			}			
			j += 2;
		}

		if (nMergeNum > 0) //词典中找到分词
		{
			int ret = 0;
			ret = wordseg_merge_result(pEngine, i, nMergeNum, type, 0, 0);
			if (TC_WSErr_SUCCESS != ret)
			{
				return ret;
			}
		}
	}

	//将单独单词的type从__LEX_OTHER_WORDS__改成__LEX_TOYCLOUD_EN_WORDS__,
	//避免下面针对英语单词中空格的处理带来误伤，如“every day”不要被改成“everyday”
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_OTHER_WORDS__ != pResult[i].type)
		{
			continue;
		}
#if TEST_FILTER_LEX_CHAR
		if (!string_is_in_enlex(pResult[i].word)) {
			continue;
		}
#endif

		strcpy(szLowerWords, pResult[i].word);
		convert_letter_upper_to_lower(szLowerWords); //都转为小写字母，英文词库也都是小写字母
		lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, szLowerWords);
		if (NULL != lex)
		{
			pResult[i].type = __LEX_TY_EN_WORDS__;
		}
	}

	return TC_WSErr_SUCCESS;
}

//内部调试接口，查询szU8Text文本是否在词典中
int ws_engine_dict_match(PWordSegEgn pEngine, const char *szU8Text, int *pilex)
{	
	uint_t i;

	if (NULL == pEngine || NULL == szU8Text || NULL == pilex)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	*pilex = -1;
		
	for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)
	{
		lex_entry_cdt_t val = NULL;
		val = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, i, i, (fstring)szU8Text);
		if (NULL != val)
		{
			*pilex = i;
			break;
		}
	} // for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)

	return TC_WSErr_SUCCESS;
}

static int myRstWordIsEqual(friso_token_t pResult, char *str2)
{
	return (0 == strcmp(pResult->word_delpunc, str2));
}

//针对英文句子，以基本标点符号进行分句
int ws_engine_sentence_seg(PWordSegEgn pEngine, int *pbSuccess, text_attr_t * pTextAttr)
{
	int i;
	friso_token_t pResult = NULL, pCurResult = NULL;	
	int nRet = 0;

	int nDouquotationNum;
	int *pbDouquotaitonIdx = NULL; // double quotations
	int nResult = 0;

	if (NULL == pEngine || NULL == pbSuccess)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	*pbSuccess = 0;

	nResult = pEngine->nResult;

	/*-----------------判断文本类型，只有英文类型才进行分句处理---------------*/
	int nTextType = -1;
	if (NULL != pTextAttr) //已经获取到文本类型
	{
		nTextType = pTextAttr->text_type;
	}
	else
	{
		nRet = wordseg_get_texttype(pEngine, pEngine->szText, pEngine->pTextAttr);
		if (TC_WSErr_SUCCESS != nRet)
		{
			ivAssert(0);
			return nRet;
		}
		nTextType = pEngine->pTextAttr->text_type;
	}
	if (TEXT_TYPE_EN != nTextType) //非英文类型不进行分句处理
	{
		return TC_WSErr_SUCCESS;
	}

	pResult = pEngine->pResult;

	nDouquotationNum = 0;
	pbDouquotaitonIdx = pEngine->pTmpBuf->pbPinyin; //复用；
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (0 == strcmp(pResult[i].word_delpunc, "\""))
		{
			pbDouquotaitonIdx[nDouquotationNum] = i;
			nDouquotationNum++;
		}
	}
	pbDouquotaitonIdx[nDouquotationNum] = 0;

	//基于分词结果进行标点符号判断，避免针对类似11.5再次处理
#if 1
	int nBeginIdx = 0;
	int nSentenceCnt = 0; //分句个数
	int bDouquotationSplit = 0;
	for (i = 0; i < pEngine->nResult; i++)
	{
		pCurResult = pResult + i;
		char *pCurWord = pCurResult->word_delpunc;

		//分句句首空格 或者属于汉语词典的 还是作为单独分词
		if (i == nBeginIdx && (__LEX_WHITESPACE__ == pResult[i].type || pResult[i].type <= __LEX_TY_CN_WORDS__))
		{
			nBeginIdx++;
			nSentenceCnt++;
			continue;
		}

		if (__LEX_PUNCTUATION__ != pCurResult->type) //非符号类型
		{
			continue;
		}
		if (!string_is_en_sentence_endpunctuation(pCurWord) && 0 != strcmp(pCurWord, "\"")) //非句子结尾符号/双引号
		{
			continue;
		}
		if (i - nBeginIdx + 1 == pEngine->nResult)
		{
			break; //若整句是一个分句，则以分词结果为准
		}

#if 1 //针对双引号单独处理
		if (0 == strcmp(pCurWord, "\"") && i > 0 &&
			(bDouquotationSplit ||
			 (i > 1 && __LEX_PUNCTUATION__ == pResult[i - 1].type) ||
			 (i > 2 && __LEX_PUNCTUATION__ == pResult[i - 2].type && 0 == strcmp(pResult[i - 1].word_delpunc, " "))))
		{
			int nMergeNum = i - nBeginIdx;
			int nMergeNumMax = 0;
			if (nMergeNum > 0)
			{
				wordseg_getmerge_max_num(pEngine, nBeginIdx, nMergeNum, (uint_t *)(&nMergeNumMax));
				if (nMergeNumMax < nMergeNum)
				{
					nMergeNum = nMergeNumMax;
				}
				nRet = wordseg_merge_result(pEngine, nBeginIdx, nMergeNum, __LEX_EN_SENTENCE__, 0, 1);
				if (TC_WSErr_SUCCESS != nRet)
				{
					return nRet;
				}
				nSentenceCnt++;

				//双引号作为一个单独分词
				nSentenceCnt++;
				i = nBeginIdx + 1;
				nBeginIdx = i + 1;
				if (0 == bDouquotationSplit)
				{
					bDouquotationSplit = 1; //记录下一次的双引号需要分开
				}
				else
				{
					bDouquotationSplit = 0;
				}
			}
			else
			{
				nSentenceCnt++;
				i = nBeginIdx;
				nBeginIdx = i + 1;
			}

			continue;
		}

		if (0 == strcmp(pCurWord, "\""))
		{
			continue;
		}
#endif

#if 1
		if (pEngine->bInnerPostProcess)
		{
			//特殊处理：当前是句号.，看前一个分词是否是特殊情况
			if ((i >= 1) && myRstWordIsEqual(pResult + i, ".")
				/*&& (i < pEngine->nResult - 1) && !myRstWordIsEqual(pResult + i + 1, " ")*/)
			{
				if (myRstWordIsEqual(pResult + i - 1, "www") ||
					myRstWordIsEqual(pResult + i - 1, "Ms") ||
					myRstWordIsEqual(pResult + i - 1, "Mr") ||
					myRstWordIsEqual(pResult + i - 1, "Mrs") ||
					myRstWordIsEqual(pResult + i - 1, "Miss"))
				{
					continue;
				}
			}

			//特殊处理：当前是句号.，看后一个分词是否是特殊情况
			if (i + 1 < pEngine->nResult && myRstWordIsEqual(pResult + i, "."))
			{
				if (myRstWordIsEqual(pResult + i + 1, "com") ||
					myRstWordIsEqual(pResult + i + 1, "cn") ||
					myRstWordIsEqual(pResult + i + 1, "net"))
				{
					continue;
				}
			}

			//特殊处理：针对省略号...进行处理，不作为分句处
			if ((i + 3 < pEngine->nResult) &&
				myRstWordIsEqual(pResult + i, ".") &&
				myRstWordIsEqual(pResult + i + 1, ".") &&
				myRstWordIsEqual(pResult + i + 2, ".") &&
				__LEX_PUNCTUATION__ != pResult[i + 3].type)
			{
				i += 2;
				continue;
			}
		} // if (pEngine->bInnerPostProcess)
#endif

		int nMergeNum = i - nBeginIdx + 1;
		int nMergeNumMax = 0;
		wordseg_getmerge_max_num(pEngine, nBeginIdx, nMergeNum,(uint_t *)( &nMergeNumMax));
		if (nMergeNumMax < nMergeNum)
		{
			nMergeNum = nMergeNumMax;
		}
		nRet = wordseg_merge_result(pEngine, nBeginIdx, nMergeNum, __LEX_EN_SENTENCE__, 0, 1);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}

		nSentenceCnt++;
		i = nBeginIdx;
		nBeginIdx = i + 1;
	} // for (int i = 0; i < pEngine->nResult; i++)

	if (nSentenceCnt > 0 && nBeginIdx < (int)(pEngine->nResult - 1))
	{
		int nMergeNum = (int)(pEngine->nResult) - nBeginIdx;
		int nMergeNumMax = 0;
		wordseg_getmerge_max_num(pEngine, nBeginIdx, nMergeNum, (uint_t *)(&nMergeNumMax));
		if (nMergeNumMax < nMergeNum)
		{
			nMergeNum = nMergeNumMax;
		}

		nRet = wordseg_merge_result(pEngine, nBeginIdx, nMergeNum, __LEX_EN_SENTENCE__, 0, 1);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}

		nSentenceCnt++;
	}
#endif

	//当分句修改了原有的分词结果后，原来解析到的有效分词可能位置异常，直接设置无有效分词
	if (nResult != pEngine->nResult && pTextAttr->valid_wordseg_id >= 0)
	{
		pTextAttr->valid_wordseg_id = -1;
	}
	return TC_WSErr_SUCCESS;
}

static int tts_en_words_to_lower(PWordSegEgn pEngine, char *pOutputText, int nOutputTextSize)
{
	if (NULL == pEngine || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	char *pBuffer = (char *)(pEngine->pTmpBuf->szOrgWords);	  //复用
	char *pBuffer2 = (char *)(pEngine->pTmpBuf->szLowerWords); //复用

#if 1 //针对有些单词大写后TTS默认按照字母读了，除了p_en_abbreviation.lex里专有缩写名词外，其他均转为小写字母 2021.9.13
	int nLen = 0, nBegOffset = 0, nCurOffset = 0;
	while (nCurOffset < (int)(strlen(pOutputText)))
	{
		//简单分词:遇到非空格的asscii码就作为一个独立词条，检测是否在p_en_abbreviation.lex里
		int nBytes = get_utf8_bytes(pOutputText[nCurOffset]);
		if (nBytes > 1)
		{
			convert_letter_upper_to_lower_ex(pOutputText + nBegOffset, nLen);
			nLen = 0;
			nBegOffset = nCurOffset + nBytes;
		}
		else if (' ' == pOutputText[nCurOffset] || '.' == pOutputText[nCurOffset] ||
				 ',' == pOutputText[nCurOffset] || '?' == pOutputText[nCurOffset] ||
				 '!' == pOutputText[nCurOffset] || nCurOffset == (int)(strlen(pOutputText) - 1))
		{
			lex_entry_cdt_t lex;
			if (nCurOffset == (int)(strlen(pOutputText) - 1) && !is_en_punctuation(FRISO_UTF8, pOutputText[nCurOffset]))
			{
				nLen++;
			}
			memcpy(pBuffer, pOutputText + nBegOffset, nLen);
			pBuffer[nLen] = 0;			
			strcpy(pBuffer2, pBuffer);
			convert_letter_upper_to_lower(pBuffer2);
			lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TTS_USER_DICT__, __LEX_TTS_USER_DICT__, pBuffer2); //
			if (NULL != lex)
			{
				//英文单词标注
				int nDiff = strlen(lex->word_new) - nLen;
				for (int k = strlen(pOutputText) - 1; k >= nBegOffset + nLen; k--)
				{
					pOutputText[k + nDiff] = pOutputText[k];
				}
				memcpy(pOutputText + nBegOffset, lex->word_new, strlen(lex->word_new));
				nCurOffset += (nDiff + nBytes);
				nBegOffset = nCurOffset;
				nLen = 0;
				continue;
			}

			lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_ABBREVIATION__, __LEX_TY_EN_ABBREVIATION__, pBuffer); //
			if (NULL == lex)
			{
				convert_letter_upper_to_lower_ex(pOutputText + nBegOffset, nLen);
			}
			nLen = 0;
			nBegOffset = nCurOffset + nBytes;
		}
		else
		{
			nLen += nBytes;
		}

		nCurOffset += nBytes;
	}

#endif

	return TC_WSErr_SUCCESS;
}

static int tts_tag_pinyin(PWordSegEgn pEngine, char *pOutputText, int nOutputTextSize)
{
	lex_entry_cdt_t pTag = NULL;

	if (NULL == pEngine || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	char *pTmpBuf = NULL;
	char *pMallocBuf = NULL;
	if (strlen(pOutputText) < sizeof(pEngine->pTmpBuf->pCornerBuf)) {
		pTmpBuf = (char *)(pEngine->pTmpBuf->pCornerBuf);
	}
	else {
		pMallocBuf = (char *)FRISO_MALLOC(strlen(pOutputText));
		if (NULL == pMallocBuf)
		{
			return TC_WSErrID_OutOfMemory;
		}
		pTmpBuf = pMallocBuf;
	}

	char *pBuffer = pEngine->pTmpBuf->szLowerWords; //复用
	int nBufLen = 0;
	int nOffset = 0;

	int bHasTonePinyin = 0; //是否含有带调拼音
#if 1						//确认是否含有带调拼音
	nOffset = 0;
	while (nOffset < (int)(strlen(pOutputText)))
	{
		int len = get_utf8_bytes(pOutputText[nOffset]);

		if ((1 == len && ']' == pOutputText[nOffset]) ||
			(len > 1 && utf8_char_is_cjk(pOutputText + nOffset))) // CJK字符 不管
		{
			nBufLen = 0;
			pBuffer[0] = 0;
			nOffset += len;
			continue;
		}

		if (1 == len && ' ' == pOutputText[nOffset] && strlen(pBuffer) > 0)
		{
			pBuffer[nBufLen] = 0;
			//使用空格打开的一个分词，查看是否是拼音
			pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer);
			if (NULL != pTag)
			{
				bHasTonePinyin = 1;
				break;
			}
		}
		else
		{
			memcpy(pBuffer + nBufLen, pOutputText + nOffset, len);
			nBufLen += len;
		}
		nOffset += len;
	}
#endif

	pBuffer[0] = 0;
	nBufLen = 0;
	nOffset = 0;
	while (nOffset < (int)(strlen(pOutputText)))
	{
		int len = get_utf8_bytes(pOutputText[nOffset]);

		if ((1 == len && ']' == pOutputText[nOffset]) ||
			(len > 1 && utf8_char_is_cjk(pOutputText + nOffset))) // CJK字符 不管
		{
			nBufLen = 0;
			pBuffer[0] = 0;
			nOffset += len;
			continue;
		}

		int bBlank = (1 == len && ' ' == pOutputText[nOffset]) ? 1 : 0;
		if (bBlank && strlen(pBuffer) > 0)
		{
			pBuffer[nBufLen] = 0;

			//使用空格打开的一个分词，查看是否是拼音
			pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer);
			if (NULL == pTag && bHasTonePinyin) //只要存在带调拼音，则无调拼音一起处理
			{
				pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_SOFT_DICT__, __LEX_PINYIN_SOFT_DICT__, pBuffer);
			}
			if (NULL != pTag && (nOutputTextSize > (int)(strlen(pOutputText) + strlen(pTag->word_new))))
			{
				strcpy(pTmpBuf, pOutputText + nOffset + 1);
				strcpy(pOutputText + nOffset - strlen(pBuffer), pTag->word_new);
				nOffset = (int)(strlen(pOutputText));
				// strcat(pOutputText, " ");
				strcat(pOutputText, pTmpBuf);
				pBuffer[0] = 0;
				nBufLen = 0;
				continue;
			}
			nBufLen = 0;
		}
		else if (!bBlank)
		{
			memcpy(pBuffer + nBufLen, pOutputText + nOffset, len);
			nBufLen += len;
		}
		nOffset += len;
	}
	if (nBufLen > 0)
	{
		pBuffer[nBufLen] = 0;
		//使用空格打开的一个分词，查看是否是拼音
		pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer);
		if (NULL == pTag && bHasTonePinyin) //只要存在带调拼音，则无调拼音一起处理
		{
			pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_SOFT_DICT__, __LEX_PINYIN_SOFT_DICT__, pBuffer);
		}
		if (NULL != pTag && (nOutputTextSize > (int)(strlen(pOutputText) + strlen(pTag->word_new))))
		{
			strcpy(pOutputText + nOffset - strlen(pBuffer), pTag->word_new);
		}
	}

	if (NULL != pMallocBuf) {
		FRISO_FREE(pMallocBuf);
	}

	return TC_WSErr_SUCCESS;
}

//针对输入的UTF8字符串，查询tts标注词典，修改为标注格式
int tts_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize)
{
	short *pnU8Offset = NULL;
	char *pBuffer = NULL, *newword = NULL;
	int nRet = 0;
	friso_hash_cdt_t pDic = NULL;
	if (NULL == pEngine || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	if ((unsigned int)pInputText == (unsigned int)pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}
	if (nOutputTextSize <= (int)(strlen(pInputText)))
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	pDic = pEngine->pResHdr->hash_res_hdr->dic + __LEX_TTS_USER_DICT__;
	if(0 == pDic->size)	
	{
		return TC_WSErr_SUCCESS;
	}

	pnU8Offset = (short *)(pEngine->pTmpBuf->pnCornerMarkID);
	ivAssert(sizeof(pEngine->pTmpBuf->pnCornerMarkID) >= sizeof(short) * WS_INPUT_TEXT_MAX_LEN);
	memset(pnU8Offset, 0, sizeof(pEngine->pTmpBuf->pnCornerMarkID));
	pBuffer = (char *)(pEngine->pTmpBuf->szOrgWords); //复用
	memset(pOutputText, 0, nOutputTextSize);
	newword = pEngine->pTmpBuf->szLowerWords; //复用

	//统计输入字符串中每个UTF8字节数
	int nCharNum = 0;
	int nMaxLen = pDic->wordmaxbytes;
	short nOffset = 0;
	while (nOffset < (short)(strlen(pInputText)))
	{
		int nBytes = get_utf8_bytes(pInputText[nOffset]);
		pnU8Offset[nCharNum] = nOffset;
		nCharNum++;
		nOffset += (short)nBytes;
	}
	pnU8Offset[nCharNum] = nOffset;

	int nNewStrLen = 0;
	for (int i = 0; i < nCharNum; i++)
	{
		int nLen, nBytes;
		lex_entry_cdt_t pTagTmp = NULL;
		int jj = 0;

		newword[0] = 0;

		nBytes = pnU8Offset[i + 1] - pnU8Offset[i];
		if (3 != nBytes || (i + 2 < nCharNum && 0 == strncmp(pInputText + pnU8Offset[i + 1], "[=", 2)))
		{
			//非汉字或者后面是[=本身就可能是tts标注，则跳过
			if (nNewStrLen + nBytes >= nOutputTextSize)
			{
				strcpy(pOutputText, pInputText);
				ivAssert(0);
				return TC_WSErr_SUCCESS;
			}
			memcpy(pOutputText + nNewStrLen, pInputText + pnU8Offset[i], nBytes);
			nNewStrLen += nBytes;
			continue;
		}
				
		memcpy(pBuffer, pInputText + pnU8Offset[i], nBytes);
		pBuffer[nBytes] = 0;
		pTagTmp = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pBuffer);
		if (NULL != pTagTmp)
		{
			memcpy(pOutputText + nNewStrLen, pInputText + pnU8Offset[i], nBytes);
			nNewStrLen += nBytes;
			continue;
		}

		pTagTmp = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TTS_USER_DICT__, __LEX_TTS_USER_DICT__, pBuffer);
		if (NULL != pTagTmp)
		{
			strcpy(newword, pTagTmp->word_new);
			jj = i;
		}

		for (int j = i + 1; j < nCharNum; j++)
		{
			int nBytesTmp = pnU8Offset[j + 1] - pnU8Offset[j];
			nLen = pnU8Offset[j] - pnU8Offset[i] + nBytesTmp;
			if (3 != nBytesTmp || nLen > nMaxLen || (j + 2 < nCharNum && 0 == strncmp(pInputText + pnU8Offset[j + 1], "[=", 2))) //非汉字 或者 文本长度查过词典词条，跳过
			{
				break;
			}
			
			//减少资源读取次数
			memcpy(pBuffer, pInputText + pnU8Offset[j], nBytesTmp);
			pBuffer[nBytesTmp] = 0;
			if (NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pBuffer)) {
				continue;
			}

			memcpy(pBuffer, pInputText + pnU8Offset[i], nLen);
			pBuffer[nLen] = 0;
			pTagTmp = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TTS_USER_DICT__, __LEX_TTS_USER_DICT__, pBuffer);
			if (NULL != pTagTmp)
			{
				strcpy(newword, pTagTmp->word_new);				
				jj = j;
			}
		} // for (int j = i+1; j < nCharNum; j++)

		if (0 == newword[0]) //当前位置朝后的字符串序列都没找到tts发音标注
		{
			if (nNewStrLen + nBytes >= nOutputTextSize)
			{
				strcpy(pOutputText, pInputText);
				ivAssert(0);
				return TC_WSErr_SUCCESS;
			}
			memcpy(pOutputText + nNewStrLen, pInputText + pnU8Offset[i], nBytes);
			nNewStrLen += nBytes;
		}
		else
		{
			int newword_len = strlen(newword);
			if (nNewStrLen + newword_len >= nOutputTextSize)
			{
				strcpy(pOutputText, pInputText);
				ivAssert(0);
				return TC_WSErr_SUCCESS;
			}
			memcpy(pOutputText + nNewStrLen, newword, newword_len);
			nNewStrLen += newword_len;
			i = jj;
		}
	}

	// 1、针对有些单词大写后TTS默认按照字母读了，除了p_en_abbreviation.lex里专有缩写名词外，其他均转为小写字母 2021.9.13
	// 2、针对p_tts_dict.lex里的英文单词进行发音维护
	nRet = tts_en_words_to_lower(pEngine, pOutputText, nOutputTextSize);
	if (0 != nRet)
	{
		return nRet;
	}

	//针对汉语拼音进行标注处理
	nRet = tts_tag_pinyin(pEngine, pOutputText, nOutputTextSize);
	if (0 != nRet)
	{
		return nRet;
	}

	//单独处理“AI" 和 “D1"错成"DI"的问题
	const char ai_str[][64] = {
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x61, 0x69, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4]ai词典笔d1
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x20, 0x61, 0x69, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4] ai词典笔d1
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x20, 0x61, 0x69, 0x20, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4] ai 词典笔d1		
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x61, 0x69, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x69, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4]ai词典笔di
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x20, 0x61, 0x69, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x69, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4] ai词典笔di
		0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x20, 0x61, 0x69, 0x20, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x69, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4] ai 词典笔di		
	};
	const char ai_str_new[64] = { 0xe9, 0x98, 0xbf, 0x5b, 0x3d, 0x61, 0x31, 0x5d, 0xe5, 0xb0, 0x94, 0x5b, 0x3d, 0x65, 0x72, 0x33, 0x5d, 0xe6, 0xb3, 0x95, 0x5b, 0x3d, 0x66, 0x61, 0x33, 0x5d, 0xe8, 0x9b, 0x8b, 0x5b, 0x3d, 0x64, 0x61, 0x6e, 0x34, 0x5d, 0x20, 0x61, 0x20, 0x69, 0x20, 0xe8, 0xaf, 0x8d, 0xe5, 0x85, 0xb8, 0xe7, 0xac, 0x94, 0x64, 0x31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }; //阿[=a1]尔[=er3]法[=fa3]蛋[=dan4] a i 词典笔d1
	for (int i = 0; i < sizeof(ai_str) / sizeof(ai_str[0]); i++)
	{
		if (0 == strcmp(pOutputText, ai_str[i]) && nOutputTextSize > strlen(ai_str_new)) {
			strcpy(pOutputText, ai_str_new);
			break;
		}		
	}

	return TC_WSErr_SUCCESS;
}

//扩展接口，单独开放TTS发音标注接口
int wordseg_tts_userdict_ex(void *handle, const char *pInputText, char *pOutputText, int nOutputTextSize)
{
	PWordSegEgn pEngine = NULL;
	int nRet = 0;

	if (NULL == handle || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	nRet = tts_userdict_match(pEngine, (char *)pInputText, pOutputText, nOutputTextSize);
	if (0 != nRet)
	{
		ivAssert(0);
		return nRet;
	}

	return TC_WSErr_SUCCESS;
}

#if 0
/*
 * 功能:针对分词结果里的翻译文本，进行是否全符号的判断，应用侧需求（以便针对异常数据不展示翻译结果）
 */
int ws_engine_set_transwords_type(PWordSegEgn pEngine)
{
	if (NULL == pEngine)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	friso_token_t pResult = pEngine->pResult;
	for (int i = 0; i < pEngine->nResult; i++)
	{
		pResult[i].bWordTransAllPunc = string_is_all_punctuation(pEngine, pResult[i].word_trans);
	}

	return TC_WSErr_SUCCESS;
}
#endif

//进行基础分词：中日韩字、符号、以空格打开的单词
int wordseg_get_min_unit_word(PWordSegEgn pEngine, const char *szU8Text)
{
	int i;
	if (NULL == pEngine || NULL == szU8Text)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	char *pBuffer = pEngine->pTmpBuf->szLowerWords; //复用下
	friso_token_t pResult = pEngine->pResult;
	int iResult = 0;
	memset(pEngine->pResult, 0, pEngine->nResultSize);

	for (i = 0; i < WS_TOKEN_MAX_NUM; i++)
	{
		pResult[i].type = __LEX_END;
	}

	i = 0;
	while (i < (int)(strlen(szU8Text)))
	{
		int len = get_utf8_bytes(szU8Text[i]);
		memcpy(pBuffer, szU8Text + i, len);
		pBuffer[len] = 0;

		if (iResult >= WS_TOKEN_MAX_NUM - 1) {
			//ivAssert(0);
			break;
		}

		/* 判断是否是符号 */
		if (NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pBuffer))
		{
			//当前是一个符号，作为一个分词
			if (0 != pResult[iResult].word[0])
				iResult++;			
			memcpy(pResult[iResult].word, szU8Text + i, len);
			pResult[iResult].type = __LEX_PUNCTUATION__;
			iResult++;
			i += len;
			continue;
		}

		/* 判断是否是空格 */
		if (0 == strcmp(pBuffer, " "))
		{
			if (0 != pResult[iResult].word[0])
				iResult++;
			memcpy(pResult[iResult].word, szU8Text + i, len);
			pResult[iResult].type = __LEX_WHITESPACE__;
			iResult++;
			i += len;
			continue;
		}

		if (1 == len) // ascii
		{
			memcpy(pResult[iResult].word + strlen(pResult[iResult].word), szU8Text + i, len);
			i += len;
			continue;
		}

		int ret = utf8_char_is_cjk((char *)(szU8Text + i));
		/* step1: 判断是否是CJK字符 */
		if (ret > 0 || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CN_CHAR__, __LEX_CN_CHAR__, pBuffer) ||
			NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_JAPANESR_CHAR__, __LEX_JAPANESR_CHAR__, pBuffer))
		{
			//当前是一个CJK字符，作为一个分词
			if (0 != pResult[iResult].word[0])
				iResult++;
			memcpy(pResult[iResult].word, szU8Text + i, len);
			pResult[iResult].type = __LEX_CJK_WORDS__;

			if (FRISO_CJK_CHK_C == ret || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CN_CHAR__, __LEX_CN_CHAR__, pBuffer)) //中文字
			{
				pResult[iResult].type = __LEX_CN_CHAR__;
			}

			iResult++;
			i += len;
			continue;
		}

		/* step4 可能是英西俄字符，拷贝，直到上述的遇到空格或者符号等，就会开一个新分词 */
		memcpy(pResult[iResult].word + strlen(pResult[iResult].word), szU8Text + i, len);
		i += len;
	}

	if (0 != pResult[iResult].word[0])
	{
		int ret = utf8_char_is_cjk((char *)(pResult[iResult].word));
		/* step1: 判断是否是CJK字符 */
		if (ret > 0 || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CN_CHAR__, __LEX_CN_CHAR__, pResult[iResult].word) ||
			NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_JAPANESR_CHAR__, __LEX_JAPANESR_CHAR__, pResult[iResult].word))
		{
			//当前是一个CJK字符，作为一个分词
			pResult[iResult].type = __LEX_CJK_WORDS__;

			if (FRISO_CJK_CHK_C == ret || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CN_CHAR__, __LEX_CN_CHAR__, pResult[iResult].word)) //中文字
			{
				pResult[iResult].type = __LEX_CN_CHAR__;
			}
		}

		iResult++;
	}

	pEngine->nResult = iResult;
	for (i = 0; i < pEngine->nResult; i++)
	{
		lex_entry_cdt_t pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pResult[i].word);
		if (NULL != pTag)
		{
			pResult[i].type = __LEX_PINYIN__;
		}
	}
	return TC_WSErr_SUCCESS;
}

/*
 *
 */
/**
 * 功能：词典笔多国语需求，针对日韩西俄文本进行分词和语种判断
 * @note
 * @param  *file:
 * @retval
 */
/*FRISO_API*/ int ws_engine_do_otherlanguage(PWordSegEgn pEngine, const char *szU8Text, text_attr_t * pTextAttr)
{	
	friso_token *pResult = NULL;
	int iCurLanuage = -1;
	int i = 0;
	int bPuncAndWhiteSpace = 1;
	int bHasOtherLanguageChar = 0; 
	int ret = 0;

	if (NULL == pEngine || NULL == szU8Text)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	if (strlen(szU8Text) > WS_INPUT_TEXT_MAX_LEN)
	{
		ivAssert(0);
		return TC_WSErrID_TextTooLong;
	}

	if (0 == szU8Text[0])
	{
		ivAssert(0);
		return TC_WSErrID_TextNull;
	}

	iCurLanuage = pEngine->nPriorLanguage; //优先设置为用户设置的语种
	pEngine->nResult = 0;
	pResult = pEngine->pResult;

#if 0
	//数字串语种和设置的多语种一致
	if (utf8_decimal_string((const fstring)szU8Text) || utf8_numeric_string((const fstring)szU8Text))
	{
		pEngine->nResult = 1;
		strcpy(pResult[0].word, szU8Text);
		pResult[0].type = __LEX_NUMERIC__;		
		goto wordseg_do_otherlanguage_end;
	}
#endif

	//先使用基本的分词原则：中日韩使用utf8字作为单位，英西俄以空格分隔的单词为单位，符号空格等不加入计数器	
	memset(pEngine->pnLanguageCnt, 0, sizeof(pEngine->pnLanguageCnt));

#if LOG_MODULE_COST_TIME
	int t1 = GetCostTimeMs();
#endif
	//进行最小单元分词
	ret = wordseg_get_min_unit_word(pEngine, szU8Text);
	if (0 != ret)
	{
		return ret;
	}
#if LOG_MODULE_COST_TIME
	int t2 = GetCostTimeMs();
	fprintf(g_fpCostTime, "\t024-1 [wordseg_get_min_unit_word] cost time = %d\r\n", t2 - t1);
	t1 = GetCostTimeMs();
#endif

	//判断文本是否存在非符号、中文、数字、字母以外的其他字符，若存在就用云端OCR结果，若不存在就用本地OCR结果
	bHasOtherLanguageChar = 0;
	for (i = 0; i < pEngine->nResult; i++)
	{
		if (__LEX_CN_CHAR__ == pResult[i].type || __LEX_PUNCTUATION__ == pResult[i].type || __LEX_WHITESPACE__ == pResult[i].type)
		{
			continue;
		}

		if (string_is_ascii(pResult[i].word))
			continue;

		if (__LEX_PINYIN__ == pResult[i].type)
			continue;

		bHasOtherLanguageChar = 1;
		break;
	}
#if LOG_MODULE_COST_TIME
	t2 = GetCostTimeMs();
	fprintf(g_fpCostTime, "\t024-2 [check bHasOtherLanguageChar] cost time = %d\r\n", t2 - t1);
	t1 = GetCostTimeMs();
#endif

	for (i = 0; i < pEngine->nResult; i++)
	{
		//统计是否全是空格和符号
		if (__LEX_PUNCTUATION__ != pResult[i].type && __LEX_WHITESPACE__ != pResult[i].type)
		{
			bPuncAndWhiteSpace = 0;
		}

		/* step1: 判断是否是CJK字符 */
		if (__LEX_CJK_WORDS__ == pResult[i].type || __LEX_CN_CHAR__ == pResult[i].type)
		{
			ret = utf8_char_is_cjk((char *)pResult[i].word);
			if (FRISO_CJK_CHK_C == ret || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_CN_CHAR__, __LEX_CN_CHAR__, pResult[i].word)) //中文字
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_CN] += 10;
				pResult[i].type = __LEX_CN_CHAR__;
			}
			else if (FRISO_CJK_CHK_K == ret) //韩文字
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_KOREAN] += 10;
				pResult[i].type = __LEX_KOREAN_CHAR__;
			}

			//特殊处理，如果是 中/日重复字，优先将type标记为日文
			if (FRISO_CJK_CHK_J == ret || NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_JAPANESR_CHAR__, __LEX_JAPANESR_CHAR__, pResult[i].word)) //日文字
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_JAPANESE] += 10;
				pResult[i].type = __LEX_JAPANESR_CHAR__;
			}
		}
		if (__LEX_PINYIN__ == pResult[i].type)
		{
			pEngine->pnLanguageCnt[TEXT_TYPE_CN] += 10; //如果是带调拼音，中文+10
		}
		if (__LEX_END == pResult[i].type || __LEX_PINYIN__ == pResult[i].type) ////对于不确定类型的这里进行判断，主要是英西俄类型
		{
			char *pTmp = pResult[0].word_delpunc; //复用
			strcpy(pTmp, pResult[i].word);
			string_to_lowcase_letter(pTmp, pTmp);
			// if (NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, pTmp))
			//{
			//	pEngine->pnLanguageCnt[TEXT_TYPE_EN] += 5; //如果是数据库中的英文单词，则+5
			// }
			if (string_is_en_words(pTmp, 0))
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_EN] += 10; //是由数字和字母组成的英文单词，+10
			}
			if (utf8str_char_is_all_in_dict(pEngine, pTmp, __LEX_RUSSIAN_CHAR__))
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_RUSSIAN] += 10; //是由俄文字符组成的单词， +10
				pResult[i].type = TEXT_TYPE_RUSSIAN;
			}
			if (utf8str_char_is_all_in_dict(pEngine, pTmp, __LEX_SPANISH_CHAR__))
			{
				pEngine->pnLanguageCnt[TEXT_TYPE_SPANISH] += 10; //是由西班牙字符组成的单词， +10
				pResult[i].type = TEXT_TYPE_SPANISH;
			}
		}
	}

	int nMaxCnt = pEngine->pnLanguageCnt[iCurLanuage];
	for (i = 0; i < sizeof(pEngine->pnLanguageCnt) / sizeof(pEngine->pnLanguageCnt[0]); i++)
	{
		if (pEngine->pnLanguageCnt[i] > nMaxCnt)
		{
			iCurLanuage = i;
			nMaxCnt = pEngine->pnLanguageCnt[i];
		}
	}

	//中日模式下，改为中文优先 20220401 【产品需求，解决日中翻译很多UNK1异常问题】
	if (TEXT_TYPE_JAPANESE == iCurLanuage && pEngine->pnLanguageCnt[TEXT_TYPE_CN] == pEngine->pnLanguageCnt[TEXT_TYPE_JAPANESE])
	{
		iCurLanuage = TEXT_TYPE_CN;
	}

	//针对输入全是符号或者空格的情况，进行处理
	if (bPuncAndWhiteSpace)
	{
		iCurLanuage = TEXT_TYPE_PUNC;
	}
#if LOG_MODULE_COST_TIME
	t2 = GetCostTimeMs();
	fprintf(g_fpCostTime, "\t024-3 [check language] cost time = %d\r\n", t2 - t1);
	t1 = GetCostTimeMs();
#endif

	/* 需要根据判断出是其他语种，重新对每个分词的类型做下修正，如果判断的是中英，还是会重新调用下原有的分词策略，不需要修正
		中/日存在重复字，英/西/俄都存在用26个英文字母组成的单词，
		比如判断出text_type=TEXT_TYPE_SPANISH 分词“Que”上面给的type可能是TEXT_TYPE_EN,现在需要修正为TEXT_TYPE_SPANISH
	*/
	if (iCurLanuage == TEXT_TYPE_SPANISH || iCurLanuage == TEXT_TYPE_RUSSIAN)
	{
		for (i = 0; i < pEngine->nResult; i++)
		{
			if (string_is_ascii(pResult[i].word))
			{
				pResult[i].type = (uchar_t)iCurLanuage;
			}
		}
	}

//wordseg_do_otherlanguage_end:
	for (i = 0; i < pEngine->nResult; i++)
	{
		strcpy(pResult[i].word_delpunc, pResult[i].word);
		strcpy(pResult[i].word_tts, pResult[i].word);
		pResult[i].length = (uchar_t)(strlen(pResult[i].word));
		pResult[i].offset = ((i == 0) ? 0 : pResult[i - 1].offset + strlen(pResult[i - 1].word));
	}

	if (NULL != pTextAttr)
	{
		pTextAttr->text_type = iCurLanuage;
		pTextAttr->bUseLocalOcr = bHasOtherLanguageChar ? 0 : 1;
		if (pTextAttr->text_type <= TEXT_TYPE_PUNC)
		{
			pTextAttr->bUseLocalOcr = 1;
		}
		pTextAttr->bTextModified = 0;
		pTextAttr->valid_wordseg_id = -1;
		strcpy(pTextAttr->word_delpunc, szU8Text);
		strcpy(pTextAttr->word_tts, szU8Text);
	}
	pEngine->pTextAttr->text_type = iCurLanuage;

#if LOG_MODULE_COST_TIME
	t2 = GetCostTimeMs();
	fprintf(g_fpCostTime, "\t024-4 [other process] cost time = %d\r\n", t2 - t1);
	t1 = GetCostTimeMs();
#endif

	return TC_WSErr_SUCCESS;
}

/* 功能：判断当前utf8字符串的所有字符是否都在指定词典里 */
static int utf8str_char_is_all_in_dict(PWordSegEgn pEngine, char *pStr, int nDicID)
{
	int i;
	char *pBuffer = NULL;

	i = 0;
	pBuffer = pEngine->pTmpBuf->szLowerWords;
	while (0 != pStr[i])
	{
		int len = get_utf8_bytes(pStr[i]);
		memcpy(pBuffer, pStr + i, len);
		pBuffer[len] = 0;
		if (NULL == friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, nDicID, nDicID, pBuffer) &&
			!(2 == len && 0xcc == (unsigned char)pBuffer[0] && 0x81 == (unsigned char)pBuffer[1]))
		{
			return 0;
		}

		i += len;
	}

	return 1;
}

#if LOG_MODULE_COST_TIME
int GetCostTimeMs()
{
#ifdef WIN32
	clock_t t = clock();
	return (int)t;
#elif 1
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return (int)(tv.tv_sec * 1000 + tv.tv_nsec / (1000 * 1000));
#else
	return xTaskGetTickCount();
#endif
}
#endif

//针对输入的UTF8字符串，查询评测标注词典，修改为标注格式，以便解决评测引擎报错的一些生僻字、多发音拼音错误等问题
int ise_userdict_match(PWordSegEgn pEngine, text_language_type szTextType, char *pInputText, char *pOutputText, int nOutputTextSize)
{
	if (NULL == pEngine || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	if ((unsigned int)pInputText == (unsigned int)pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}
	if (nOutputTextSize <= (int)(strlen(pInputText)))
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	friso_hash_cdt_t pDic = pEngine->pResHdr->hash_res_hdr->dic;
	if (TEXT_TYPE_CN == szTextType && pDic[__LEX_ISE_CN_USER_DICT__].size > 0)
	{
		return ise_cn_userdict_match(pEngine, pInputText, pOutputText, nOutputTextSize);
	}
	else if (TEXT_TYPE_EN == szTextType && pDic[__LEX_ISE_EN_USER_DICT__].size > 0)
	{
		return ise_en_userdict_match(pEngine, pInputText, pOutputText, nOutputTextSize);
	}
	else
	{
		// ivAssert(0);
		strcpy(pOutputText, pInputText);
	}

	return TC_WSErr_SUCCESS;
}

//针对输入的UTF8字符串，查询评测标注词典，修改为标注格式，以便解决评测引擎报错的一些生僻字、多发音拼音错误等问题
static int ise_en_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize)
{
	int nRet = 0;
	friso_lex_t lexid = __LEX_ISE_EN_USER_DICT__;

	if (NULL == pEngine || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	//英文评测目前仅简单处理，解决单词'aah‘的问题
	strcpy(pOutputText, pInputText);
	lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, lexid, lexid, pOutputText);
	if (TC_WSErr_SUCCESS == nRet && NULL != lex && nOutputTextSize > (int)(strlen(lex->word_new)))
	{
		strcpy(pOutputText, lex->word_new);
	}
	return TC_WSErr_SUCCESS;
}

//针对输入的UTF8字符串，查询评测标注词典，修改为标注格式，以便解决评测引擎报错的一些生僻字、多发音拼音错误等问题
static int ise_cn_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize)
{
	short *pnU8Offset = NULL;
	char *pBuffer = NULL;
	friso_lex_t lexid = __LEX_ISE_CN_USER_DICT__;
	if (NULL == pEngine || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pnU8Offset = (short *)(pEngine->pTmpBuf->pnCornerMarkID);
	ivAssert(sizeof(pEngine->pTmpBuf->pnCornerMarkID) >= sizeof(short) * WS_INPUT_TEXT_MAX_LEN);
	memset(pnU8Offset, 0, sizeof(pEngine->pTmpBuf->pnCornerMarkID));
	pBuffer = (char *)(pEngine->pTmpBuf->szOrgWords); //复用
	memset(pOutputText, 0, nOutputTextSize);

	//统计输入字符串中每个UTF8字节数
	int nCharNum = 0;
	int nMaxLen = pEngine->pResHdr->hash_res_hdr->dic[lexid].wordmaxbytes;
	short nOffset = 0;
	while (0 != pInputText[nOffset])
	{
		int nBytes = get_utf8_bytes(pInputText[nOffset]);
		pnU8Offset[nCharNum] = nOffset;
		nCharNum++;
		nOffset += (short)nBytes;
	}
	pnU8Offset[nCharNum] = nOffset;

	if (1 == nCharNum) //单个汉字
	{
		lex_entry_cdt_t pTag = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, lexid, lexid, pInputText);
		if (NULL == pTag)
		{
			strcpy(pOutputText, pInputText);
		}
		else
		{
			//需要将标注里的如“一<da1>”改为评测全部拼音标注方式 “一\r\nda1”
			char *pNewText = pTag->word_new;
			strcpy(pOutputText, pInputText);
			if (nOutputTextSize > (int)(strlen(pNewText)))
			{
				strcpy(pOutputText, pNewText);
			}
		}
		return TC_WSErr_SUCCESS;
	}
	else //多个汉字
	{
		int nNewStrLen = 0;
		for (int i = 0; i < nCharNum; i++)
		{
			int nLen, nBytes;
			lex_entry_cdt_t pTag = NULL, pTagTmp = NULL;
			int jj = 0;

			nBytes = pnU8Offset[i + 1] - pnU8Offset[i];
			if (3 != nBytes || (i + 2 < nCharNum && 0 == strncmp(pInputText + pnU8Offset[i + 1], "<", 1)))
			{
				//非汉字或者后面是[=本身就可能是tts标注，则跳过
				if (nNewStrLen + nBytes >= nOutputTextSize)
				{
					strcpy(pOutputText, pInputText);
					ivAssert(0);
					return TC_WSErr_SUCCESS;
				}
				memcpy(pOutputText + nNewStrLen, pInputText + pnU8Offset[i], nBytes);
				nNewStrLen += nBytes;
				continue;
			}

			memcpy(pBuffer, pInputText + pnU8Offset[i], nBytes);
			pBuffer[nBytes] = 0;

			for (int j = i + 1; j < nCharNum; j++)
			{
				int nBytesTmp = pnU8Offset[j + 1] - pnU8Offset[j];
				nLen = pnU8Offset[j] - pnU8Offset[i] + nBytesTmp;
				if (3 != nBytesTmp || nLen > nMaxLen || (j + 2 < nCharNum && 0 == strncmp(pInputText + pnU8Offset[j + 1], "<", 1))) //非汉字 或者 文本长度超过词典词条，跳过
				{
					break;
				}

				memcpy(pBuffer, pInputText + pnU8Offset[i], nLen);
				pBuffer[nLen] = 0;

				pTagTmp = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, lexid, lexid, pBuffer);
				if (NULL != pTagTmp)
				{
					pTag = pTagTmp;
					jj = j;
				}
			} // for (int j = i+1; j < nCharNum; j++)

			if (NULL == pTag) //当前位置朝后的字符串序列都没找到tts发音标注
			{
				if (nNewStrLen + nBytes >= nOutputTextSize)
				{
					strcpy(pOutputText, pInputText);
					ivAssert(0);
					return TC_WSErr_SUCCESS;
				}
				memcpy(pOutputText + nNewStrLen, pInputText + pnU8Offset[i], nBytes);
				nNewStrLen += nBytes;
			}
			else
			{
				if (nNewStrLen + (int)(strlen(pTag->word_new)) >= nOutputTextSize)
				{
					strcpy(pOutputText, pInputText);
					ivAssert(0);
					return TC_WSErr_SUCCESS;
				}
				memcpy(pOutputText + nNewStrLen, pTag->word_new, strlen(pTag->word_new));
				nNewStrLen += strlen(pTag->word_new);
				i = jj;
			}
		}
	}

#if 0 //暂时不需要
	//中文评测部分拼音标注不能超过总汉字数的1/3限制，统计是否超过该限制，通过返回值告知
	nOffset = 0;
	int nHanziNum = get_utf8_hanzi_num(pOutputText);
	int nTagNum = 0;
	while (nOffset < strlen(pOutputText)-1)
	{
		int nBytes = get_utf8_bytes(pOutputText[nOffset]);
		if (1 == nBytes && '<' == pOutputText[nOffset] && utf8_en_letter(pOutputText[nOffset +1]))
		{
			nTagNum++;
		}
		nOffset += nBytes;
	}
	if (nHanziNum > 0 && nTagNum > 0 && nHanziNum != nTagNum &&  nHanziNum*1.0/nTagNum < 3.0f)
	{
		ivAssert(0);
		return TC_WSErrID_CnIsePinyinTagMore;
	}
#endif

	return TC_WSErr_SUCCESS;
}

/* 
	根据用户输入的文本进行基础分词
*/
int ws_engine_get_base_segmentation(PWordSegEgn pEngine)
{
	char *szU8Text = NULL;
	int i,j;
	lex_entry_cdt_t val = NULL;
	char ch;
	if (NULL == pEngine)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	szU8Text = pEngine->szText;
	char *pBuffer = pEngine->pTmpBuf->szOrgWords; //复用下	
	friso_token_t pResult = pEngine->pResult;
	int iResult = 0;	
	
	//整词是拼音不再支持拆分
	if (pEngine->nTextType == TEXT_TYPE_CN || pEngine->nTextType == TEXT_TYPE_CN_EN) {
		strcpy(pBuffer, szU8Text);
		string_to_lowcase_letter(pBuffer, pBuffer);
		val = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pBuffer);
		if (NULL != val) {
			strcpy(pResult[iResult].word, szU8Text);
			pResult[iResult].type = val->type;
			pResult[iResult].length = (unsigned char)(pEngine->nTextLen);
			pEngine->nResult = 1;
			return TC_WSErr_SUCCESS;
		}
	}

	i = 0;
	while (0 != szU8Text[i])
	{
		int bLetterNumPinyin = 1;
		int bAscii = 1;
		int bCJK = 1;
		int len = get_utf8_bytes(szU8Text[i]);
		//ivAssert(len + i <= strlen(szU8Text));
		//memcpy(pBuffer, szU8Text + i, len);
		//pBuffer[len] = 0;

		if (iResult >= WS_TOKEN_MAX_NUM - 1) {
			//ivAssert(0);
			break;
		}

		if (1 != len) {
			bAscii = 0;			 
		}

		ch = szU8Text[i + len];
		szU8Text[i + len] = 0;
		//ivAssert(0 == strcmp(szU8Text+i, pBuffer));
		//if (len>2 || !string_is_letter_num_pinyin(szU8Text + i)) 
		if (!utf8_char_is_letter_num_pinyin(szU8Text + i, len))
		{
			bLetterNumPinyin = 0;
		}
		if (1 == len || !utf8_string_is_cjk(szU8Text + i)) {
			bCJK = 0;
		}
		szU8Text[i + len] = ch;

		int tokenLen = len;  //最小分词单元
		uchar_t tokenType = __LEX_OTHER_WORDS__;
		j = i + len;
		while (0 != szU8Text[j])
		{
			int len2 = get_utf8_bytes(szU8Text[j]);
			//ivAssert(len2 + j <= strlen(szU8Text));
			//memcpy(pBuffer + len, szU8Text + j, len2);
			len += len2;
			//pBuffer[len] = 0;

			ch = szU8Text[j + len2];
			szU8Text[j + len2] = 0;
			
			//ivAssert(0 == strcmp(szU8Text+j, pBuffer+len-len2));
			
			//if (bLetterNumPinyin && (len2>2 || !string_is_letter_num_pinyin(szU8Text + j))) 
			//if(bLetterNumPinyin && (len2 > 2 || 
			//	!(utf8_letter_number(szU8Text[j])|| NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, szU8Text + j))))
			if(bLetterNumPinyin && !utf8_char_is_letter_num_pinyin(szU8Text+j, len2) && !string_is_connector(szU8Text+j))
			{
				bLetterNumPinyin = 0;
			}
			if (bAscii && 1 != len2) {
				bAscii = 0;				
			}
			if (bCJK && (1==len2 || !utf8_char_is_cjk(szU8Text + j))) {
				bCJK = 0;
			}
			szU8Text[j + len2] = ch;

			if (bLetterNumPinyin && len < WS_PER_TOKEN_MAX_LEN) //连续字母数字分到一起
			{
				int condtion1 = (len == pEngine->nTextLen && string_is_connector(szU8Text + j)); ////使得 can' 可继续分为can/'
				int condtion2 = 0; //使得 good/gud/这种单词和音标格式不要分到一起
				if ('/' == szU8Text[j]) {
					szU8Text[j] = 0;
					if (NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_TY_EN_WORDS__, __LEX_TY_EN_WORDS__, szU8Text + i)) {
						condtion2 = 1;
					}
					szU8Text[j] = '/';  
				}
				if (condtion2) {
					break;
				}
				if (!condtion1) { 
					tokenLen = len;
					j += len2;
					continue;
				}				
			}
			if (bAscii) {
				j += len2;
				continue;
			}

			if (len >= (int)(pEngine->nTextLen)) {
				break; //不可分整词
			}
			
#if TEST_FILTER_LEX_CHAR
			if (!string_is_in_cnlex(szU8Text + j)) {
				j += len2;
				continue;
			}
#endif

			val = NULL;
			if (TEXT_TYPE_EN != pEngine->nTextType && bCJK) {		
				ch = szU8Text[j + len2];
				szU8Text[j + len2] = 0;
				//ivAssert(0==strcmp( szU8Text+i, pBuffer));
				val = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_CN_WORDS__, __LEX_TY_CN_WORDS__, szU8Text + i);
				szU8Text[j + len2] = ch;
			}
			if (NULL != val) {				
				tokenLen = len;
				tokenType = val->type;
			}
						
			j += len2;
		}

		memcpy(pResult[iResult].word, szU8Text + i, tokenLen);		
		pResult[iResult].word[tokenLen] = 0;
		//if (0 == strcmp(pResult[iResult].word, " ")) {
		if ((0 == pResult[iResult].word[1]) && (' ' == pResult[iResult].word[0])) {
			pResult[iResult].type = __LEX_WHITESPACE__;
		}
		else if(TEXT_TYPE_EN != pEngine->nTextType && NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pResult[iResult].word))
		{ 
			pResult[iResult].type = __LEX_PINYIN__;
		}
		else {
			if (TEXT_TYPE_EN != pEngine->nTextType && tokenType == __LEX_OTHER_WORDS__ && utf8_string_is_cjk(pResult[iResult].word)) {
				tokenType = __LEX_CN_CHAR__;
			}
			pResult[iResult].type = tokenType;
		}
		pResult[iResult].length = (unsigned char)tokenLen;
		i += tokenLen;
		iResult++;
	}

	pEngine->nResult = iResult;

	return TC_WSErr_SUCCESS;
}


#if 0
/*
功能：针对输入的pStr字符串，若全是符号表里的字符则返回1，否则返回0
*/
static int string_is_all_punctuation(PWordSegEgn pEngine, const char *pStr)
{
	int nOffset = 0, nLen = 0;

	nLen = strlen(pStr);
	while (nOffset < nLen)
	{
		char szTmp[10] = { 0 };
		int nBytes = get_utf8_bytes(pStr[nOffset]);
		memcpy(szTmp, pStr + nOffset, nBytes);

		lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, szTmp); //先在中文词库中查找
		if (NULL == lex)
		{
			return 0;
		}

		nOffset += nBytes;
	}

	return 1;
}
#endif

//将分词结果里的type从friso_lex_t转成text_language_type对应的值
int ws_engine_texttype_convert(PWordSegEgn pEngine, friso_token_t pResult, int nResult)
{
	for (int i = 0; i < nResult; i++)
	{
		if (__LEX_PINYIN__ == pResult[i].type) {
			pResult[i].type = TEXT_TYPE_PY;
		}
		else if (__LEX_PUNCTUATION__ == pResult[i].type || __LEX_WHITESPACE__ == pResult[i].type ||
			__LEX_CORNERMARK__ == pResult[i].type) {
			pResult[i].type = TEXT_TYPE_PUNC;
		}
		else if (pResult[i].type >= __LEX_CJK_WORDS__ &&  pResult[i].type <= __LEX_TY_CN_WORDS__) {
			pResult[i].type = TEXT_TYPE_CN;
		}
		else if (__LEX_TY_EN_WORDS__ == pResult[i].type || __LEX_INTEGER__ == pResult[i].type ||
			__LEX_DECIMAL__ == pResult[i].type) {
			pResult[i].type = TEXT_TYPE_EN;
		}
		else {
			int type = 0;
			ws_engine_texttype_get(pResult[i].word, &type);
			pResult[i].type = type;
		}
	}
	return 0;
}