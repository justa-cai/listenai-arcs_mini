/**
 * 将对外接口函数

 *
 * @author  qungao
 * @date    2021.7.14
 */

#include <time.h>
#include "wordseg_kernel.h"
#include "friso_ctype.h"
#include "friso.h"
#include "wordseg_version.h"
#include "wordseg_engine.h"
#include "wordseg_res.h"

#define WORDSEG_WHOLEMATCH_MAX_BYTES_CN (30)
#define WORDSEG_WHOLEMATCH_MAX_BYTES_KO (60)

#if TEST_MEMORY_LEAK
extern int g_nMallocCnt;
extern int g_nFreeCnt;
extern unsigned int g_nMallocSize;
extern FILE *g_fpMemMallocLog;
extern FILE *g_fpMemFreeLog;
extern char g_szMallFile[256];
extern char g_szFreeFile[256];
extern char g_szMemOut[256];
#endif

#if LOG_MODULE_COST_TIME
int g_t1 = 0, g_t2 = 0, g_tBegin = 0, g_tEnd = 0;
#ifdef WIN32
int g_print_time = 0;
#else
int g_print_time = 1;
#endif
FILE *g_fpCostTime = NULL;

#endif

#define IV_PTR_GRID		(4)
#define ivGridSize(n)	((int)(((int)(n)+(IV_PTR_GRID-1))&(~IV_PTR_GRID + 1)))

#if SUPPORT_READ_HASH_RES_CALLBACK
extern void *g_hash_res;
extern PWSCallBack g_func_read_hash_res;
#endif

static int wordseg_text_is_minunit(const char *text, friso_lex_t * type);

//分词引擎初始化
WS_API int wordseg_init(void * pResidentBuf, int * pResidentBufSize, void *pTmpBuf, int * pTmpBufSize, const char * pHashRes, void * pDataRes, const PWSCallBack pReadResFunc)
{
	int resident_need_size = 0, tmp_need_size = 0;
	int resident_use_size = 0, tmp_use_size = 0;
	int ret = 0;
	PWordSegEgn pEngine = NULL;
#if TEST_LEX_IN_RAM //将部分内存占用少需要频繁访问的hash表data全部导入内存	
	PHashDataInRam hash_data = NULL;
	PHashResHdr hash_hdr = NULL;
	friso_lex_t lex_id[] = { __LEX_PUNCTUATION__,__LEX_CORNERMARK__, __LEX_PINYIN_DICT__ };
#endif

	//printf("sizeof(TWSTmpBuf)=%d\n", sizeof(TWSTmpBuf));
	//printf("sizeof(TWordSegEgn)=%d\n", sizeof(TWordSegEgn));

	if (NULL == pResidentBufSize || NULL == pTmpBufSize  || NULL == pHashRes || NULL == pDataRes || NULL == pReadResFunc)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	
#if TEST_MEMORY_LEAK
	g_nMallocCnt = 0;
	g_nFreeCnt = 0;
	g_nMallocSize = 0;	
	_mkdir("内存分析");
	strcpy(g_szMallFile, "内存分析\\malloc.log");
	strcpy(g_szFreeFile, "内存分析\\free.log");
	strcpy(g_szMemOut, "内存分析\\异常情况.log");
	g_fpMemMallocLog = fopen(g_szMallFile, "wb");
	g_fpMemFreeLog = fopen(g_szFreeFile, "wb");
#endif

#if LOG_MODULE_COST_TIME
#ifdef WIN32
	g_fpCostTime = fopen("CostTimePerModule-pc.txt", "ab");
#else
	//g_fpCostTime = fopen("/sdcard/CostTimePerModule-esp32.txt", "ab");
#endif
	if (NULL == g_fpCostTime){
		printf("xxxxx Error:open CostTimePerModule.txt failed.\r\n");
	}
	else{
		printf("xxxxx Log:CostTimePerModule.txt\r\n");
	}
#endif

	resident_need_size = ivGridSize(sizeof(TWordSegEgn));
	hash_hdr = (PHashResHdr)pHashRes;
	//printf("[wordseg xx]sizeof(THashResHdr)=%d\n", sizeof(THashResHdr));
	if (0 != strcmp(hash_hdr->version, WORDSEG_RES_VERSION)) //资源检查
	{
		return TC_WSErrID_InvalidRes;
	}
#if TEST_LEX_IN_RAM	
	for (int i = 0; i < sizeof(lex_id) / sizeof(lex_id[0]); i++)
	{
		friso_lex_t iLex = lex_id[i];
		resident_need_size += ivGridSize(hash_hdr->dic[iLex].table_data_size);
		resident_need_size += SD_CARD_SECTOR_SIZE * 2; //配合core1使用，裸读SD卡按照sector读，需要多预留二个sector大小
		//printf("[wordseg xx] hash_hdr->dic[iLex].table_data_size=%d\n", hash_hdr->dic[iLex].table_data_size);
	}
	//printf("[wordseg xx]resident_need_size=%d\n", resident_need_size);

#endif

	//pReadResFunc((void *)pDataRes, 20, sizeof(uint_t), &hash_text_max_size, sizeof(uint_t)); //临时内存	
	char buffer[SD_CARD_SECTOR_SIZE]; //不能用malloc	
	void *ptr = NULL;
	ptr = buffer;	
	pReadResFunc((void *)pDataRes, 0, SD_CARD_SECTOR_SIZE, &ptr, SD_CARD_SECTOR_SIZE); //临时内存	
	PDataResHdr data_res_hdr = (PDataResHdr)buffer;
	if ((unsigned int)(&(data_res_hdr->hash_text_max_size)) - (unsigned int)data_res_hdr + sizeof(data_res_hdr->hash_text_max_size) > SD_CARD_SECTOR_SIZE)
	{
		return TC_WSErrID_ReadDataResErr;
	}
	if (0 != strcmp(data_res_hdr->version, WORDSEG_RES_VERSION)) //资源检查
	{
		return TC_WSErrID_InvalidRes;
	}
	uint_t hash_text_max_size = data_res_hdr->hash_text_max_size;

	tmp_need_size += ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);
	tmp_need_size += ivGridSize(hash_text_max_size + SD_CARD_SECTOR_SIZE);
	tmp_need_size += ivGridSize(sizeof(TWsTmpBuf));
	//printf("[wordseg xx]tmp_need_size=%d, hash_text_max_size=%d\n", tmp_need_size, hash_text_max_size);

	if (NULL == pResidentBuf || NULL == pTmpBuf) 
	{
		//返回需要的常驻内存和临时内存大小
		*pResidentBufSize = resident_need_size;
		*pTmpBufSize = tmp_need_size;
		return TC_WSErrID_TellSize;
	}

	if (*pResidentBufSize < resident_need_size || *pTmpBufSize < tmp_need_size) 
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	pEngine = (PWordSegEgn)pResidentBuf;	
	memset(pEngine, 0, sizeof(TWordSegEgn));
	pEngine->dwCheck = (uint_t)WORDSEG_ENGINE_CHECK;
	resident_use_size = ivGridSize(sizeof(TWordSegEgn));

	pEngine->func_read_data_res = pReadResFunc;
// 	pEngine->friso->func_read_data_res = pReadResFunc;
// 	ret = friso_init(pEngine->friso, NULL, NULL, NULL);
// 	if (0 != ret)
// 	{
// 		return ret;
// 	}

	ret = wordseg_load_res(pEngine, pHashRes, pDataRes);
	if (0 != ret)
	{
		return ret;
	}

	pEngine->pResult = (friso_token_t)pTmpBuf;		//使用pTmpBuf
	tmp_use_size = ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);
	pEngine->nResultSize = ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);

	if (pEngine->pResHdr->data_res_hdr->hash_text_max_size != hash_text_max_size) {
		ivAssert(0);
		return TC_WSErrID_ReadResErr; //请查看上述代码读取hash_text_max_size的偏移值设置是否有误
	}
	pEngine->pResHdr->buffer_size = pEngine->pResHdr->data_res_hdr->hash_text_max_size + SD_CARD_SECTOR_SIZE;
	pEngine->pResHdr->buffer = (char *)pTmpBuf + tmp_use_size;	//使用pTmpBuf
	tmp_use_size += ivGridSize(pEngine->pResHdr->buffer_size);
	pEngine->pTmpBuf = (PWsTmpBuf)((char *)pTmpBuf + tmp_use_size);	//使用pTmpBuf
	tmp_use_size += ivGridSize(sizeof(TWsTmpBuf));
	//pEngine->friso->new_res = (void *)pEngine->pResHdr; 
	
	pEngine->bProcessTransTxt = PROCESS_TRANSTXT_OFF;
	pEngine->bSupportSentenceSeg = SENTENCESEG_OFF;
	pEngine->bSupportPostProcess = WS_POSTPROCESS_ON;
	pEngine->nPriorLanguage = WS_LANGUAGE_CNEN;
	pEngine->nWholeMatchMaxBytes = WORDSEG_WHOLEMATCH_MAX_BYTES_CN;

