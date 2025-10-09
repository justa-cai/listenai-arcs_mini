#include <stdint.h>

#include "platform.h"
#include "lisaui_page.h"

#include "lisaui_log.h"
#define TAG "lisaui.page"

extern uint32_t __lisaui_pages_start;
extern uint32_t __lisaui_pages_end;

lisaui_page_t *lisaui_page_create(uint32_t group_id,uint32_t page_id){
    lisaui_page_t *page;
    lisaui_page_t *new_page;
    
    lisaui_page_t **page_array = (lisaui_page_t **)&__lisaui_pages_start;
    for (int i = 0; page_array[i] != NULL && &page_array[i] < (lisaui_page_t **)&__lisaui_pages_end; i++) {
        lisaui_page_t *page = page_array[i];
        LISAUI_LOGI(TAG,"pages group index:%d,page index:%d",page->group_index,page->page_index);
        if((page->group_index == group_id)&&(page->page_index == page_id)){
            new_page = lisaui_malloc(sizeof(lisaui_page_t));
            memcpy(new_page,page,sizeof(lisaui_page_t));
            new_page->create(new_page);
            return new_page;
        }
    }

    return NULL;
}

int lisaui_page_destroy(lisaui_page_t *page){

    if(page != NULL){
        if(page->destroy != NULL){
            page->destroy(page);
        }
        lisaui_free(page);
    }
    
    return 0;
}