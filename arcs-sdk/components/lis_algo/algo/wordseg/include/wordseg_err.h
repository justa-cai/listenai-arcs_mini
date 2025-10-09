/*
 */

#ifndef _wordseg_err_h
#define _wordseg_err_h


#define TC_WSErr_SUCCESS                                (0)		//成功

#define TC_WSErrID_TextTooLong		                    (202001)    //分词的输入文本太长
#define TC_WSErrID_OutOfMemory		                    (202002)    //内存不足
#define TC_WSErrID_InvArg								(202003)    //无效参数
#define TC_WSErrID_InvCal								(202004)    //无效参数
#define TC_WSErrID_RebuildHash                          (202005)    //生成词典失败
#define TC_WSErrID_InvHandle							(202006)    //无效句柄
#define TC_WSErrID_NotInit								(202007)    //未初始化
#define TC_WSErrID_OpenFile								(202008)    //打开文件失败
#define TC_WSErrID_LexType								(202009)    //friso.lex.ini里的词典类型不支持
#define TC_WSErrID_LexWordTooLong						(202010)    //词典文件里的词条长度超过了WORDSEG_MAX_LENGTH
#define TC_WSErrID_ResultTooLong						(202011)    //分词结果字符串太长
#define TC_WSErrID_UserDic_Exist						(202012)    //用户词典已存在
#define TC_WSErrID_UserDic_Error						(202013)    //用户词典格式错误
#define TC_WSErrID_NumTransTxtTooLong					(202014)    //数字翻译文本太长
#define TC_WSErrID_Ch2EnPunctuation 					(202015)    //将字符串中的中文符号转为英文时出错了
#define TC_WSErrID_TextNull								(202016)    //分词的输入文本stlen为0
#define TC_WSErrID_InvalidRes							(202017)    //分词资源无效，不是该版本配套的资源
#define TC_WSErrID_InvIseType							(202019)    //调用wordseg_get_ise_text接口输入的text_type不支持
#define TC_WSErrID_CnIsePinyinTagMore					(202020)    //调用wordseg_get_ise_text接口获取的评测文本中部分拼音超过1/3的限制了
#define TC_WSErrID_BuildRes_Failed						(202021)
#define TC_WSErrID_Uninit_Failed						(202022)
#define TC_WSErrID_OpenResFile							(202023)	//打开资源文件失败
#define TC_WSErrID_LexNameTooLong						(202024)
#define TC_WSErrID_CallBackNoSet						(202025)
#define TC_WSErrID_TellSize								(202026)
#define TC_WSErrID_ReadResErr							(202027)
#define TC_WSErrID_ReadDataResErr						(202028)
#define TC_WSErrID_DataResNotAlign						(202029)
#define TC_WSErrID_DataHashResNotMatch					(202030)	//data.bin和hash.bin不匹配


 //#define TC_WSErr_DONE                                   (1)    //已完成
 //#define TC_WSErrID_FAIL                             (-1)    //错误
 //#define TC_WSErrID_EXCEPTION                        (-2)    //异常
 //#define TC_WSErrID_GENERAL                          (20000)    //Generic Error
 //#define TC_WSErrID_FILE_NOT_FOUND                   (20002)    //文件不存在
 //#define TC_WSErrID_NOT_SUPPORT                      (20003)    //方法、属性不支持
 //#define TC_WSErrID_INVALID_PARA                     (20006)    //无效函数参数
 //#define TC_WSErrID_INVALID_PARA_VALUE               (20007)    //无效函数参数值
 //#define TC_WSErrID_INVALID_DATA                     (20009)    //无效资源
 //#define TC_WSErrID_NO_LICENSE                       (20010)    //无授权信息（保留）
 //#define TC_WSErrID_NULL_HANDLE                      (20012)    //空句柄
 //#define TC_WSErrID_OVERFLOW                         (20013)    //溢出错误
 //#define TC_WSErrID_TIME_OUT                         (20014)    //超时错误
 //#define TC_WSErrID_NO_ENOUGH_BUFFER                 (20017)    //缓冲区大小不足
 //#define TC_WSErrID_NO_DATA                          (20018)    //无数据
 //#define TC_WSErrID_NEED_MORE_DATA                   (20019)    //数据不完整
 //#define TC_WSErrID_RES_MISSING                      (20020)    //资源不存在
 //#define TC_WSErrID_SKIPPED                          (20021)    //跳过了重要数据或者过程
 //#define TC_WSErrID_ALREADY_EXIST                    (20022)    //项目已存在
 //#define TC_WSErrID_LOAD_MODULE                      (20023)    //加载模块错误
 //#define TC_WSErrID_BUSY                             (20024)    //处理过程正在执行
 //#define TC_WSErrID_INVALID_CONFIG                   (20025)    //系统配置错误
 //#define TC_WSErrID_VERSION_CHECK                    (20026)    //版本错误
 //#define TC_WSErrID_INVALID_STATUS                   (20027)    //状态错误
 //#define TC_WSErrID_NO_TASK                          (20031)    //无任务
 //#define TC_WSErrID_NO_PERMISSION                    (20032)    //检查授权失败
 //#define TC_WSErrID_PERMISSION_EXPIRED               (20033)    //授权已过期
 //#define TC_WSErrID_AUTOGEN_INVALID_PARAM            (20034)    //atogen错误
 //#define TC_WSErrID_IP_GENERAL                       (20100)    //一般错误
 //#define TC_WSErrID_IP_NULL_HANDLE                   (20101)    //无效句柄
 //#define TC_WSErrID_IP_UNSPPORT_DATA                 (20102)    //数据不支持
 //#define TC_WSErrID_TPREPROC_GENERAL                 (20800)    //一般错误
 //#define TC_WSErrID_TPREPROC_NULL_HANDLE             (20801)    //无效句柄
 //#define TC_WSErrID_RESULT_GENERAL                   (20700)    //一般错误
 //#define TC_WSErrID_RESULT_NULL_DATA                 (20701)    //结果为NULL
 //#define TC_WSErrID_RESULT_MISS_MATCH                (20702)    //匹配失败
 //#define TC_WSErrID_RESULT_COMPLETE                  (20703)    //完成时错误


#endif /*end ifndef*/
