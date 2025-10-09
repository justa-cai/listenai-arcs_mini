/*
 */

#ifndef _wordseg_config_h
#define _wordseg_config_h

//分词数据量限制
#if 0
#define WS_INPUT_TEXT_MAX_LEN			(3000) //wordseg_do输入文本最大字节数,utf8一个汉字占3字节
#define WS_TOKEN_MAX_NUM				(1500)//最多分词数
#else
#define WS_INPUT_TEXT_MAX_LEN			(512) //wordseg_do输入文本最大字节数,utf8一个汉字占3字节

#define WS_TOKEN_MAX_NUM				(280)//最多分词数
#endif
#define WS_PER_TOKEN_MAX_LEN			(128)//(WS_INPUT_TEXT_MAX_LEN)//(128)  //每个分词最大字节数

#if (512 == WS_INPUT_TEXT_MAX_LEN)
#define WS_TREE_TEXT_MAX_SIZE			(20*1024) //经验值.wordseg_tree_do相关
#define WS_TREE_TOKEN_MAX_CNT			(1000)
#endif

#define SUPPORT_DICT_WORD_MAX_LEN		(100)	//分词资源打包时支持的词条最大字节数

#define SUPPORT_PERFECT_MATCH			(0) //是否支持全匹配，如词库有"白日依山尽"，输入"白日依山尽"是否分成一条词
#define SUPPORT_PROCESS_CORNER_MARKER	(1) //支持处理角标等，开启后输出结果和输入文本可能对不上了


/* 以下是内部调试开关项 */
#define TEST_MEMORY_LEAK				(0) //测试内存分配是否存在泄漏
#define PRINT_RES_READ					(0) //打印资源读取量，评估资源放到TF卡的情况

/* -----------------资源读取次数裁剪------------------------*/
#define GREEDY_TOKEN					(1)//friso里不进行看三步候选消歧义判断，直接取当前分词最长的


#define TEST_FILTER_LEX_CHAR			(0)

#define TEST_LEX_IN_RAM					(1) //测试将data小且访问频繁的hash data放到内存

#define SUPPORT_READ_HASH_RES_CALLBACK	(1) //测试针对hash.bin也通过回调函数读取，可测试放到到tf卡, 通过裸读tf接口访问

#endif /*end ifndef*/
