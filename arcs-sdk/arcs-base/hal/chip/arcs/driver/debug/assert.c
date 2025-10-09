/**
  ******************************************************************************
  * @file    assert.c
  * @author  ListenAI Application Team
  * @brief   assert function implement.
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

#include "dbg_assert.h"
#include <stdint.h>
#include "log_print.h"
#include "arcs_ap.h"

volatile uint8_t __dbg_assert_block_value = 1;

__attribute__((optimize("O0"))) void __dbg_assert()
{
    volatile uint32_t r_sp;
    asm volatile("mv %0, sp" : "=r"(r_sp));
    r_sp += 32;

    volatile uint32_t r_lr;
    // void * __builtin_return_address (unsigned int level) 
    r_lr = (uint32_t)__builtin_return_address(0);

    disable_GINT();
    CLOGE("ASSERT: 0x%x; 0x%x", r_lr, r_sp);

    //PTCH(void, __dbg_assert);

    while(__dbg_assert_block_value);

    enable_GINT();

    return;

}
