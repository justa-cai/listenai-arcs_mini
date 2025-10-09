/*

*/

#ifndef _wordsegmentation_wordcorrection_h
#define _wordsegmentation_wordcorrection_h

#include "wordseg_err.h"

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

	/*
* 功能：计算strA经过多少步插入/删除/修改操作后和strB相同
* 算法参考：https://www.cnblogs.com/boris1221/p/9375047.html
*/
int GetStrDist(const char* strA, const char* strB, void* pBuffer, int nBufferSize, int nLenDistMax, int* pnDist);

//配合相似单词推荐功能，加速查询
int wordseg_get_en_word_list(void* handle);

#ifdef __cplusplus
} /* extern "C" */
#endif /* C++ */

#endif /*end ifndef*/

