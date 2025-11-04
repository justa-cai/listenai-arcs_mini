/*
 * Project:      GPT (General Purpose Timer)
 *               Driver definitions
 */

#ifndef __GPT_COMMON_H
#define __GPT_COMMON_H

#include "Driver_Common.h"

#define CSK_DRIVER_VERSION_MAJOR_MINOR(major,minor) (((major) << 8) | (minor))
#define CSK_GPT_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,01)  /* API version */


/*---------------------Control mode for application---------------------------------*/
typedef enum {
  GPT_CHANNEL0                    = 0,                /**< GPT channel 0 transfer complete */
  GPT_CHANNEL1                    = 1,                /**< GPT channel 1 transfer complete */
  GPT_CHANNEL2                    = 2,                /**< GPT channel 2 transfer complete */
  GPT_CHANNEL3                    = 3,                /**< GPT channel 3 transfer complete */
  GPT_CHANNEL4                    = 4,                /**< GPT channel 4 transfer complete */
  GPT_CHANNEL5                    = 5,                /**< GPT channel 5 transfer complete */
  GPT_CHANNEL6                    = 6,                /**< GPT channel 6 transfer complete */
  GPT_CHANNEL7                    = 7,                /**< GPT channel 7 transfer complete */
} GPT_CHANNEL_TYPE;


typedef enum {
  GPT_CHANNEL0_SYNC               = (1<<0),                /**< GPT channel 0 sync channel */
  GPT_CHANNEL1_SYNC               = (1<<1),                /**< GPT channel 1 sync channel */
  GPT_CHANNEL2_SYNC               = (1<<2),                /**< GPT channel 2 sync channel */
  GPT_CHANNEL3_SYNC               = (1<<3),                /**< GPT channel 3 sync channel */
  GPT_CHANNEL4_SYNC               = (1<<4),                /**< GPT channel 4 sync channel */
  GPT_CHANNEL5_SYNC               = (1<<5),                /**< GPT channel 5 sync channel */
  GPT_CHANNEL6_SYNC               = (1<<6),                /**< GPT channel 6 sync channel */
  GPT_CHANNEL7_SYNC               = (1<<7),                /**< GPT channel 7 sync channel */
} GPT_CHANNEL_SYNC_TYPE;


/****** GPT specific error codes *****/
// hardware resource confliction, change a channel.
#define CSK_GPT_ERROR_HARDWARE_CONFLICTION                 (CSK_DRIVER_ERROR_SPECIFIC - 1)


/****** GPT Event *****/
#define CSK_GPT_EVENT_OVERFLOW      (1UL << 0)  			///< GPT channel overflow
#define CSK_GPT_EVENT_INPUTCAPTURE  (1UL << 1)  			///< GPT channel input capture
#define CSK_GPT_EVENT_LEDC_TX_DONE  (1UL << 2)  			///< GPT channel ledc tx done

/*---------------------Function Interface for application---------------------------*/

/**
 \brief GPT Status
 */
typedef struct _CSK_GPT_STATUS
{
    uint32_t configured :8;         ///< GPT channel state: 1=Configured, 0=Unconfigured
} CSK_GPT_STATUS;



/**
 \fn          void CSK_GPT_SignalEvent (uint32_t event)
 \brief       Signal GPT Events.
 \param[in]   event  \ref GPT_events notification mask
 \return      none
  */
typedef void
(*CSK_GPT_SignalEvent_t)(uint32_t event, void *param);

/**
 \brief GPT Device Driver Capabilities.
 */
typedef struct _CSK_GPT_CAPABILITIES
{
    uint32_t channels :8;  ///< supports GPT channel numbers
} CSK_GPT_CAPABILITIES;

/**
 \brief Access function of the GPT Driver.
 */

/**
 * @brief Initialize the GPT (General Purpose Timer) module.
 *
 * This function initializes the General Purpose Timer (GPT) with the provided parameters.
 * It sets up the timer based on the configuration specified in user_param.
 *
 * @param pGpt A pointer to the GPT instance to be initialized.
 * @param user_param A pointer to user-defined parameters for initialization.
 *                   This could include settings like timer mode, prescaler value, etc.
 *
 * @return int32_t Returns a status code indicating the result of the initialization.
 *                 A return value of 0 indicates success, while any other value indicates an error.
 */
int32_t GPT_Initialize(void *pGpt, void *user_param);

/**
 * @brief Uninitialize the GPT (General Purpose Timer) module.
 *
 * This function uninitializes the General Purpose Timer (GPT) and releases any resources
 * that were allocated during initialization. It ensures that the timer is stopped and
 * all associated hardware settings are reset to their default states.
 *
 * @param pGpt A pointer to the GPT instance to be uninitialized.
 *
 * @return int32_t Returns a status code indicating the result of the uninitialization.
 *                 A return value of 0 indicates success, while any other value indicates an error.
 */
int32_t GPT_Uninitialize(void *pGpt);

/*
* This function retrieves the current version of the GPT driver being used.
* The version information can be useful for compatibility checks, debugging, or logging purposes.
*
* @return CSK_DRIVER_VERSION Returns the version of the GPT driver.
*/
CSK_DRIVER_VERSION GPT_GetVersion(void);


/**
 * @brief Control the power state of the GPT (General Purpose Timer).
 *
 * This function allows the user to control the power state of the specified GPT instance.
 * The power state can be set to various predefined states such as ON, OFF, or STANDBY, depending on the capabilities of the hardware.
 *
 * @param pGpt A pointer to the GPT instance whose power state is being controlled.
 * @param state The desired power state to be set for the GPT.
 *
 * @return int32_t Returns a status code indicating the result of the operation.
 *                A return value of 0 indicates success, while any other value indicates an error.
 */
int32_t GPT_PowerControl(void *pGpt, CSK_POWER_STATE state);

/**
 * @brief Get the capabilities of the GPT (General Purpose Timer).
 *
 * This function retrieves the capabilities supported by the specified GPT instance.
 * Capabilities might include features like timer modes, resolution, or specific hardware functionalities.
 *
 * @param pGpt A pointer to the GPT instance for which capabilities are being queried.
 *
 * @return CSK_GPT_CAPABILITIES Returns a structure containing the capabilities of the GPT.
 */
CSK_GPT_CAPABILITIES GPT_GetCapabilities(void *pGpt);


/**
 * @brief Get the current status of the GPT (General Purpose Timer).
 *
 * This function retrieves the current operational status of the specified GPT instance.
 * The status information can include whether the timer is running, if it has encountered any errors,
 * or other relevant state indicators.
 *
 * @param pGpt A pointer to the GPT instance whose status is being queried.
 *
 * @return CSK_GPT_STATUS Returns a structure containing the current status of the GPT.
 */
CSK_GPT_STATUS GPT_GetStatus(void *pGpt);


#endif /* __GPT_COMMON_H */
