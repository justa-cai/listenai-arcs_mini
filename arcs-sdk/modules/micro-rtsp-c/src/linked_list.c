#include "linked_list.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

llist_item_t* llist_init(void* value) {
	llist_item_t* item = (llist_item_t*)malloc(sizeof(llist_item_t));
	item->next = NULL;
	item->prev = NULL;
	item->value = value;
	return item;
}

void llist_append(llist_item_t* head, llist_item_t* item) {
	assert(head != NULL);
	assert(item != NULL);

	if (head->next == NULL) {
		head->next = item;
		item->prev = head;
		return;
	}

	llist_append(head->next, item);
}

void llist_foreach(llist_item_t* head, llist_callback callback) {
	assert(head != NULL);

	llist_item_t* i = head;
	while (i != NULL) {
		callback(i->value);
		i = i->next;
	}
}

bool llist_is_empty(llist_item_t* head) {
	return head == NULL;
}

void llist_remove_item(llist_item_t* item) {
	if (item->next == NULL && item->prev == NULL) {
		free(item);
		return;
	}

	if (item->next != NULL && item->prev != NULL) {
		item->next->prev = item->prev;
		item->prev->next = item->next;
		free(item);
		return;
	}

	if (item->next != NULL && item->prev == NULL) {
		item->next->prev = NULL;
		free(item);
		return;
	}

	if (item->next == NULL && item->prev != NULL) {
		item->prev->next = NULL;
		free(item);
		return;
	}
}

void llist_deinit(llist_item_t* item) {
	if (item == NULL) {
		return;
	}

	llist_deinit(item->next);
	llist_deinit(item->prev);

	free(item);
}
