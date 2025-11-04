/*
 *  chip.h
 *
 *  Created on: Aug 14, 2020
 *
 */

#ifndef INCLUDE_CHIP_H_
#define INCLUDE_CHIP_H_

#include "chip_id.h"


#if (CHIP == vega)
#include "vega_ap.h"
#elif (CHIP == vegap)
#include "vegap_ap.h"
#elif (CHIP == vegah)
#include "vegah_ap.h"
#elif (CHIP == venus)
#include "venus_ap.h"
#elif (CHIP == arcs)
#include "arcs_ap.h"
//#elif (CHIP == arcs_cp)
//#include "arcs_cp.h"
#elif (CHIP == mars)
#include "mars.h"
#elif (CHIP == apus)
#include "apus.h"
#elif (CHIP == jupiter)
#include "jupiter_ap.h"
#elif (CHIP == venusa)
#include "venusa_ap.h"
#endif

#endif /* INCLUDE_CHIP_H_ */
