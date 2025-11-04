#ifndef __LIST_H
#define __LIST_H

#if defined ( __CC_ARM   )
#ifndef inline
#define inline			__inline
#endif
#endif

/* This file is from Linux Kernel (include/linux/list.h) 
 * and modified by simply removing hardware prefetching of list items. 
 * Here by copyright, credits attributed to wherever they belong.
 * Kulesh Shanmugasundaram (kulesh [squiggly] isis.poly.edu)
 */

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

//struct list_head {
//	struct list_head *next, *prev;
//};

#define DLIST_HEAD_INIT(name) { &(name), &(name) }

#define DLIST_HEAD(name) \
	struct dlist_head name = LIST_HEAD_INIT(name)

#define DINIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

struct dlist_head {
	struct dlist_head *next, *prev;
};

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __dlist_add(struct dlist_head *newitem,
			      struct dlist_head *prev,
			      struct dlist_head *next)
{
	next->prev = newitem;
	newitem->next = next;
	newitem->prev = prev;
	prev->next = newitem;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void dlist_add(struct dlist_head *newitem, struct dlist_head *head)
{
	__dlist_add(newitem, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void dlist_add_tail(struct dlist_head *newitem, struct dlist_head *head)
{
	__dlist_add(newitem, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __dlist_del(struct dlist_head *prev, struct dlist_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static inline void dlist_del(struct dlist_head *entry)
{
	__dlist_del(entry->prev, entry->next);
	entry->next = (struct dlist_head *) 0;
	entry->prev = (struct dlist_head *) 0;
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void dlist_del_init(struct dlist_head *entry)
{
	__dlist_del(entry->prev, entry->next);
	DINIT_LIST_HEAD(entry); 
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void dlist_move(struct dlist_head *list, struct dlist_head *head)
{
        __dlist_del(list->prev, list->next);
        dlist_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void dlist_move_tail(struct dlist_head *list,
				  struct dlist_head *head)
{
        __dlist_del(list->prev, list->next);
        dlist_add_tail(list, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int dlist_empty(struct dlist_head *head)
{
	return head->next == head;
}

static inline void __dlist_splice(struct dlist_head *list,
				 struct dlist_head *head)
{
	struct dlist_head *first = list->next;
	struct dlist_head *last = list->prev;
	struct dlist_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void dlist_splice(struct dlist_head *list, struct dlist_head *head)
{
	if (!dlist_empty(list))
		__dlist_splice(list, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void dlist_splice_init(struct dlist_head *list,
				    struct dlist_head *head)
{
	if (!dlist_empty(list)) {
		__dlist_splice(list, head);
		DINIT_LIST_HEAD(list);
	}
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define dlist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
* list_first_entry - get the first element from a list
* @ptr:        the list head to take the element from.
* @type:       the type of the struct this is embedded in.
* @member:     the name of the list_head within the struct.
*
* Note, that list is expected to be not empty.
*/

#define dlist_first_entry(ptr, type, member) \
        dlist_entry((ptr)->next, type, member)


/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define dlist_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
        	pos = pos->next)
/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define dlist_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); \
        	pos = pos->prev)
        	
/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define dlist_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define dlist_for_each_entry(pos, head, member, type)				\
	for (pos = dlist_entry((head)->next, type, member);	\
	     &pos->member != (head); 					\
	     pos = dlist_entry(pos->member.next, type, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define dlist_for_each_entry_safe(pos, n, head, member, type)			\
	for (pos = dlist_entry((head)->next, type, member),	\
		n = dlist_entry(pos->member.next, type, member);	\
	     &pos->member != (head); 					\
	     pos = n, n = dlist_entry(n->member.next, type, member))

#endif