#if TEST_LEX_IN_RAM //将部分内存占用少需要频繁访问的hash表data全部导入内存
	hash_data = pEngine->pResHdr->hash_data_in_ram;
	hash_hdr = pEngine->pResHdr->hash_res_hdr;	
	for (int i = 0; i < sizeof(lex_id) / sizeof(lex_id[0]); i++)
	{
		friso_lex_t iLex = lex_id[i];
		hash_data[iLex].data_offset = hash_hdr->dic[iLex].table_data_offset;
		hash_data[iLex].data_size = hash_hdr->dic[iLex].table_data_size;
		hash_data[iLex].data_buf = (char *)pResidentBuf + resident_use_size;
		resident_use_size += ivGridSize(hash_data[iLex].data_size);
		resident_use_size += SD_CARD_SECTOR_SIZE * 2;
		ret = pReadResFunc((void *)pDataRes, hash_data[iLex].data_offset, hash_data[iLex].data_size, (void **)&(hash_data[iLex].data_buf), hash_data[iLex].data_size + SD_CARD_SECTOR_SIZE * 2);
		if (ret != 0) {
			return ret;
		}

#if 0 //test		
		PHashTab hashTab = (PHashTab)((char *)hash_hdr + hash_hdr->dic[iLex].table_offset);
		//get_hash_tab_info_datainram(hashTab, hash_hdr->dic[iLex].length, hash_data+iLex);
		printf("lexid=%02d ", iLex);
		my_esp_rom_crc32_le(0, (const unsigned char *)(hash_data[iLex].data_buf), (unsigned int)(hash_data[iLex].data_size)); //测试
#endif
	}	
#endif
	ivAssert(resident_need_size == resident_use_size);
	ivAssert(tmp_need_size == tmp_use_size);

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL == g_fpReadResLog)
	{
		if (0 != g_pReadResLogFileName[0]) {
			g_fpReadResLog = fopen(g_pReadResLogFileName, "wb");			
		}
		if (NULL == g_fpReadResLog) 
		{
			g_fpReadResLog = fopen("./output-pc/read_res_log/result-read_res_info.log", "wb");						
		}
		if (NULL == g_fpReadResLog) {
			g_fpReadResLog = fopen("result-read_res_info.log", "wb");
		}
		g_nCallLexCntMax = 0;
	}
#endif
#endif

	return ret;
}

#if SUPPORT_READ_HASH_RES_CALLBACK
/*
* 引擎初始化扩展接口，支持hash.bin通过回调读取
*/
WS_API int wordseg_init_ext(void * pResidentBuf, int * pResidentBufSize, void *pTmpBuf, int * pTmpBufSize, const char * pHashRes, const PWSCallBack pReadHashResFunc, void * pDataRes, const PWSCallBack pReadDataResFunc)
{
	int resident_need_size = 0, tmp_need_size = 0;
	int resident_use_size = 0, tmp_use_size = 0;
	int ret = 0;
	PWordSegEgn pEngine = NULL;
#if TEST_LEX_IN_RAM //将部分内存占用少需要频繁访问的hash表data全部导入内存	
	PHashDataInRam hash_data = NULL;
	PHashResHdr hash_hdr = NULL;
	//friso_lex_t lex_id[] = { __LEX_PUNCTUATION__,__LEX_CORNERMARK__, __LEX_PINYIN_DICT__ };
	friso_lex_t lex_id[] = { __LEX_PUNCTUATION__,__LEX_CORNERMARK__ };
#endif

	//printf("sizeof(TWSTmpBuf)=%d\n", sizeof(TWSTmpBuf));
	//printf("sizeof(TWordSegEgn)=%d\n", sizeof(TWordSegEgn));

	if (NULL == pResidentBufSize || NULL == pTmpBufSize || NULL == pHashRes || NULL == pDataRes || NULL == pReadDataResFunc)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	if (NULL == pReadHashResFunc)
	{
		return wordseg_init(pResidentBuf, pResidentBufSize, pTmpBuf, pTmpBufSize, pHashRes, pDataRes, pReadDataResFunc);
	}

#if TEST_MEMORY_LEAK
	g_nMallocCnt = 0;
	g_nFreeCnt = 0;
	g_nMallocSize = 0;
	_mkdir("内存分析");
	strcpy(g_szMallFile, "内存分析\\malloc.log");
	strcpy(g_szFreeFile, "内存分析\\free.log");
	strcpy(g_szMemOut, "内存分析\\异常情况.log");
	g_fpMemMallocLog = fopen(g_szMallFile, "wb");
	g_fpMemFreeLog = fopen(g_szFreeFile, "wb");
#endif

#if LOG_MODULE_COST_TIME
#ifdef WIN32
	g_fpCostTime = fopen("CostTimePerModule-pc.txt", "ab");
#else
	//g_fpCostTime = fopen("/sdcard/CostTimePerModule-esp32.txt", "ab");
#endif
	if (NULL == g_fpCostTime) {
		printf("xxxxx Error:open CostTimePerModule.txt failed.\r\n");
	}
	else {
		printf("xxxxx Log:CostTimePerModule.txt\r\n");
	}
#endif

	resident_need_size = ivGridSize(sizeof(TWordSegEgn));
	resident_need_size += ivGridSize(sizeof(THashResHdr)) +SD_CARD_SECTOR_SIZE*2; //1764 Bytes

	hash_hdr = NULL;
	pReadHashResFunc((void *)pHashRes, 0, sizeof(THashResHdr), (void **)&hash_hdr, 0);	
	if (0 != strcmp(hash_hdr->version, WORDSEG_RES_VERSION)) //资源检查
	{
		return TC_WSErrID_InvalidRes;
	}
#if TEST_LEX_IN_RAM	
	for (int i = 0; i < sizeof(lex_id) / sizeof(lex_id[0]); i++)
	{
		friso_lex_t iLex = lex_id[i];
		resident_need_size += ivGridSize(hash_hdr->dic[iLex].table_data_size);
// #ifdef WIN32
// 		printf("iLex=%d, name=%s, len=%d\n", iLex, hash_hdr->dic[iLex].szName, hash_hdr->dic[iLex].length);
// #else
// 		printk("iLex=%d, name=%s, len=%d\n", iLex, hash_hdr->dic[iLex].szName, hash_hdr->dic[iLex].length);
// #endif
		resident_need_size += SD_CARD_SECTOR_SIZE * 2; //配合core1使用，裸读SD卡按照sector读，需要多预留二个sector大小
													   //printf("[wordseg xx] hash_hdr->dic[iLex].table_data_size=%d\n", hash_hdr->dic[iLex].table_data_size);
	}
	//printf("[wordseg xx]resident_need_size=%d\n", resident_need_size);

#endif

	//pReadDataResFunc((void *)pDataRes, 20, sizeof(uint_t), &hash_text_max_size, sizeof(uint_t)); //临时内存	
	char buffer[SD_CARD_SECTOR_SIZE]; //不能用malloc	
	void *ptr = NULL;
	ptr = buffer;
	pReadDataResFunc((void *)pDataRes, 0, SD_CARD_SECTOR_SIZE, &ptr, SD_CARD_SECTOR_SIZE); //临时内存	
	PDataResHdr data_res_hdr = (PDataResHdr)buffer;
	if ((unsigned int)(&(data_res_hdr->hash_text_max_size)) - (unsigned int)data_res_hdr + sizeof(data_res_hdr->hash_text_max_size) > SD_CARD_SECTOR_SIZE)
	{
		return TC_WSErrID_ReadDataResErr;
	}
	if (0 != strcmp(data_res_hdr->version, WORDSEG_RES_VERSION)) //资源检查
	{
		return TC_WSErrID_InvalidRes;
	}
	uint_t hash_text_max_size = data_res_hdr->hash_text_max_size;

	tmp_need_size += ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);
	tmp_need_size += ivGridSize(hash_text_max_size + SD_CARD_SECTOR_SIZE);
	tmp_need_size += ivGridSize(sizeof(TWsTmpBuf));
	//printf("[wordseg xx]tmp_need_size=%d, hash_text_max_size=%d\n", tmp_need_size, hash_text_max_size);

	if (NULL == pResidentBuf || NULL == pTmpBuf)
	{
		//返回需要的常驻内存和临时内存大小
		*pResidentBufSize = resident_need_size;
		*pTmpBufSize = tmp_need_size;
		return TC_WSErrID_TellSize;
	}

	if (*pResidentBufSize < resident_need_size || *pTmpBufSize < tmp_need_size)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	pEngine = (PWordSegEgn)pResidentBuf;
	memset(pEngine, 0, sizeof(TWordSegEgn));
	pEngine->dwCheck = (uint_t)WORDSEG_ENGINE_CHECK;
	resident_use_size = ivGridSize(sizeof(TWordSegEgn));
	pEngine->pHashResHdr = (PHashResHdr)((char *)pEngine + resident_use_size);
	pEngine->nHashResHdrSize = ivGridSize(sizeof(THashResHdr)) + SD_CARD_SECTOR_SIZE * 2;
	resident_use_size += pEngine->nHashResHdrSize;

	pEngine->func_read_data_res = pReadDataResFunc;
	pEngine->func_read_hash_res = pReadHashResFunc;
	pEngine->pResHdr->hash_res = (void *)pHashRes;	
	g_hash_res = (void *)pHashRes;
	g_func_read_hash_res = pReadHashResFunc;

	ret = wordseg_load_res(pEngine, pHashRes, pDataRes);
	if (0 != ret)
	{
		return ret;
	}

	pEngine->pResult = (friso_token_t)pTmpBuf;		//使用pTmpBuf
	tmp_use_size = ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);
	pEngine->nResultSize = ivGridSize(sizeof(friso_token)*WS_TOKEN_MAX_NUM);

	if (pEngine->pResHdr->data_res_hdr->hash_text_max_size != hash_text_max_size) {
		ivAssert(0);
		return TC_WSErrID_ReadResErr; //请查看上述代码读取hash_text_max_size的偏移值设置是否有误
	}
	pEngine->pResHdr->buffer_size = pEngine->pResHdr->data_res_hdr->hash_text_max_size + SD_CARD_SECTOR_SIZE;
	pEngine->pResHdr->buffer = (char *)pTmpBuf + tmp_use_size;	//使用pTmpBuf
	tmp_use_size += ivGridSize(pEngine->pResHdr->buffer_size);
	pEngine->pTmpBuf = (PWsTmpBuf)((char *)pTmpBuf + tmp_use_size);	//使用pTmpBuf
	tmp_use_size += ivGridSize(sizeof(TWsTmpBuf));
	//pEngine->friso->new_res = (void *)pEngine->pResHdr; 

	pEngine->bProcessTransTxt = PROCESS_TRANSTXT_OFF;
	pEngine->bSupportSentenceSeg = SENTENCESEG_OFF;
	pEngine->bSupportPostProcess = WS_POSTPROCESS_ON;
	pEngine->nPriorLanguage = WS_LANGUAGE_CNEN;
	pEngine->nWholeMatchMaxBytes = WORDSEG_WHOLEMATCH_MAX_BYTES_CN;

