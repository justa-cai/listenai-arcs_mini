
#include "wordseg_kernel.h"
#include "wordseg_res.h"
#include "friso.h"
#include "wordseg_engine.h"
#include "friso_UTF8.h"


#define ivGridSize(n, m) ((size_t)(((size_t)(n) + ((m) - 1)) & (~(m) + 1)))


#ifdef WIN32
static int SaveBinToFile(const char *szFile, char *buf, int buf_size);
static char *ReadFileBin(const char *szFile, int *pnSize);
static void TestResIsOk(const char *hash_res_file, const char *data_res_file, char *log_dir);
static void StateLexWordInfo(friso_entry_t friso, const char *resultFile);
#endif
static char *get_hash_data_by_callback(PHashTab pHashTab, uint_t nHashLength, void *pTextRes, PWSCallBack pFuncReadTextRes, char* pWord, void **pBuffer, uint_t nBufferSize);
char *get_hash_data_by_callback_ext(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res, uint_t bucket, void **buffer, uint_t buffer_size);

#if TEST_LEX_IN_RAM
static char *get_hash_data_by_ram(PHashTab hashTab, uint_t hashTab_len, PHashDataInRam dataInfo, char* word, void **buffer, uint_t buffer_size);
#endif
static void make_crc(short *pData, unsigned int nSize, unsigned int * pnCRC, unsigned int * piCounter);

