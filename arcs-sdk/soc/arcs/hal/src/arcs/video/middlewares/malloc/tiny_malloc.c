#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tiny_malloc.h"

#define ERROR_OFFSET 0xFFFFFFFF                         /*!< error offset */

#ifdef MEM_ADDR
static unsigned char *membase = (unsigned char *)MEM_ADDR;
#else
__attribute__ ((aligned(MEM_ALIGN_BYTES))) static unsigned char membase[MEM_MAX_SIZE];             /*!< Total memory size */
#endif
static unsigned char memmapbase[MEM_ALLOC_TABLE_SIZE];  /*!< memory table size */


/**
* @brief memory management initialization
* @param[in] void
* @return void
**/
static void mem_init(void)
{
    static unsigned char men_init_flag = 0;
    if(men_init_flag == 0)
    {
        /* Memory status table data is cleared */
        memset(memmapbase, 0, (MEM_ALLOC_TABLE_SIZE * sizeof(unsigned char)));

        /* memory pool all data is cleared */
        //memset(membase, 0, (MEM_MAX_SIZE * sizeof(unsigned char)));

        /* Memory management initialization OK */
        men_init_flag = 1;
    }
}


/**
* @brief memory allocation
* @param[in] size: the size of the memory to be allocated (bytes)
* @return -  0XFFFFFFFF:representing the error
*         -  other:memory offset address
**/
static unsigned int mem_malloc(unsigned int size)
{
    /* memory offset address */
    signed long offset = 0;

    /* The number of memory blocks needed */
    unsigned int nmemb = 0;

    /* Continuous empty memory block count */
    unsigned int cmemb = 0;

    unsigned int cnt = 0;

    /* is not initialized, first perform initialization */
    mem_init();

    /* No need to allocate */
    if(size == 0)
        return ERROR_OFFSET;

    /* Get the number of contiguous memory blocks that need to be allocated */
    nmemb = size / MEM_BLOCK_SIZE;
    if(size % MEM_BLOCK_SIZE)
        nmemb++;

    /* Search the entire memory control area */
    for(offset = MEM_ALLOC_TABLE_SIZE - 1; offset >= 0; offset--)
    {
        /* The number of consecutive empty memory blocks increases */
        if(!memmapbase[offset])
            cmemb++;
        else
            /* Continuous memory block is cleared */
            cmemb = 0;

        /* Found a continuous nmemb empty memory block */
        if(cmemb == nmemb)
        {
            /* Mark the memory block is not empty */
            for(cnt = 0; cnt < nmemb; cnt++)
                memmapbase[offset + cnt] = nmemb;

            /* Return offset address */
            return (offset * MEM_BLOCK_SIZE);
        }
    }

    /* No memory block matching the allocation condition was found */
    return ERROR_OFFSET;
}


/**
* @brief Release the memory
* @param[in] offset: memory address offset
* @return SUCCESS or FAILED
**/
static int mem_free(unsigned int offset)
{
    int cnt = 0;

    /* Offset in the memory pool */
    if(offset < MEM_MAX_SIZE)
    {
        /* The memory block number where the offset is located */
        int index = offset / MEM_BLOCK_SIZE;

        /* The number of memory blocks */
        int nmemb = memmapbase[index];

        /* The memory block is cleared */
        for(cnt = 0; cnt < nmemb; cnt++)
            memmapbase[index + cnt] = 0;

        return 0;
    }
    else
    {
        /* Offset super zone */
        return -1;
    }
}


/**
* @brief Release the memory
* @param[in] offset: memory first address
* @return SUCCESS or FAILED
**/
int tiny_free(void *ptr)
{
    int ret;

    /* memory address offset */
    unsigned int offset;

    /* save interrupted information */
    //unsigned int irqsave = 0;

    /* The address is 0 */
    if(ptr == NULL)
        return -1;

    /* close and save all interrupt */
    //irqsave = hal_lock_irqsave();

    offset = (unsigned int)ptr - (unsigned int)membase;
    ret = mem_free(offset);

    /* state before recovery */
    //hal_unlock_irqrestore(irqsave);

    return ret;
}


/**
* @brief Allocate memory
* @param[in] size: memory size (bytes)
* @return The first address of the memory allocated
**/
void *tiny_malloc(unsigned int size)
{
    /* memory address offset */
    unsigned int offset;

    /* save interrupted information */
    //unsigned int irqsave = 0;

    /* close and save all interrupt */
    //irqsave = hal_lock_irqsave();

    offset = mem_malloc(size);

    /* state before recovery */
    //hal_unlock_irqrestore(irqsave);

    if(offset == ERROR_OFFSET)
        return NULL;
    else
        return (void*)((unsigned int)membase + offset);
}


/**
* @brief Redistribute memory
* @param[in] - *ptr: old memory first address
             - size: memory size (bytes)
* @return The newly allocated memory first address
**/
void *tiny_realloc(void *ptr, unsigned int size)
{
    /* memory address offset */
    unsigned int offset;

    /* save interrupted information */
    //unsigned int irqsave = 0;

    /* close and save all interrupt */
    //irqsave = hal_lock_irqsave();

    offset = mem_malloc(size);
    if(offset == 0xffffffff)
    {
        /* state before recovery */
        //hal_unlock_irqrestore(irqsave);
        return NULL;
    }
    else
    {
        /* Copy the old memory content to the new memory */
        memcpy((void*)((unsigned int)membase + offset), ptr, size);

        /* release the old memory */
        tiny_free(ptr);

        /* state before recovery */
        //hal_unlock_irqrestore(irqsave);

        /* Return to the new memory first address */
        return (void*)((unsigned int)membase + offset);
    }
}


/**
* @brief Get memory usage
* @param[in] void
* @return Usage rate (0 ~ 100)
**/
unsigned char tiny_mem_perused(void)
{
    unsigned int used = 0;
    unsigned int cnt = 0;
    for(cnt = 0; cnt < MEM_ALLOC_TABLE_SIZE; cnt++)
    {
        if(memmapbase[cnt])
            used++;
    }
    return ((used * 100) / (MEM_ALLOC_TABLE_SIZE));
}



