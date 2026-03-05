#ifndef __LISA_COMMON_TYPEDEF__
#define __LISA_COMMON_TYPEDEF__

#include <stdbool.h>

#ifndef false
#define false (0)
#endif

#ifndef true
#define true (1)
#endif

#define LISA_OS_WAIT_FOREVER (0xffffffffU) /* Wait forever timeout value */
#define LISA_WAIT_FOREVER    LISA_OS_WAIT_FOREVER
#define LISA_NO_WAIT         0 /* wait 0 second timeout value */

typedef enum {
    LISA_OS_PRIORITY_IDLE = 5,
    LISA_OS_PRIORITY_LOW             = 6,
    LISA_OS_PRIORITY_BELOW_NORMAL    = 7,
    LISA_OS_PRIORITY_NORMAL          = 8,
    LISA_OS_PRIORITY_ABOVE_NORMAL    = 9,
    LISA_OS_PRIORITY_HIGH            = 10,
    LISA_OS_PRIORITY_REAL_TIME       = 11
} LISA_OS_Priority;

#endif  // v
