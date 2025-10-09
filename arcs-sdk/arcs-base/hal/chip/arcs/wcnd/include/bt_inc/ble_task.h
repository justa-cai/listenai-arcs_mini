/**
 ****************************************************************************************
 *
 * @file ble_task.h
 *
 * @brief Header file - BLE APP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_TASK_H_
#define BLE_TASK_H_

/**
 ****************************************************************************************
 * @addtogroup BLE_TASK
 * @ingroup
 *
 * @brief BLE task entry point.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "stdint.h"


/*
 * DEFINES
 ****************************************************************************************
 */



/*
 * MACROS
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
/// result of sleep state.
enum ble_sleep_state
{
    /// Some activity pending, can not enter in sleep state
    BLE_ACTIVE    = 0,
    /// CPU can be put in sleep state
    BLE_CPU_SLEEP,
    /// IP could enter in deep sleep
    BLE_DEEP_SLEEP,
};


/// Types of initialization of the IP
enum ble_init_type
{
    /// IP initialization
    BLE_TASK_INIT    = 0,
    /// IP first reset (done once after initialization, before protocol stack is used)
    BLE_TASK_1ST_RST,
    /// Normal IP reset (can be done at any time when protocol stack is in use)
    BLE_TASK_RST,
};
/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


typedef struct ble_task_cb
{
    /**
     ****************************************************************************************
     * @brief Call back when the stack is initialize
     *
     * @param type      Initialize type @see enum ble_init_type
     ****************************************************************************************
     */
    void (*cb_ble_init)(uint8_t type);

    /**
     ****************************************************************************************
     * @brief Reception of stack reset complete
     ****************************************************************************************
     */
    void (*cb_ble_reset_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief check if the system can sleep
     *
     * @return sleep status, 1 for sleep
     ****************************************************************************************
     */
    uint8_t (*cb_ble_can_sleep)();

    /**
     ****************************************************************************************
     * @brief System enter sleep mode
     ****************************************************************************************
     */
    void (*cb_ble_sleep)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief execute user schedule one step
     ****************************************************************************************
     */
    void (*cb_user_schedule)(void);

} ble_task_cb_t;
/*
 * GLOBAL VARIABLE DECLARATION
 ****************************************************************************************
 */


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialize user interface.
 *
 * @param cb      task callback
 *
 ****************************************************************************************
 */
void ble_task_pre_init(const ble_task_cb_t *cb);

/**
 ****************************************************************************************
 * @brief Initialize the BLE task.
 ****************************************************************************************
 */
void ble_task_init(void);

/**
 ****************************************************************************************
 * @brief Reset the BLE task.
 ****************************************************************************************
 */
void ble_task_reset(void);

/**
 ****************************************************************************************
 * @brief Execute the BLE task.
 ****************************************************************************************
 */
void ble_task_execute(void);

/**
 ****************************************************************************************
 * @brief Check if the BLE task can sleep.
 ****************************************************************************************
 */
uint8_t ble_task_sleep(void);

typedef void (*ble_task_timer_cb)(void* p_env);

/**
 ****************************************************************************************
 * @brief Initialize timer structure.
 *
 * @param[in] p_timer    Pointer to the timer structure.
 * @param[in] cb         Function to be called upon timer expiration.
 * @param[in] p_env      Pointer to be passed to the callback
 ****************************************************************************************
 */
void ble_task_timer_init(void** p_timer, ble_task_timer_cb cb, void* p_env);

void ble_task_timer_uninit(void* p_timer);

/**
 ****************************************************************************************
 * @brief Program a timer to be scheduled in the future.
 *        If timer is already programmed, it is restarted.
 *        If delay is less than 10ms, delay is set to 10ms.
 *
 * @param[in] p_timer    Pointer to the timer structure.
 * @param[in] delay_ms   Duration before expiration of the timer (in milliseconds).
 ****************************************************************************************
 */
void ble_task_timer_set(void* p_timer, uint32_t delay_ms);

/**
 ****************************************************************************************
 * @brief Program a timer to be scheduled periodically. If timer is already programmed,
 *        it is restarted.
 *        If period exceed maximum value, timer is programmed using maximum period.
 *        If period less than 10ms, period is set to 10ms.
 *
 * @param[in] p_timer    Pointer to the timer structure.
 * @param[in] period_ms  Periodic duration (in milliseconds). Range [10, 48388607] max ~2 hours
 ****************************************************************************************
 */
void ble_task_timer_periodic_set(void* p_timer, uint32_t period_ms);

uint8_t ble_task_timer_get(void* p_timer);

/**
 ****************************************************************************************
 * @brief Stop a programmed timer.
 *
 * @param[in] p_timer    Pointer to the timer structure.
 ****************************************************************************************
 */
void ble_task_timer_stop(void* p_timer);
/// @} BLE_TASK
///

#endif // BLE_TASK_H_
