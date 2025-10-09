#ifndef __IC_MUTEX_H__
#define __IC_MUTEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ic_platform.h"

typedef enum {
    IC_MUTEX_TYPE_CRYPTO,
    IC_MUTEX_TYPE_GPADC,
    IC_MUTEX_TYPE_I2C,
    IC_MUTEX_TYPE_IPC,
    IC_MUTEX_TYPE_LUNA,
    IC_MUTEX_TYPE_5,
    IC_MUTEX_TYPE_6,
    IC_MUTEX_TYPE_7,
    IC_MUTEX_TYPE_MAX = 8,
} ic_mutex_type;

#if IC_MUTEX_TYPE_MAX > IC_MUTEX_CHANNEL_MAX
#error "IC_MUTEX_TYPE_MAX must be less than IC_MUTEX_CHANNEL_MAX."
#endif

typedef enum {
    /*! No Error */
    IC_MUTEX_OK                                     =  0,
    /*! Arguments to the API functions are not valid  */
    IC_MUTEX_ERR_INVALID_ARG                        = -1,
    /*! Attempting to release a mutex not acquired by the core */
    IC_MUTEX_ERR_NOT_OWNER                          = -2,
    /*! Mutex has already been acquired */
    IC_MUTEX_ERR_ACQUIRED                           = -3,
    /*! Maximum number of waiters limit reached on mutex */
    IC_MUTEX_ERR_MAX_WAITERS                        = -4,

    IC_MUTEX_ERR_PREEMPTED                          = -5,

    IC_MUTEX_ERR_REACQUIRE                          = -6,
    /*! Encountered an internal error */
    IC_MUTEX_ERR_INTERNAL                           = -10
} ic_mutex_status_t;


/*! Minimum size of a mutex object in bytes. */
#define IC_MUTEX_STRUCT_SIZE     (20)

#ifndef __IC_MUTEX_INTERNAL_H__
struct _ic_mutex {
    char _[IC_MUTEX_STRUCT_SIZE];
};
#endif

/*! Inter-core Mutex object type. */
typedef struct _ic_mutex IC_Mutex;

/*!
 * This enum defines what a core needs to do when waiting for an inter-core
 * mutex to be released.
 *
 * The spin-wait is useful if the application expects the synchronization to
 * happen rather quickly and wants to avoid the overheads of the block and
 * wakeup.
 *
 * The sleep-wait provides a low-power alternative if the wait time is
 * significant. If sleep-waiting, the core that releases the inter-core mutex,
 * uses the mailbox-interrupt to notify the suspended cores.
 *
 * [sleep-wait behavior on RTOS]  
 *    The core do a sleep-wait by waiting on a RTOS semephore or else.
 *
 * [sleep-wait behavior on bare-metal]  
 *    The core do a sleep-wait using the wait-for-interrupt instruction.
 */
typedef enum {
    /*! Spins checking for the inter-core mutex to be available */
    IC_MUTEX_SPIN_WAIT  = 0, 
    /*! Blocks waiting for the inter-core mutex's notification */
    IC_MUTEX_SLEEP_WAIT = 1
} ic_mutex_wait_kind;

/*!
 * Initialize the inter-core mutex. The inter-core mutex object needs to be
 * initialized prior to use.
 *
 * \param mutex     Pointer to inter-core mutex object  
 * \param wait_kind Spin wait or sleep when waiting on the inter-core mutex.  
 * \return          IC_MUTEX_OK if successful, else returns one of  
 *                  - IC_ERROR_INTERNAL  
 *                  - IC_ERROR_INVALID_ARG
 */
extern ic_mutex_status_t IC_Mutex_init(IC_Mutex *mutex, ic_mutex_wait_kind wait_kind, ic_mutex_type mutex_id);

/*!
 * Acquire the inter-core mutex.  
 * If the inter-core mutex is not available, the core that does the acquire
 * either spin-waits or sleep-waits until the inter-core mutex is released.  
 * If blocked, the core that releases the inter-core mutex triggers the
 * mailbox-interrupt to unblock this core.  
 * Note, a core cannot reacquire an inter-core mutex that has already been
 * acquired by the same core prior to a release.
 *
 * \param mutex   Pointer to inter-core mutex object.  
 * \return        IC_MUTEX_OK if successful, else returns  
 *                - IC_ERROR_INTERNAL
 */
extern ic_mutex_status_t IC_Mutex_acquire(IC_Mutex *mutex);

/*!
 * Releases the inter-core mutex that was acquired using a prior call to
 * ic_mutex_acquire.  
 * If there are other blocked cores waiting to acquire the inter-core mutex, the
 * release unblocks the ealiest waiter. The inter-core mutex needs to be
 * released by the core that originally acquired it.
 *
 * \param mutex   Pointer to inter-core mutex object.  
 * \return        IC_MUTEX_OK, if successful, else returns  
 *                - IC_ERROR_MUTEX_NOT_OWNER  
 *                  if attempting to release the inter-core mutex that was not
 *                  acquired by this core.
 */
extern ic_mutex_status_t IC_Mutex_release(IC_Mutex *mutex);

/*!
 * Try to acquire the inter-core mutex and returns an error code if it cannot.  
 * On success, the inter-core mutex is acquired. Unlike ic_mutex_acquire, this
 * does not block.
 *
 * \param mutex Pointer to inter-core mutex object.  
 * \return      IC_MUTEX_OK if successful, else returns  
 *              - IC_ERROR_MUTEX_ACQUIRED  
 *                if inter-core mutex is already acquired.
 */
extern ic_mutex_status_t IC_Mutex_try_acquire(IC_Mutex *mutex);

#endif /* __IC_MUTEX_H__ */