//生成二进制分词资源
FRISO_API int wordseg_save_dict_to_bin(const char *lex_dir, const char *hash_res_file, const char *data_res_file)
{
	int ret = 0;	
	uint_t hash_file_write_size = 0, data_file_write_size = 0;
	uint_t i, j;
	hash_entry_t e;
	friso_hash_cdt_t dic = NULL;
	friso_hash_cdt_t pDic = NULL;
	PHashResHdr hash_res_hdr = NULL;
	PDataResHdr data_res_hdr = NULL;
	PDataHdr dataHdr = NULL;
	uint_t hash_data_match_crc = 0, hash_data_match_crc_size = 0;

	if (NULL == lex_dir || NULL == hash_res_file || NULL == data_res_file)
	{
		ivAssert(0);
		return TC_WSErrID_InvArg;
	}
#ifdef WIN32
	printf("lex_dir		 =%s\r\n", lex_dir);
	printf("hash_res_file=%s\r\n", hash_res_file);
	printf("data_res_file=%s\r\n", data_res_file);
#endif

	friso_entry *friso = NULL;
	friso_config_entry *config = NULL;
	friso_task *task = NULL;

	friso = (friso_entry *)FRISO_MALLOC(sizeof(friso_entry));
	config = (friso_config_entry *)FRISO_MALLOC(sizeof(friso_config_entry));
	task = (friso_task *)FRISO_MALLOC(sizeof(friso_task));
	if (NULL == friso || NULL == config || NULL == task) {
		ivAssert(0);
		return TC_WSErrID_BuildRes_Failed;
	}

	FILE *fp_hash = fopen(hash_res_file, "wb");
	if (NULL == fp_hash)
	{
		ivAssert(0);
		return TC_WSErrID_OpenFile;
	}
	FILE *fp_data = fopen(data_res_file, "wb");
	if (NULL == fp_data)
	{
		ivAssert(0);
		return TC_WSErrID_OpenFile;
	}

	ret = friso_init(friso, config, task, lex_dir);
	if (0 != ret)
	{
		return ret;
	}

	ivAssert(TEXT_ONE_BLOCK_SIZE < SD_CARD_SECTOR_SIZE);
	char *fill_buf = (char *)FRISO_MALLOC(SD_CARD_SECTOR_SIZE);
	memset(fill_buf, 0, SD_CARD_SECTOR_SIZE);

	int dataHdr_malloc_size = sizeof(TDataHdr) + 1024; //变长数组
	dataHdr = (PDataHdr)FRISO_MALLOC(dataHdr_malloc_size);
	if (NULL == dataHdr)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}

	hash_res_hdr = (PHashResHdr)FRISO_MALLOC(sizeof(THashResHdr));
	if (NULL == hash_res_hdr)
	{
		ivAssert(0);
		return TC_WSErrID_OutOfMemory;
	}
	memset(hash_res_hdr, 0, sizeof(THashResHdr));
	hash_res_hdr->check_str = WORDSEG_RES_CHECK_NEW;
	hash_res_hdr->file_type = HASH_TAB_RES_FILE;

	if (sizeof(TDataResHdr) % SD_CARD_SECTOR_SIZE != 0)
	{
		//为了裸读TF卡，每次读取位置是block_size的倍数减少二次memcpy
		ivAssert(0);
		return TC_WSErrID_DataResNotAlign;
	}

	data_res_hdr = (PDataResHdr)FRISO_MALLOC(sizeof(TDataResHdr));
	if (NULL == data_res_hdr)
	{
		ivAssert(0);
		FRISO_FREE(hash_res_hdr);
		return TC_WSErrID_OutOfMemory;
	}
	memset(data_res_hdr, 0, sizeof(TDataResHdr));
	data_res_hdr->check_str = WORDSEG_RES_CHECK_NEW;
	data_res_hdr->file_type = TEXT_RES_FILE;

	pDic = hash_res_hdr->dic;
	memcpy(pDic, friso->dic, sizeof(friso->dic));

	// hash文件存放顺序：ResHdr
	hash_file_write_size += fwrite(hash_res_hdr, 1, sizeof(THashResHdr), fp_hash);

	data_file_write_size += fwrite(data_res_hdr, 1, sizeof(TDataResHdr), fp_data);

	for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)
	{
		int hash_text_max_size = 0;

		pDic[i].table = NULL; //防止打包资源这个字段每次值不一样导致二进制不一致

		dic = friso->dic + i;
		if (dic->size == 0)
		{
			continue;
		}

		pDic[i].table_offset = hash_file_write_size;
		pDic[i].table_data_offset = data_file_write_size;

		int hash_all_text_size = 0;
		int text_size = 0, text_size_tmp = 0;
		for (j = 0; j < dic->length; j++)
		{
			e = dic->table[j];
			THashTab hash_tab[1] = {0};
			//pHashtab->nOffset = 0;
			hash_tab->mask = 0;
			if (NULL == e)
			{
				hash_file_write_size += fwrite(hash_tab, 1, sizeof(THashTab), fp_hash);
				make_crc((short *)hash_tab, sizeof(THashTab), &hash_data_match_crc, &hash_data_match_crc_size);
				continue;
			}
			//pHashtab->nOffset = nTextFileWriteSize;
			hash_tab->mask = (data_file_write_size << HASH_MARK_BLOCK_BIT);

			memset(dataHdr, 0, dataHdr_malloc_size);
			ushort_t word_num = 0;
			uint_t offset = 0;
			for (; NULL != e;)
			{
				lex_entry_cdt_t wordDsc = (lex_entry_cdt_t)e->_val;
				//pTextHdr->pnOffset[nWordsNum] = (ushort_t)nOffset;
				offset += sizeof(uchar_t);  //存储PTextInfo->len
				offset += wordDsc->length + 1;
				if (NULL != wordDsc->word_new) { //针对合成/评测/拼音等词典会有纠错文本
					offset += strlen(wordDsc->word_new) + 1;
				}
				e = e->_next;
				word_num++; // get word num
			}
			ivAssert(offset <= 65536);
			ivAssert(word_num < 256);
			dataHdr->n = (uchar_t)word_num;
			text_size = sizeof(TDataHdr);
			text_size += offset;		//一个hash对应的文本大小=头大小+文本大小
#if DATA_RES_BLOCK_ALIGN
			text_size = ivGridSize(text_size, SD_CARD_SECTOR_SIZE);
#endif
			hash_text_max_size = text_size > hash_text_max_size ? text_size : hash_text_max_size;
			uint_t block_size = (text_size + TEXT_ONE_BLOCK_SIZE - 1) / TEXT_ONE_BLOCK_SIZE;
			if (block_size > (1 << HASH_MARK_BLOCK_BIT)) {
				ivAssert(0);
				return TC_WSErrID_LexWordTooLong;
			}
			//pHashtab->nBlock = block_size;
			hash_tab->mask += block_size;
			hash_file_write_size += fwrite(hash_tab, 1, sizeof(THashTab), fp_hash);
			make_crc((short *)hash_tab, sizeof(THashTab), &hash_data_match_crc, &hash_data_match_crc_size);

			ushort_t nSize = (ushort_t)(sizeof(TDataHdr));			
			data_file_write_size += fwrite(dataHdr, 1, nSize, fp_data);
			make_crc((short *)dataHdr, nSize, &hash_data_match_crc, &hash_data_match_crc_size);
			text_size_tmp = sizeof(TDataHdr);

			e = dic->table[j];
			for (; NULL != e;)
			{
				int word_len = 0, word_new_len = 0;
				lex_entry_cdt_t pWordDsc = (lex_entry_cdt_t)e->_val;	

				word_len = pWordDsc->length;				
				uchar_t len_all = (uchar_t)(word_len + 1);
				if (NULL != pWordDsc->word_new) {
					word_new_len = strlen(pWordDsc->word_new);
					len_all += (uchar_t)(word_new_len + 1);
				}
				if (word_len + word_new_len + 2 > 256) { //使用uchar_t 存储的
					ivAssert(0);
					return TC_WSErrID_LexWordTooLong;
				}
				data_file_write_size += fwrite(&len_all, 1, sizeof(uchar_t), fp_data);				
				text_size_tmp += sizeof(uchar_t);
				data_file_write_size += fwrite(pWordDsc->word, 1, word_len +1, fp_data);
				make_crc((short *)(pWordDsc->word), word_len + 1, &hash_data_match_crc, &hash_data_match_crc_size);
				text_size_tmp += word_len + 1;

				if (word_new_len >0) {					
					data_file_write_size += fwrite(pWordDsc->word_new, 1, word_new_len +1, fp_data);
					make_crc((short *)(pWordDsc->word_new), word_new_len + 1, &hash_data_match_crc, &hash_data_match_crc_size);
					text_size_tmp += word_new_len + 1;
				}

				//读取词典内容
				word_num++;
				// fprintf(fpLog, "bucket=%-6d	length=%-2d				fre=%-7d	word=[%s]\r\n", j, pWordDsc->length, pWordDsc->fre, pWordDsc->word);
				// fprintf(fpLogAll, "bucket=%-6d	length=%-2d				fre=%-7d	lex=%d		word=[%s]\r\n", j, pWordDsc->length, pWordDsc->fre, i, pWordDsc->word);

				e = e->_next;
			}
#if DATA_RES_BLOCK_ALIGN				
			int fill_num = ivGridSize(text_size_tmp, SD_CARD_SECTOR_SIZE) - text_size_tmp;
			if (fill_num > 0)
			{
				data_file_write_size += fwrite(fill_buf, 1, fill_num, fp_data);
				make_crc(fill_buf, fill_num, &hash_data_match_crc, &hash_data_match_crc_size);
				text_size_tmp += fill_num;
			}
#endif
			ivAssert(text_size == text_size_tmp);
			hash_all_text_size += text_size;
			//nZeroNum = ivGridSize(nTextSize2, TEXT_ONE_BLOCK_SIZE) - nTextSize2;
			//nTextFileWriteSize += fwrite(pStrZero, 1, nZeroNum, fpText);
			//make_crc(pStrZero, nZeroNum, &crc, &crc_size);
		} // for (j = 0; j < dic->length; j++)
		hash_text_max_size += TEXT_ONE_BLOCK_SIZE;
		data_res_hdr->per_hash_text_max_size[i] = hash_text_max_size;
		data_res_hdr->hash_text_max_size = (uint_t)hash_text_max_size > data_res_hdr->hash_text_max_size ? (uint_t)hash_text_max_size : data_res_hdr->hash_text_max_size;
		pDic[i].table_data_size = hash_all_text_size;

	}	  // for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)

	//文件结尾补些0，防止按照block读取越位
	data_file_write_size += fwrite(fill_buf, 1, TEXT_ONE_BLOCK_SIZE, fp_data);
	make_crc((short *)fill_buf, TEXT_ONE_BLOCK_SIZE, &hash_data_match_crc, &hash_data_match_crc_size);

	fseek(fp_hash, 0, SEEK_SET);
	hash_res_hdr->hash_data_match_crc = hash_data_match_crc;
	strcpy(hash_res_hdr->version, WORDSEG_RES_VERSION);
	fwrite(hash_res_hdr, 1, sizeof(THashResHdr), fp_hash);
	fclose(fp_hash);
	
	fseek(fp_data, 0, SEEK_SET);	
	data_res_hdr->hash_data_match_crc = hash_data_match_crc;
	strcpy(data_res_hdr->version, WORDSEG_RES_VERSION);
	fwrite(data_res_hdr, 1, sizeof(TDataResHdr), fp_data);
	fclose(fp_data);	

