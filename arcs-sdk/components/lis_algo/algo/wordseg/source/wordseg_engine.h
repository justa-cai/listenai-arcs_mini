/*********************************************************************#
//	文件名	：wordseg_engine.h
//	文件功能	：定义引擎内部使用的结构体
//	作者		：qungao
//	创建时间	：2021年7月13日
//	项目名称	：
//	备注		：
/---------------------------------------------------------------------#
//	历史记录：
//	编号	日期		作者	备注
#**********************************************************************/

#include "wordseg.h"
#include "wordseg_res.h"

#define WORDSEG_POSTPROCESS_CNT_LIMIT	(100) //很多分词后处理很耗时，对于分词个数过多的情况就不处理了

#define LOG_MODULE_COST_TIME			(0) //内部调试. 打印各模块耗时，看看可能优化 2021.3.9

#define WORDSEG_ENGINE_CHECK			(0x20200528)

#define WS_TMP_BUFFER_SIZE				(10*1024)

typedef struct tagWords
{
    int length;
    char word[WS_PER_TOKEN_MAX_LEN + 4];
} TWord, *PWord;

typedef struct tagWordsegTmpBuf {

#if SUPPORT_PROCESS_CORNER_MARKER
	int				pnCornerMarkID[WS_TOKEN_MAX_NUM]; //用于去角标处理  1120字节
	TWord			pCornerBuf[512];                 // 136*50=6800字节;
#endif

													//英文分词处理临时内存
	char			szOrgWords[WS_INPUT_TEXT_MAX_LEN + 1];	//512字节
	char			szLowerWords[WS_INPUT_TEXT_MAX_LEN + 1]; //512字节 

															 //临时buf
	int				pbPinyin[WS_TOKEN_MAX_NUM];
	char			szPinyinLst[20][WS_PER_TOKEN_MAX_LEN + 4]; //2640字节
}TWsTmpBuf, *PWsTmpBuf;

typedef struct {
	PWSMallocCallBack	func_malloc;
	PWSFreeCallBack		func_free;
	int					callcnt;		//分词树调用wordseg_do次数	

	tree_node_t *			node;
	unsigned int		node_malloc_cnt;
	unsigned int		node_used_cnt;
	int					valid_wordseg_id;			//对应调用wordseg_do的返回值ppResult的下标(>=0)，表示第几个分词是有效分词. 返回-1表示未检测到,无需处理

	char *				text;	
	unsigned int		text_malloc_size;
	unsigned int		text_used_size;
}TWsTreeDsc, *PWsTreeDsc;

typedef struct tagWordsSegmentationEngine
{
    uint_t			dwCheck;    
    char /*fstring*/szText[WS_INPUT_TEXT_MAX_LEN+4];           //用户输入的分词文本
    uint_t			nTextLen;          //用户输入的分词文本有效字节数

    uint_t			nLexMaxBytes;      // lex中词条最长字节数    
    uint_t			nWholeMatchMaxBytes; // wordseg_dict_wholematch时限制最长字节数，防止效率过低

	TWordsegDict	pResHdr[1];	

    //friso_entry		friso[1];
	//friso_config_entry config[1];
	//friso_task		task[1];

	//输入参数:控制内部某些功能是否开启
	int				bProcessTransTxt;    //是否对进翻译文本进行规整处理，以便命中干预数据库
	int				bSupportSentenceSeg; //是否支持英文分句，默认不支持  2021.7.14
	int				bSupportPostProcess; //是否支持后处理，如去角标等
	int				bInnerPostProcess;   //针对一些额外处理耗时，如果输入文本过长，就不进行处理
	int				nPriorLanguage;      //用户设置的优先语种  支持中英/日/韩/西/俄
    
	int				pnLanguageCnt[TEXT_TYPE_END]; //多国语的语种类型判断计数器
	text_language_type nTextType;     //引擎内部判断得到文本类型

	PWSCallBack		func_read_data_res;
#if SUPPORT_READ_HASH_RES_CALLBACK
	PWSCallBack		func_read_hash_res;
	PHashResHdr		pHashResHdr;
	int				nHashResHdrSize;
#endif

#if 1 //分词树wordseg_tree_do接口相关
	PWSMallocCallBack	func_malloc;
	PWSFreeCallBack		func_free;
	TWsTreeDsc			pTreeDsc[1];	
#endif

    int				nResult;
	int				nResultSize;	//pResult分配的大小
	friso_token_t	pResult;		//使用wordseg_init传入的pTmpBuf
    //friso_token		pResult[WS_TOKEN_MAX_NUM];
	text_attr_t		pTextAttr[1]; //

	//给出英语单词，从词库中找出相似单词推荐相关
	TWordCorrRst        pWordCorrRst[1]; //相似单词候选推荐

	PWsTmpBuf		pTmpBuf;	//每次分词使用的临时buf，外部可复用	
} TWordSegEgn, *PWordSegEgn;