#if TEST_LEX_IN_RAM //将部分内存占用少需要频繁访问的hash表data全部导入内存
	hash_data = pEngine->pResHdr->hash_data_in_ram;
	hash_hdr = pEngine->pResHdr->hash_res_hdr;
	for (int i = 0; i < sizeof(lex_id) / sizeof(lex_id[0]); i++)
	{
		friso_lex_t iLex = lex_id[i];
// #ifdef WIN32
// 		printf("iLex=%d, name=%s, len=%d\n", iLex, hash_hdr->dic[iLex].szName, hash_hdr->dic[iLex].length);
// #else
// 		printk("iLex=%d, name=%s, len=%d\n", iLex, hash_hdr->dic[iLex].szName, hash_hdr->dic[iLex].length);
// #endif
		hash_data[iLex].data_offset = hash_hdr->dic[iLex].table_data_offset;
		hash_data[iLex].data_size = hash_hdr->dic[iLex].table_data_size;
		hash_data[iLex].data_buf = (char *)pResidentBuf + resident_use_size;
		resident_use_size += ivGridSize(hash_data[iLex].data_size);
		resident_use_size += SD_CARD_SECTOR_SIZE * 2;
		ret = pReadDataResFunc(pDataRes, hash_data[iLex].data_offset, hash_data[iLex].data_size, (void **)&(hash_data[iLex].data_buf), hash_data[iLex].data_size + SD_CARD_SECTOR_SIZE * 2);
		if (ret != 0) {
			return ret;
		}

#if 0 //test		
		PHashTab hashTab = (PHashTab)((char *)hash_hdr + hash_hdr->dic[iLex].table_offset);
		//get_hash_tab_info_datainram(hashTab, hash_hdr->dic[iLex].length, hash_data+iLex);
		printf("lexid=%02d ", iLex);
		my_esp_rom_crc32_le(0, (const unsigned char *)(hash_data[iLex].data_buf), (unsigned int)(hash_data[iLex].data_size)); //测试
#endif
	}
#endif
	ivAssert(resident_need_size == resident_use_size);
	ivAssert(tmp_need_size == tmp_use_size);

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL == g_fpReadResLog)
	{
		if (0 != g_pReadResLogFileName[0]) {
			g_fpReadResLog = fopen(g_pReadResLogFileName, "wb");
		}
		if (NULL == g_fpReadResLog)
		{
			g_fpReadResLog = fopen("./output-pc/read_res_log/result-read_res_info.log", "wb");
		}
		if (NULL == g_fpReadResLog) {
			g_fpReadResLog = fopen("result-read_res_info.log", "wb");
		}
		g_nCallLexCntMax = 0;
	}
#endif
#endif

	return ret;
}
#endif

WS_API int	wordseg_get_read_data_buf_size(void *handle, int *size)
{
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
	
	*size = pEngine->pResHdr->buffer_size + SD_CARD_SECTOR_SIZE * 2;
#if SUPPORT_READ_HASH_RES_CALLBACK
	if (sizeof(THashResHdr) + SD_CARD_SECTOR_SIZE * 2 > *size)
	{
		*size = sizeof(THashResHdr) + SD_CARD_SECTOR_SIZE * 2;
	}
#endif
	return TC_WSErr_SUCCESS;
}

WS_API int wordseg_setparam(void *handle, int param, int param_value)
{
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

	if (WORDSEG_PARAM_PROCESS_TRANSTXT == param)
	{
		pEngine->bProcessTransTxt = param_value;
	}
	else if (WORDSEG_PARAM_SENTENCESEG == param)
	{
		pEngine->bSupportSentenceSeg = param_value;
	}
	else if (WORDSEG_PARAM_LANGUAGE == param)
	{
		if (WS_LANGUAGE_CNEN != param_value && WS_LANGUAGE_JAPANESE != param_value &&
			WS_LANGUAGE_KOREAN != param_value && WS_LANGUAGE_RUSSIAN != param_value &&
			WS_LANGUAGE_SPANISH != param_value)
		{
			ivAssert(0);
			return TC_WSErrID_InvArg;
		}

		pEngine->nPriorLanguage = param_value;
		// pEngine->nWholeMatchMaxBytes = (pEngine->nLanguage== WS_LANGUAGE_CNEN) ? WORDSEG_WHOLEMATCH_MAX_BYTES_CN : WORDSEG_WHOLEMATCH_MAX_BYTES_KO;
		pEngine->nWholeMatchMaxBytes = WORDSEG_WHOLEMATCH_MAX_BYTES_CN;
	}
	else if (WORDSEG_PARAM_POSTPROCESS == param)
	{
		pEngine->bSupportPostProcess = param_value;
	}
	else
	{
		return TC_WSErrID_InvArg;
	}

	return TC_WSErr_SUCCESS;
}

static void print_wordseg_result(char *text, friso_token_t result, int result_num)
{	
	printf("wordseg_do: text=%s, token_num=%d\n    token=", text, result_num);
	for (int j = 0; j < result_num; j++)
	{
		printf("%s|", result[j].word);
	}
	printf("\n");	
}

static int ws_tree_node_create(PWsTreeDsc treedsc, unsigned short parent_idx, char *word, char *word_tts, unsigned short text_type)
{
	if (NULL == treedsc || NULL == word || NULL == word_tts)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	int word_size = strlen(word) + 1;
	int word_tts_size = strlen(word_tts) + 1;

	unsigned int text_need_size = word_size + word_tts_size;
	if (treedsc->node_used_cnt >= treedsc->node_malloc_cnt ||
		treedsc->text_malloc_size - treedsc->text_used_size <= text_need_size)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	tree_node_t * node = treedsc->node + treedsc->node_used_cnt;
	node->idx = treedsc->node_used_cnt;
	node->text_type = text_type;
	node->parent_idx = parent_idx;
	char *text = treedsc->text + treedsc->text_used_size;
	node->word = text;
	memcpy(text, word, word_size);
	treedsc->text_used_size += word_size;
	text = treedsc->text + treedsc->text_used_size;
	node->word_tts = text;
	memcpy(text, word_tts, word_tts_size);
	treedsc->text_used_size += word_tts_size;
	treedsc->node_used_cnt++;
	return 0;
}

//递归生成分词树
static int wordseg_recursive_tree(void *handle, tree_node_t * node, int node_num)
{
	int i;
	int ret = 0;
	PWordSegEgn pEngine = (PWordSegEgn)handle;
	PWsTreeDsc pTreeDsc = NULL;
	if (NULL == handle)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	if (1 == node_num) {
		return 0;
	}

	pTreeDsc = pEngine->pTreeDsc;
	for (i = 0; i < node_num; i++)
	{
		friso_token_t result_tmp = NULL;
		int result_tmp_num = 0;
		ret = wordseg_do(handle, node[i].word, &result_tmp_num, &result_tmp, NULL);
		pTreeDsc->callcnt++;
		if (0 != ret) {
			ivAssert(0);
			goto wordseg_recursive_tree_end;
		}
		if (result_tmp_num <= 1)
		{
			continue;
		}

		int node_id = pTreeDsc->node_used_cnt;		
		node[i].firstchild_idx = node_id;
		node[i].child_num = result_tmp_num;				
		for (int i = 0; i < result_tmp_num; i++)
		{
			ret = ws_tree_node_create(pTreeDsc, node[i].idx, result_tmp[i].word, result_tmp[i].word_tts, result_tmp[i].type);
			if (0 != ret) {
				ivAssert(0);
				goto wordseg_recursive_tree_end;
			}

		}
		//printf_wordseg_result(result_tmp, result_tmp_num);
		wordseg_recursive_tree(handle, pTreeDsc->node + node_id, result_tmp_num);		
	}

wordseg_recursive_tree_end:

	return ret;
}

