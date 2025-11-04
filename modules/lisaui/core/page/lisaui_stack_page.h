/**
 * @file lisaui_stack_page.h
 * @brief Page stack management module, using dlist to implement stack-based page management
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_STACK_PAGE_H__
#define __LISAUI_STACK_PAGE_H__

#include <stddef.h>
#include <stdbool.h>
#include "lisaui_page.h"
#include "lisaui_type.h"
#include "utils/dlist.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Page stack structure
 * @details Uses dlist doubly-linked list as the stack's data structure
 */
typedef struct {
    sys_dlist_t page_list; /**< Page doubly-linked list */
} lisaui_page_stack_t;

/**
 * @brief Create a page stack
 * @return Pointer to the newly created page stack, or NULL if failed
 */
lisaui_page_stack_t *lisaui_page_stack_create(void);
/**
 * @brief Destroy a page stack
 * @param stack Pointer to the page stack to be destroyed
 * @note This function only frees the stack structure memory, not the page objects in the stack
 */
void lisaui_page_stack_destroy(lisaui_page_stack_t *stack);
/**
 * @brief Push a page to the top of the stack
 * @param stack Pointer to the page stack
 * @param page Pointer to the page to be pushed
 * @return true if successful, false if failed
 */
bool lisaui_page_stack_push(lisaui_page_stack_t *stack, lisaui_page_t *page);
/**
 * @brief Pop the top page from the stack
 * @param stack Pointer to the page stack
 * @return Pointer to the popped page, or NULL if the stack is empty
 */
lisaui_page_t *lisaui_page_stack_pop(lisaui_page_stack_t *stack);

/**
 * @brief Peek the top page from the stack
 * @param stack Pointer to the page stack
 * @return Pointer to the peek page, or NULL if the stack is empty
 */
lisaui_page_t *lisaui_page_stack_peek(lisaui_page_stack_t *stack);

/**
 * @brief Pop a specific page by its page ID
 * @param stack Pointer to the page stack
 * @param page_id The ID of the page to be popped
 * @return Pointer to the found page, or NULL if not found
 * @note This function will pop the specified page and all pages above it
 */
lisaui_page_t *lisaui_page_stack_pop_by_index(lisaui_page_stack_t *stack, int page_id);
/**
 * @brief Look at a page in the stack by index (without popping)
 * @param stack Pointer to the page stack
 * @param index Index of the page, 0 means the top of the stack
 * @return Pointer to the page at the specified index, or NULL if the index is invalid
 */
lisaui_page_t *lisaui_page_stack_peek_by_index(lisaui_page_stack_t *stack, int index);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_STACK_PAGE_H__ */