int ws_engine_process_en(PWordSegEgn pEngine);              //对friso结果进行二次处理，实现英文分词
int ws_engine_process_decimal(PWordSegEgn pEngine);         //对friso结果进行二次处理，将小数进行合并、%‰合并，比如"在/3/./25/亿/年/"->"在/3.25/亿/年/"
int ws_engine_process_connector(PWordSegEgn pEngine);	//处理连接符"/-&"前后是英文/数字则合并
int ws_engine_process_other(PWordSegEgn pEngine);		//一些特殊处理，如"1940s"
int ws_engine_process_chinesenum(PWordSegEgn pEngine);	//处理中文数字，如“一千五百”，需要合并成一个分词
int ws_engine_process_chineseyear(PWordSegEgn pEngine);	//处理中文年份，如“一九四五年”-> 一九四五/年/
int ws_engine_process_specialcase(PWordSegEgn pEngine);	//补丁：处理一些特例
int ws_engine_dict_wholematch(PWordSegEgn pEngine);		//对friso结果进行二次处理，和词典进行全匹配，以便实现一些特殊分词，如[清平乐·宫怨]
int ws_engine_set_punc_type(PWordSegEgn pEngine);		//设置分词结果里标点符号类型
int ws_engine_dict_match(PWordSegEgn pEngine, const char *szU8Text, int *pilex);

#if SUPPORT_PROCESS_CORNER_MARKER
//处理教材中古诗词有很多角标，导致分词和内容查询都不对的问题。
//大致策略：去除角标符号等后到p_sentence词典中如果能查到，就去除无关符号，否则保持原样
int ws_engine_process_cornermarker(PWordSegEgn pEngine);
#endif

int ws_engine_process_whitespace(PWordSegEgn pEngine); //处理汉字中间空格导致的无法分到一起
int ws_engine_process_endpunc(PWordSegEgn pEngine);    //处理结尾符号
//针对用户想扫描单独的单词，但是可能会扫到前后其他词的一部分情况进行处理
int ws_engine_process_enword_beg_end_invalidstr(PWordSegEgn pEngine, text_attr_t * pTextAttr);
int ws_engine_process_pinduchaifen(PWordSegEgn pEngine); //处理拼音拼读拆分可以继续分词，如 "b-à-bà" -> b/-/à/-/bà/

int ws_engine_process_tts_trans_text(PWordSegEgn pEngine); //每条分词的翻译和tts结果针对拼音单独处理
int ws_engine_get_texttype_new(PWordSegEgn pEngine, const char *szU8Text, int bHasResult, text_attr_t * pTextAttr);
//对分词结果二次处理，如果出了符号外，只有一个有效分词，则设置valid_wordseg_id为有效分词
int ws_engine_set_valid_wordseg_id(PWordSegEgn pEngine, text_attr_t * pTextAttr);
//针对英文句子，以基本标点符号进行分句
int ws_engine_sentence_seg(PWordSegEgn pEngine, int *pbSuccess, text_attr_t * pTextAttr);
int tts_userdict_match(PWordSegEgn pEngine, char *pInputText, char *pOutputText, int nOutputTextSize); // tts用户标注词典查询
int ise_userdict_match(PWordSegEgn pEngine, text_language_type szTextType, char *pInputText, char *pOutputText, int nOutputTextSize); //评测用户标注词典查询
//功能：混合中英日韩西俄语种的分词
int ws_engine_do_otherlanguage(PWordSegEgn pEngine, const char *szU8Text, text_attr_t * pTextAttr);
int ws_engine_get_base_segmentation(PWordSegEgn pEngine);
int ws_engine_set_result_offset(PWordSegEgn pEngine);
int ws_engine_texttype_get(const char *szU8Text, int *pnType);
int ws_engine_texttype_convert(PWordSegEgn pEngine, friso_token_t pResult, int nResult);

#if LOG_MODULE_COST_TIME
int GetCostTimeMs();
#endif