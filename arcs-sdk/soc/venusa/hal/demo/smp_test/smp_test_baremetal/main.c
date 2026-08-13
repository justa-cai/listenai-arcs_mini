#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "nmsis_core.h"


#if !defined(__riscv_atomic)
#error "RVA(atomic) extension is required for SMP"
#endif

#if !defined(SMP_CPU_CNT)
#error "SMP_CPU_CNT macro is not defined, please set SMP_CPU_CNT to integer value > 1"
#endif


typedef struct {
    uint32_t state;
} spin_lock_t;


__STATIC_FORCEINLINE void spinlock_init(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = __AMOSWAP_W((&lock->state), 1);
       if (old == 0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock(spin_lock_t *lock)
{
    lock->state = 0;
}

#define TEST_ATOMIC_ADD_CORE0    (1000000)
#define TEST_ATOMIC_ADD_CORE1    (1500000)
static int test_atomic_cnt = 0;


__attribute__((used, noinline, optimize("O0")))
void atomic_add(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock(plock);
        (*pval)++;
       spinlock_unlock(plock);
    }
}

spin_lock_t lock;
volatile uint32_t lock_ready = 0;
volatile uint32_t cpu_count = 0;
volatile uint32_t finished = 0;


int boot_hart_main(unsigned long hartid);
int other_harts_main(unsigned long hartid);


int main( void )
{
    while(1)
        ;
}

/* Reimplementation of smp_main for multi-harts */
int smp_main(void)
{
    int ret;
    // get hart id in current cluster
    unsigned long hartid = __get_hart_id();
    if (hartid == BOOT_HARTID) { // boot hart

        spinlock_init(&lock);
        lock_ready = 1;
        finished = 0;
        __SMP_RWMB();
        ret = boot_hart_main(hartid);
    } else { // other harts
        // wait for lock initialized
        while (lock_ready == 0);
        ret = other_harts_main(hartid);
    }
    return ret;
}


int boot_hart_main(unsigned long hartid)
{
    volatile unsigned long waitcnt = 0;

    spinlock_lock(&lock);
    logInit(0, 921600);
    CLOGD("Hello world from hart %lu\n", hartid);

    /* Start the Core-1 */
    extern uint32_t _start;
    start_core1((uint32_t)&_start);


    
    cpu_count += 1;
    spinlock_unlock(&lock);

    // wait for all harts boot
    while (cpu_count < SMP_CPU_CNT) {
        waitcnt++;
        __NOP();
        // The waitcnt compare value need to be adjust according
        // to cpu frequency
        if (waitcnt >= SystemCoreClock) {
            break;
        }
    }

    if (cpu_count == SMP_CPU_CNT) {
        CLOGD("All harts boot successfully!\n");
        finished = 1;

    } else {
        CLOGD("Some harts boot failed, only %d/%d booted!\n", cpu_count, SMP_CPU_CNT);
    }


    // atomic add
    atomic_add(&lock, &test_atomic_cnt, TEST_ATOMIC_ADD_CORE0);

    while(finished == 1) {

    }

    CLOGD("All harts finished work, %d(core0) + %d(core1) = %d\n", TEST_ATOMIC_ADD_CORE0, TEST_ATOMIC_ADD_CORE1, test_atomic_cnt);
    // check result
    if(test_atomic_cnt == (TEST_ATOMIC_ADD_CORE0 + TEST_ATOMIC_ADD_CORE1)) {
        CLOGD("Test atomic add passed!\n");
    } else {
        CLOGD("Test atomic add failed!\n");
    }

    while(1)
        ;

    return 0;
}

int other_harts_main(unsigned long hartid)
{
    spinlock_lock(&lock);
    CLOGD("Hello world from hart %lu\n", hartid);

    cpu_count += 1;
    spinlock_unlock(&lock);

    // wait for all harts boot
    while (cpu_count < SMP_CPU_CNT);

    // wait for boot hart to set finished flag
    while (finished == 0);

    // atomic add
    atomic_add(&lock, &test_atomic_cnt, TEST_ATOMIC_ADD_CORE1);

    // let boot hart know the work is done
    finished++;

    while(1)
    	;

    return 0;
}