#ifdef WIN32
	//增加文件的crc值，以便esp32平台判断是否需要拷贝资源到预留sector
	char *buf = NULL;
	int buf_size = 0;
	buf = ReadFileBin(data_res_file, &buf_size);
	uint_t crc = 0, crc_size = 0;
	make_crc((short *)(buf + sizeof(TDataResHdr)), buf_size - sizeof(TDataResHdr), &crc, &crc_size);
	PDataResHdr ptr = (PDataResHdr)buf;
	ptr->file_crc = crc;
	ptr->file_crc_size = crc_size;
	SaveBinToFile(data_res_file, buf, buf_size);
	free(buf);

	buf = ReadFileBin(hash_res_file, &buf_size);
	crc = 0;
	crc_size = 0;
	make_crc((short *)(buf + sizeof(THashResHdr)), buf_size - sizeof(THashResHdr), &crc, &crc_size);	
	PHashResHdr ptr2 = (PHashResHdr)buf;
	ptr2->file_crc = crc;
	ptr2->file_crc_size = crc_size;
	SaveBinToFile(hash_res_file, buf, buf_size);
	free(buf);

	printf("hash res size = %.2f MB\r\n", hash_file_write_size * 1.0 / 1024 / 1024);
	printf("text res size = %.2f MB\r\n", data_file_write_size * 1.0 / 1024 / 1024);
	//StateLexWordInfo(friso, NULL);
#endif

	ret = friso_uninit(friso, config, task, 1);
	if (0 != ret) {
		ivAssert(0);
		return TC_WSErrID_BuildRes_Failed;
	}

	FRISO_FREE(hash_res_hdr);
	FRISO_FREE(data_res_hdr);
	FRISO_FREE(fill_buf);
	FRISO_FREE(dataHdr);
	FRISO_FREE(friso);
	FRISO_FREE(config);
	FRISO_FREE(task);

#ifdef LEX_PRINT
	char tmp[256] = { 0 };
	get_file_dir((char *)hash_res_file, tmp);	
	TestResIsOk(hash_res_file, data_res_file,  tmp);
#endif

	return ret;
}

