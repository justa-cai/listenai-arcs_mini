/**
 ****************************************************************************************
 *
 * @file co_list.h
 *
 * Copyright (C) ListenAI  2024-2025
 *
 * @brief Common list structures definitions
 *
 ****************************************************************************************
 */

#ifndef _CO_LIST_H_
#define _CO_LIST_H_

/**
 ****************************************************************************************
 * @defgroup CO_LIST CO_LIST
 * @ingroup COMMON
 * @brief  List management.
 *
 * This module contains the list structures and handling functions.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for __INLINE
#include <nmsis_gcc.h>      // for __INLINE


/*
 * STRUCTURE DECLARATIONS
 ****************************************************************************************
 */
/// structure of a list element header
struct ls_list_hdr
{
    /// Pointer to the next element in the list
    struct ls_list_hdr *next;
};

/// structure of a list
struct ls_list
{
    /// pointer to first element of the list
    struct ls_list_hdr *first;
    /// pointer to the last element
    struct ls_list_hdr *last;
};


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialize a list to defaults values.
 * @param[in] list           Pointer to the list structure.
 ****************************************************************************************
 */
void ls_list_init(struct ls_list *list);

/**
 ****************************************************************************************
 * @brief Initialize a pool to default values, and initialize the relative free list.
 *
 * @param[in] list           Pointer to the list structure
 * @param[in] pool           Pointer to the pool to be initialized
 * @param[in] elmt_size      Size of one element of the pool
 * @param[in] elmt_cnt       Nb of elements available in the pool
 * @param[in] default_value  Pointer to the default value of each element (may be NULL)
 ****************************************************************************************
 */
void ls_list_pool_init(struct ls_list *list,
                       void *pool,
                       size_t elmt_size,
                       uint32_t elmt_cnt,
                       void *default_value);

/**
 ****************************************************************************************
 * @brief Add an element as last on the list.
 *
 * @param[in] list           Pointer to the list structure
 * @param[in] list_hdr       Pointer to the header to add at the end of the list
 ****************************************************************************************
 */
void ls_list_push_back(struct ls_list *list,
                       struct ls_list_hdr *list_hdr);

/**
 ****************************************************************************************
 * @brief Add an element as first on the list.
 *
 * @param[in] list           Pointer to the list structure
 * @param[in] list_hdr       Pointer to the header to add at the beginning of the list
 ****************************************************************************************
 */
void ls_list_push_front(struct ls_list *list,
                        struct ls_list_hdr *list_hdr);
/**
 ****************************************************************************************
 * @brief Extract the first element of the list.
 *
 * @param[in] list           Pointer to the list structure
 *
 * @return The pointer to the element extracted, and NULL if the list is empty.
 ****************************************************************************************
 */
struct ls_list_hdr *ls_list_pop_front(struct ls_list *list);

/**
 ****************************************************************************************
 * @brief Search for a given element in the list, and extract it if found.
 *
 * @param[in] list           Pointer to the list structure
 * @param[in] list_hdr       Pointer to the searched element
 ****************************************************************************************
 */
void ls_list_extract(struct ls_list *list,
                     struct ls_list_hdr *list_hdr);

/**
 ****************************************************************************************
 * @brief Searched a given element in the list.
 *
 * @param[in] list           Pointer to the list structure
 * @param[in] list_hdr       Pointer to the searched element
 *
 * @return true if the element is found in the list, false otherwise
 ****************************************************************************************
 */
bool ls_list_find(struct ls_list *list,
                  struct ls_list_hdr *list_hdr);

/**
 ****************************************************************************************
 * @brief Insert an element in a sorted list.
 *
 * This primitive use a comparison function from the parameter list to select where the
 * element must be inserted.
 *
 * @param[in]  list     Pointer to the list.
 * @param[in]  element  Pointer to the element to insert.
 * @param[in]  cmp      Comparison function (return true if first element has to be
 *                      inserted before the second one).
 ****************************************************************************************
 */
void ls_list_insert(struct ls_list * const list,
                    struct ls_list_hdr * const element,
                    bool (*cmp)(struct ls_list_hdr const *elementA,
                                struct ls_list_hdr const *elementB));

