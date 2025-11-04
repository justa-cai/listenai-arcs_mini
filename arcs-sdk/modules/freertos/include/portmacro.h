/*
 * FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*-----------------------------------------------------------
* Portable layer API.  Each function must be defined for each port.
*----------------------------------------------------------*/


#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR                    char
#define portFLOAT                   float
#define portDOUBLE                  double
#define portLONG                    long
#define portSHORT                   short
#define portSTACK_TYPE              unsigned long
#define portBASE_TYPE               long
#define portPOINTER_SIZE_TYPE       unsigned long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if( configUSE_16_BIT_TICKS == 1 )
typedef uint16_t TickType_t;
#define portMAX_DELAY           ( TickType_t )0xffff
#else
/* RISC-V TIMER is 64-bit long */
typedef uint64_t TickType_t;
#define portMAX_DELAY           ( TickType_t )0xFFFFFFFFFFFFFFFFULL
#endif
/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH            ( -1 )
#define portTICK_PERIOD_MS          ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT          8
/*-----------------------------------------------------------*/

extern void vPortYield(void);
#define portYIELD()                 vPortYield()

#define portEND_SWITCHING_ISR( xSwitchRequired )    if ( xSwitchRequired != pdFALSE ) portYIELD()
#define portYIELD_FROM_ISR( x )                     portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/

/* Critical section management. */
extern uint8_t ulPortRaiseBASEPRI(void);
extern void vPortSetBASEPRI(uint8_t ulNewMaskValue);
extern void vPortRaiseBASEPRI(void);
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
extern uint64_t ulPortGetRunTimeCounterValue(void);

#define portSET_INTERRUPT_MASK_FROM_ISR()       ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)    vPortSetBASEPRI(x)
#define portDISABLE_INTERRUPTS()                vPortRaiseBASEPRI()
#define portENABLE_INTERRUPTS()                 vPortSetBASEPRI(0)
#define portENTER_CRITICAL()                    vPortEnterCritical()
#define portEXIT_CRITICAL()                     vPortExitCritical()
extern BaseType_t xPortIsInsideInterrupt( void );
extern UBaseType_t uxCriticalNesting;
#define portGET_CRITICAL_NESTING_COUNT()            ( uxCriticalNesting )
#define portSET_CRITICAL_NESTING_COUNT( x )         ( uxCriticalNesting = ( x ) )
#define portINCREMENT_CRITICAL_NESTING_COUNT()      ( uxCriticalNesting++ )
#define portDECREMENT_CRITICAL_NESTING_COUNT()      ( uxCriticalNesting-- )

/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
not necessary for to use this port.  They are defined so the common demo files
(which build with all the ports) will build. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )      void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )            void vFunction( void *pvParameters )
/*-----------------------------------------------------------*/

/* Tickless idle/low power functionality. */
#ifndef portSUPPRESS_TICKS_AND_SLEEP
extern void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime);
#define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime )   vPortSuppressTicksAndSleep( xExpectedIdleTime )
#endif
/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/

#ifdef configASSERT
extern void vPortValidateInterruptPriority(void);
#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()          vPortValidateInterruptPriority()
#endif

/* portNOP() is not required by this port. */
#define portNOP()                   __NOP()

#define portINLINE                  __inline

#ifndef portFORCE_INLINE
#define portFORCE_INLINE        inline __attribute__(( always_inline))
#endif

#define portMEMORY_BARRIER()        __asm volatile( "" ::: "memory" )

#ifndef portFAST_FUNC
    #ifdef CONFIG_FREERTOS_FAST_FUNC_SECTION
        #define FREERTOS_FAST_FUNC_SECTION CONFIG_FREERTOS_FAST_FUNC_SECTION
    #else
        #define FREERTOS_FAST_FUNC_SECTION ".fast_text"
    #endif // CONFIG_FREERTOS_FAST_FUNC_SECTION

    #define portFAST_FUNC       __attribute__ ((section (FREERTOS_FAST_FUNC_SECTION))) 
#endif //portFAST_FUNC

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */

