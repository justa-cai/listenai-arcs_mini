#ifndef __LISA_OS_QUEUE__
#define __LISA_OS_QUEUE__

#include <stdbool.h>
#include <stdint.h>
#include <lisa_err.h>

typedef void *lisa_queuehandle_t;

typedef struct {
	lisa_queuehandle_t handle;
	uint32_t count;
	uint32_t item_size;
} lisa_queue_t;

/**
 * @brief create
 * @param  count            
 * @param  queue_name       
 * @param  item_size        
 * @return lisa_queue_t* 
 */
lisa_queue_t *lisa_queue_create(uint32_t count, uint8_t *queue_name, uint32_t item_size);

/**
 * @brief  Push item to queue back
 * @param  queue		Queue Handle
 * @param  item			Item
 * @param  item_size	Item Size
 * @param  wait			Wait ticks
 * @return lisa_err_t	Result
 */
lisa_err_t lisa_queue_push(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait);

/**
 * @brief  Push item to queue front
 * @param  queue		Queue Handle
 * @param  item			Item
 * @param  item_size	Item Size
 * @param  wait			Wait ticks
 * @return lisa_err_t	Result
 */
lisa_err_t lisa_queue_push_front(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait);

/**
 * @brief receive
 * @param  queue            
 * @param  item             
 * @param  item_size        
 * @param  wait             
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_pop(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait);

/**
 * @brief expand queue
 * @param  queue
 * @param  expand_count
 * @param  item_size
 * @return lisa_err_t
 */
lisa_err_t lisa_queue_expand(lisa_queue_t *queue, uint32_t expand_count, uint32_t item_size);

/**
 * @brief queue full
 * @param  queue
 * @return lisa_err_t
 */
bool lisa_queue_full(lisa_queue_t *queue);

/**
 * @brief delete
 * @param  queue            
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_delete(lisa_queue_t *queue);

/**
 * @brief waiting size
 * @param  queue            
 * @return uint32_t 
 */
uint32_t lisa_queue_waiting(lisa_queue_t *queue);

/**
 * @brief clear
 * @param  queue            
 * @return lisa_err_t 
 */
lisa_err_t lisa_queue_clear(lisa_queue_t *queue);

/**
 * @brief get size
 * @param  queue
 * @return queue size
 */
uint32_t lisa_queue_size(lisa_queue_t *queue);

#endif  //__LISA_OS_QUEUE__