static int wordseg_recursive_tree_bak(void *handle, friso_token_t result, int result_num)
{
	int i;
	int ret = 0;
	PWordSegEgn pEngine = (PWordSegEgn)handle;
	PWsTreeDsc pTreeDsc = NULL;
	if (NULL == handle)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	
	if (1 == result_num) {
		return 0;
	}

	int text_size = 0;
	char *text = NULL;
	for (i = 0; i < result_num; i++) {		
		text_size += ivGridSize(result[i].length + 1);
	}
	char **ppWord = (char **)pEngine->func_malloc(result_num * sizeof(char *) + text_size, WS_MEM_ERAM);
	text = (char *)ppWord + result_num * sizeof(char *);	
	int text_use_size = 0;
	for (i = 0; i < result_num; i++) {		
		char *word = text + text_use_size;
		text_use_size += ivGridSize(result[i].length + 1);
		strcpy(word, result[i].word);
		ppWord[i] = word;
	}

	pTreeDsc = pEngine->pTreeDsc;
	for (i = 0; i < result_num; i++)
	{
		friso_token_t result_tmp = NULL;
		int result_tmp_num = 0;		
		ret = wordseg_do(handle, ppWord[i], &result_tmp_num, &result_tmp, NULL);		
		pTreeDsc->callcnt++;

		if (0 != ret) {
			break;
		}
		//printf_wordseg_result(result_tmp, result_tmp_num);
		if (result_tmp_num > 1) {
			wordseg_recursive_tree(handle, (tree_node_t *)result_tmp, result_tmp_num);
		}		
	}

	pEngine->func_free(ppWord);

	return ret;
}

WS_API int printf_wordseg_tree(tree_result_t * pTreeRst)
{
	if (NULL == pTreeRst)	return 0;
	tree_node_t * node = pTreeRst->node;
	unsigned int node_num = pTreeRst->node_num;
#ifdef WIN32
	int codepage = getenv("LANG");
	system("chcp 65001");
#endif
	//tree_node * node = treedsc->node;
	for (int i = 0; i < node_num; i++)
	{
		printf("tree%04d: type=%d, firstchild=%3d, childnum=%3d, parent=%3d, word=%s, word_tts=%s\n", i + 1, 
			node[i].text_type, node[i].firstchild_idx, node[i].child_num, node[i].parent_idx, node[i].word, node[i].word_tts);
	}
	printf("valid_node_id = %d\n", pTreeRst->valid_node_id);

#ifdef WIN32
	codepage = getenv("LANG");
	system("chcp 936");
#endif
	return 0;
}

static int tree_result_to_bin(PWsTreeDsc pTreeDsc, tree_result_t **ppResult, int *pnResultsize)
{
	if (NULL == pTreeDsc || NULL == ppResult || NULL == pnResultsize)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	int size = ivGridSize(sizeof(tree_result_t));
	int node_offset = size;
	size += ivGridSize(sizeof(tree_node_t)*pTreeDsc->node_used_cnt);
	int text_offset = size;
	size += ivGridSize(pTreeDsc->text_used_size);
	char *data = pTreeDsc->func_malloc(size, WS_MEM_ERAM);
	if (NULL == data)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}
	memset(data, 0, size);
	tree_result_t * result = (tree_result_t *)data;
	result->size = size;
	result->node_num = pTreeDsc->node_used_cnt;
	result->node_offset = node_offset;
	result->call_wordsegdo_cnt = pTreeDsc->callcnt;
	result->valid_node_id = (pTreeDsc->valid_wordseg_id >= 0) ? (pTreeDsc->valid_wordseg_id + 1) : -1;	
	tree_node_t * node = (tree_node_t *)(data + node_offset);
	char *text = data + text_offset;
	memcpy(node, pTreeDsc->node, result->node_num * sizeof(tree_node_t));
	memcpy(text, pTreeDsc->text, pTreeDsc->text_used_size);
	int text_used_size = 0;
	result->node = node;
	for (int i = 0; i < result->node_num; i++)
	{
		node[i].word = (char *)(text + (pTreeDsc->node[i].word - pTreeDsc->text));
		node[i].word_tts = (char *)(text + (pTreeDsc->node[i].word_tts - pTreeDsc->text));
	}

#ifdef WIN32
	//printf_wordseg_tree(pTreeDsc);
#endif

	//转成偏移值，配合spi
	for (int i = 0; i < result->node_num; i++)
	{
		node[i].word = (char *)(node[i].word - data); 
		node[i].word_tts = (char *)(node[i].word_tts - data);
	}

	result->node = NULL;

	*ppResult = result;
	*pnResultsize = size;

	return 0;
}

WS_API int wordseg_tree_result_convert(tree_result_t * pResult)
{
	if (NULL == pResult)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	pResult->node = (tree_node_t *)((char *)pResult + pResult->node_offset);

	//csk返回的偏移值转回成指针,以便esp32使用
	for (int i = 0; i < pResult->node_num; i++)
	{
		pResult->node[i].word = (char *)((unsigned int)(pResult->node[i].word) + (unsigned int)pResult);
		pResult->node[i].word_tts = (char *)((unsigned int)(pResult->node[i].word_tts) + (unsigned int)pResult);
	}

#ifdef WIN32
	//printf_wordseg_tree(pResult);
#endif

	return 0;
}

WS_API int wordseg_tree_do(void *pResidentBuf, const char *szU8Text, tree_result_t **ppResult, int *pnResultsize)
{
	int ret;
	int nToken = 0;
	friso_token_t pToken = NULL;
	PWordSegEgn pEngine = (PWordSegEgn)pResidentBuf;
	PWsTreeDsc pTreeDsc = NULL;

	if (NULL == pResidentBuf)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}
	if (NULL == pEngine->func_malloc || NULL == pEngine->func_free)
	{
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}

	pTreeDsc = pEngine->pTreeDsc;
	pTreeDsc->func_malloc = pEngine->func_malloc;
	pTreeDsc->func_free = pEngine->func_free;
	text_attr_t * attr = pEngine->pTextAttr;	
	pTreeDsc->text = (char *)pEngine->func_malloc(WS_TREE_TEXT_MAX_SIZE, WS_MEM_ERAM);
	pTreeDsc->node = (tree_node_t *)pEngine->func_malloc(WS_TREE_TOKEN_MAX_CNT * sizeof(tree_node_t), WS_MEM_ERAM);
	if (NULL == pTreeDsc->text || NULL == pTreeDsc->node)
	{
		ivAssert(0);
		ret = TC_WSErrID_OutOfMemory;
		goto wordseg_tree_do_end;
	}
	pTreeDsc->text_malloc_size = WS_TREE_TEXT_MAX_SIZE;
	pTreeDsc->text_used_size = 0;
	pTreeDsc->node_malloc_cnt = WS_TREE_TOKEN_MAX_CNT;
	pTreeDsc->node_used_cnt = 0;
	memset(pTreeDsc->text, 0, pTreeDsc->text_malloc_size);
	memset(pTreeDsc->node, 0, pTreeDsc->node_malloc_cnt * sizeof(tree_node_t));

	pTreeDsc->callcnt = 0;

	attr->b_chk_valid_wordseg = 1; //进行有效词检测
	ret = wordseg_do(pResidentBuf, szU8Text, &nToken, &pToken, attr);
	pTreeDsc->callcnt++;
	if (ret != 0) {
		ivAssert(0);
		return ret;
	}

	if (0 != strcmp(szU8Text, attr->word_delpunc)) {
		char *word = (char *)pEngine->func_malloc(strlen(attr->word_delpunc)+1, WS_MEM_ERAM);
		strcpy(word, attr->word_delpunc);
		ret = wordseg_do(pResidentBuf, word, &nToken, &pToken, attr);
		pTreeDsc->callcnt++;
		pEngine->func_free(word);
		if (ret != 0) {
			ivAssert(0);
			return ret;
		}
	}
	pTreeDsc->valid_wordseg_id = attr->valid_wordseg_id;

	ret = ws_tree_node_create(pTreeDsc, 0, attr->word_delpunc, attr->word_tts, attr->text_type);
	if (0 != ret) {
		ivAssert(0);
		goto wordseg_tree_do_end;
	}
	
	int parent_id = pTreeDsc->node_used_cnt - 1;
	int node_id = pTreeDsc->node_used_cnt;
	if (nToken > 1)
	{
		pTreeDsc->node[0].firstchild_idx = 1;
		pTreeDsc->node[0].child_num = nToken;
		for (int i = 0; i < nToken; i++)
		{
			ret = ws_tree_node_create(pTreeDsc, pTreeDsc->node[0].idx, pToken[i].word, pToken[i].word_tts, pToken[i].type);
			if (0 != ret) {
				ivAssert(0);
				goto wordseg_tree_do_end;
			}
		}
	}	

	ret = wordseg_recursive_tree(pResidentBuf, pTreeDsc->node + node_id, nToken);
	if (0 != ret) {
		ivAssert(0);
		goto wordseg_tree_do_end;
	}

	//printf("wordseg_tree_do: call wordseg_do cnt=%d\n", pTreeDsc->callcnt);

#ifdef WIN32
	//printf_wordseg_tree(pTreeDsc);
#endif

	//配合分词树结果需要csk和esp的spi传输，需要转成bin
	ret = tree_result_to_bin(pTreeDsc, ppResult, pnResultsize);

wordseg_tree_do_end:
	if (NULL != pTreeDsc->text) {
		pEngine->func_free(pTreeDsc->text);
		pTreeDsc->text = NULL;
	}
	if (NULL != pTreeDsc->node) {
		pEngine->func_free(pTreeDsc->node);
		pTreeDsc->node = NULL;
	}

	return ret;
}

