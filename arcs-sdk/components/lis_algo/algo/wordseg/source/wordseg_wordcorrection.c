/***********************************************************
 * @file wordseg_wordcorrection.h
 * 描述：产品需求：考虑OCR扫描单词可能出错，希望给出推荐单词
		实现英文单词纠错功能，跟词库里的上万条英文单词对比，通过计算字符串距离给出二个最像的单词
 * @author qungao
 * @date 2022-01-12
**********************************************************/

#include <time.h>
#include "wordseg_kernel.h"
#include "wordseg.h"
#include "wordseg_engine.h"
#include "friso_UTF8.h"
#include "wordseg_wordcorrection.h"


#define WORD_MAX_BYTES_DIST		(2) //计算相似单词，运行的字节数最大差值，例如good 和dooded差值为2

#define LOG_WORD_TO_FILE		(0)

static int str_has_blank(const char* str);
static void LogEnWordToFile(friso_hash_cdt_t dic, char* szFile);

#if SUPPORT_READ_HASH_RES_CALLBACK
extern int g_hash_offset;
#endif
extern char *get_hash_data_by_callback_ext(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res, uint_t bucket, void **buffer, uint_t buffer_size);


/* 功能：实现英文单词纠错功能，跟词库里的上万条英文单词对比，通过计算字符串距离给出二个最像的候选单词
	* @param handle     [in] wordseg_init返回的工作句柄
	* @param szWord		[in] 输入单词，UTF8格式
	* @param pResult	[out]存储纠错结果信息结构体指针
*/
FRISO_API int wordseg_wordcorrection(void* handle, const char* szText, PWordCorrRst* ppResult)
{
	PWordSegEgn pEngine = NULL;
	PWordCorrRst pResult = NULL;
	int		nResult = 0;
	int i, k;
	int nTextLen = 0;

	if (NULL == handle || NULL == szText || NULL == ppResult)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

#if PRINT_RES_READ
	//printf("-------------------%s begin----------------------\r\n", __FUNCTION__);
	g_nReadResCnt = 0;
	g_nReadResSize = 0;
#endif

	*ppResult = NULL;

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	pResult = pEngine->pWordCorrRst;	
	*ppResult = pResult;
	for (i = 0; i < WORDSEG_SIMILAR_WORD_MAX_NUM; i++)
	{
		pResult->pnDist[i] = 100000;
	}
	pResult->nResult = 0;

#if LOG_WORD_TO_FILE
	friso_hash_cdt_t dic = pEngine->friso->dic + __LEX_TOYCLOUD_EN_WORDS__;
	LogEnWordToFile(dic, "数据库中英文单词.txt");
#endif

	int iLex = __LEX_TY_EN_WORD__;
	PWordsegDict dic = pEngine->pResHdr;
	PHashResHdr hash_res_hdr = dic->hash_res_hdr;

	nTextLen = strlen(szText);
	//输入文本长度限制，若超过词典中最长词一丢丢，就不再寻找相似词
	if (nTextLen + 2 > hash_res_hdr->dic[iLex].wordmaxbytes)
	{
		return TC_WSErr_SUCCESS;
	}

	char* pBuffer = (char *)(pEngine->pTmpBuf->pCornerBuf);
	int nBufferSize = sizeof(pEngine->pTmpBuf->pCornerBuf);
	char* pInputWord = pEngine->pTmpBuf->szLowerWords;
	strcpy(pInputWord, szText);
	string_to_lowcase_letter(pInputWord, pInputWord); //转为小写字母后计算

	
	PWSCallBack func_read_data_res = pEngine->func_read_data_res;
	PHashTab hashTab = (PHashTab)((char *)hash_res_hdr + hash_res_hdr->dic[iLex].table_offset);
#if SUPPORT_READ_HASH_RES_CALLBACK
	g_hash_offset = hash_res_hdr->dic[iLex].table_offset;
#endif

	//先检查是否就是词库单词
	if (NULL != wordseg_dic_get(dic, func_read_data_res, iLex, iLex, pInputWord))
	{
		nResult = 1;
		pResult->pnDist[0] = 0;
		strcpy(pResult->ppWord[0], pInputWord);
		goto wordseg_wordcorrection_end;
	}

	int bExit = 0;
	for (int i = 0; i < hash_res_hdr->dic[iLex].length; i++)
	{
		uint_t ibucket = i;

		char *data = NULL;
		void *buf = NULL;
		data = get_hash_data_by_callback_ext(hashTab, hash_res_hdr->dic[iLex].length, dic->data_res, (PWSCallBack)func_read_data_res, ibucket, &buf, 0);

		if(NULL == data)	continue;

		PDataHdr dataHdr = (PDataHdr)data;
		PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
		for (uchar_t ii = 0; ii < dataHdr->n; ii++)
		{
			int j = 0;
			char *pWord = data_info->text;

			int nDist = 0;
			int err = GetStrDist((const char*)pInputWord, (const char *)pWord, (void *)pBuffer, nBufferSize, 3, &nDist);
			if (0 != err)	continue;

			if (nDist < 0 || nDist >(nTextLen + 3) / 4)	continue;
			if (nResult > 0 && (nDist > pResult->pnDist[nResult - 1]))	continue;

			for (j = 0; j < nResult; j++)
			{
				if (nDist <= pResult->pnDist[j]) break;
			}
			k = nResult - 1 <= WORDSEG_SIMILAR_WORD_MAX_NUM - 2 ? nResult - 1 : WORDSEG_SIMILAR_WORD_MAX_NUM - 2;
			for (; k >= j; k--)
			{
				pResult->pnDist[k + 1] = pResult->pnDist[k];
				strcpy(pResult->ppWord[k + 1], pResult->ppWord[k]);
			}
			pResult->pnDist[j] = nDist;
			strcpy(pResult->ppWord[j], pWord);
			nResult++;
			nResult = nResult > WORDSEG_SIMILAR_WORD_MAX_NUM ? WORDSEG_SIMILAR_WORD_MAX_NUM : nResult;

			if (nResult >= 2 && pResult->pnDist[0] <= 1 && pResult->pnDist[1] <= 1) {
				goto wordseg_wordcorrection_end;
			}

			int offset = sizeof(TDataInfo) + data_info->len;
			data_info = (PDataInfo)((char *)data_info + offset);
		}		
	}

wordseg_wordcorrection_end:
	pResult->nResult = nResult;
	return TC_WSErr_SUCCESS;
}

