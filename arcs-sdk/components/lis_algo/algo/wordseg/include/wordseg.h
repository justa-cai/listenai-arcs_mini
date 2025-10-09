/*

*/

#ifndef _wordsegmentation_h
#define _wordsegmentation_h

#include "wordseg_err.h"
#include "wordseg_type.h"


/*
待确认
1、r7有相似单词推荐，会单独调用分词的wordseg_wordcorrection接口，q3s分词暂未实现该功能 （如扫描 loook，会给出look/sook），D1是否需要；
2、r7有评测文本干预，会单独调用分词的wordseg_get_ise_text接口，已实现，D1是否需要？
3、r7会单独调用wordseg_get_texttype接口，主要获取文本类型（中文/英文/拼音等）,q3s代码无，D1是否需要？

一、csk需实现的接口
1、初始化/逆初始化相关
	wordseg_getversion
	wordseg_init
	wordseg_init_ext
	wordseg_uninit
	wordseg_get_read_data_buf_size （TF卡裸读接口辅助）
2、参数设置
	wordseg_setparam
3、分词：支持单次/分词树
	wordseg_do
	wordseg_result_convert（辅助接口）
	wordseg_tree(暂无，待实现）
4、合成文本干预接口
	wordseg_get_tts_text

二、 csk是否实现，待确认
1、评测文本干预接口 （csk是否实现，待确认）
	wordseg_get_ise_text
2、获取文本类型接口
	wordseg_get_texttype
3、获取相似单词(q5有,q3s无），如扫描 loook，会给出look/sook
	wordseg_wordcorrection（现状：无该接口）

三、csk无需实现
1、资源生成接口
	wordseg_builddict
2、国际音标转讯飞音标接口
	IPA2IflyPhone

*/

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

	/**
	* @brief 获取引擎版本号
	* @param
	* @retval 引擎版本号字符串
	* @note
	**/
	WS_API const char * wordseg_getversion(void);
	
	/**
	* @brief 引擎初始化
	* @param pResidentBuf		[in/out] 常驻内存，初始化后直到wordseg_uninit不可与其他模块复用；
	* @param pResidentBufSize	[in/out] 常驻内存大小的指针，当pResidentBuf传入NULL，会通过该指针返回需要的内存字节数；当不为NULL，用于检测pResidentBuf大小是否够用；
	* @param pTmpBuf			[in/out] 临时内存，每次分词的临时内存，可与其他模块复用
	* @param pTmpBufSize		[in/out] 临时内存大小的指针，当pTmpBuf传入NULL，会通过该指针返回需要的内存字节数；当不为NULL，用于检测pTmpBuf大小是否够用；
	* @param pHashRes			[in] 资源分二部分，该部分需要放到PSRAM或者NorFlash中，直接访问
	* @param pDataRes			[in] 资源分二部分，该部分需要能放在RAM最好，支持按照文件方式读取，具体读取方式应用层回调实现
									文件方式读取：data_res需要传句柄，应用层负责文件句柄打开和关闭
									内存方式读取：data_res需要传内存首地址
	* @param pReadResFunc		[in] pDataRes的读回调函数，具体由调用方实现
	* @retval 错误码，请见wordseg_err.h
	* @note
	**/
	WS_API int wordseg_init(void * pResidentBuf, int * pResidentBufSize, void *pTmpBuf, int * pTmpBufSize, const char * pHashRes, void * pDataRes, const PWSCallBack pReadResFunc);
	
	/**
	* @brief 引擎初始化扩展接口，在wordseg_init基础上支持pHashRes也通过回调读取
	* @param
	* @retval 错误码，请见wordseg_err.h
	* @note
	**/
	WS_API int wordseg_init_ext(void * pResidentBuf, int * pResidentBufSize, void *pTmpBuf, int * pTmpBufSize, const char * pHashRes, const PWSCallBack pReadHashResFunc, void * pDataRes, const PWSCallBack pReadDataResFunc);
	
	/**
	* @brief 完成一次分词
	* @param pResidentBuf		[in] wordseg_init返回的工作句柄
	* @param szU8Text			[in] 待分词文本(要求UTF-8文本，以\0结束）
	* @param pnResult			[out] 返回分词个数
	* @param ppResult			[out] 返回分词结果
	* @param pTextAttr			[out] 若输入非NULL，则返回文本类型和修正后文本信息
	* @retval 错误码，请见wordseg_err.h
	* @note 文本类型判断规则：1）全拼音：类型=拼音； 2）汉字个数或拼音个数大于英文单词个数，类型=中文； 3）其他：类型=英文;
	* @note 识字笔调用方式：外部申请int nResult=0, friso_token_t pResult=NULL, 调用wordseg_do(handle, szU8Text, &nResult, &pResult)获取分词结果
	* @note pResult使用的是内部内存，调用者无需free。
	**/	
	WS_API int wordseg_do(void * pResidentBuf, const char * szU8Text, int * pnResult, friso_token_t * ppResult, text_attr_t * pTextAttr);

	/**
	* @brief 完成一次分词树调用
	* @param pResidentBuf		[in] wordseg_init返回的工作句柄
	* @param szU8Text			[in] 待分词文本(要求UTF-8文本，以\0结束）
	* @param pTextAttr			[out] 若输入非NULL，则返回文本类型和修正后文本信息
	* @retval 错误码，请见wordseg_err.h	
	**/
	WS_API int wordseg_tree_do(void *pResidentBuf, const char *szU8Text, tree_result_t  **ppResult, int *pnResultsize);

	/**
	* @brief wordseg_tree_do返回的结果需要调用该接口将指针还原
	* @param pResult		[in] wordseg_tree_do返回的结果	
	* @retval 错误码，请见wordseg_err.h
	**/
	WS_API int wordseg_tree_result_convert(tree_result_t* pResult);
	WS_API int printf_wordseg_tree(tree_result_t * pTreeRst);

	/**
	* @brief 设置内存分配/释放回调接口
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄
	* @param mallocFunc		[in]
	* @param freeFunc		[in]
	* @retval 错误码，请见wordseg_err.h
	* @note wordseg_tree_do时使用
	**/
	WS_API int wordseg_set_memcallback(void *pResidentBuf, const PWSMallocCallBack mallocFunc, const PWSFreeCallBack freeFunc);

	/**
	* @brief 引擎逆初始化
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄
	* @retval 错误码，请见wordseg_err.h
	* @note 接口无实际内容，内存都是外部传入的，不需要做任何逆初始化操作
	**/
	WS_API int wordseg_uninit(void *pResidentBuf);

	/**
	* @brief 进行参数设置，在调用wordseg_init后设置
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄
	* @param nParam			[in] 设置项，参见wordseg_type.h
	* @param nParamValue	[in] 设置值
	* @retval 错误码，请见wordseg_err.h
	* @note csk上暂时可不调用
	**/
	WS_API int wordseg_setparam(void * pResidentBuf, int nParam, int nParamValue);

	/**
	* @brief 将wordseg_do获取的friso_token_t分词结果格式转为新格式wordseg_result *
	* @param pResult		[in] wordseg_do获取的分词结果
	* @param nResult		[in] wordseg_do获取的分词个数
	* @param pResultExt		[in] 转换后的分词结果结构体指针；
	* @param pnResultExtSize[in/out] pResultExt的内存大小，若给的*pnResultExtSize不够用，会通过pnResultExtSize返回需要的大小；
	* @retval 错误码，请见wordseg_err.h
	* @note friso_token_t结构体字节数较多，若分词放到csk上，需要通过spi将结果返回的话，需要将不用的信息裁剪，减少spi传输数据量
	**/	
	WS_API int wordseg_result_convert(friso_token_t pResult, int nResult, wordseg_result_t* pResultExt, int *pnResultExtSize);

	/**
	* @brief 为esp32定制，告知wordseg_init的读data资源回调pReadResFunc，每次wordseg_do的最大数据量，以便外部分配IRAM，加速low_sd_read_sectors速度
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄	
	* @param pnSize			[in/out] 返回需要的大小；
	* @retval 错误码，请见wordseg_err.h
	* @note
	**/	
	WS_API int	wordseg_get_read_data_buf_size(void *pResidentBuf, int *pnSize);
	
	/**
	* @brief 针对要送入TTS的文本进行修正（实现原来TTS用户词典标注发音功能）
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄
	* @param pInputText		[in] 准备送入TTS的文本(要求UTF-8文本，以\0结束）
	* @param pOutputText	[in/out] 调用者负责内存申请和释放，建议申请内存大小至少是pInputText大小的5倍以上(例如输入“装“,TTS标注为"装[=zhuang1]")
								  若输出pOutputText内存不足，则不进行修改，直接拷贝pInputText内容
	* @retval 错误码，请见wordseg_err.h
	* @note 和分词功能无关，可能在任何调用tts的地方被调用
	**/	
	WS_API int wordseg_get_tts_text(void * pResidentBuf, const char *pInputText, char *pOutputText, int nOutputTextSize);
		
	/*
	* @brief 针对要送入评测的文本进行修正（针对一些多发音的字进行干预）
	* @param pResidentBuf [in] wordseg_init返回的工作句柄
	* @param pInputText  [in] 准备送入评测的文本(要求UTF-8文本，以\0结束）
	  @param pTextType   [in]输入文本的语种，目前仅支持TEXT_TYPE_CN和TEXT_TYPE_EN二个值；
	* @param pOutputText [in/out] 调用者负责内存申请和释放，建议申请内存大小至少是pInputText大小的5倍以上(例如输入“装“,TTS标注为"装<zhuang1>")
						若输出pOutputText内存不足，则不进行修改，直接拷贝pInputText内容
	* @param nOutputTextSize	[in] pOutputText的大小
	* @retval 错误码，请见wordseg_err.h
	* @note 返回值说明：TC_WSErrID_CnIsePinyinTagMore 该返回值表示对输入文本进行了修改，但是输出结果里评测标注的拼音数介于总汉字数(1/3,1)之间了，评测不支持，应用层需要使用原文本送入评测
	* @note 样例：如输入“且鞮侯 且鞮侯”输出是“且<ju1>鞮侯 且<ju1>鞮侯”
	* @note 待确认：q3s上未使用，q5上使用了，D1是否使用？
	*/
	WS_API int wordseg_get_ise_text(void * pResidentBuf, const char *pInputText, text_language_type pTextType, char *pOutputText, int nOutputTextSize);

	/**
	* @brief 对输入文本进行中文/英文判断. 判断规则：没有汉字和带调拼音，就是英文
	* @param pResidentBuf	[in] wordseg_init/wordseg_init_ext返回的工作句柄
	* @param szU8Text		[in] 待分词文本(要求UTF-8文本，以\0结束）
	* @param pTextAttr		[out] 若输入非NULL，则返回文本类型和修正后文本信息  TEXT_TYPE_CN/TEXT_TYPE_EN	
	* @retval 错误码，请见wordseg_err.h
	* @note 注意事项：wordseg_init之后才能调用。
	* @note 和wordseg_do返回的文本类型区别是没有拼音类型，仅会返回 TEXT_TYPE_CN/TEXT_TYPE_EN
	* @note 待确认：q3s上未使用，q5上使用了，D1是否使用？
	**/
	WS_API int wordseg_get_texttype(void * pResidentBuf, const char *szU8Text, text_attr_t * pTextAttr);

	/* 功能：实现英文单词纠错功能，跟词库里的上万条英文单词对比，通过计算字符串距离给出二个最像的候选单词
	* @param handle     [in] wordseg_init返回的工作句柄
	* @param szWord		[in] 输入单词，UTF8格式
	* @param pResult	[out]存储纠错结果信息结构体指针
	*/
	WS_API int wordseg_wordcorrection(void *handle, const char* szWord, PWordCorrRst *ppResult);

	/**
	* @brief 生成词典资源
	* @param lex_dir       [in]词典lex和ini所在目录，如"./resource/lex_taoyun/"，注意事项：目录结尾一定要有"/"or"\\"
	* @param szHashResFile [in] 文件名，生成的分词资源有二个文件，其中之一
	* @param szDataResFile [in] 文件名，生成的分词资源有二个文件，其中之一
	* @retval 错误码，请见wordseg_err.h
	* @note 该接口主要用于离线生成词典资源
	* @note 不需要在csk上实现
	**/
