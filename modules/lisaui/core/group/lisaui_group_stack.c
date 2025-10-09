/**
 * @file page_stack.c
 * @brief Implementation of the page stack management module
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdint.h>
#include "platform.h"
// Note: Use the same include method as the header file to avoid path issues
#include "lisaui_group_stack.h"


/**
 * @brief Create a group stack
 * @return Pointer to the newly created group stack, or NULL if failed
 */
lisaui_group_stack_t *lisaui_group_stack_create(void)
{
    lisaui_group_stack_t *stack = (lisaui_group_stack_t *)lisaui_malloc(sizeof(lisaui_group_stack_t));
    if (stack == NULL) {
        return NULL;
    }
    
    // Initialize the doubly-linked list
    sys_dlist_init(&stack->group_list);
    
    return stack;
}

/**
 * @brief Destroy a group stack
 * @param stack Pointer to the group stack to be destroyed
 * @note This function only frees the stack structure memory, not the group objects in the stack
 */
void lisaui_group_stack_destroy(lisaui_group_stack_t *stack)
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
bool lisaui_group_stack_push(lisaui_group_stack_t *stack, lisaui_group_t *group)
{
    if (stack == NULL || group == NULL) {
        return false;
    }
    
    // Initialize the page node and add it to the tail of the list (top of the stack)
    sys_dlist_append(&stack->group_list, &group->node);
    
    return true;
}

/**
 * @brief Pop the top page from the stack
 * @param stack Pointer to the page stack
 * @return Pointer to the popped page, or NULL if the stack is empty
 */
lisaui_group_t *lisaui_group_stack_pop(lisaui_group_stack_t *stack)
{
    if (stack == NULL) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->group_list)) {
        return NULL;
    }
    
    // Get the tail node (top of the stack) and remove it from the list
    sys_dnode_t *node = sys_dlist_peek_tail(&stack->group_list);
    if (node == NULL) {
        return NULL;
    }
    
    // Remove the node from the list
    sys_dlist_remove(node);
    
    // Return the pointer to the page structure containing this node
    return CONTAINER_OF(node, lisaui_group_t, node);
}

/**
 * @brief Pop a specific page by its group ID
 * @param stack Pointer to the group stack
 * @param group_id The ID of the group to be popped
 * @return Pointer to the found group, or NULL if not found
 * @note This function will only pop the specified group, keeping other groups in the stack
 */
lisaui_group_t *lisaui_group_stack_pop_by_index(lisaui_group_stack_t *stack, int group_id)
{
    lisaui_group_t *target_group = NULL;
    sys_dnode_t *node;
    sys_dnode_t *tmp_node;
    lisaui_group_t *group;
    
    if (stack == NULL) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->group_list)) {
        return NULL;
    }
    
    // Traverse the stack to find the group with the specified ID
    // Since there is no group_id field in the lisaui_group_t structure, we use the view address of the group as a unique identifier
    // Treat group_id as the pointer address value of the group's view for comparison
    SYS_DLIST_FOR_EACH_NODE_SAFE(&stack->group_list, node, tmp_node) {
        group = CONTAINER_OF(node, lisaui_group_t, node);
        
        // Convert group_id to a pointer address for comparison, assuming the passed group_id is actually an integer form of the pointer address
        if ((intptr_t)group->info.id == (intptr_t)group_id) {
            target_group = group;
            // Found the target group, remove only this group from the list
            sys_dlist_remove(node);
            return target_group;
        }
    }
    
    // The group with the specified ID was not found
    return NULL;
}

/**
 * @brief Look at a group in the stack by index (without popping)
 * @param stack Pointer to the group stack
 * @param index Index of the group, 0 means the top of the stack
 * @return Pointer to the group at the specified index, or NULL if the index is invalid
 */
lisaui_group_t *lisaui_group_stack_peek_by_index(lisaui_group_stack_t *stack, int index)
{
    if (stack == NULL || index < 0) {
        return NULL;
    }
    
    // Check if the stack is empty
    if (sys_dlist_is_empty(&stack->group_list)) {
        return NULL;
    }
    
    int current_index = 0;
    sys_dnode_t *node;
    size_t list_len = sys_dlist_len(&stack->group_list);
    
    // Check if the index is out of range
    if (index >= list_len) {
        return NULL;
    }
    
    // Start traversing from the top of the stack (tail of the list)
    SYS_DLIST_FOR_EACH_NODE(&stack->group_list, node) {
        // Calculate the reverse index, because we use the tail of the list as the top of the stack
        int reverse_index = list_len - 1 - current_index;
        if (reverse_index == index) {
            // Found the group at the specified index
            return CONTAINER_OF(node, lisaui_group_t, node);
        }
        current_index++;
    }
    
    // Should not reach here under normal circumstances
    return NULL;
}