#ifdef WIN32
//统计每个lex中不重复的单utf8字符个数和信息，以便用于拦截不进行lex查询，优化读取资源次数
static void StateLexWordInfo(friso_entry_t friso, const char *resultFile)
{
	typedef struct tagWord {
		char word[8];
	}TWordDsc, *PWordDsc;
	int word_max_num = 30 * 10000, word_num = 0;
	PWordDsc wordLst = (PWordDsc)malloc(word_max_num * sizeof(TWordDsc));		
	memset(wordLst, 0, word_max_num * sizeof(TWordDsc));
	for (int i = 0; i < __FRISO_LEXICON_LENGTH__; i++)
	{
		//int hash_text_max_size = 0;

		char filename[256] = { 0 };
		sprintf(filename, "lex_log/lex%2d.txt", i);
		FILE *fp = fopen(filename, "wb");
		int words_size = 0, word_max_len = 0;
		word_num = 0;		

		friso_hash_cdt *dic = friso->dic + i;
		if (dic->size == 0)
		{
			continue;
		}

		for (int j = 0; j < dic->length; j++)
		{
			hash_entry_t e = dic->table[j];			
			if (NULL == e)
			{	
				continue;
			}

			printf("\rlex%d/%d  line%d/%d", i + 1, __FRISO_LEXICON_LENGTH__, j + 1, dic->length);
			
			for (; NULL != e;)
			{
				lex_entry_cdt_t wordDsc = (lex_entry_cdt_t)e->_val;
				 //wordDsc->word

				char *word = wordDsc->word;
				int offset = 0;
				while (word[offset] != 0) {
					int len = get_utf8_bytes(word[offset]);
					char tmp[8] = { 0 };
					memcpy(tmp, word + offset, len);
					
					int isExist = 0;
					for (int k = 0; k < word_num; k++) {
						if (0 == strcmp(tmp, wordLst[k].word)) {
							isExist = 1;
							break;
						}
					}
					if (!isExist && word_num < word_max_num) {
						strcpy(wordLst[word_num].word, tmp);
						words_size += strlen(tmp) + 1;
						if (strlen(tmp) > word_max_len) {
							word_max_len = strlen(tmp);
						}
						word_num++;
					}
					offset += len;
				}
				e = e->_next;
				
			}			
			e = dic->table[j];			
		} // for (j = 0; j < dic->length; j++)
		printf("\r\n");

		printf("lex=%-32s\tword_num=%6d\tword_size=%d\r\n", dic->szName, word_num,words_size);
// 		for (int xx = 0; xx < word_num; xx++)
// 		{
// 			fprintf(fp, "%s\n", wordLst[xx].word);
// 		}
		fclose(fp);

		if (i == __LEX_TY_CN_WORDS__ || i == __LEX_TY_EN_WORDS__) {
			char tmp[256] = { 0 };
			sprintf(tmp, "lex_log/lex%02d.table", i);
			fp = fopen(tmp, "wb");
			fprintf(fp, "const char g_lex[%d][%d] = {\r\n", word_num, word_max_len+1);
			for (int xx = 0; xx < word_num; xx++)
			{
				fprintf(fp, "{ ");
				for (int yy = 0; yy < word_max_len + 1; yy++) {
					fprintf(fp, "0x%x, ", (unsigned char)(wordLst[xx].word[yy]));
				}
				fprintf(fp, "}, //%s\r\n", wordLst[xx].word);
			}
			fprintf(fp, "};\r\n");
			fclose(fp);
		}

	}	  // for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)

	//fclose(fp);
	free(wordLst);
}

static void TestResIsOk(const char *hash_res_file, const char *data_res_file, char *log_dir)
{
	char tmp[512] = { 0 };
	int hash_file_size = 0, data_file_size = 0;
	char *hash_res = NULL, *data_res = NULL;
	friso_hash_cdt_t pDic = NULL;

	printf("TestResIsOk： log_dir=%s\r\n", log_dir);
	hash_res = ReadFileBin(hash_res_file, &hash_file_size);
	data_res = ReadFileBin(data_res_file, &data_file_size);
	if (NULL == hash_res)
	{
		return;
	}
	if (NULL == data_res)
	{
		FRISO_FREE(hash_res);
		return;
	}
	PHashResHdr hash_res_hdr = (PHashResHdr)hash_res;
	PDataResHdr data_res_hdr = (PDataResHdr)data_res;
	pDic = hash_res_hdr->dic;

	sprintf(tmp, "%s/-----lex_all-------.txt", log_dir);
	FILE *fp_out = fopen(tmp, "wb");
	printf("log_file:%s\r\n", tmp);
	if (NULL == fp_out)
	{
		ivAssert(0);
		printf("Error: open %s failed.\r\n", tmp);
		return;
	}
		
	for (int i = 0; i < __FRISO_LEXICON_LENGTH__; i++)
	{
		pDic = hash_res_hdr->dic + i;
		if (pDic->size == 0)
		{
			continue;
		}

		PHashTab hashTab = (PHashTab)((char *)hash_res_hdr + pDic->table_offset);
		for (int j = 0; j < (int)(pDic->length); j++)
		{
			PHashTab e = hashTab + j;
			int offfset = e->mask >> HASH_MARK_BLOCK_BIT;
			int block = (e->mask << HASH_MARK_OFFSET_BIT) >> HASH_MARK_OFFSET_BIT;
			if (offfset == HASH_IS_NULL)
			{
				continue;
			}
			size_t data_size = block * TEXT_ONE_BLOCK_SIZE;
			char *data_buf = (char *)FRISO_MALLOC(block * TEXT_ONE_BLOCK_SIZE);
			memcpy((void *)data_buf, (const void *)((char *)data_res_hdr + offfset), data_size);
			PDataHdr data_hdr = (PDataHdr)data_buf;
			PDataInfo text_info = (PDataInfo)((char *)data_hdr + sizeof(TDataHdr));
			for (uchar_t k = 0; k < data_hdr->n; k++)
			{
				//char *pWords = (char *)pTextHdr + pTextHdr->pnOffset[k];
				char *word = text_info->text;
				//ivAssert((unsigned int)(pWords + strlen(pWords)) < (unsigned int)(pTextBuf + nTextSize));
				if (i > __LEX_OTHER_DIC_BEGIN__ && i < __LEX_OTHER_DIC_END__) {
					char *word_new = word + strlen(word);
					while (0 == *word_new) {
						word_new++;
					}
					fprintf(fp_out, "bucket=%-6d\tlength=%-3d\tlex=%d\tword=/%s/\twordnew=/%s/\r\n", j, strlen(word), i, word, word_new);
				}
				else {
					fprintf(fp_out, "bucket=%-6d\tlength=%-3d\tlex=%d\tword=/%s/\r\n", j, strlen(word), i, word);					
				}

				int offset_tmp = sizeof(TDataInfo) + text_info->len;
				text_info = (PDataInfo)((char *)text_info + offset_tmp);
			}
			FRISO_FREE(data_buf);
		} // for (j = 0; j < dic->length; j++)		
	}	  // for (i = 0; i < __FRISO_LEXICON_LENGTH__; i++)

	int hash_num = 0, word_num = 0;
	for (int i = 0; i < __FRISO_LEXICON_LENGTH__; i++) {
		if (strlen(hash_res_hdr->dic[i].szName)>0) {
			fprintf(fp_out, "ilex=%02d\t", i);
			fprintf(fp_out, "name=%-32s\t", hash_res_hdr->dic[i].szName);
			fprintf(fp_out, "malloc_hash=%-6d\t", hash_res_hdr->dic[i].length);
			fprintf(fp_out, "use_hash=%-6d\t", hash_res_hdr->dic[i].size);
			fprintf(fp_out, "one_word_size=[%-6d,%-6d]\t", hash_res_hdr->dic[i].wordminbytes, hash_res_hdr->dic[i].wordmaxbytes);
			fprintf(fp_out, "one_hash_text_max_size=%-6d\t", data_res_hdr->per_hash_text_max_size[i]);
			fprintf(fp_out, "hash_data_size_all=%6d\t", hash_res_hdr->dic[i].table_data_size);
			fprintf(fp_out, "\r\n");
			hash_num += hash_res_hdr->dic[i].length;
			word_num += hash_res_hdr->dic[i].size;
		}
	}
	fprintf(fp_out, "hash_num = %d, word_num=%d\r\n\r\n", hash_num, word_num);
	fprintf(fp_out, "one hash text max size=%d\r\n", data_res_hdr->hash_text_max_size);

	fprintf(fp_out, "\r\n\r\nhash file size=%d\r\n", hash_file_size);
	fprintf(fp_out, "text file size=%d\r\n", data_file_size);
	fprintf(fp_out, "---all--:			%d\r\n", hash_file_size + data_file_size);

	FRISO_FREE(hash_res);
	FRISO_FREE(data_res);

	fclose(fp_out);
}

