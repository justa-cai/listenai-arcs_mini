/*

*/

#ifndef _wordsegmentation_type_h
#define _wordsegmentation_type_h

#include "wordseg_cfg.h"

// yat, just take it as this way, 99 percent you will find no problem
#if (defined(_WIN32) || defined(_WINDOWS_) || defined(__WINDOWS_))
#define WS_WINNT
#else
#define WS_LINUX
#endif

#ifdef WS_WINNT
#define WS_API extern __declspec(dllexport)
#else
/*platform shared library statement :: unix*/
#define WS_API extern
#endif

/* 可配置项：规整进翻译的文本,以便命中干预数据库 */
#define  WORDSEG_PARAM_PROCESS_TRANSTXT			(int)(202012)
#define PROCESS_TRANSTXT_ON						(int)(1)
#define PROCESS_TRANSTXT_OFF					(int)(0)    //默认值

/* 可配置项：针对英文类型文本是否支持分句 */
#define  WORDSEG_PARAM_SENTENCESEG				(int)(202107)
#define SENTENCESEG_ON							(int)(1)
#define SENTENCESEG_OFF							(int)(0) //默认值

/* 可配置项：是否针对去角标等进行后处理 */
#define  WORDSEG_PARAM_POSTPROCESS				(int)(202108)
#define WS_POSTPROCESS_ON							(int)(1) //默认值
#define WS_POSTPROCESS_OFF							(int)(0) 

/* 可配置项：设置分词语种 */
#define  WORDSEG_PARAM_LANGUAGE					(int)(202109)
#define WS_LANGUAGE_CNEN						(int)(TEXT_TYPE_CN)	  //默认值
#define WS_LANGUAGE_KOREAN						(int)(TEXT_TYPE_KOREAN) 
#define WS_LANGUAGE_JAPANESE					(int)(TEXT_TYPE_JAPANESE) 
#define WS_LANGUAGE_SPANISH						(int)(TEXT_TYPE_SPANISH) 
#define WS_LANGUAGE_RUSSIAN						(int)(TEXT_TYPE_RUSSIAN) 

#define WS_MEM_IRAM    0   //芯片内部ram
#define WS_MEM_ERAM    1   //芯片外部ram 

typedef enum {
	TEXT_TYPE_EN = 0,		    //英文
	TEXT_TYPE_CN = 1,		    //中文
	TEXT_TYPE_PY = 2,		    //汉语拼音
	TEXT_TYPE_PUNC = 3,      	//"空格+符号"随意组合	

	TEXT_TYPE_JAPANESE = 4,     //日文
	TEXT_TYPE_KOREAN = 5,		//韩文	
	TEXT_TYPE_SPANISH = 6,	    //西班牙语
	TEXT_TYPE_RUSSIAN = 7,	    //俄文
	TEXT_TYPE_CN_EN = 8,		//含有中英字符
	TEXT_TYPE_END
}text_language_type;

typedef enum
{
	CallBackFuncNameReadDataRes = 0,
	CallBackFuncNameCount
}CallBackType;
typedef int(*PWSCallBack)(void *p, int offset, int size, void **dst, int dst_size);
typedef void *(*PWSMallocCallBack)(unsigned int size, int type);
typedef void (*PWSFreeCallBack)(void *p);

/*
* Type: friso_lex_t
* -----------
* This type used to represent the type of the lexicon.
*/
typedef enum
{
	__LEX_CJK_WORDS__ = 0,			// CJK

	__LEX_CN_CHAR__,				//汉字表（含词频，覆盖6500一二级）p_characters

	__LEX_TY_CN_WORDS__,			//淘云-汉语词表 p_words

	__LEX_TY_EN_WORDS__,		//淘云-单词信息表 p_english_words

	__LEX_PUNCTUATION__, //标点符号
	__LEX_CN_EN_END__,	 //中英文分词词典结束标记

	__LEX_TY_EN_ABBREVIATION__, //淘云-英语常用缩写表，用于TTS发音需求(按照字母发音）2021.9.13
		
	__LEX_KOREAN_CHAR__,	//韩语字
	__LEX_KOREAN_WORDS__,	//韩语词组
	__LEX_JAPANESR_CHAR__,	//日语字
	__LEX_JAPANESR_WORDS__, //日语词组
	__LEX_SPANISH_CHAR__,	//西班牙语字符
	__LEX_SPANISH_WORDS__,	//西班牙语词组等
	__LEX_RUSSIAN_CHAR__,	//俄语字符
	//__LEX_RUSSIAN_WORDS__,	//俄语词组
	__LEX_TY_EN_WORD__,	//英语单词表，用于相似单词推荐

	__LEX_CORNERMARK__,		//角标处理符号集

							//!!!!! 以下均为各不同功能的词典，针对lex中含有"\t"隔开的含原文和纠正文本的，需要放在这里 !!!!!
	__LEX_OTHER_DIC_BEGIN__,  //不对应具体词典，表征开始
	__LEX_TTS_USER_DICT__,	  // TTS发音标注词典 20210803
	__LEX_ISE_EN_USER_DICT__, //英文评测：用户标注发音词典 20220314
	__LEX_ISE_CN_USER_DICT__, //中文评测：用户标注拼音词典 20220314

	__LEX_PINYIN_DICT__,		 //汉语带调拼音表及对应TTS发音，如 chuì	哈[=chui4]
	__LEX_PINYIN_SOFT_DICT__,	 //汉语轻声拼音表及对应TTS发音，如 bang	哈[=bang5]
	__LEX_PINYIN_CHAIFEN_DICT__, //汉语拼音拆分表及对应TTS发音，如 zh-ào-zhào	哈[=zhi1] 哈[=ao4] 哈[=zhao4]
	__LEX_OTHER_DIC_END__,		 //不对应具体词典，表征结束

	__LEX_END, //不对应具体词典，只是标记词库结束点

			   /* !!!!!!以上部分和资源打包强相关,不可随意更改!!!!!! */

			   //以下仅表示词条类型，不含有对应lex词典
#if 1
	__LEX_OTHER_WORDS__,  //
	__LEX_UNKNOW_WORDS__, // unrecognized words.
	__LEX_WHITESPACE__,	  //空格
	__LEX_DECIMAL__,	  //小数，如23.5
	__LEX_INTEGER__,	  //数字  如整数、23.5% 23.5‰、￥100、100,000等
	__LEX_CHINESE_NUM__,  //中文数字读法，如一千五百 或者 一九四五
	__LEX_PINYIN__,		  //汉语拼音
	__LEX_NUMERIC__,	  //整数
	__LEX_EN_SENTENCE__,  //英文句子
	__LEX_LETTER_OR_DIGIT__,  //字母数字串
#endif
} friso_lex_t;


