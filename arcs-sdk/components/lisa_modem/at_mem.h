/**
 * @file at_mem.h
 * @brief Memory Allocation Abstraction Layer
 * @details Provides replaceable memory allocation interface.
 *          Default uses stdlib malloc/free, can be overridden
 *          with custom allocator (e.g., PSRAM).
 */

#ifndef __AT_MEM_H__
#define __AT_MEM_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory allocation operations
 */
typedef struct {
    void *(*alloc)(size_t size);
    void *(*calloc)(size_t count, size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void  (*free)(void *ptr);
} at_mem_ops_t;

/**
 * @brief Set custom memory allocator
 *
 * Must be called before any at_mem_* allocation.
 * If not called, stdlib malloc/free is used.
 *
 * @param ops Memory operations (NULL to reset to default)
 */
void at_mem_set_ops(const at_mem_ops_t *ops);

/**
 * @brief Allocate memory
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *at_mem_alloc(size_t size);

/**
 * @brief Allocate zero-initialized memory
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure
 */
void *at_mem_calloc(size_t count, size_t size);

/**
 * @brief Reallocate memory
 * @param ptr Pointer to previously allocated memory (can be NULL)
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *at_mem_realloc(void *ptr, size_t size);

/**
 * @brief Free memory
 * @param ptr Pointer to memory to free (can be NULL)
 */
void at_mem_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __AT_MEM_H__ */
