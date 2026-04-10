#ifndef __RTSP__LINKED_LIST__H
#define __RTSP__LINKED_LIST__H

#include <stdbool.h>


typedef void(llist_callback)(void* value);
typedef struct llist_item_t llist_item_t;

struct llist_item_t {
    llist_item_t* next;
    llist_item_t* prev;
    void* value;
};

llist_item_t* llist_init(void* value);
void llist_append(llist_item_t* head, llist_item_t* item);
void llist_foreach(llist_item_t* head, llist_callback callback);
bool llist_is_empty(llist_item_t* head);
void llist_remove_item(llist_item_t* item);
void llist_deinit(llist_item_t* item);

#endif // __RTSP__LINKED_LIST__H