WS_API int wordseg_do(void *handle, const char *szU8Text, int *pnResult, friso_token_t *ppResult, text_attr_t * pTextAttr)
{
	//friso_config_t config = NULL;
	//friso_task_t task = NULL;
	PWordSegEgn pEngine = NULL;
	friso_token_t pResult = NULL;
	int nRet = 0;
	int i;
	int bChkValidWord = 0;

	if (NULL == handle || NULL == szU8Text || NULL == pnResult || NULL == ppResult)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	if(NULL != pTextAttr)
	{
		bChkValidWord = pTextAttr->b_chk_valid_wordseg;
	}

#if PRINT_RES_READ
	// printf("-------------------%s begin----------------------\r\n", __FUNCTION__);
	g_nReadResCnt = 0;
	g_nReadResSize = 0;
	memset(g_nReadResCntPerLex, 0, sizeof(g_nReadResCntPerLex));
#endif

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
	if (0 == strlen(szU8Text))
	{
		ivAssert(0);
		return TC_WSErrID_TextNull;
	}

#if LOG_MODULE_COST_TIME
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, "\r\nText:%s\r\n", szU8Text);
		fprintf(g_fpCostTime, "TextLen:%d\r\n", strlen(szU8Text));
	}	
	if (g_print_time) {
		printf("\r\nText:%s\r\n", szU8Text);
		printf("TextLen:%d\r\n", strlen(szU8Text));
	}
	g_t1 = GetCostTimeMs();
	g_tBegin = GetCostTimeMs();
	int time_add = 0;
#endif

	/* 增加多语种 日韩西俄分词需求
	   因为云端OCR扫描是语种无关的，所以传入的文本可能是多个语种字符混合，需要先统计不同语种的字/单词个数来判断语种，
	   如果上述语种判断是日韩西俄，则只进行基础分词，中/日/韩每个字是一个分词,英/西/俄以单词为准，碰到空格就作为一个单词，其他符号空格等都作为单独一个分词；
	   如果上述语种判断是中英，则还是按原来的分词逻辑执行
	*/
	if (WS_LANGUAGE_JAPANESE == pEngine->nPriorLanguage || WS_LANGUAGE_KOREAN == pEngine->nPriorLanguage ||
		WS_LANGUAGE_SPANISH == pEngine->nPriorLanguage || WS_LANGUAGE_RUSSIAN == pEngine->nPriorLanguage)
	{
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_do_otherlanguage(pEngine, szU8Text, pTextAttr);
		if (nRet != 0)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 024 [wordseg_do_otherlanguage] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if(g_print_time)
			printf(" 024 [wordseg_do_otherlanguage] cost time = %d ms\r\n", g_t2 - g_t1);
		time_add += (g_t2 - g_t1);
		g_t1 = GetCostTimeMs();
#endif

		if (TEXT_TYPE_JAPANESE == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_KOREAN == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_SPANISH == pEngine->pTextAttr->text_type ||
			TEXT_TYPE_RUSSIAN == pEngine->pTextAttr->text_type)
		{
			*pnResult = pEngine->nResult;
			*ppResult = pEngine->pResult;

#if LOG_MODULE_COST_TIME
			g_tEnd = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, "textlen=%d, language=%d,  all cost time = %d ms\r\n\r\n", strlen(szU8Text), pEngine->nPriorLanguage, g_tEnd - g_tBegin);
			}
			if (g_print_time) {
				printf("textlen=%d, language=%d,  all cost time = %d ms\r\n\r\n", strlen(szU8Text), pEngine->nPriorLanguage, g_tEnd - g_tBegin);
			}
			fflush(g_fpCostTime);
#endif

			return nRet;
		}
	}

#if LOG_MODULE_COST_TIME
	g_t1 = GetCostTimeMs();
#endif
	ws_engine_get_texttype_new(pEngine, szU8Text, 0, pEngine->pTextAttr);	
#if LOG_MODULE_COST_TIME
	g_t2 = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, " 000 [ws_engine_get_texttype_new] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	if (g_print_time) {
		printf(" 000 [ws_engine_get_texttype_new] cost time = %d ms\r\n", g_t2 - g_t1);		
	}
	time_add += (g_t2 - g_t1);
	g_t2 = GetCostTimeMs();
#endif

	friso_lex_t		type = 0;
	if (wordseg_text_is_minunit(szU8Text, &type))
	{
		pEngine->nResult = 1;
		pEngine->pResult->is_modified = 0;
		pEngine->pResult->type = type;
		pEngine->pResult->length = strlen(szU8Text);
		pEngine->pResult->offset = 0;
		strcpy(pEngine->pResult->word, szU8Text);
		strcpy(pEngine->pResult->word_delpunc, pEngine->pResult->word);
		strcpy(pEngine->pResult->word_tts, pEngine->pResult->word);

		*pnResult = pEngine->nResult;
		*ppResult = pEngine->pResult;		
		if (NULL != pTextAttr)
		{
			pTextAttr->bTextModified = 0;
			pTextAttr->text_type = pEngine->pTextAttr->text_type;
			pTextAttr->bUseLocalOcr = 1;
			pTextAttr->valid_wordseg_id = -1;
			strcpy(pTextAttr->word_delpunc, szU8Text);
			strcpy(pTextAttr->word_tts, szU8Text);
		}

		//处理拼音
		if (NULL != friso_dic_get(pEngine->pResHdr, pEngine->func_read_data_res, __LEX_PINYIN_DICT__, __LEX_PINYIN_DICT__, pEngine->pResult->word))
		{
			pEngine->pResult->type = __LEX_PINYIN_DICT__;
			if(NULL != pTextAttr) {
				pTextAttr->text_type = TEXT_TYPE_PY;
			}
		}

		return TC_WSErr_SUCCESS;
	}

	if (strlen(szU8Text) > 512)
	{
		pEngine->bInnerPostProcess = 0;
	}
	else
	{
		pEngine->bInnerPostProcess = 1;
	}

	strcpy(pEngine->szText, szU8Text);	
	pEngine->nTextLen = strlen(szU8Text);	
	pResult = pEngine->pResult;
	pEngine->nResult = 0;
	for (i = 0; i < WS_TOKEN_MAX_NUM; i++) {
		pResult[i].type = 0;		
		pResult[i].is_modified = 0;		
		pResult[i].word[0] = 0;
		pResult[i].word_delpunc[0] = 0;
		pResult[i].word_tts[0] = 0;		
	}	

	//memset(pResult, 0, sizeof(pEngine->pResult));

#if 1  //规避下面#else里的friso分词动态内存分配
#if LOG_MODULE_COST_TIME	
	g_t1 = GetCostTimeMs();
	if (g_print_time) {
		printf(" [xxxx2] cost time = %d ms\r\n", g_t1 - g_t2);
	}
#endif
	nRet = ws_engine_get_base_segmentation(pEngine);
