/******************************************************************************
 * File Name		       : wordseg_res.h
 * Description          : 针对esp32重构分词资源结构
 * Author               : qungao
 * Date Of Creation     : 2022-08-18
 * Platform             : Any
 * Modification History :
 *------------------------------------------------------------------------------
 * Date        Author     Modifications
 *------------------------------------------------------------------------------
 * 2022-08-18  qungao     Created
 ******************************************************************************/

#ifndef WORDSEG__2022_08_18__BUILDRES__H
#define WORDSEG__2022_08_18__BUILDRES__H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WIN32
#include <crtdbg.h>
#endif
#include "wordseg_cfg.h"
#include "wordseg_type.h"
#include "friso.h"

#define TEXT_ONE_BLOCK_SIZE		 		(8) //配置项. 为减少存储，后续改动，将pHashTab->指向的文本size保障是该宏的倍数 ，以便HashTab里一个uint存储offset和size !!!!
#define HASH_IS_NULL 					(0)
#define WORDSEG_RES_CHECK_NEW 			(uint_t)(0x20220818) //资源头check字段
#define SD_CARD_SECTOR_SIZE				(512)

#define WORDSEG_RES_VERSION				("2.0.0")

 //hash.bin中每个hash值对应的data.bin中文本读取起始地址保证是512对齐的，以便裸读TF卡
#define DATA_RES_BLOCK_ALIGN			(0) //资源会大很对，data.bin从11MB->187MB

typedef enum
{
	HASH_TAB_RES_FILE	= 1,
	TEXT_RES_FILE		= 2
} res_type;

typedef struct tagHashResHdr
{	
	uint_t			file_crc;			//当前文件的crc值
	uint_t			file_crc_size;
	char			version[8];	
	uint_t			check_str;
	uint_t			hash_data_match_crc;			//对hashtab和text进行crc的值，用于加载资源后比对二个文件关联性
	uint_t/*res_type*/		file_type;
	friso_hash_cdt	dic[__LEX_END]; // friso dictionary
	uint_t			reserved[2];	//保留字段
} THashResHdr, *PHashResHdr;

#define HASH_MARK_OFFSET_BIT	(24)
#define HASH_MARK_BLOCK_BIT		(8) 
typedef struct tagHashTab
{
	uint_t			mask;		//高24bit存储nOffset,低8bit存储nBlock
	//uint_t		nOffset; //文本偏移， NULL_HASH则表示无信息
	//short			nBlock;	//文本大小=nBlock * TEXT_ONE_BLOCK_SIZE
} THashTab, *PHashTab;

typedef struct tagDataResHdr
{
	uint_t			file_crc;			//当前文件的crc值
	uint_t			file_crc_size;
	//要保证是该结构体大小SD_CARD_SECTOR_SIZE的倍数
	char			version[8];
	uint_t			check_str;
	uint_t			hash_data_match_crc;		//对hashtab和text进行crc的值，用于加载资源后比对二个文件关联性
	uint_t/*res_type*/		file_type;
	uint_t			hash_text_max_size;	//记录一个hash对应的文本最大字节数
	uint_t			per_hash_text_max_size[__FRISO_LEXICON_LENGTH__];
	uint_t			reserved[96];		//保留字段
} TDataResHdr, *PDataResHdr;

/* 一个hash值对应的data结构如下 */
typedef struct tagDataHdr
{
	uchar_t	 n;		//一个hash对应的词条个数
} TDataHdr, *PDataHdr;

typedef struct tagDataInfo
{
	/* 以下信息需要按顺序读取n次：读取按照先读len，然后读取len长度的文本 */
	uchar_t	 len;     //存储text词条的长度(含/0)
	char	 text[0]; //如果是合成/评测纠错词典，text文本/0结束后跟着的是纠错文本
}TDataInfo, *PDataInfo;

/* 原friso hash结果都是各种指针,临时存储hash结果 */
typedef struct tagTmpRst
{
	char	word[WS_PER_TOKEN_MAX_LEN + 4]; 
	char	wordnew[WS_PER_TOKEN_MAX_LEN *4 + 4];
	uchar_t type;
} TTmpRst, *PTmpRst;

typedef struct tagHashDataInRam
{
	uint_t			data_offset;	//在data.bin中起始偏移
	uint_t			data_size;		//在data.bin中总size
	char *			data_buf;		//data.bin中data_offset起始的data_size读入内存
}THashDataInRam, *PHashDataInRam;

/* 以下为引擎运行时结构体 */
typedef struct tagWordsegDictDsc
{
	PHashResHdr		hash_res_hdr;
	void			*hash_res;		//文件句柄或内存首地址

	TDataResHdr		data_res_hdr[1];//可能是文件句柄，需要拷贝一份
	void			*data_res;		//文件句柄或内存首地址
	
	uint_t			buffer_size;
	char*			buffer;			//工作buffer，存储每次hash从text文件读到内存的文本信息

	TTmpRst			result[1];		//存储hash查询结果
	lex_entry_cdt	val[1];			//debug	适配friso

	THashDataInRam	hash_data_in_ram[__LEX_END]; //记录hash表的data全部导入内存的情况，将一些比较小但频繁访问的hash表可全部导入ram,如符号表

}TWordsegDict, *PWordsegDict;


FRISO_API int wordseg_save_dict_to_bin(const char *lex_dir, const char *hash_res_file, const char *data_res_file);

int wordseg_read_data_res(void *pSrc, int nOffset, int nSize, void *pDst, int nDstSize, int bFileHandle);

int wordseg_load_res(void *pHandle, const char * pHashResBuf, const char * pTextResBuf);

PTmpRst wordseg_dic_get(PWordsegDict dic, PWSCallBack func_read_data_res, friso_lex_t lex_start, friso_lex_t lex_end, fstring word);

//unsigned int my_esp_rom_crc32_le(unsigned int crc, unsigned char const * buf, unsigned int len);

void get_hash_tab_info_datainram(PHashTab hashTab, uint_t hashTab_len, PHashDataInRam dataInfo);
void get_hash_tab_info_dataintf(PHashTab hashTab, uint_t hashTab_len, void *data_res, PWSCallBack func_read_data_res);

#endif
