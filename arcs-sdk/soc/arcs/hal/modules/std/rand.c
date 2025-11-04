/**
  ******************************************************************************
  * @file    rand.c
  * @author  ListenAI Application Team
  * @brief   generate pseudo random.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */


////////////////////////////////////////////////////////////////////////////////
//                                                                            //
/// @file rand.c                                                              //
/// Standard pseudo random generator                                          //
//                                                                            //
//////////////////////////////////////////////////////////////////////////////// 

#include <stdlib.h>
#include <stdint.h>
#include "chip.h"

#if (CHIP == vegah)
extern unsigned long long g_currentRandValue;
#else
static unsigned long long g_currentRandValue;
#endif

// ============================================================================
// srand
// ----------------------------------------------------------------------------
/// Initializes the generator
// ============================================================================
void srand(unsigned int init)
{
    g_currentRandValue = init;
}

// ============================================================================
// rand
// ----------------------------------------------------------------------------
/// Generates a new value
// ============================================================================
int rand()
{
    //Based on Knuth "The Art of Computer Programming"
    g_currentRandValue = g_currentRandValue * 1103515245 + 12345;
    return ( (unsigned int) (g_currentRandValue / 65536) % (RAND_MAX+1) );
}
