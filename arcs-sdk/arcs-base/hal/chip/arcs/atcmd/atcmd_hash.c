/**
 ****************************************************************************************
 *
 * @file atcmd_hash.c
 *
 * @brief
 *
 * Copyright (C) ListenAI  2023-2024
 *
 ****************************************************************************************
 */
#include "atcmd_hash.h"
#include "log_print.h"
#include "wifi_api.h"

#define ATC_INDEX_NUM 32
struct dlist_head atcmd_item_hash_list[ATC_INDEX_NUM];


static int hash_index(char *str)
{
    uint32_t seed = 131; // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);	
}

/**
 * @brief 添加AT命令到哈希表
 *
 * @param item_table 指向atcmd_item_t结构数组的指针
 * @param len 数组的长度
 * @return void
 */
void atcmd_entry_add_table(const atcmd_item_t *item_table, int len)
{
	int i;
    int index;
	for(i=0; i<len; i++) 
    {
        if((item_table[i].atcmd_entry.name != NULL) && 
           (item_table[i].atcmd_entry.func != NULL))
        {
            index = hash_index(item_table[i].atcmd_entry.name) % ATC_INDEX_NUM;
            // atcmd_item_t item_copy = item_table[i];
            atcmd_item_t *item_copy = (atcmd_item_t *)pvPortMalloc(sizeof(atcmd_item_t));
            memcpy(item_copy, &item_table[i], sizeof(atcmd_item_t));
            dlist_add(&item_copy->node, &atcmd_item_hash_list[index]);
        }
    }
}

/**
 * @brief 执行AT命令
 *
 * @param cmd 指向AT命令字符串的指针
 * @return void* 指向atcmd_entry_t结构的指针，如果未找到则返回NULL 
 */
void* atcmd_item_action(char *cmd)
{
	int search_cnt=0;
	int index = hash_index(cmd) % ATC_INDEX_NUM;
	struct dlist_head *head = &atcmd_item_hash_list[index];
	struct dlist_head *iterator;
	atcmd_item_t *item;
	void *act = NULL;
	// atcmd_entry_t *entry = NULL;
	dlist_for_each(iterator, head) 
    {
		item = dlist_entry(iterator, atcmd_item_t, node);
		//item = (atcmd_item_t *)iterator;
		search_cnt++;
		if( strcmp(item->atcmd_entry.name, cmd) == 0) 
        {
			//printf("%s match %s, search cnt %d\n\r", cmd, item->atcmd_entry.name, search_cnt);
			act = (void*)&(item->atcmd_entry);
            // entry = &(item->atcmd_entry);
            // return &(item->atcmd_entry);
			break;
		}
	}
	return act;
}

void atcmd_hash_init(void)
{
    int i;
	for(i = 0; i < ATC_INDEX_NUM; i++)
		DINIT_LIST_HEAD(&atcmd_item_hash_list[i]);

}