/**
 * @file page_stack.c
 * @brief Implementation of the page stack management module
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdint.h>
// Note: Use the same include method as the header file to avoid path issues
#include "lisaui_stack_page.h"

/**
 * @brief Create a page stack
 * @return Pointer to the newly created page stack, or NULL if failed
 */
lisaui_page_stack_t *lisaui_page_stack_create(void)
{
    lisaui_page_stack_t *stack = (lisaui_page_stack_t *)malloc(sizeof(lisaui_page_stack_t));
    if (stack == NULL) {
        return NULL;
    }
    
    // Initialize the doubly-linked list
    sys_dlist_init(&stack->page_list);
    
    return stack;
}

/**
 * @brief Destroy a page stack
 * @param stack Pointer to the page stack to be destroyed
 * @note This function only frees the stack structure memory, not the page objects in the stack
 */
void lisaui_page_stack_destroy(lisaui_page_stack_t *stack)
{
    if (stack == NULL) {
        return;
    }
    
    // Free the stack structure memory
    free(stack);
}

/**
 * @brief Push a page to the top of the stack
 * @param stack Pointer to the page stack
 * @param page Pointer to the page to be pushed
 * @return true if successful, false if failed
 */
bool lisaui_page_stack_push(lisaui_page_stack_t *stack, lisaui_page_t *page)
{
    if (stack == NULL || page == NULL) {
        return false;
    }
    
    // Initialize the page node and add it to the tail of the list (top of the stack)
    sys_dlist_append(&stack->page_list, &page->node);
    
    return true;
}

/**
 * @brief Pop the top page from the stack
 * @param stack Pointer to the page stack
 * @return Pointer to the popped page, or NULL if the stack is empty
 */
lisaui_page_t *lisaui_page_stack_pop(lisaui_page_stack_t *stack)
{
    if (stack == NULL) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->page_list)) {
        return NULL;
    }
    
    // Get the tail node (top of the stack) and remove it from the list
    sys_dnode_t *node = sys_dlist_peek_tail(&stack->page_list);
    if (node == NULL) {
        return NULL;
    }
    
    // Remove the node from the list
    sys_dlist_remove(node);
    
    // Return the pointer to the page structure containing this node
    return CONTAINER_OF(node, lisaui_page_t, node);
}

/**
 * @brief Peek the top page from the stack
 * @param stack Pointer to the page stack
 * @return Pointer to the peek page, or NULL if the stack is empty
 */
lisaui_page_t *lisaui_page_stack_peek(lisaui_page_stack_t *stack)
{
    if (stack == NULL) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->page_list)) {
        return NULL;
    }
    
    // Get the tail node (top of the stack) and remove it from the list
    sys_dnode_t *node = sys_dlist_peek_tail(&stack->page_list);
    if (node == NULL) {
        return NULL;
    }
    
    // Return the pointer to the page structure containing this node
    return CONTAINER_OF(node, lisaui_page_t, node);
}

/**
 * @brief Pop a specific page by its page ID
 * @param stack Pointer to the page stack
 * @param page_id The ID of the page to be popped
 * @return Pointer to the found page, or NULL if not found
 * @note This function will only pop the specified page, keeping other pages in the stack
 */
lisaui_page_t *lisaui_page_stack_pop_by_index(lisaui_page_stack_t *stack, int page_id)
{
    lisaui_page_t *target_page = NULL;
    sys_dnode_t *node;
    sys_dnode_t *tmp_node;
    lisaui_page_t *page;
    
    if (stack == NULL) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->page_list)) {
        return NULL;
    }
    
    // Traverse the stack to find the page with the specified ID
    // Since there is no page_id field in the lisaui_page_t structure, we use the view address of the page as a unique identifier
    // Treat page_id as the pointer address value of the page's view for comparison
    SYS_DLIST_FOR_EACH_NODE_SAFE(&stack->page_list, node, tmp_node) {
        page = CONTAINER_OF(node, lisaui_page_t, node);
        
        if (page->page_index == page_id) {
            target_page = page;
            // Found the target page, remove only this page from the list
            sys_dlist_remove(node);
            return target_page;
        }
    }
    
    // The page with the specified ID was not found
    return NULL;
}

/**
 * @brief Look at a page in the stack by index (without popping)
 * @param stack Pointer to the page stack
 * @param index Index of the page, 0 means the top of the stack
 * @return Pointer to the page at the specified index, or NULL if the index is invalid
 */
lisaui_page_t *lisaui_page_stack_peek_by_index(lisaui_page_stack_t *stack, int index)
{
    if (stack == NULL || index < 0) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->page_list)) {
        return NULL;
    }
    
    int current_index = 0;
    sys_dnode_t *node;
    size_t list_len = sys_dlist_len(&stack->page_list);
    
    // Check if the index is out of range
    if (index >= list_len) {
        return NULL;
    }
    
    // Start traversing from the top of the stack (tail of the list)
    SYS_DLIST_FOR_EACH_NODE(&stack->page_list, node) {
        // Calculate the reverse index, because we use the tail of the list as the top of the stack
        int reverse_index = list_len - 1 - current_index;
        if (reverse_index == index) {
            // Found the page at the specified index
            return CONTAINER_OF(node, lisaui_page_t, node);
        }
        current_index++;
    }
    
    // Should not reach here under normal circumstances
    return NULL;
}


