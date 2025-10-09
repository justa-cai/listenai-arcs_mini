/*
 * friso hash table functions implementation defined in header file "friso_API.h".

 * @author  lionsoul<chenxin619315@gmail.com>
 */

#include "wordseg_kernel.h"

#if TEST_MEMORY_LEAK
#define SAVE_MEMORY_TO_FILE (1)
int g_nMallocCnt = 0;
int g_nFreeCnt = 0;
unsigned int g_nMallocSize = 0;
FILE *g_fpMemMallocLog = NULL;
FILE *g_fpMemFreeLog = NULL;
char g_szMallFile[256] = {0};
char g_szFreeFile[256] = {0};
char g_szMemOut[256] = {0};
void *FRISO_CALLOC(int _bytes, int _blocks)
{
	void *p = calloc(_bytes, _blocks);

#if SAVE_MEMORY_TO_FILE
	if (NULL == g_fpMemMallocLog)
	{
		g_fpMemMallocLog = fopen("malloc.log", "wb");
	}
	g_nMallocCnt++;
	g_nMallocSize += _bytes * _blocks;
	fprintf(g_fpMemMallocLog, "%04d: p=0x%p, size=%d, allsize=%d\r\n", g_nMallocCnt, p, _bytes, g_nMallocSize);
	fflush(g_fpMemMallocLog);
#endif

	return p;
}

void *FRISO_MALLOC(int _bytes)
{
	void *p = malloc(_bytes);
#if SAVE_MEMORY_TO_FILE
	if (NULL == g_fpMemMallocLog)
	{
		g_fpMemMallocLog = fopen("malloc.log", "wb");
	}
	g_nMallocCnt++;
	g_nMallocSize += _bytes;
	fprintf(g_fpMemMallocLog, "%04d: p=0x%p, size=%d, allsize=%d,\r\n", g_nMallocCnt, p, _bytes, g_nMallocSize);
	fflush(g_fpMemMallocLog);
#endif
	// printf("malloc=0x%p\tsize=%d\r\n", p, _bytes);
	return p;
}
void FRISO_FREE(void *p)
{
#if SAVE_MEMORY_TO_FILE
	if (NULL == g_fpMemFreeLog)
	{
		g_fpMemFreeLog = fopen("free.log", "wb");
	}
	g_nFreeCnt++;
	fprintf(g_fpMemFreeLog, "%04d: p=0x%p,\r\n", g_nFreeCnt, p);
	fflush(g_fpMemFreeLog);
#endif
	// printf("free=0x%p\r\n", p);

	free(p);
}

typedef struct myMemDsc
{
	char str[256];
	char p[12];
	int nSize;
	int bFree;
} TMemDsc, *PMemDsc;
void AnalyseMemoryLeak()
{
	char szLine[256];
	PMemDsc pMemMalloc, pMemFree;
	int nMalloc, nFree;
	int i, j;
	// 0001: p=0x011758E8, size=8, allsize=8,
	// 0001: p=0x01175920,

	//读取malloc文件
	FILE *fp = fopen(g_szMallFile, "rb");
	nMalloc = 0;
	while (fgets(szLine, sizeof(szLine), fp))
	{
		if (NULL != strstr(szLine, "p="))
		{
			nMalloc++;
		}
	}
	fseek(fp, 0, SEEK_SET);
	pMemMalloc = (PMemDsc)malloc(nMalloc * sizeof(TMemDsc));
	memset(pMemMalloc, 0, nMalloc * sizeof(TMemDsc));
	i = 0;
	while (fgets(szLine, sizeof(szLine), fp))
	{
		char *p = strstr(szLine, "p=");
		if (NULL == p)
		{
			continue;
		}
		strcpy(pMemMalloc[i].str, szLine);
		strncpy(pMemMalloc[i].p, p + 2, 10);
		p = strstr(szLine, ", size=");
		pMemMalloc[i].nSize = atoi(p + strlen(", size="));
		// 0001: p=0x011758E8, size=8, allsize=8,
		i++;
	}
	nMalloc = i;
	fclose(fp);

	//读取free文件
	fp = fopen(g_szFreeFile, "rb");
	nFree = 0;
	while (fgets(szLine, sizeof(szLine), fp))
	{
		if (NULL != strstr(szLine, "p="))
		{
			nFree++;
		}
	}
	fseek(fp, 0, SEEK_SET);
	pMemFree = (PMemDsc)malloc(nFree * sizeof(TMemDsc));
	memset(pMemFree, 0, nFree * sizeof(TMemDsc));
	i = 0;
	while (fgets(szLine, sizeof(szLine), fp))
	{
		char *p = strstr(szLine, "p=");
		if (NULL == p)
		{
			continue;
		}
		strcpy(pMemFree[i].str, szLine);
		strncpy(pMemFree[i].p, p + 2, 10);
		// 0001: p=0x01175920,
		i++;
	}
	nFree = i;
	fclose(fp);

	fp = fopen(g_szMemOut, "wb");
	for (i = 0; i < nMalloc; i++)
	{
		printf("\ranalysis malloc %d/%d", i + 1, nMalloc);
		for (j = 0; j < nFree; j++)
		{
			if (pMemFree[j].bFree)
			{
				continue;
			}
			if (0 == strcmp(pMemMalloc[i].p, pMemFree[j].p))
			{
				pMemMalloc[i].bFree = 1;
				pMemFree[j].bFree = 1;
				break;
			}
		}
		if (0 == pMemMalloc[i].bFree)
		{
			fprintf(fp, "未释放：%s", pMemMalloc[i].str);
			fflush(fp);
		}
	}

	for (i = 0; i < nFree; i++)
	{
		if (0 == pMemFree[i].bFree)
		{
			fprintf(fp, "释放指针不存在：%s", pMemFree[i].str);
			fflush(fp);
		}
	}
	fclose(fp);

	free(pMemFree);
	free(pMemMalloc);

	printf("内存分析完成，请见 %s\r\n", g_szMemOut);
}
#endif //#if TEST_MEMORY_LEAK