#if LOG_MODULE_COST_TIME
	g_t2 = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, " 001 [ws_engine_get_base_segmentation] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	if (g_print_time) {
		printf(" 001 [ws_engine_get_base_segmentation] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	time_add += (g_t2 - g_t1);
#endif
#ifdef WIN32
	int n1 = 0;
	for (i = 0; i < pEngine->nResult; i++) {
		n1 += strlen(pResult[i].word);
		//ivAssert(0 == strlen(pResult[i].word_delpunc));
		//ivAssert(0 == strlen(pResult[i].word_tts));
		ivAssert(n1 <= 512);
	}
#endif
#else
	friso_set_text(pEngine->task, (fstring)szU8Text);
	config = pEngine->config;
	task = pEngine->task;
	while (NULL != (config->next_token(pEngine->friso, config, task)))
	{
		pResult[pEngine->nResult].type = task->token->type;
		pResult[pEngine->nResult].length = task->token->length;
		pResult[pEngine->nResult].rlen = task->token->rlen;
		// pResult[pEngine->nResult].pos = task->token->pos;
		pResult[pEngine->nResult].offset = task->token->offset;
		memcpy(pResult[pEngine->nResult].word, task->token->word, strlen(task->token->word));

		//异常处理：分词个数达到上限，则将剩下的文本放在一个分词里
		if (WS_TOKEN_MAX_NUM - 1 == pEngine->nResult)
		{
			int nLen = strlen(pEngine->szText + pResult[pEngine->nResult].offset);
			if (nLen >= WS_PER_TOKEN_MAX_LEN)
			{
				//文本过长，暂时直接丢弃也不能越界，理论上分词个数和支持的字节数保持一致，不会出现这种异常
				nLen = WS_PER_TOKEN_MAX_LEN;
			}

			memcpy(pResult[pEngine->nResult].word, pEngine->szText + pResult[pEngine->nResult].offset, nLen);
			pResult[pEngine->nResult].length = (uchar_t)(strlen(pResult[pEngine->nResult].word));
			ivAssert(pResult[pEngine->nResult].length < sizeof(pResult[0].word));
			pResult[pEngine->nResult].rlen = pResult[pEngine->nResult].length;
			pResult[pEngine->nResult].type = __LEX_UNKNOW_WORDS__;
			pEngine->nResult++;
			break;
		}

		pEngine->nResult++;

		if (pEngine->nResult > (int)(WS_TOKEN_MAX_NUM - 20) && nCallCnt < nCallCntLimit) //防止内存不够用，先合并一轮
		{
			//对结果二次处理，和词典进行全匹配，以便实现一些特殊分词，如[清平乐·宫怨]
			nRet = wordseg_dict_wholematch(handle); //长句子耗时函数
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
			nCallCnt++; //控制次数，否则乱扫长句子，很耗时
		}
	} // while (NULL != (config->next_token(friso, config, task)))
#endif

#if 0
	for (i = 0; i < pEngine->nResult; i++)
	{
		printf("%s/", pEngine->pResult[i].word);
	}
	printf("\r\n");
#endif

	if (pEngine->nResult > 1)
	{
#if 0
		//处理数字、英文连接符/-&.:'前后是英文则合并
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_process_connector(handle);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 002 [ws_engine_process_connector] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 002 [ws_engine_process_connector] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
#endif

		//对结果二次处理，实现英文词库分词
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_process_en(pEngine);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 003 [ws_engine_process_en] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 003 [ws_engine_process_en] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif	
		
		if (pEngine->bSupportPostProcess && pEngine->bInnerPostProcess)
		{
			////处理中文年份，如“一九四五年”-> 一九四五/年/
#if LOG_MODULE_COST_TIME
			g_t1 = GetCostTimeMs();
#endif
			nRet = ws_engine_process_chineseyear(handle);
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
#if LOG_MODULE_COST_TIME
			g_t2 = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, " 004 [ws_engine_process_chineseyear] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			if (g_print_time) {
				printf(" 004 [ws_engine_process_chineseyear] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			time_add += (g_t2 - g_t1);
#endif
			//对结果二次处理，实现小数、%、‰分到一起   耗时长XXXX
#if LOG_MODULE_COST_TIME
			g_t1 = GetCostTimeMs();
#endif
			nRet = ws_engine_process_decimal(handle);
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
#if LOG_MODULE_COST_TIME
			g_t2 = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, " 005 [ws_engine_process_decimal] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			if (g_print_time) {
				printf(" 005 [ws_engine_process_decimal] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			time_add += (g_t2 - g_t1);
#endif

			//处理中文数字，如“一千五百”，需要合并成一个分词    耗时长XXXX
#if 0
#if LOG_MODULE_COST_TIME
			g_t1 = GetCostTimeMs();
#endif
			nRet = ws_engine_process_chinesenum(handle);
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
#if LOG_MODULE_COST_TIME
			g_t2 = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, " 006 [ws_engine_process_chinesenum] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			if (g_print_time) {
				printf(" 006 [ws_engine_process_chinesenum] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			time_add += (g_t2 - g_t1);
#endif
#endif

		} // if (pEngine->bSupportPostProcess)

#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		  //更新下符号类型   先放这，后续优化
		for (i = 0; i < pEngine->nResult; i++)
		{
			if((pResult[i].type < __LEX_END && pResult[i].type != __LEX_CORNERMARK__) || __LEX_WHITESPACE__ == pResult[i].type)	continue;

			lex_entry_cdt_t lex = friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pResult[i].word);
			if (NULL != lex)
			{
				pResult[i].type = __LEX_PUNCTUATION__;
			}
			//else if (NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_TY_SPECIAL_WORDS__, __LEX_TY_SPECIAL_WORDS__, pResult[i].word))
			//{
			//	pResult[i].type = __LEX_TY_SPECIAL_WORDS__;
			//}
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 025 [update punctuation type] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 025 [update punctuation type] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif

		//对结果二次处理，和词典进行全匹配，以便实现一些特殊分词，如[清平乐·宫怨]
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_dict_wholematch(handle); //长句子耗时函数
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 007 [ws_engine_dict_wholematch] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 007 [ws_engine_dict_wholematch] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif

		if (pEngine->bSupportPostProcess)
		{
			//一些特殊处理，如“1924s”分到一起
			//数字:数字..分到一起， 如“12:14”，”12:14:15“
#if LOG_MODULE_COST_TIME
			g_t1 = GetCostTimeMs();
#endif
			nRet = ws_engine_process_other(handle);
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
#if LOG_MODULE_COST_TIME
			g_t2 = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, " 008 [ws_engine_process_other] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			if (g_print_time) {
				printf(" 008 [ws_engine_process_other] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			time_add += (g_t2 - g_t1);
#endif
		} // if (pEngine->bSupportPostProcess)

		//设置分词结果里标点符号类型
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_set_punc_type(handle);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 009 [ws_engine_set_punc_type] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 009 [ws_engine_set_punc_type] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif

#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		for (i = 0; i < pEngine->nResult; i++) //防止遗漏
		{
			pEngine->pResult[i].word[pEngine->pResult[i].length] = '\0';
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 888 [xxxx1] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 888 [xxxx1] cost time = %d ms\r\n", g_t2 - g_t1);			
		}
		time_add += (g_t2 - g_t1);
#endif

		if (pEngine->bSupportPostProcess)
		{
#if SUPPORT_PROCESS_CORNER_MARKER
			//角标处理
#if LOG_MODULE_COST_TIME
			g_t1 = GetCostTimeMs();
#endif
			nRet = ws_engine_process_cornermarker(handle);
			if (TC_WSErr_SUCCESS != nRet)
			{
				return nRet;
			}
#endif
#if LOG_MODULE_COST_TIME
			g_t2 = GetCostTimeMs();
			if (NULL != g_fpCostTime) {
				fprintf(g_fpCostTime, " 011 [ws_engine_process_cornermarker] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			if (g_print_time) {
				printf(" 011 [ws_engine_process_cornermarker] cost time = %d ms\r\n", g_t2 - g_t1);
			}
			time_add += (g_t2 - g_t1);
#endif

			if (pEngine->bInnerPostProcess)
			{
				//处理汉字之前有空格导致的分词分不到一起的问题 20201029
#if LOG_MODULE_COST_TIME
				g_t1 = GetCostTimeMs();
#endif
				nRet = ws_engine_process_whitespace(handle);
				if (TC_WSErr_SUCCESS != nRet)
				{
					return nRet;
				}
#if LOG_MODULE_COST_TIME
				g_t2 = GetCostTimeMs();
				if (NULL != g_fpCostTime) {
					fprintf(g_fpCostTime, " 012 [ws_engine_process_whitespace] cost time = %d ms\r\n", g_t2 - g_t1);
				}
				if (g_print_time) {
					printf(" 012 [ws_engine_process_whitespace] cost time = %d ms\r\n", g_t2 - g_t1);
				}
				time_add += (g_t2 - g_t1);
#endif

				//结尾符号处理
#if LOG_MODULE_COST_TIME
				g_t1 = GetCostTimeMs();
#endif
				nRet = ws_engine_process_endpunc(handle);
				if (TC_WSErr_SUCCESS != nRet)
				{
					return nRet;
				}
#if LOG_MODULE_COST_TIME
				g_t2 = GetCostTimeMs();
				if (NULL != g_fpCostTime) {
					fprintf(g_fpCostTime, " 013 [ws_engine_process_endpunc] cost time = %d ms\r\n", g_t2 - g_t1);
				}
				if (g_print_time) {
					printf(" 013 [ws_engine_process_endpunc] cost time = %d ms\r\n", g_t2 - g_t1);
				}
				time_add += (g_t2 - g_t1);
#endif
			}
		}
	}
	else
	{
		if (__LEX_PUNCTUATION__ != pResult[0].type && NULL != friso_dic_get(pEngine->pResHdr,pEngine->func_read_data_res, __LEX_PUNCTUATION__, __LEX_PUNCTUATION__, pResult[0].word))
		{
			pResult[0].type = __LEX_PUNCTUATION__;
		}
		strcpy(pResult[0].word_delpunc, pResult[0].word);
	}

	//额外零碎处理:拼音中间无空格被分到一起的情况   耗时长XXXXX
#if 0
	if (pEngine->bSupportPostProcess && pEngine->bInnerPostProcess)
	{		
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_process_specialcase(handle);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 021 [ws_engine_process_specialcase] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 021 [ws_engine_process_specialcase] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
	}
#endif

	//每条分词的翻译和tts结果针对拼音单独处理
#if LOG_MODULE_COST_TIME
	g_t1 = GetCostTimeMs();
#endif
	nRet = ws_engine_process_tts_trans_text(handle); //长句子耗时函数
	if (TC_WSErr_SUCCESS != nRet)
	{
		return nRet;
	}
#if LOG_MODULE_COST_TIME
	g_t2 = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, " 014 [ws_engine_get_tts_trans_text] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	if (g_print_time) {
		printf(" 014 [ws_engine_get_tts_trans_text] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	time_add += (g_t2 - g_t1);
#endif

	//拼音拼读拆分处理：TTS发音标注、支持继续细分
#if LOG_MODULE_COST_TIME
	g_t1 = GetCostTimeMs();
#endif
	nRet = ws_engine_process_pinduchaifen(pEngine);
	if (TC_WSErr_SUCCESS != nRet)
	{
		return nRet;
	}
#if LOG_MODULE_COST_TIME
	g_t2 = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, " 016 [ws_engine_process_pinduchaifen] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	if (g_print_time) {
		printf(" 016 [ws_engine_process_pinduchaifen] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	time_add += (g_t2 - g_t1);
#endif

	//针对纯数字串，类型改为整数，以便后续数字翻译和数字串文本类型=中文
	if (1 == pEngine->nResult && __LEX_OTHER_WORDS__ == pResult->type && utf8_numeric_string(pResult->word_delpunc))
	{
		pResult->type = __LEX_NUMERIC__;
	}

	//获取输入文本类型：中文/英文/拼音
	if (pEngine->bProcessTransTxt || NULL != pTextAttr)
	{
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_get_texttype_new(handle, szU8Text, 1, pEngine->pTextAttr);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 017 [ws_engine_get_texttype_new] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 017 [ws_engine_get_texttype_new] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
	}

	//获取文本类型和修正后文本
	if (NULL != pTextAttr)
	{ //用户传入的参数
		memcpy(pTextAttr, pEngine->pTextAttr, sizeof(pEngine->pTextAttr));
		pTextAttr->b_chk_valid_wordseg = bChkValidWord;

		//新需求：针对"取单个单词时，容易取到前后单词的一部分"预处理 20201214
		//处理规则：所有分词是 "一个有效单词 + 若干符号 +若干无意义英文字母串"组合，则处理成只留“一个有效单词”（p_english_word.lex存在的）
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_process_enword_beg_end_invalidstr(handle, pTextAttr);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 019 [ws_engine_process_enword_beg_end_invalidstr] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 019 [ws_engine_process_enword_beg_end_invalidstr] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
	}

	if (pEngine->bSupportPostProcess && pEngine->bInnerPostProcess)
	{
		//新需求：若首尾都是符号，且中间是词典里的某个词组，则默认选中
		//对分词结果二次处理，如果出了符号外，只有一个有效分词，则设置valid_wordseg_id为有效分词
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_set_valid_wordseg_id(handle, pTextAttr);
		if (TC_WSErr_SUCCESS != nRet)
		{
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 021 [ws_engine_set_valid_wordseg_id] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 021 [ws_engine_set_valid_wordseg_id] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
	}

	//若设置支持分句，先尝试进行分句 2021.7.14
#if 1
	if (pEngine->bSupportSentenceSeg)
	{
		int bSuccess = 0;
#if LOG_MODULE_COST_TIME
		g_t1 = GetCostTimeMs();
#endif
		nRet = ws_engine_sentence_seg(handle, &bSuccess, pTextAttr);
		if (TC_WSErr_SUCCESS != nRet)
		{
			ivAssert(0);
			return nRet;
		}
#if LOG_MODULE_COST_TIME
		g_t2 = GetCostTimeMs();
		if (NULL != g_fpCostTime) {
			fprintf(g_fpCostTime, " 022 [ws_engine_sentence_seg] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		if (g_print_time) {
			printf(" 022 [ws_engine_sentence_seg] cost time = %d ms\r\n", g_t2 - g_t1);
		}
		time_add += (g_t2 - g_t1);
#endif
	}
#endif

#if LOG_MODULE_COST_TIME
	g_t1 = GetCostTimeMs();
#endif
	if (NULL != pTextAttr && 0 != strcmp(szU8Text, pTextAttr->word_delpunc))
	{
		pTextAttr->bTextModified = 1;
	}

	ws_engine_set_result_offset(pEngine);

#if LOG_MODULE_COST_TIME
	g_t2 = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, " 888 [xxxx2] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	if (g_print_time) {
		printf(" 888 [xxxx2] cost time = %d ms\r\n", g_t2 - g_t1);
	}
	time_add += (g_t2 - g_t1);
#endif
	
	*pnResult = pEngine->nResult;
	*ppResult = pEngine->pResult;

	ws_engine_texttype_convert(pEngine, pEngine->pResult, pEngine->nResult);

#ifdef _DEBUG
	for (i = 0; i < pEngine->nResult; i++)
	{
		ivAssert(strlen(pEngine->pResult[i].word) > 0);
		ivAssert(strlen(pEngine->pResult[i].word_delpunc) > 0);
		ivAssert(strlen(pEngine->pResult[i].word_tts) > 0);
		ivAssert(strlen(pEngine->pResult[i].word) <= WS_PER_TOKEN_MAX_LEN);
		ivAssert(strlen(pEngine->pResult[i].word_delpunc) <= WS_PER_TOKEN_MAX_LEN);
		ivAssert(strlen(pEngine->pResult[i].word_tts) <= WS_PER_TOKEN_MAX_LEN);
	}
#endif

#if LOG_MODULE_COST_TIME
	g_tEnd = GetCostTimeMs();
	if (NULL != g_fpCostTime) {
		fprintf(g_fpCostTime, "textlen=%d, language=%d,  all cost time = %d ms\r\n\r\n", strlen(szU8Text), pEngine->nPriorLanguage, g_tEnd - g_tBegin);
	}
	if (g_print_time) {
		printf("textlen=%d, language=%d,  all cost time = %d ms\r\n\r\n", strlen(szU8Text), pEngine->nPriorLanguage, g_tEnd - g_tBegin);
		printf("all api time = %d\r\n", time_add);
	}

	if (NULL != g_fpCostTime) {
		fflush(g_fpCostTime);
	}
#endif

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL != g_fpReadResLog) {
		if (g_nReadResCnt > g_nCallLexCntMax) g_nCallLexCntMax = g_nReadResCnt;
		fprintf(g_fpReadResLog, "\r\ntext_len=%d\ttext=%s\r\n", strlen(pEngine->szText), pEngine->szText);
		fprintf(g_fpReadResLog, "func=%s\tread res cnt=%d\tsize=%d\tmax_cnt=%d\r\n", __FUNCTION__, g_nReadResCnt, g_nReadResSize, g_nCallLexCntMax);
		fprintf(g_fpReadResLog, "read_res_all_cnt=%d, ", g_nReadResCnt);
		if (g_nReadResCntPerLex[__LEX_CN_CHAR__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_CN_CHAR__=%d, ", g_nReadResCntPerLex[__LEX_CN_CHAR__]);
		}
		if (g_nReadResCntPerLex[__LEX_TY_CN_WORDS__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_TY_CN_WORDS__=%d, ", g_nReadResCntPerLex[__LEX_TY_CN_WORDS__]);
		}
		if (g_nReadResCntPerLex[__LEX_TY_EN_WORDS__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_TY_EN_WORDS__=%d, ", g_nReadResCntPerLex[__LEX_TY_EN_WORDS__]);
		}		
		if (g_nReadResCntPerLex[__LEX_PUNCTUATION__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_PUNCTUATION__=%d, ", g_nReadResCntPerLex[__LEX_PUNCTUATION__]);
		}
		if (g_nReadResCntPerLex[__LEX_CORNERMARK__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_CORNERMARK__=%d, ", g_nReadResCntPerLex[__LEX_CORNERMARK__]);
		}
		if (g_nReadResCntPerLex[__LEX_PINYIN_DICT__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_PINYIN_DICT__=%d, ", g_nReadResCntPerLex[__LEX_PINYIN_DICT__]);
		}
		if (g_nReadResCntPerLex[__LEX_PINYIN_SOFT_DICT__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_PINYIN_SOFT_DICT__=%d, ", g_nReadResCntPerLex[__LEX_PINYIN_SOFT_DICT__]);
		}
		if (g_nReadResCntPerLex[__LEX_PINYIN_CHAIFEN_DICT__] > 0) {
			fprintf(g_fpReadResLog, "__LEX_PINYIN_CHAIFEN_DICT__=%d, ", g_nReadResCntPerLex[__LEX_PINYIN_CHAIFEN_DICT__]);
		}	
		fprintf(g_fpReadResLog, "\r\n");		
		fflush(g_fpReadResLog);
	}
#endif
	//printf("func=wordseg_do\tread res cnt=%d\tsize=%d\r\n", g_nReadResCnt, g_nReadResSize);
#endif

	return nRet;
}

WS_API int wordseg_get_tts_text(void *handle, const char *pInputText, char *pOutputText, int nOutputTextSize)
{
	PWordSegEgn pEngine = NULL;
	if (NULL == handle || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

#if PRINT_RES_READ
	// printf("-------------------%s begin----------------------\r\n", __FUNCTION__);
	g_nReadResCnt = 0;
	memset(g_nReadResCntPerLex, 0, sizeof(g_nReadResCntPerLex));
	g_nReadResSize = 0;
#endif

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	//查询TTS用户词典，修正TTS发音标注 202010811 qungao
	int ret = tts_userdict_match(pEngine, (char *)pInputText, pOutputText, nOutputTextSize);

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL != g_fpReadResLog) {
		if (g_nReadResCnt > g_nCallLexCntMax) g_nCallLexCntMax = g_nReadResCnt;
		fprintf(g_fpReadResLog, "\r\ntext_len=%d\ttext=%s\r\n", strlen(pInputText), pInputText);
		fprintf(g_fpReadResLog, "func=%s\tread res cnt=%d\tsize=%d\tmax_cnt=%d\r\n", __FUNCTION__, g_nReadResCnt, g_nReadResSize, g_nCallLexCntMax);
		fflush(g_fpReadResLog);
	}
#endif
	//printf("func=wordseg_get_tts_text\tread res cnt=%d\tsize=%d\r\n", g_nReadResCnt, g_nReadResSize);
#endif

	return ret;
}

WS_API int wordseg_get_ise_text(void *handle, const char *pInputText, text_language_type text_type, char *pOutputText, int nOutputTextSize)
{
	PWordSegEgn pEngine = NULL;
	if (NULL == handle || NULL == pInputText || NULL == pOutputText)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

#if PRINT_RES_READ
	// printf("-------------------%s begin----------------------\r\n", __FUNCTION__);
	g_nReadResCnt = 0;
	g_nReadResSize = 0;
	memset(g_nReadResCntPerLex, 0, sizeof(g_nReadResCntPerLex));
#endif

	pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}
	
	if (text_type != TEXT_TYPE_CN && text_type != TEXT_TYPE_EN)
	{
		ivAssert(0);
		return TC_WSErrID_InvIseType;
	}

	//查询评测用户词典，进行评测文本预干预
	int ret = ise_userdict_match(pEngine, text_type, (char *)pInputText, pOutputText, nOutputTextSize);

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL != g_fpReadResLog) {
		if (g_nReadResCnt > g_nCallLexCntMax) g_nCallLexCntMax = g_nReadResCnt;
		fprintf(g_fpReadResLog, "\r\ntext_len=%d\ttext=%s\r\n", strlen(pInputText), pInputText);
		fprintf(g_fpReadResLog, "func=%s\tread res cnt=%d\tsize=%d\tmax_cnt=%d\r\n", __FUNCTION__, g_nReadResCnt, g_nReadResSize, g_nCallLexCntMax);
		fflush(g_fpReadResLog);
	}
#endif
	//printf("func=wordseg_get_ise_text\tread res cnt=%d\tsize=%d\r\n", g_nReadResCnt, g_nReadResSize);
#endif

	return ret;
}

WS_API int wordseg_get_texttype(void *handle, const char *szU8Text, text_attr_t * pTextAttr)
{
#if PRINT_RES_READ
	// printf("-------------------%s begin----------------------\r\n", __FUNCTION__);
	g_nReadResCnt = 0;
	g_nReadResSize = 0;
#endif
	int ret = ws_engine_get_texttype_new(handle, szU8Text, 0, pTextAttr);

#if PRINT_RES_READ
#ifdef WIN32
	if (NULL != g_fpReadResLog) {
		if (g_nReadResCnt > g_nCallLexCntMax) g_nCallLexCntMax = g_nReadResCnt;
		fprintf(g_fpReadResLog, "\r\ntext_len=%d\ttext=%s\r\n", strlen(szU8Text), szU8Text);
		fprintf(g_fpReadResLog, "func=%s\tread res cnt=%d\tsize=%d\tmax_cnt=%d\r\n", __FUNCTION__, g_nReadResCnt, g_nReadResSize, g_nCallLexCntMax);
		fflush(g_fpReadResLog);
	}
#endif
	//printf("func=ws_engine_get_texttype\tread res cnt=%d\tsize=%d\r\n", g_nReadResCnt, g_nReadResSize);
#endif

	return ret;
}

WS_API int wordseg_set_memcallback(void *handle, const PWSMallocCallBack mallocFunc, const PWSFreeCallBack freeFunc)
{
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

	pEngine->func_malloc = mallocFunc;
	pEngine->func_free = freeFunc;

	return 0;
}

//分词引擎逆初始化
WS_API int wordseg_uninit(void *handle)
{
	PWordSegEgn pEngine = NULL;
	//friso_task_t task = NULL;

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

// 	int ret = friso_uninit(pEngine->friso, NULL, NULL, pEngine->bHasDict);
// 	if (0 != ret) {
// 		ivAssert(0);
// 		return TC_WSErrID_Uninit_Failed;
// 	}
// 	pEngine->bHasDict = 0;

#if TEST_MEMORY_LEAK
	fclose(g_fpMemMallocLog);
	fclose(g_fpMemFreeLog);
	g_fpMemMallocLog = NULL;
	g_fpMemFreeLog = NULL;
	printf("wordseg_init/do/uninit 内存分析中...\r\n");
	AnalyseMemoryLeak(); //内存泄漏检查，对比malloc和free里是否完全匹配
#endif

#if LOG_MODULE_COST_TIME
	if (NULL != g_fpCostTime) {
		fclose(g_fpCostTime);
	}
#endif

	return TC_WSErr_SUCCESS;
}

WS_API const char *wordseg_getversion(void)
{
	return g_szVersion;
}

//将加载的词典地址重定向后保存为二进制文件，以便减少初始化时间
#ifdef WIN32
WS_API int __stdcall wordseg_builddict(const char *lex_dir, const char *hash_res_file, const char *data_res_file)
#else
WS_API int wordseg_builddict(const char *lex_dir, const char *hash_res_file, const char *data_res_file)
#endif
{
	return wordseg_save_dict_to_bin(lex_dir, hash_res_file, data_res_file);

}

#if 0
/*
*  设置回调函数
* @param handle     [in] wordseg_init返回的工作句柄
* @param func_type  [in] 回调函数类型
* @param func	    [in] 回调函数，目前支持设置对资源data.bin的读取方式：支持文件读取和内存读取，
文件读取：wordseg_init中data_res需要传句柄，应用层负责文件句柄打开和关闭
内存读取：wordseg_init中data_res需要传内存首地址
*/
int wordseg_register_callback(void *handle, CallBackType func_type, const PWSCallBack func)
{
	if (NULL == handle)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	PWordSegEgn pEngine = (PWordSegEgn)handle;
	if (WORDSEG_ENGINE_CHECK != pEngine->dwCheck)
	{
		ivAssert(0);
		return TC_WSErrID_InvHandle;
	}

	if (func_type < CallBackFuncNameCount && func_type >= 0) {
		//pEngine->pFuncCallBack[func_type] = func;
		pEngine->func_read_data_res = func;
		pEngine->friso->func_read_data_res = func;
	}
	else {
		return TC_WSErrID_InvArg;
	}

	return TC_WSErr_SUCCESS;
}
#endif

//提供接口将friso_token_t分词结果格式转为新格式wordseg_result *，以便esp32使用
WS_API int wordseg_result_convert(friso_token_t pResult, int nResult, wordseg_result_t * pResultExt, int *pnResultExtSize)
{
	int i;
	if (NULL == pResult)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	int need_size = sizeof(wordseg_result_t) * nResult;
	for (i = 0; i < nResult; i++)
	{
		need_size += strlen(pResult[i].word) + 1;
		need_size += strlen(pResult[i].word_delpunc) + 1;
		need_size += strlen(pResult[i].word_tts) + 1;
	}	

	if (*pnResultExtSize < need_size)
	{		
		*pnResultExtSize = need_size;
		return TC_WSErrID_OutOfMemory;
	}

	if (NULL == pResultExt)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}

	int offset = sizeof(wordseg_result_t) * nResult;
	char *text = (char *)pResultExt + offset;
	for (i = 0; i < nResult; i++)
	{
		pResultExt[i].type = pResult[i].type;
		pResultExt[i].length = pResult[i].length;
		pResultExt[i].is_modified = pResult[i].is_modified;
		pResultExt[i].offset = pResult[i].offset;

		//copy word
		int j = 0;
		while (0 != pResult[i].word[j])
		{
			text[j] = pResult[i].word[j];
			j++;
		}
		text[j] = 0;		
		pResultExt[i].word = text;
		text += j + 1;

		//copy word_delpunc
		j = 0;
		while (0 != pResult[i].word_delpunc[j])
		{
			text[j] = pResult[i].word_delpunc[j];
			j++;
		}
		text[j] = 0;
		pResultExt[i].word_delpunc = text;
		text += j + 1;

		//copy word_tts
		j = 0;
		while (0 != pResult[i].word_tts[j])
		{
			text[j] = pResult[i].word_tts[j];
			j++;
		}
		text[j] = 0;
		pResultExt[i].word_tts = text;
		text += j + 1;
	}
	ivAssert(text - (char *)pResultExt == need_size);

	*pnResultExtSize = need_size;

	return TC_WSErr_SUCCESS;
}


/* 功能：检查输入文本是否是最小分词单元进行判断，如一个汉字、符号、英文字母串、数字串等
* @param text	[in] 输入字符串，UTF8格式
* 返回值：是最小分词单元返回非0值，否则返回0值。如单个汉字、符号、英文字母数字串、纯数字串都会返回1
*/
static int wordseg_text_is_minunit(const char *text, friso_lex_t * type)
{
	if (NULL == text)
	{
		ivAssert(0);
		return 0;
	}

	if (string_is_en_words(text, 0))
	{
		*type = __LEX_TY_EN_WORDS__;
		return 1;
	}

	if (string_is_numeric(text))
	{
		*type = __LEX_NUMERIC__;
		return 1;
	}

	if (get_utf8_bytes(text[0]) == strlen(text))
	{
		*type = __LEX_CN_CHAR__;
		return 1;
	}

	//分词最小单元：一个utf8字符、英文字母和数字串组成的字符串
	//if (get_utf8_bytes(text[0]) == strlen(text) || string_is_en_words(text) || string_is_numeric(text))
	//{
	//	return 1;
	//}

	return 0;
}

/**
* @brief 资源打包通用接口，供内容后台集成使用，只需传入输入目录和输出目录，不关心生成几个文件
* @param szLexDir [in]词典lex和ini所在目录，如"./resource/lex_taoyun/"，注意事项：目录结尾一定要有"/"or"\\"
* @param szOutDir [in] 输出文件目录;
* @retval 错误码，请见wordseg_err.h
* @note 不需要在csk上实现
**/
#ifdef WIN32
WS_API int __stdcall wordseg_unified_builddict(const char *szLexDir, const char *szOutDir)
#else
WS_API int wordseg_unified_builddict(const char *szLexDir, const char *szOutDir)
#endif
{
	//WS_API int __stdcall wordseg_builddict(const char *szLexDir, const char *szHashResFile, const char *szDataResFile);
	char hashpath[512] = { 0 }, datapath[512] = { 0 };
	sprintf(hashpath, "%s\\%s", szOutDir, "hash.bin");
	sprintf(datapath, "%s\\%s", szOutDir, "data.bin");
	return wordseg_builddict(szLexDir, hashpath, datapath);
}
