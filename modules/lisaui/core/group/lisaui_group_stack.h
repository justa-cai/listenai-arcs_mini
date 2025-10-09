/**
 * @file lisaui_group_stack.h
 * @brief Group stack management module, using dlist to implement stack-based group management
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_GROUP_STACK_H__
#define __LISAUI_GROUP_STACK_H__

#include <stddef.h>
#include <stdbool.h>
#include "lisaui_group.h"
#include "lisaui_type.h"
#include "dlist.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Group stack structure
 * @details Uses dlist doubly-linked list as the stack's data structure
 */
typedef struct {
    sys_dlist_t group_list; /**< Group doubly-linked list */
} lisaui_group_stack_t;

/**
 * @brief Create a group stack
 * @return Pointer to the newly created group stack, or NULL if failed
 */
lisaui_group_stack_t *lisaui_group_stack_create(void);
/**
 * @brief Destroy a group stack
 * @param stack Pointer to the group stack to be destroyed
 * @note This function only frees the stack structure memory, not the group objects in the stack
 */
void lisaui_group_stack_destroy(lisaui_group_stack_t *stack);
/**
 * @brief Push a group to the top of the stack
 * @param stack Pointer to the group stack
 * @param group Pointer to the group to be pushed
 * @return true if successful, false if failed
 */
bool lisaui_group_stack_push(lisaui_group_stack_t *stack, lisaui_group_t *group);
/**
 * @brief Pop the top group from the stack
 * @param stack Pointer to the group stack
 * @return Pointer to the popped group, or NULL if the stack is empty
 */
lisaui_group_t *lisaui_group_stack_pop(lisaui_group_stack_t *stack);
/**
 * @brief Pop a specific group by its group ID
 * @param stack Pointer to the group stack
 * @param group_id The ID of the group to be popped
 * @return Pointer to the found group, or NULL if not found
 * @note This function will pop the specified group and all groups above it
 */
lisaui_group_t *lisaui_group_stack_pop_by_index(lisaui_group_stack_t *stack, int group_id);
/**
 * @brief Look at a group in the stack by index (without popping)
 * @param stack Pointer to the group stack
 * @param index Index of the group, 0 means the top of the stack
 * @return Pointer to the group at the specified index, or NULL if the index is invalid
 */
lisaui_group_t *lisaui_group_stack_peek_by_index(lisaui_group_stack_t *stack, int index);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_GROUP_STACK_H__ */