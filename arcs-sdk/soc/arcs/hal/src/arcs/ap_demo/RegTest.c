/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* LED not used at present, so just increment a variable to keep a count of the
number of times the LED would otherwise have been toggled. */
#define configTOGGLE_LED()  ulLED++

/* Definitions for the messages that can be sent to the check task. */
#define configREG_TEST_1_STILL_EXECUTING    ( 0 )
#define configREG_TEST_2_STILL_EXECUTING    ( 1 )
#define configTIMER_STILL_EXECUTING         ( 2 )
#define configPRINT_SYSTEM_STATUS           ( 3 )

/* Parameters that are passed into the third and fourth register check tasks
solely for the purpose of ensuring parameters are passed into tasks correctly. */
#define configREG_TEST_TASK_1_PARAMETER ( ( void * ) 0x11112222 )
#define configREG_TEST_TASK_3_PARAMETER ( ( void * ) 0x12345678 )
#define configREG_TEST_TASK_4_PARAMETER ( ( void * ) 0x87654321 )

/*
 * "Reg test" tasks - These fill the registers with known values, then check
 * that each register maintains its expected value for the lifetime of the
 * task.  Each task uses a different set of values.  The reg test tasks execute
 * with a very low priority, so get preempted very frequently.  A register
 * containing an unexpected value is indicative of an error in the context
 * switching mechanism.
 */

void vRegTest1Implementation( void *pvParameters );
void vRegTest2Implementation( void *pvParameters );
void vRegTest3Implementation( void );
void vRegTest4Implementation( void );

/*
 * Used as an easy way of deleting a task from inline assembly.
 */
extern void vMainDeleteMe( void ) __attribute__((noinline));

/*
 * Used by the first two reg test tasks and a software timer callback function
 * to send messages to the check task.  The message just lets the check task
 * know that the tasks and timer are still functioning correctly.  If a reg test
 * task detects an error it will delete itself, and in so doing prevent itself
 * from sending any more 'I'm Alive' messages to the check task.
 */
extern void vMainSendImAlive( QueueHandle_t xHandle, uint32_t ulTaskNumber );

/* The queue used to send a message to the check task. */
extern QueueHandle_t xGlobalScopeCheckQueue;

/*-----------------------------------------------------------*/
/*
int _write(int file, char *data, int len)
{
      static char buf[32];
      int i;

      len = len <= 32 ? len : 32;

      for(i = 0; i < len; i++) {
            buf[i] = *data;
        }
    return len;
}*/

void vRegTest1Implementation( void *pvParameters )
{
/* This task is created in privileged mode so can access the file scope
queue variable.  Take a stack copy of this before the task is set into user
mode.  Once this task is in user mode the file scope queue variable will no
longer be accessible but the stack copy will. */
QueueHandle_t xQueue = xGlobalScopeCheckQueue;
const TickType_t xDelayTime = pdMS_TO_TICKS( 100UL );

    /* Now the queue handle has been obtained the task can switch to user
    mode.  This is just one method of passing a handle into a protected
    task, the other reg test task uses the task parameter instead. */
//  portSWITCH_TO_USER_MODE();

    /* First check that the parameter value is as expected. */
    if( pvParameters != ( void * ) configREG_TEST_TASK_1_PARAMETER )
    {
        /* Error detected.  Delete the task so it stops communicating with
        the check task. */
        vMainDeleteMe();
    }

    for( ;; )
    {


        /* Send configREG_TEST_1_STILL_EXECUTING to the check task to indicate that this
        task is still functioning. */
        vMainSendImAlive( xQueue, configREG_TEST_1_STILL_EXECUTING );
        vTaskDelay( xDelayTime );

//      #if defined ( __GNUC__ )
//      {
//          /* Go back to check all the register values again. */
//          __asm volatile( "       B reg1loop  " );
//      }
//      #endif /* __GNUC__ */
    }
}
/*-----------------------------------------------------------*/

void vRegTest2Implementation( void *pvParameters )
{
/* The queue handle is passed in as the task parameter.  This is one method of
passing data into a protected task, the other reg test task uses a different
method. */
QueueHandle_t xQueue = ( QueueHandle_t ) pvParameters;
const TickType_t xDelayTime = pdMS_TO_TICKS( 100UL );

    for( ;; )
    {

        /* Send configREG_TEST_2_STILL_EXECUTING to the check task to indicate
        that this task is still functioning. */
        vMainSendImAlive( xQueue, configREG_TEST_2_STILL_EXECUTING );
        vTaskDelay( xDelayTime );

        #if defined ( __GNUC__ )
        {
            /* Go back to check all the register values again. */
        //  __asm volatile( "       B reg2loop  " );
        }
        #endif /* __GNUC__ */
    }
}
/*-----------------------------------------------------------*/

void vRegTest3Implementation( void )
{

}
/*-----------------------------------------------------------*/

void vRegTest4Implementation( void )
{

}
/*-----------------------------------------------------------*/

/* Fault handlers are here for convenience as they use compiler specific syntax
and this file is specific to the Keil compiler. */
void hard_fault_handler( uint32_t * hardfault_args )
{
volatile uint32_t stacked_r0;
volatile uint32_t stacked_r1;
volatile uint32_t stacked_r2;
volatile uint32_t stacked_r3;
volatile uint32_t stacked_r12;
volatile uint32_t stacked_lr;
volatile uint32_t stacked_pc;
volatile uint32_t stacked_psr;

    stacked_r0 = ((uint32_t) hardfault_args[ 0 ]);
    stacked_r1 = ((uint32_t) hardfault_args[ 1 ]);
    stacked_r2 = ((uint32_t) hardfault_args[ 2 ]);
    stacked_r3 = ((uint32_t) hardfault_args[ 3 ]);

    stacked_r12 = ((uint32_t) hardfault_args[ 4 ]);
    stacked_lr = ((uint32_t) hardfault_args[ 5 ]);
    stacked_pc = ((uint32_t) hardfault_args[ 6 ]);
    stacked_psr = ((uint32_t) hardfault_args[ 7 ]);

    /* Inspect stacked_pc to locate the offending instruction. */
    for( ;; );
}
/*-----------------------------------------------------------*/

void HardFault_Handler( void );
void HardFault_Handler( void )
{

}
/*-----------------------------------------------------------*/

void MemManage_Handler( void );
void MemManage_Handler( void )
{

}
/*-----------------------------------------------------------*/