#if LOG_WORD_TO_FILE
static void LogEnWordToFile(friso_hash_cdt_t dic, char* szFile)
{
	hash_entry_t e, n;
	int nCount = 0;
	int nSize = 0;

	FILE* fp = fopen(szFile, "wb");	   	
	for (uint_t j = 0; j < dic->length; j++)
	{
		int i, k;
		e = dic->table[j];
		if (NULL == e)
		{
			continue;
		}

		for (; NULL != e; )
		{
			lex_entry_cdt_t pWordDsc = (lex_entry_cdt_t)e->_val;
			char* pWord = pWordDsc->word;			

			n = e->_next;
			e = n;

			if (str_has_blank(pWord))	continue;

			fprintf(fp, "%s\r\n", pWord);
			nSize += strlen(pWord) + 1 + sizeof(char *);
			nCount++;
		}
	}
	fclose(fp);

	printf("english_words=%d, word=%d, needsize=%d\r\n", dic->length, nCount, nSize);
}
#endif

//判断输入字符串是否含有空格，有返回1，否则返回0
static int str_has_blank(const char* str)
{
	for (uint_t i = 0; i < strlen(str); i++)
	{
		if (' ' == str[i])
		{
			return 1;
		}
	}
	return 0;
}

/*
* 功能：计算strA经过多少步插入/删除/修改操作后和strB相同
* 算法参考：https://www.cnblogs.com/boris1221/p/9375047.html
*/
int GetStrDist(const char* strA, const char* strB, void* pBuffer, int nBufferSize, int nLenDistMax, int* pnDist)
{
	int nA, nB;
	int i, j;
	signed char* pnDists;
	if (NULL == strA || NULL == strB || NULL == pBuffer || NULL == pnDist)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	nA = strlen(strA);
	nB = strlen(strB);

	if (nA == nB)  //字符串长度相同，直接算修改次数即可
	{
		int nDist = 0;
		for (j = 0; j < nA; j++)
		{
			if (strA[j] != strB[j])    nDist++;
		}
		*pnDist = nDist;
		return TC_WSErr_SUCCESS;
	}

	if ((nA > nB && nA - nB > nLenDistMax) || (nB > nA && nB - nA > nLenDistMax)) {
		*pnDist = -1;
		return TC_WSErr_SUCCESS;
	}

	int nNeedSize = (nA + 1) * (nB + 1) * sizeof(signed char);
	if (nBufferSize < nNeedSize)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}
	memset(pBuffer, 0, nNeedSize);

	pnDists = (signed char *)pBuffer;
	for (i = 0; i < nB + 1; i++)
	{
		pnDists[i] = (unsigned char)i;
	}
	for (i = 0; i < nA + 1; i++)
	{
		pnDists[(nB + 1) * i] = (unsigned char)i;
	}

	char* s1 = (char*)strA;
	for (i = 1; i < nA + 1; i++, s1++)
	{
		char* s2 = (char*)strB;
		for (j = 1; j < nB + 1; j++, s2++)
		{
			if (*s1 == *s2) {
				pnDists[i * (nB + 1) + j] = pnDists[(i - 1) * (nB + 1) + j - 1];
			}
			else
			{
				signed char d = pnDists[(i - 1) * (nB + 1) + j - 1];
				if (pnDists[(i - 1) * (nB + 1) + j] < d) d = pnDists[(i - 1) * (nB + 1) + j];
				if (pnDists[i * (nB + 1) + j - 1] < d) d = pnDists[i * (nB + 1) + j - 1];
				pnDists[i * (nB + 1) + j] = d + 1;
			}
		}
	}
	*pnDist = (int)(pnDists[(nB + 1) * (nA + 1) - 1]);

	return 	TC_WSErr_SUCCESS;
}
