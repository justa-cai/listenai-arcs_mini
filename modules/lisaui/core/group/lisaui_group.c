
#include "lisaui_group.h"
#include "lisaui_log.h"

#define TAG "lisaui.group"


extern uint32_t __lisaui_group_level1_start;
extern uint32_t __lisaui_group_level1_end;
extern uint32_t __lisaui_group_level2_start;
extern uint32_t __lisaui_group_level2_end;

/**
 * @brief Initialize all registered groups based on their priority levels
 * 
 * This function initializes groups registered in special sections of the binary.
 * Groups are organized in two priority levels:
 * - Level 1: High priority groups that need to be initialized first
 * - Level 2: Lower priority groups that are initialized after level 1
 *
 * For each priority level, the function iterates through all registered group entries
 * and calls their initialization functions.
 *
 * @return int 0 on success, non-zero on error
 */
static int groups_init(void){

    group_entry_t *start;
    group_entry_t *end;
    group_entry_t *entry;

    /*priority level1*/
    start = (group_entry_t *)&__lisaui_group_level1_start;
    end = (group_entry_t *)&__lisaui_group_level1_end;
    LISAUI_LOGI(TAG,"Lisaui group init level1 start(%p -> %p)...",start,end);
    for (entry = start; entry < end; entry++) { 
        if (entry->group != NULL && entry->init_func != NULL) { 
            entry->init_func(); 
        } 
    }

    /*priority level2*/
    start = (group_entry_t *)&__lisaui_group_level2_start;
    end = (group_entry_t *)&__lisaui_group_level2_end;
    LISAUI_LOGI(TAG,"Lisaui group init level2 start(%p -> %p)...",start,end);
    for (entry = start; entry < end; entry++) { 
        if (entry->group != NULL && entry->init_func != NULL) { 
            entry->init_func(); 
        } 
    }

    return 0;
}  

lisaui_group_t *lisaui_group_find_by_id(int index){
    group_entry_t *start;
    group_entry_t *end;
    group_entry_t *entry;

    /*priority level1*/
    start = (group_entry_t *)&__lisaui_group_level1_start;
    end = (group_entry_t *)&__lisaui_group_level2_end;
    LISAUI_LOGI(TAG,"Lisaui find group by index:%d(%p -> %p)...",index,start,end);
    for (entry = start; entry < end; entry++) { 
        if (entry->group != NULL) { 
            if(entry->group->info.id == index){
                return entry->group;
            }
        }
    }
    return NULL;
}

int lisaui_group_init(void)
{
   
    groups_init();
    return 0;
}

