#ifndef __TINY_MALLOC_H__
#define __TINY_MALLOC_H__

#include "config.h"

#ifndef NULL
#define NULL 0
#endif


#if MALLOC_FROM_ADDR_ENABLE
#define MEM_ADDR                MALLOC_ADDR
#define MEM_MAX_SIZE            MALLOC_SIZE                             /*!< Total memory size 4MB */
#define MEM_ALLOC_TABLE_SIZE    128                                     /*!< memory table size, must less 255*/
#define MEM_BLOCK_SIZE          (MEM_MAX_SIZE / MEM_ALLOC_TABLE_SIZE)   /*!< memory block size 16KB */
#else

#define MEM_MAX_SIZE            MALLOC_SIZE                             /*!< Total memory size 160KB */
#define MEM_ALLOC_TABLE_SIZE    128                                     /*!< memory table size, must less 255 */
#define MEM_BLOCK_SIZE          (MEM_MAX_SIZE / MEM_ALLOC_TABLE_SIZE)   /*!< memory block size 2048Bytes */
#define MEM_ALIGN_BYTES         4                                       /*!< __dtsibute__ ((aligned(4))) */
#endif

//#define MALLOC_PSRAM_ENABLE     0
//
//#if MALLOC_PSRAM_ENABLE
//#define MEM_PSRAM_ADDR          0x28000000
//#define MEM_BLOCK_SIZE          (16*1024)                 /*!< memory block size 16KB */
//#define MEM_MAX_SIZE            (4*1024*1024)            /*!< Total memory size 4MB */
//#define MEM_ALLOC_TABLE_SIZE    (MEM_MAX_SIZE/MEM_BLOCK_SIZE) /*!< memory table size, must less 255*/
//#else
//#define MEM_BLOCK_SIZE          2048                /*!< memory block size 2048Bytes */
//#define MEM_MAX_SIZE            (256*1024)            /*!< Total memory size 160KB */
//#define MEM_ALLOC_TABLE_SIZE    (MEM_MAX_SIZE/MEM_BLOCK_SIZE) /*!< memory table size, must less 255 */
//#endif

/**
* @brief Release the memory
* @param[in] offset: memory first address
* @return SUCCESS:0 or FAILED:-1
**/
int tiny_free(void *ptr);

/**
* @brief Allocate memory
* @param[in] size: memory size (bytes)
* @return The first address of the memory allocated
**/
void *tiny_malloc(unsigned int size);

/**
* @brief Redistribute memory
* @param[in] - *ptr: old memory first address
             - size: memory size (bytes)
* @return The newly allocated memory first address
**/
void *tiny_realloc(void *ptr, unsigned int size);

/**
* @brief Get memory usage
* @param[in] void
* @return Usage rate (0 ~ 100)
**/
unsigned char tiny_mem_perused(void);

#endif