#ifdef WIN32
	WS_API int __stdcall wordseg_builddict(const char *szLexDir, const char *szHashResFile, const char *szDataResFile);
	WS_API int __stdcall wordseg_unified_builddict(const char *szLexDir, const char *szOutDir); //扩展接口，供内容后台集成使用，只需传入输入目录和输出目录，不关心生成几个文件
#else
	WS_API int wordseg_builddict(const char *lex_dir, const char *hash_res_file, const char *data_res_file);
	WS_API int wordseg_unified_builddict(const char *szLexDir, const char *szOutDir);
#endif	
	
	/**
	* @brief 独立接口，实现国际音标转为讯飞音标,给英文评测用的
	* @param szIPA			[in] 国际音标串(utf8, 以0结尾)	 如  ˈbedruːm
	* @param szIPASplit		[out]对szIPA进行音素拆分（注意：内存由调用者分配，建议长度大于szIPA字符串长度的3-4倍）  如 b e dr uː m
	* @param szIflyPhone	[out]szIPA转成的讯飞音标体系，每个phone之间用空格隔开	   （注意：内存由调用者分配，建议长度大于szIPA字符串长度的3-4倍） 如 b eh dr uw m
	* @retval 错误码，请见wordseg_err.h
	* @note 不需要在csk上实现
	**/
	WS_API int IPA2IflyPhone(const char* szIPA, char* szIPASplit, char* szIflyPhone);

#ifdef __cplusplus
} /* extern "C" */
#endif /* C++ */

#endif /*end ifndef*/