static int SaveBinToFile(const char *szFile, char *buf, int buf_size)
{
	FILE *fp = fopen(szFile, "wb");
	if (NULL == fp)
	{
		printf("Error:new file %s failed.\r\n", szFile);
		return -1;
	}
	fwrite(buf, 1, buf_size, fp);
	fclose(fp);
	return 0;
}

static char *ReadFileBin(const char *szFile, int *pnSize)
{
	int nSize = 0;
	FILE *fp = fopen(szFile, "rb");
	if (NULL == fp)
	{
		printf("Error:open file %s failed.\r\n", szFile);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	nSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *p = (char *)FRISO_MALLOC(nSize + 1);
	memset(p, 0, nSize + 1);
	if (NULL == p)
	{
		fclose(fp);
		return NULL;
	}
	fread(p, nSize, 1, fp);
	fclose(fp);

	if (NULL != pnSize)
	{
		*pnSize = nSize;
	}

	return p;
}
#endif

int wordseg_read_data_res(void *src,int offset, int size, void *dst, int dst_size, int is_file_handle) 
{
	if (NULL == src || NULL == dst || dst_size < size) {
		ivAssert(0);
		return TC_WSErrID_InvCal;
	}
	if (is_file_handle) {
		FILE *fp = (FILE *)src;
		fseek(fp, offset, SEEK_SET);
		fread(dst, size, 1, fp);
	}
	else {
		memcpy(dst, (char *)src + offset, size);
	}

	return TC_WSErr_SUCCESS;
}

//hash_res和data_res可能是内存首址，也可能是文件句柄
int wordseg_load_res(void *handle, const char * hash_res, const char * data_res)
{
	PWordSegEgn pEngine = (PWordSegEgn)handle;
	PWordsegDict res_hdr = pEngine->pResHdr;

#if SUPPORT_READ_HASH_RES_CALLBACK
	if (NULL != pEngine->func_read_hash_res)
	{
		pEngine->func_read_hash_res((void *)hash_res, 0, sizeof(THashResHdr), (void **)(&(pEngine->pHashResHdr)), pEngine->nHashResHdrSize);
		res_hdr->hash_res_hdr = pEngine->pHashResHdr;
	}
	else 
	{
		res_hdr->hash_res_hdr = (PHashResHdr)hash_res;
	}
#else
	res_hdr->hash_res_hdr = (PHashResHdr)hash_res;
#endif
	res_hdr->data_res = (void *)data_res;
	void *ptr = (void *)(res_hdr->data_res_hdr);
	pEngine->func_read_data_res((void *)data_res, 0, sizeof(TDataResHdr), &ptr, sizeof(TDataResHdr));	
		
	if (res_hdr->hash_res_hdr->check_str != WORDSEG_RES_CHECK_NEW ||
		res_hdr->data_res_hdr->check_str != WORDSEG_RES_CHECK_NEW ) 
	{
		ivAssert(0);
		return TC_WSErrID_InvalidRes;
	}

	//检查二个资源文件是否匹配
	if(res_hdr->hash_res_hdr->hash_data_match_crc != res_hdr->data_res_hdr->hash_data_match_crc)
	{
		//printf("[wordseg]error. hash_data_match_crc %u %u\n", res_hdr->hash_res_hdr->hash_data_match_crc, res_hdr->data_res_hdr->hash_data_match_crc);
		return TC_WSErrID_DataHashResNotMatch;
	}
	else {
		//printf("[wordseg]hash_data_match_crc %u\n", res_hdr->hash_res_hdr->hash_data_match_crc);
	}

	friso_hash_cdt_t pDic = res_hdr->hash_res_hdr->dic;

	//统计lex中最长词条的字节数
	pEngine->nLexMaxBytes = pDic[0].wordmaxbytes;
	for (int i = 1; i < __LEX_END; i++)
	{
		if (pDic[i].wordmaxbytes > pEngine->nLexMaxBytes)
		{
			pEngine->nLexMaxBytes = pDic[i].wordmaxbytes;
		}
	}	
		
	return TC_WSErr_SUCCESS;
}

#if PRINT_RES_READ
int g_iLex = 0;
#endif

#if SUPPORT_READ_HASH_RES_CALLBACK
int g_hash_offset = 0;
void *g_hash_res = NULL;
PWSCallBack g_func_read_hash_res = NULL;
static int read_hash_res_in_file(void *p, int offset, int size, void **dst, int dst_size)
{
	//if (NULL == p || NULL == dst || dst_size < size) {
	//	return TC_WSErrID_InvCal;
	//}
	FILE *fp = (FILE *)p;
	fseek(fp, offset, SEEK_SET);
	if (*dst == NULL)
	{
		ivAssert(0);
	}	
	else
	{
		fread(*dst, size, 1, fp);
	}

	return TC_WSErr_SUCCESS;
}
#endif

// get the lex_entry_cdt_t associated with the word.
//从dic[lex_start, lex_end]查询word
PTmpRst wordseg_dic_get(PWordsegDict dic, PWSCallBack func_read_data_res, friso_lex_t lex_start, friso_lex_t lex_end, fstring word)
{
	int iLex;
	PHashResHdr hash_res_hdr = dic->hash_res_hdr;	

	if (NULL == word || 0 == strlen(word))
		return NULL;

	for (iLex = lex_start; iLex <= lex_end; iLex++)
	{
		if (iLex < 0 || iLex >= __FRISO_LEXICON_LENGTH__ || 0 == hash_res_hdr->dic[iLex].size ||
			strlen(word) > hash_res_hdr->dic[iLex].wordmaxbytes)
		{
			continue;
		}

#if PRINT_RES_READ
		g_iLex = iLex;
#endif
		PHashTab hashTab = (PHashTab)((char *)hash_res_hdr + hash_res_hdr->dic[iLex].table_offset);
#if SUPPORT_READ_HASH_RES_CALLBACK
		g_hash_offset = hash_res_hdr->dic[iLex].table_offset;		
#endif


// 		if (NULL != dic->hash_data_in_ram[iLex].data_buf)
// 		{	
// 			printf("lexid=%02d ", iLex);			
// 			my_esp_rom_crc32_le(0, (const unsigned char *)(dic->hash_data_in_ram[iLex].data_buf), (unsigned int)(dic->hash_data_in_ram[iLex].data_size));	 //test		
// 		}

		char *data = NULL;
		void *buf = NULL;
#if TEST_LEX_IN_RAM
		if (NULL != dic->hash_data_in_ram[iLex].data_buf) {
			data = get_hash_data_by_ram(hashTab, hash_res_hdr->dic[iLex].length, dic->hash_data_in_ram+iLex, word, &buf, 0);
		}
		else
#endif
		{			
			//data = get_hash_data_by_callback(hashTab, hash_res_hdr->dic[iLex].length, dic->data_res, (PWSCallBack)func_read_data_res, word, &(dic->buffer), dic->buffer_size);
			data = get_hash_data_by_callback(hashTab, hash_res_hdr->dic[iLex].length, dic->data_res, (PWSCallBack)func_read_data_res, word, &buf, 0);
		}
		
		//val = (lex_entry_cdt_t)hash_get_value(dic + iLex, word);
		if (NULL != data)
		{
			PTmpRst result = dic->result;
			result->type = (uchar_t)iLex;
			//snprintf(result->word, sizeof(result->word), "%s", word);
			ivAssert(strlen(word) < sizeof(result->word));
			strcpy(result->word, word);
			result->wordnew[0] = 0;
			if (iLex > __LEX_OTHER_DIC_BEGIN__ && iLex < __LEX_OTHER_DIC_END__) {
				char *wordnew = data + strlen(data);
				while (0 == *wordnew) {
					wordnew++;
				}
				//snprintf(result->wordnew, sizeof(result->wordnew), "%s", wordnew);				
				ivAssert(strlen(wordnew) < sizeof(result->wordnew));
				strcpy(result->wordnew, wordnew);
			}
			return result;
		}
	}

	return NULL;
}


#if TEST_LEX_IN_RAM
static char *get_hash_data_by_ram(PHashTab hashTab, uint_t hashTab_len, PHashDataInRam dataInfo, char* word, void **buffer, uint_t buffer_size)
{
	uint_t mask = 0;
	//uint_t bucket = (word == NULL) ? 0 : hash(word, hashTab_len);
	uint_t bucket = hash(word, hashTab_len);

#if SUPPORT_READ_HASH_RES_CALLBACK	
	if (NULL != g_func_read_hash_res)
	{
		g_func_read_hash_res(g_hash_res, g_hash_offset + sizeof(THashTab)*bucket, sizeof(THashTab), buffer, buffer_size);
		mask = ((uint_t *)(*buffer))[0];
	}
	else
	{
		mask = hashTab[bucket].mask;
	}
#else
	mask = hashTab[bucket].mask;
#endif

	int offset = mask >> HASH_MARK_BLOCK_BIT;
	if (offset == HASH_IS_NULL) {
		return NULL;
	}

	//int block = (hashTab[bucket].mask << HASH_MARK_OFFSET_BIT) >> HASH_MARK_OFFSET_BIT;

	void *ptr = dataInfo->data_buf + (offset - dataInfo->data_offset);
	//ivAssert(dataInfo->data_size - (buffer - dataInfo->data_buf) >= block * TEXT_ONE_BLOCK_SIZE);

	PDataHdr dataHdr = (PDataHdr)ptr;
	PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
	for (uchar_t i = 0; i < dataHdr->n; i++)
	{
		//char *pText = (pBuffer + pHdr->pnOffset[i]);  //pHdr->pnOffset存储是变长数组，实际为nCount
		char *pText = data_info->text;
		if (0 == strcmp(word, pText)) {
			return pText;
		}

		offset = sizeof(TDataInfo) + data_info->len;
		data_info = (PDataInfo)((char *)data_info + offset);
	}

	return NULL;
}

//遍历hash表，查看词条信息
void get_hash_tab_info_datainram(PHashTab hashTab, uint_t hashTab_len, PHashDataInRam dataInfo)
{
	printf("------------------hashtab datainram len=%d--------------\n", hashTab_len);
	for (int bucket_id = 0; bucket_id < hashTab_len; bucket_id++)
	{
		//uint_t bucket = (word == NULL) ? 0 : hash(word, hashTab_len);
		uint_t bucket = bucket_id;

		int offset = hashTab[bucket].mask >> HASH_MARK_BLOCK_BIT;
		if (offset == HASH_IS_NULL) {
			continue;
		}

		void *ptr = dataInfo->data_buf + (offset - dataInfo->data_offset);
		
		PDataHdr dataHdr = (PDataHdr)ptr;
		PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
		for (uchar_t i = 0; i < dataHdr->n; i++)
		{
			//char *pText = (pBuffer + pHdr->pnOffset[i]);  //pHdr->pnOffset存储是变长数组，实际为nCount
			char *pText = data_info->text;
			printf(" bucket=%d, word=%s\n", bucket, pText);
			offset = sizeof(TDataInfo) + data_info->len;
			data_info = (PDataInfo)((char *)data_info + offset);
		}
	}	
}

#endif

// get the value associated with the given key.
static char *get_hash_data_by_callback(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res, char* word, void **buffer, uint_t buffer_size)
{
	void *buf_value = *buffer;
	uint_t mask = 0;
	//uint_t bucket = (word == NULL) ? 0 : hash(word, hashTab_len);
	uint_t bucket = hash(word, hashTab_len);

#if SUPPORT_READ_HASH_RES_CALLBACK	
	if (NULL != g_func_read_hash_res)
	{		
		g_func_read_hash_res(g_hash_res, g_hash_offset + sizeof(THashTab)*bucket, sizeof(THashTab), buffer, buffer_size);
		mask = ((uint_t *)(*buffer))[0];
	}
	else
	{
		mask = hashTab[bucket].mask;
	}
#else
	mask = hashTab[bucket].mask;
#endif

	int offset = mask >> HASH_MARK_BLOCK_BIT;
	if (offset == HASH_IS_NULL) {
		return NULL;
	}

	int block = (mask << HASH_MARK_OFFSET_BIT) >> HASH_MARK_OFFSET_BIT;

	//先将该hash值对应的1-N个text信息读到内存中
	//int ret = wordseg_read_data_res(pTextRes, nOffset, nBlock * TEXT_ONE_BLOCK_SIZE, pBuffer, nBufferSize, bTextResIsFile);	
	*buffer = buf_value;
	int ret = func_read_data_res(data_res, offset, block * TEXT_ONE_BLOCK_SIZE, buffer, buffer_size);
	if (0 != ret) {
		ivAssert(0);
		return NULL;
	}

#if PRINT_RES_READ
	// printf("read res:func=%s\tsize=%d\r\n", __FUNCTION__, sizeof(friso_hash_entry) + strlen(e->_word));
	g_nReadResSize += block * TEXT_ONE_BLOCK_SIZE;
	g_nReadResCnt++;	
	g_nReadResCntPerLex[g_iLex] ++;	
	if (g_iLex == __LEX_PUNCTUATION__) {
		int xxx = 0;
	}
#endif

	PDataHdr dataHdr = (PDataHdr)(*buffer);
	PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
	for (uchar_t i = 0; i < dataHdr->n; i++)
	{		
		//char *pText = (pBuffer + pHdr->pnOffset[i]);  //pHdr->pnOffset存储是变长数组，实际为nCount
		char *pText = data_info->text;
		if (0 == strcmp(word, pText)) {
			return pText;
		}

		offset = sizeof(TDataInfo) + data_info->len;
		data_info = (PDataInfo)((char *)data_info + offset);
	}

	return NULL;
}

char *get_hash_data_by_callback_ext(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res, uint_t bucket, void **buffer, uint_t buffer_size)
{
	void *buf_value = *buffer;
	uint_t mask = 0;
	//uint_t bucket = (word == NULL) ? 0 : hash(word, hashTab_len);
	//uint_t bucket = hash(word, hashTab_len);

#if SUPPORT_READ_HASH_RES_CALLBACK	
	if (NULL != g_func_read_hash_res)
	{
		g_func_read_hash_res(g_hash_res, g_hash_offset + sizeof(THashTab)*bucket, sizeof(THashTab), buffer, buffer_size);
		mask = ((uint_t *)(*buffer))[0];
	}
	else
	{
		mask = hashTab[bucket].mask;
	}
#else
	mask = hashTab[bucket].mask;
#endif

	int offset = mask >> HASH_MARK_BLOCK_BIT;
	if (offset == HASH_IS_NULL) {
		return NULL;
	}

	int block = (mask << HASH_MARK_OFFSET_BIT) >> HASH_MARK_OFFSET_BIT;

	//先将该hash值对应的1-N个text信息读到内存中
	//int ret = wordseg_read_data_res(pTextRes, nOffset, nBlock * TEXT_ONE_BLOCK_SIZE, pBuffer, nBufferSize, bTextResIsFile);	
	*buffer = buf_value;
	int ret = func_read_data_res(data_res, offset, block * TEXT_ONE_BLOCK_SIZE, buffer, buffer_size);
	if (0 != ret) {
		ivAssert(0);
		return NULL;
	}

	return (*buffer);

#if 0
	PDataHdr dataHdr = (PDataHdr)(*buffer);
	PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
	for (uchar_t i = 0; i < dataHdr->n; i++)
	{
		//char *pText = (pBuffer + pHdr->pnOffset[i]);  //pHdr->pnOffset存储是变长数组，实际为nCount
		char *pText = data_info->text;
		if (0 == strcmp(word, pText)) {
			return pText;
		}

		offset = sizeof(TDataInfo) + data_info->len;
		data_info = (PDataInfo)((char *)data_info + offset);
	}
	return NULL;
#endif
}

static void make_crc(short *data, unsigned int data_size, unsigned int * crc, unsigned int * crc_size)
{
	unsigned int  i;
	for (i = 0; i < (data_size >> 1); i++, ++*crc_size) {
		*crc += (data[i] * (1 + *crc_size)) << ((*crc_size) & 0xf);
		*crc += data[i];
	}
}

void get_hash_tab_info_dataintf(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res)
{
	void *buffer = NULL;
	uint_t buffer_size = 0;
	printf("------------------hashtab dataintf len=%d--------------\n", hashTab_len);
	for (int bucket_id = 0; bucket_id < hashTab_len; bucket_id++)
	{
		uint_t bucket = bucket_id;

		int offset = hashTab[bucket].mask >> HASH_MARK_BLOCK_BIT;
		if (offset == HASH_IS_NULL) {
			continue;
		}

		int block = (hashTab[bucket].mask << HASH_MARK_OFFSET_BIT) >> HASH_MARK_OFFSET_BIT;

		//先将该hash值对应的1-N个text信息读到内存中
		//int ret = wordseg_read_data_res(pTextRes, nOffset, nBlock * TEXT_ONE_BLOCK_SIZE, pBuffer, nBufferSize, bTextResIsFile);	
		int ret = func_read_data_res(data_res, offset, block * TEXT_ONE_BLOCK_SIZE, &buffer, buffer_size);
		if (0 != ret) {
			ivAssert(0);
			continue;
		}

		PDataHdr dataHdr = (PDataHdr)(buffer);
		PDataInfo data_info = (PDataInfo)((char *)dataHdr + sizeof(TDataHdr));
		for (uchar_t i = 0; i < dataHdr->n; i++)
		{
			//char *pText = (pBuffer + pHdr->pnOffset[i]);  //pHdr->pnOffset存储是变长数组，实际为nCount
			char *pText = data_info->text;
			printf(" bucket=%d, word=%s\n", bucket, pText);

			offset = sizeof(TDataInfo) + data_info->len;
			data_info = (PDataInfo)((char *)data_info + offset);
		}
	}
	
}


#if 0
static const unsigned int crc32_le_table[256] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
	0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
	0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL, 0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
	0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
	0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
	0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
	0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L, 0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
	0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
	0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
	0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
	0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L, 0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
	0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
	0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,

	0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
	0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L, 0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
	0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
	0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
	0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
	0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L, 0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
	0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
	0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
	0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
	0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL, 0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
	0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
	0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
	0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};
unsigned int my_esp_rom_crc32_le(unsigned int crc, unsigned char const * buf, unsigned int len)
{
	unsigned int i;
	crc = ~crc;
	for (i = 0; i < len; i++) {
		crc = crc32_le_table[(crc^buf[i]) & 0xff] ^ (crc >> 8);
	}
	crc = ~crc;
	printf("data=%p, data_size=%-8d, crc=%u\n", buf, len, crc);
	return crc;
}
#endif
