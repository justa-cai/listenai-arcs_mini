#include <stdio.h>
#include <stdlib.h>
#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "nmsis_core.h"
#include "spinlock.h"
#include "unity.h"

#if !defined(__riscv_atomic)
#error "RVA(atomic) extension is required for SMP"
#endif

#if !defined(SMP_CPU_CNT)
#error "SMP_CPU_CNT macro is not defined, please set SMP_CPU_CNT to integer value > 1"
#endif


spinlock lock;
volatile uint32_t cpu_count = 0;
volatile uint32_t test_array[2][2] = {{0, 0}, {0, 0}};

int boot_hart_main(unsigned long hartid);
int other_harts_main(unsigned long hartid);
int main(void);

void test_idu_ici(void);

void eclic_inter_core_int_handler()
{
    uint32_t sender_id = 0;
    unsigned long hartid = __get_hart_id();

    uint32_t val = CIDU_QueryCoreIntSenderMask(hartid);
    /* Query sender's ID */
    while (0 != val) {
        sender_id++;
        if (val & 0x01) {
            /* find one sender */
            // CLOGD("Core %lu has received interrupt from core %u\n", hartid, sender_id - 1);
            test_array[hartid][sender_id - 1]++;
            /* Job finished, reset the core interrupt status */
            CIDU_ClearInterCoreIntReq(sender_id - 1, hartid);
        }
        val >>= 1;
    }
}

void print_misa(void)
{
    CSR_MISA_Type misa_bits = (CSR_MISA_Type) __RV_CSR_READ(CSR_MISA);
    char misa_chars[30];
    uint8_t index = 0;
    if (misa_bits.b.mxl == 1) {
        misa_chars[index++] = '3';
        misa_chars[index++] = '2';
    } else if (misa_bits.b.mxl == 2) {
        misa_chars[index++] = '6';
        misa_chars[index++] = '4';
    } else if (misa_bits.b.mxl == 3) {
        misa_chars[index++] = '1';
        misa_chars[index++] = '2';
        misa_chars[index++] = '8';
    }
    if (misa_bits.b.i) {
        misa_chars[index++] = 'I';
    }
    if (misa_bits.b.m) {
        misa_chars[index++] = 'M';
    }
    if (misa_bits.b.a) {
        misa_chars[index++] = 'A';
    }
    if (misa_bits.b.b) {
        misa_chars[index++] = 'B';
    }
    if (misa_bits.b.c) {
        misa_chars[index++] = 'C';
    }
    if (misa_bits.b.e) {
        misa_chars[index++] = 'E';
    }
    if (misa_bits.b.f) {
        misa_chars[index++] = 'F';
    }
    if (misa_bits.b.d) {
        misa_chars[index++] = 'D';
    }
    if (misa_bits.b.q) {
        misa_chars[index++] = 'Q';
    }
    if (misa_bits.b.h) {
        misa_chars[index++] = 'H';
    }
    if (misa_bits.b.j) {
        misa_chars[index++] = 'J';
    }
    if (misa_bits.b.l) {
        misa_chars[index++] = 'L';
    }
    if (misa_bits.b.n) {
        misa_chars[index++] = 'N';
    }
    if (misa_bits.b.s) {
        misa_chars[index++] = 'S';
    }
    if (misa_bits.b.p) {
        misa_chars[index++] = 'P';
    }
    if (misa_bits.b.t) {
        misa_chars[index++] = 'T';
    }
    if (misa_bits.b.u) {
        misa_chars[index++] = 'U';
    }
    if (misa_bits.b.v) {
        misa_chars[index++] = 'V';
    }
    if (misa_bits.b.x) {
        misa_chars[index++] = 'X';
    }

    misa_chars[index++] = '\0';

    spinlock_lock(&lock);
    CLOGD("hart-%lu MISA: RV%s\r\n", __get_hart_id(), misa_chars);
    spinlock_unlock(&lock);
}


int main(void)
{
    logInit(0, 115200);
    spinlock_lock(&lock);
    CLOGD("IDU ICI Test");
    spinlock_unlock(&lock);

    print_misa();

    register_ISR(IRQ_IDU_VECTOR, (void*)eclic_inter_core_int_handler, NULL);
    enable_IRQ(IRQ_IDU_VECTOR);

    spinlock_lock(&lock);
    cpu_count += 1;
    spinlock_unlock(&lock);

    UNITY_BEGIN();

    RUN_TEST(test_idu_ici);

    return UNITY_END();
}

void smp_main(void)
{
    // for SMP system, we need to enable the non-cacheable region for shared memory
    non_cacheable_region_enable_0(0x20000000, 0x08000000); // 128MB
    
    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        // wait for lock initialized
        other_harts_main(__RV_CSR_READ(CSR_MHARTID) & 0xFF);
    }
    else
    {
        spinlock_init(&lock);
        __SMP_RWMB();
        
        /* Start the Core-1 */
        extern uint32_t _start;
        start_core1((uint32_t)&_start);

        main();
    }
}

int other_harts_main(unsigned long hartid)
{
    print_misa();

    // already registered by boot hart, just enable related core interrupt here
    // register_ISR(IRQ_IDU_VECTOR, (void*)eclic_inter_core_int_handler, NULL);
    enable_IRQ(IRQ_IDU_VECTOR);

    spinlock_lock(&lock);
    cpu_count += 1;
    spinlock_unlock(&lock);

    // wait for all harts boot and print hello
    while (cpu_count < SMP_CPU_CNT);

    spinlock_lock(&lock);
    CLOGD("Core 1 sends interrupt to core 0 \n");
    spinlock_unlock(&lock);
    /* core n send interrupt to core 0 */
    CIDU_TriggerInterCoreInt(hartid, hartid == 0 ? 1 : 0);

    while(1)
    	;

    return 0;
}


void test_idu_ici(void)
{
    volatile unsigned long waitcnt = 0;

    unsigned long hartid = __get_hart_id();

    // wait for all harts boot and print hello
    while (cpu_count < SMP_CPU_CNT) {
        waitcnt++;
        __NOP();
        // The waitcnt compare value need to be adjust according
        // to cpu frequency
        if (waitcnt >= SystemCoreClock) {
            break;
        }
    }

    if (cpu_count != SMP_CPU_CNT) {
        TEST_FAIL_MESSAGE("Some harts boot failed");
    }

    spinlock_lock(&lock);
    CLOGD("Core 0 sends interrupt to core 1 \n");
    spinlock_unlock(&lock);
    /* Core 0 sends interrupt to core 1 */
    CIDU_TriggerInterCoreInt(hartid, hartid == 0 ? 1 : 0);

    // delay some time
    SysTick_Delay_Ms(100);

    TEST_ASSERT_EQUAL_UINT32(1, test_array[0][1]);
    TEST_ASSERT_EQUAL_UINT32(1, test_array[1][0]);
    TEST_ASSERT_EQUAL_UINT32(0, test_array[0][0]);
    TEST_ASSERT_EQUAL_UINT32(0, test_array[1][1]);
}

// Test setup - called before each test
void setUp(void)
{

}

// Test teardown - called after each test
void tearDown(void)
{

}