/*the segmentation token entry.*/
typedef struct
{
#ifdef WIN32	
	friso_lex_t	type;				// type of the word. (item of friso_lex_t)
#else
	short		type;				// type of the word. (item of friso_lex_t)
#endif
	unsigned short	length;			// length of the word.
	unsigned short	is_modified;		//记录分词结果是否是被修改的（去掉角标等）   =		
	short			offset;					// start offset of the word.	
	
	char			word[WS_PER_TOKEN_MAX_LEN + 4]; //和原文本一致的文本
#if SUPPORT_PROCESS_CORNER_MARKER
	char			word_delpunc[WS_PER_TOKEN_MAX_LEN + 4];	//可能去除角标等符号的文本
#endif
	char			word_tts[WS_PER_TOKEN_MAX_LEN + 4];		//用于tts的文本，（对汉语拼音做了转格式处理）	
} friso_token;
typedef friso_token *friso_token_t;

/*the segmentation token entry.*/
typedef struct
{
#ifdef WIN32	
	friso_lex_t	type;				// type of the word. (item of friso_lex_t)
#else
	short		type;				// type of the word. (item of friso_lex_t)
#endif
	unsigned short	length;			// length of the word.
	unsigned short	is_modified;		//记录分词结果是否是被修改的（去掉角标等）   =		
	short			offset;					// start offset of the word.	
	
	char			*word; //和原文本一致的文本
#if SUPPORT_PROCESS_CORNER_MARKER
	char			*word_delpunc;	//可能去除角标等符号的文本
#endif
	char			*word_tts;		//用于tts的文本，（对汉语拼音做了转格式处理）	
}wordseg_result_t;

typedef struct tagTextTypeDsc {
#ifdef WIN32
	text_language_type	text_type;					//TEXT_TYPE_CN/TEXT_TYPE_EN/TEXT_TYPE_PY
#else
	int					text_type;
#endif
	int					bTextModified;								//word_delpunc和输入的文本不一致了，可能做了去角标等处理
	int					bUseLocalOcr;				//针对多国语版本，在线和离线OCR二个结果，根据在线ocr结果判断是否使用本地OCR结果 2022.3.28

	//针对扫描单词首尾可能多无效字符的处理. 20201216
	int					b_chk_valid_wordseg;		//当该值传入非0时才会检测
	int					valid_wordseg_id;			//对应调用wordseg_do的返回值ppResult的下标(>=0)，表示第几个分词是有效分词. 返回-1表示未检测到,无需处理

	char				word_delpunc[WS_INPUT_TEXT_MAX_LEN + 4];	//可能去除角标等符号的文本
	char				word_tts[WS_INPUT_TEXT_MAX_LEN + 4 + 1024];	//用于tts的文本，（对汉语拼音做了转格式处理）
}text_attr_t;

typedef struct {
	unsigned short	idx;
	unsigned short	firstchild_idx;	
	unsigned short  child_num;
	unsigned short	parent_idx;
	unsigned short	text_type; //text_language_type
	char *			word;
	char *			word_tts;	
}tree_node_t;

typedef struct {
	unsigned int	size;
	unsigned int	node_num;
	unsigned int	node_offset;  //csk返回使用
	unsigned int	call_wordsegdo_cnt;
	int				valid_node_id;		//第一次分词后有效词id
	tree_node_t *	node;		  //esp32应用端使用	
	//text_attr		attr[1];
}tree_result_t;

#define WORDSEG_SIMILAR_WORD_MAX_NUM		(2)	    //推荐的相似单词最大个数
typedef struct tagWordCorrectionResult
{
	int		nResult;
	char	ppWord[WORDSEG_SIMILAR_WORD_MAX_NUM][128];
	int		pnDist[WORDSEG_SIMILAR_WORD_MAX_NUM];
}TWordCorrRst, *PWordCorrRst;

#endif /*end ifndef*/

