#ifndef __LISA_MEM__
#define __LISA_MEM__

#include <stddef.h>
#include <stdint.h>

/**
 * @brief malloc
 * @param  size             
 * @return void* 
 */
void *lisa_mem_alloc(uint32_t size);

void *lisa_mem_align_alloc(uint32_t align,uint32_t size);
/**
 * @brief realloc
 * @param  ptr              
 * @param  size             
 * @return void* 
 */
void *lisa_mem_realloc(void *ptr, uint32_t size);

/**
 * @brief calloc
 * @param  count            
 * @param  size             
 * @return void* 
 */
void *lisa_mem_calloc(uint32_t count, uint32_t size);

/**
 * @brief free
 * @param  ptr              
 */
void lisa_mem_free(void *ptr);

/**
 * @brief sram malloc
 * @param size
 * @return void*
 */
void *lisa_mem_sram_alloc(uint32_t size);

/**
 * @brief sram calloc
 * @param  count
 * @param  size
 * @return void*
 */
void *lisa_mem_sram_calloc(uint32_t count, uint32_t size);

/**
 * @brief sram realloc
 * @param  ptr
 * @param  size
 * @return void*
 */
void *lisa_mem_sram_realloc(void *ptr, uint32_t size);

/**
 * @brief sram free
 * @param  ptr
 */
void lisa_mem_sram_free(void *ptr);

#endif  // __LISA_MEM__