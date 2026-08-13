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

/**
 * \brief  Atomic Add with 32bit value
 * \details Atomically ADD 32bit value with value in memory using amoadd.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be ADDed
 * \return  return memory value + add value
 */
__STATIC_FORCEINLINE int32_t R__AMOADD_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amoadd.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic And with 32bit value
 * \details Atomically AND 32bit value with value in memory using amoand.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be ANDed
 * \return  return memory value & and value
 */
__STATIC_FORCEINLINE int32_t R__AMOAND_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amoand.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic OR with 32bit value
 * \details Atomically OR 32bit value with value in memory using amoor.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be ORed
 * \return  return memory value | and value
 */
__STATIC_FORCEINLINE int32_t R__AMOOR_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amoor.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
	return result;
}

/**
 * \brief  Atomic XOR with 32bit value
 * \details Atomically XOR 32bit value with value in memory using amoxor.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be XORed
 * \return  return memory value ^ and value
 */
__STATIC_FORCEINLINE int32_t R__AMOXOR_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amoxor.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic unsigned MAX with 32bit value
 * \details Atomically unsigned max compare 32bit value with value in memory using amomaxu.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be compared
 * \return  return the bigger value
 */
__STATIC_FORCEINLINE uint32_t R__AMOMAXU_W(volatile uint32_t *addr, uint32_t value)
{
    uint32_t result;

    __ASM volatile ("amomaxu.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic signed MAX with 32bit value
 * \details Atomically signed max compare 32bit value with value in memory using amomax.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be compared
 * \return the bigger value
 */
__STATIC_FORCEINLINE int32_t R__AMOMAX_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amomax.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic unsigned MIN with 32bit value
 * \details Atomically unsigned min compare 32bit value with value in memory using amominu.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be compared
 * \return the smaller value
 */
__STATIC_FORCEINLINE uint32_t R__AMOMINU_W(volatile uint32_t *addr, uint32_t value)
{
    uint32_t result;

    __ASM volatile ("amominu.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}

/**
 * \brief  Atomic signed MIN with 32bit value
 * \details Atomically signed min compare 32bit value with value in memory using amomin.d.
 * \param [in]    addr   Address pointer to data, address need to be 4byte aligned
 * \param [in]    value  value to be compared
 * \return  the smaller value
 */
__STATIC_FORCEINLINE int32_t R__AMOMIN_W(volatile int32_t *addr, int32_t value)
{
    int32_t result;

    __ASM volatile ("amomin.w %0, %2, %1" : \
            "=r"(result), "+A"(*addr) : "r"(value) : "memory");
    return result;
}


typedef struct {
    uint32_t state;
} spin_lock_t;


__STATIC_FORCEINLINE void spinlock_init(spin_lock_t *lock, int val)
{
    lock->state = val;
}

__STATIC_FORCEINLINE void spinlock_lock_swap(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       old = __AMOSWAP_W((&lock->state), 1);
       if (old == 0x0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_swap(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock_and(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       old = R__AMOAND_W((int32_t *)(&lock->state), 0x0);
       if (old == 0x1) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_and(spin_lock_t *lock)
{
    lock->state = 1;
}

__STATIC_FORCEINLINE void spinlock_lock_or(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOOR_W((int32_t *)(&lock->state), 0x1);
       if (old == 0x0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_or(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock_xor(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOXOR_W((int32_t *)(&lock->state), 0x1);
       if (old == 0x0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_xor(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock_max(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOMAX_W((int32_t *)(&lock->state), 0x1);
       if (old == 0x0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_max(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock_maxu(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOMAXU_W((&lock->state), 0x1);
       if (old == 0x0) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_maxu(spin_lock_t *lock)
{
    lock->state = 0;
}

__STATIC_FORCEINLINE void spinlock_lock_min(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOMIN_W((int32_t *)(&lock->state), 0x0);
       if (old == 0x1) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_min(spin_lock_t *lock)
{
    lock->state = 1;
}

__STATIC_FORCEINLINE void spinlock_lock_minu(spin_lock_t *lock)
{
   uint32_t old;
   uint32_t backoff = 10;
   do {
       // Use amoswap as spinlock
       old = R__AMOMINU_W((&lock->state), 0x0);
       if (old == 0x1) {
           break;
       }
       for (volatile int i = 0; i < backoff; i ++) {
           __NOP();
       }
       backoff += 10;
   } while (1);
}

__STATIC_FORCEINLINE void spinlock_unlock_minu(spin_lock_t *lock)
{
    lock->state = 1;
}

#define TEST_ATOMIC_ADD_CORE0    (1000000)
#define TEST_ATOMIC_ADD_CORE1    (1500000)
static int test_atomic_cnt_add = 0;
static int test_atomic_cnt_swap = 0;
static int test_atomic_cnt_and = 0;
static int test_atomic_cnt_or = 0;
static int test_atomic_cnt_xor = 0;
static int test_atomic_cnt_max = 0;
static int test_atomic_cnt_maxu = 0;
static int test_atomic_cnt_min = 0;
static int test_atomic_cnt_minu = 0;

typedef void (*atomic_add_func_t)(spin_lock_t *, int *, int);


__attribute__((used, noinline, optimize("O0")))
void atomic_add(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
		__AMOADD_W((int32_t *)pval, 1);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_swap(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_swap(plock);
        (*pval)++;
       spinlock_unlock_swap(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_and(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_and(plock);
        (*pval)++;
       spinlock_unlock_and(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_or(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_or(plock);
        (*pval)++;
       spinlock_unlock_or(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_xor(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_xor(plock);
        (*pval)++;
       spinlock_unlock_xor(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_max(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_max(plock);
        (*pval)++;
       spinlock_unlock_max(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_maxu(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_maxu(plock);
        (*pval)++;
       spinlock_unlock_maxu(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_min(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_min(plock);
        (*pval)++;
       spinlock_unlock_min(plock);
    }
}

__attribute__((used, noinline, optimize("O0")))
void atomic_add_minu(spin_lock_t *plock, int *pval, int cnt)
{
    // atomic add
    for(int i = 0; i < cnt; i++) {
       spinlock_lock_minu(plock);
        (*pval)++;
       spinlock_unlock_minu(plock);
    }
}

spin_lock_t lock_swap;
spin_lock_t lock_and;
spin_lock_t lock_or;
spin_lock_t lock_and;
spin_lock_t lock_xor;
spin_lock_t lock_max;
spin_lock_t lock_maxu;
spin_lock_t lock_min;
spin_lock_t lock_minu;


volatile uint32_t lock_ready = 0;
volatile uint32_t cpu_count = 0;
volatile uint32_t finished = 0;
volatile uint32_t finished_core1 = 0;


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

        spinlock_init(&lock_swap, 0);
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

void test_atomic(atomic_add_func_t func, int *atomic_cnt, spin_lock_t *plock)
{	
	finished_core1 = 3;
    // atomic add
    func(plock, atomic_cnt, TEST_ATOMIC_ADD_CORE0);

    while(finished != 2) {

    }

	finished_core1 = 4;
    CLOGD("All harts finished work, %d(core0) + %d(core1) = %d\n", TEST_ATOMIC_ADD_CORE0, TEST_ATOMIC_ADD_CORE1, *atomic_cnt);
    // check result
    if(*atomic_cnt == (TEST_ATOMIC_ADD_CORE0 + TEST_ATOMIC_ADD_CORE1)) {
        CLOGD("Test atomic add passed!\n");
    } else {
        CLOGD("Test atomic add failed!\n");
    }
}

int boot_hart_main(unsigned long hartid)
{
    volatile unsigned long waitcnt = 0;

	
	
    spinlock_lock_swap(&lock_swap);
	logInit(0, 115200);

    /* Start the Core-1 */
    extern uint32_t _start;
    start_core1((uint32_t)&_start);

    cpu_count += 1;
    spinlock_unlock_swap(&lock_swap);

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

	CLOGD("test atomic add!\n");
	test_atomic(atomic_add, &test_atomic_cnt_add, NULL);
	CLOGD("test atomic swap!\n");
	spinlock_init(&lock_swap, 0);
	test_atomic(atomic_add_swap, &test_atomic_cnt_swap, &lock_swap);
	CLOGD("test atomic and!\n");
	spinlock_init(&lock_and, 1);
	test_atomic(atomic_add_and, &test_atomic_cnt_and, &lock_and);
	CLOGD("test atomic or!\n");
	spinlock_init(&lock_or, 0);
	test_atomic(atomic_add_or, &test_atomic_cnt_or, &lock_or);
	CLOGD("test atomic xor!\n");
	spinlock_init(&lock_xor, 0);
	test_atomic(atomic_add_xor, &test_atomic_cnt_xor, &lock_xor);
	CLOGD("test atomic max!\n");
	spinlock_init(&lock_max, 0);
	test_atomic(atomic_add_max, &test_atomic_cnt_max, &lock_max);
	CLOGD("test atomic maxu!\n");
	spinlock_init(&lock_maxu, 0);
	test_atomic(atomic_add_maxu, &test_atomic_cnt_maxu, &lock_maxu);
	CLOGD("test atomic min!\n");
	spinlock_init(&lock_min, 1);
	test_atomic(atomic_add_min, &test_atomic_cnt_min, &lock_min);
	CLOGD("test atomic minu!\n");
	spinlock_init(&lock_minu, 1);
	test_atomic(atomic_add_minu, &test_atomic_cnt_minu, &lock_minu);

	CLOGD("test atomic end!\n");
    while(1)
        ;

    return 0;
}

void test_atomic_core1(atomic_add_func_t func, int *atomic_cnt, spin_lock_t *plock)
{
	while(finished_core1 != 3) {

    }
	finished = 1;
    // atomic add
    func(plock, atomic_cnt, TEST_ATOMIC_ADD_CORE1);

    // let boot hart know the work is done
    finished=2;
	while(finished_core1 != 4) {

    }
}
int other_harts_main(unsigned long hartid)
{
    spinlock_lock_swap(&lock_swap);
    CLOGD("Hello world from hart %lu\n", hartid);

    cpu_count += 1;
    spinlock_unlock_swap(&lock_swap);

    // wait for all harts boot
    while (cpu_count < SMP_CPU_CNT);

    // wait for boot hart to set finished flag
    while (finished == 0) {
        ;
    }

	test_atomic_core1(atomic_add, &test_atomic_cnt_add, NULL);
	test_atomic_core1(atomic_add_swap, &test_atomic_cnt_swap, &lock_swap);
	test_atomic_core1(atomic_add_and, &test_atomic_cnt_and, &lock_and);
	test_atomic_core1(atomic_add_or, &test_atomic_cnt_or, &lock_or);
	test_atomic_core1(atomic_add_xor, &test_atomic_cnt_xor, &lock_xor);
	test_atomic_core1(atomic_add_max, &test_atomic_cnt_max, &lock_max);
	test_atomic_core1(atomic_add_maxu, &test_atomic_cnt_maxu, &lock_maxu);
	test_atomic_core1(atomic_add_min, &test_atomic_cnt_min, &lock_min);
	test_atomic_core1(atomic_add_minu, &test_atomic_cnt_minu, &lock_minu);

    while(1)
    	;

    return 0;
}

