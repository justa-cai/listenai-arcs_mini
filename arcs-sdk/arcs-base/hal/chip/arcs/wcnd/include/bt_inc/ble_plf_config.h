/**
 ****************************************************************************************
 *
 * @file ble_plf_config.h
 *
 * @brief API file for the runtime IP Configure
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef PLF_CONFIG_H_
#define PLF_CONFIG_H_

/**
 ****************************************************************************************
 * @defgroup PLF_CONFIG runtime Configure
 * @ingroup ROOT
 * @brief runtime Configure
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

// include app bt config header
#include "bt_config.h"
// include chip config header
#include <stdbool.h>        // boolean definition
#include <stdint.h>         // standard integer definition
#include "ble_drv.h"
/*
 * DEFINES
 ****************************************************************************************
 */

#define PLF_FEATS_ALL       (0xffff)


/// List of NVS identifiers
enum NVS_ID
{
    /// Local Bd Address
    NVS_ID_BD_ADDRESS                 = 0x01,
    /// Device Name
    NVS_ID_DEVICE_NAME                = 0x02,
    NVS_LEN_DEVICE_NAME               = 50,
    /// local IRK
    NVS_ID_LOC_IRK                    = 0x03,
    
    /// bt class link key.
    NVS_ID_LK_INDEX                   = 0x60,
    NVS_COUNT_LK                      = 8,
    NVS_ID_LK_FIRST                   = 0x61,
    NVS_ID_LK_LAST                    = NVS_ID_LK_FIRST + NVS_COUNT_LK - 1,
    NVS_LEN_LK                        = 24,
    
    NVS_ID_LTK_INDEX                  = 0x70,
    NVS_COUNT_LTK                     = 8,
    NVS_ID_LTK_FIRST                  = 0x71,
    NVS_ID_LTK_LAST                   = NVS_ID_LTK_FIRST + NVS_COUNT_LTK - 1,
    NVS_LEN_LTK                       = 36,
    NVS_FREQ_OFFSET_COMPENSATION      = 38,


    /// Last ble connect peer ba addr
    NVS_ID_PEER_ADDRESS               = 0x90,
    /// Last bt connect peer ba addr
    NVS_ID_BT_PEER_ADDRESS               = 0x91,

    /// Application specific
    NVS_ID_APP_SPECIFIC_FIRST         = 0xA0,
    NVS_ID_APP_SPECIFIC_LAST          = 0xAF,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// structure of software configure
struct plf_sys_config {
    /// transport feature, see @enum plf_hcit_feat
    uint8_t  hcit_feat;
	/// core feature, read only, see @enum plf_core_feat
	uint8_t  core_feat;
	/// host stack feature, read only, see @enum plf_stack_feat
	uint16_t stack_feat;
	/// message heap size
	uint16_t msg_heap;
	/// env heap size
	uint16_t env_heap;
	/// big buffer size
	uint16_t big_buf;
	/// small buffer size
	uint16_t small_buff;
	/// big buffer number
	uint8_t  big_nb;
	/// small buff number
	uint8_t  small_nb;

};

/// memory heap configure
struct plf_mem_config {
    /// pointer to message heap base
    uint8_t *heap_msg;
    /// pointer to message heap base
    uint8_t *heap_env;
    /// pointer to message heap base
    uint8_t *heap_noret;
    /// pointer to message heap base
    uint8_t *heap_buf;
    /// message heap size
    uint16_t msg_size;
    /// env heap size
    uint16_t env_size;
    /// env heap size
    uint16_t noret_size;
    /// buffer heap size
    uint16_t buf_size;
};

struct plf_mem_info {
    /// stack free
    uint16_t stack_free;
    /// heap free
    uint16_t heap_free;
    /// non retention heap free
    uint16_t non_ret_free;
};

/// structure of adv type data
struct plf_adv_type_data {
    /// adv type data 
    uint8_t  adv_type_data[3];
};

/*
 * GLOBAL VARIABLE DEFINITION
 *****************************************************************************************
 */



/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief get feature
 *
 * @param[in] feat_idx   feature index, see @enum plf_feat_idx
 *
 * @return feature of given index
 ****************************************************************************************
 */
uint16_t plf_get_feat(uint8_t feat_idx);

/**
 ****************************************************************************************
 * @brief set hcit feature
 *
 * @param[in] feat_idx   feature index, see @enum plf_feat_idx
 *
 * @return feature of given index
 ****************************************************************************************
 */
void plf_set_hcit_feat(uint8_t hcit_feat);

/**
 ****************************************************************************************
 * @brief get hcit feature
 *
 * @param[in] void
 *
 * @return feature of hci
 ****************************************************************************************
 */
uint8_t plf_get_hcit_feat(void);

/**
 ****************************************************************************************
 * @brief get config data, the caller can not change the data
 *
 * @param[out] pointer to the config structure
 *
 ****************************************************************************************
 */
void plf_get_config(struct plf_sys_config *config_data);

/**
 ****************************************************************************************
 * @brief set config data, the data will enable when next reset
 *
 * @param[in] config_id  ID of configure (@see enum plf_config_id)
 * @param[in] pointer to the config structure
 *
 ****************************************************************************************
 */
void plf_set_config(struct plf_sys_config *config_data);

/**
 ****************************************************************************************
 * @brief set config features, the config will enable when next reset
 *
 * @param[in]   hcit hcit features
 * @param[in]   core core features
 * @param[in]   stack stack features
 *
 ****************************************************************************************
 */
void plf_set_feats(uint8_t hcit, uint8_t core, uint16_t stack);

/**
 ****************************************************************************************
 * @brief get stack free size
 *
 *
 * @return stack free size
 ****************************************************************************************
 */
uint16_t plf_get_stack_free();

/**
 ****************************************************************************************
 * @brief get memory config data
 *
 * @param[out] pointer to the memory config structure
 *
 ****************************************************************************************
 */
void plf_get_mem_cfg(struct plf_mem_config *config);

/**
 ****************************************************************************************
 * @brief get memory information
 *
 * @param[out] pointer to the memory info structure
 *
 ****************************************************************************************
 */
void plf_get_mem_info(struct plf_mem_info *info);

/**
 ****************************************************************************************
 * @brief allocate memory block from heap
 *
 * @param[in]size   memory block size to allocate
 *
 ****************************************************************************************
 */
void *plf_malloc(uint16_t size);

/**
 ****************************************************************************************
 * @brief allocate memory block from non retention heap, the memory will be invalid after sleep
 *
 * @param[in]size   memory block size to allocate
 *
 ****************************************************************************************
 */
void *plf_malloc_noret(uint16_t size);

/**
 ****************************************************************************************
 * @brief release memory block
 *
 * @param[in]ptr    pointer to the memory block
 *
 ****************************************************************************************
 */
void plf_free(void *ptr);

/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */
void plf_init();

/**
 ****************************************************************************************
 * @brief reset platform
 *
 *
 ****************************************************************************************
 */
void plf_reset(uint8_t lsip_rst_state);

/**
 ****************************************************************************************
 * @brief platform sleep
 *
 *
 ****************************************************************************************
 */
void plf_sleep(uint8_t sleep_state);

/// @} PLF_CONFIG

#endif // PLF_CONFIG_H_