/**
 ****************************************************************************************
 * @brief Insert an element in a list after the provided element.
 *
 * If @p prev_element is NULL then @p element is added in the front of the list.
 * Otherwise this primitive first ensure that @p prev_element is part of the list before
 * adding @p element, and does nothing if this is not the case.
 *
 * @param[in]  list           Pointer to the list.
 * @param[in]  prev_element   Pointer to the element to find in the list
 * @param[in]  element        Pointer to the element to insert.
 *
 ****************************************************************************************
 */
void ls_list_insert_after(struct ls_list * const list,
                          struct ls_list_hdr * const prev_element,
                          struct ls_list_hdr * const element);

/**
 ****************************************************************************************
 * @brief Insert an element in a list after the provided element.
 *
 * Same as @ref ls_list_insert_after except that if @p prev_element is not NULL no check
 * is done to ensure it is part of the list.
 *
 * @param[in]  list           Pointer to the list.
 * @param[in]  prev_element   Pointer to the element after which the new element must
 *                            be added
 * @param[in]  element        Pointer to the element to insert.
 *
 ****************************************************************************************
 */
void ls_list_insert_after_fast(struct ls_list * const list,
                               struct ls_list_hdr * const prev_element,
                               struct ls_list_hdr * const element);

/**
 ****************************************************************************************
 * @brief Insert an element in a sorted list before the provided element.
 *
 * This primitive use a comparison function from the parameter list to select where the
 * element must be inserted.
 *
 * @param[in]  list           Pointer to the list.
 * @param[in]  next_element   Pointer to the element to find in the list
 * @param[in]  element        Pointer to the element to insert.
 *
 * If next_element is not found, the provided element is not inserted
 ****************************************************************************************
 */
void ls_list_insert_before(struct ls_list * const list,
                           struct ls_list_hdr * const next_element,
                           struct ls_list_hdr * const element);

/**
 ****************************************************************************************
 * @brief Concatenate two lists.
 * The resulting list is the list passed as the first parameter. The second list is
 * emptied.
 *
 * @param[in]  list1          First list (will get the result of the concatenation)
 * @param[in]  list2          Second list (will be emptied after the concatenation)
 ****************************************************************************************
 */
void ls_list_concat(struct ls_list *list1, struct ls_list *list2);

/**
 ****************************************************************************************
 * @brief Remove the element in the list after the provided element.
 *
 * This primitive removes an element in the list. It is assume that element is part of
 * the list.
 *
 * @param[in] list          Pointer to the list.
 * @param[in] prev_element  Pointer to the previous element.
 *                          NULL if @p element is the first element in the list
 * @param[in] element       Pointer to the element to remove.
 *
 ****************************************************************************************
 */
void ls_list_remove(struct ls_list *list,
                    struct ls_list_hdr *prev_element,
                    struct ls_list_hdr *element);
/**
 ****************************************************************************************
 * @brief Test if the list is empty.
 *
 * @param[in] list           Pointer to the list structure.
 *
 * @return true if the list is empty, false else otherwise.
 ****************************************************************************************
 */
__INLINE bool ls_list_is_empty(const struct ls_list *list)
{
    return (list->first == NULL);
}

/**
 ****************************************************************************************
 * @brief Return the number of element of the list.
 *
 * @param[in] list           Pointer to the list structure.
 *
 * @return The number of elements in the list.
 ****************************************************************************************
 */
uint32_t ls_list_cnt(const struct ls_list *list);

/**
 ****************************************************************************************
 * @brief Pick the first element from the list without removing it.
 *
 * @param[in] list           Pointer to the list structure.
 *
 * @return First element address. Returns NULL pointer if the list is empty.
 ****************************************************************************************
 */
__INLINE struct ls_list_hdr *ls_list_pick(const struct ls_list * const list)
{
    return list->first;
}

/**
 ****************************************************************************************
 * @brief Pick the last element from the list without removing it.
 *
 * @param[in] list           Pointer to the list structure.
 *
 * @return Last element address. Returns invalid value if the list is empty.
 ****************************************************************************************
 */
__INLINE struct ls_list_hdr *ls_list_pick_last(const struct ls_list * const list)
{
    return list->last;
}

/**
 ****************************************************************************************
 * @brief Return following element of a list element.
 *
 * @param[in] list_hdr     Pointer to the list element.
 *
 * @return The pointer to the next element.
 ****************************************************************************************
 */
__INLINE struct ls_list_hdr *ls_list_next(const struct ls_list_hdr * const list_hdr)
{
    return list_hdr->next;
}


/// @} end of group COLIST

#endif // _CO_LIST_H_
