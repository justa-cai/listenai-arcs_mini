/*
 * PowerManager.h
 *
 *  Created on: Feb 2, 2023
 *      Author: USER
 */

#ifndef SRC_VEGAP_REMOTE_PM_POWERMANAGER_H_
#define SRC_VEGAP_REMOTE_PM_POWERMANAGER_H_

#include "stdint.h"

typedef void pm_callonce_hook(void);
void PowerManager_CallOnce_Hook(pm_callonce_hook* __hook);

#endif /* SRC_VEGAP_REMOTE_PM_POWERMANAGER_H_ */
