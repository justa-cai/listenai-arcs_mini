#include "venusa_ap.h"
#include "log_print.h"

#include "unity.h"
#include "core_feature_cidu.h"
#include "Driver_DUAL_TIMER.h"
#include "spinlock.h"
#include "systick.h"

/*Global control macro*/
#define TEST_DUAL_TIMER_LOOP_MAX       (100)
#define TEST_DUAL_TIMER_RELOAD_COUNT   (160) // 10ms


spinlock lock;
static volatile uint32_t int_cnt_core0 = 0;
static volatile uint32_t int_cnt_core1 = 0;
static volatile uint32_t int_cnt_all = 0;

void DUAL_TIMER_Handler(){

    unsigned long hartid = __get_hart_id();

    // Set first-come-first-claim mode
    int status = CIDU_SetFirstClaimMode(IRQn_MAP_TO_EXT_ID(IRQ_CMN_TIMER0_VECTOR), hartid);
    if (0 != status) {
        return;
    }

    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        int_cnt_core1++;
    }
    else
    {
        int_cnt_core0++;
    }
    spinlock_lock(&lock);
    int_cnt_all++;
    spinlock_unlock(&lock);

    // CLOGD("Core %lu entered DUAL_TIMER_Handler: %lu(Core-0) + %lu(Core-1) = %lu\n", hartid, int_cnt_core0, int_cnt_core1, int_cnt_all);
    
    IP_DUALTIMERS0->REG_INTCLR0.all = 1; // clear interrupt
    

    CIDU_ResetFirstClaimMode(IRQn_MAP_TO_EXT_ID(IRQ_CMN_TIMER0_VECTOR));


    // block until the timer value reaches zero
    while(IP_DUALTIMERS0->REG_VALUE0.all > 0);
    // delay some time for another core to react
    for(volatile uint32_t i = 0; i < 10000; i++);

}

void DUAL_TIMER_Handler_Conflict(){

    unsigned long hartid = __get_hart_id();

    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        int_cnt_core1++;
    }
    else
    {
        int_cnt_core0++;
    }
    spinlock_lock(&lock);
    int_cnt_all++;
    spinlock_unlock(&lock);


    // CLOGD("Core %lu entered DUAL_TIMER_Handler_Conflict: %lu(Core-0) + %lu(Core-1) = %lu\n", hartid, int_cnt_core0, int_cnt_core1, int_cnt_all);
    
    IP_DUALTIMERS0->REG_INTCLR0.all = 1; // clear interrupt
    
}

void test_idu_claim(void)
{
    int_cnt_core0 = 0;
    int_cnt_core1 = 0;
    int_cnt_all = 0;

    DUALTIMERS_Initialize(DUALTIMERS0());

    DUALTIMERS_PowerControl(DUALTIMERS0(), CSK_POWER_FULL);

    // replace the interrupt handler with our own for testing
    register_ISR(IRQ_CMN_TIMER0_VECTOR, DUAL_TIMER_Handler, NULL);

    DUALTIMERS_Control(DUALTIMERS0(), CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_Periodic | \
                                    CSK_TIMER_INTERRUPT_Enabled, CSK_TIMER_CHANNEL_0);

    DUALTIMERS_SetTimerCallback(DUALTIMERS0(), CSK_TIMER_CHANNEL_0, NULL, NULL);

    DUALTIMERS_SetTimerPeriodByCount(DUALTIMERS0(), CSK_TIMER_CHANNEL_0, TEST_DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUALTIMERS0(), CSK_TIMER_CHANNEL_0);

	while(int_cnt_all < TEST_DUAL_TIMER_LOOP_MAX)
		;

    DUALTIMERS_StopTimer(DUALTIMERS0(), CSK_TIMER_CHANNEL_0);

    DUALTIMERS_PowerControl(DUALTIMERS0(), CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUALTIMERS0());
    CLOGD("Entered DUAL_TIMER_Handler: %lu(Core-0) + %lu(Core-1) = %lu\n", int_cnt_core0, int_cnt_core1, int_cnt_all);

    // delay for all cores to finish
    SysTick_Delay_Ms(100);

    TEST_ASSERT_EQUAL_UINT32((int_cnt_core0 + int_cnt_core1), int_cnt_all);
    TEST_ASSERT_UINT32_WITHIN(1, TEST_DUAL_TIMER_LOOP_MAX, int_cnt_all);

}


void test_idu_claim_conflict(void)
{
    int_cnt_core0 = 0;
    int_cnt_core1 = 0;
    int_cnt_all = 0;
    
    DUALTIMERS_Initialize(DUALTIMERS0());

    DUALTIMERS_PowerControl(DUALTIMERS0(), CSK_POWER_FULL);

    // replace the interrupt handler with our own for testing
    register_ISR(IRQ_CMN_TIMER0_VECTOR, DUAL_TIMER_Handler_Conflict, NULL);

    DUALTIMERS_Control(DUALTIMERS0(), CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_Periodic | \
                                    CSK_TIMER_INTERRUPT_Enabled, CSK_TIMER_CHANNEL_0);

    DUALTIMERS_SetTimerCallback(DUALTIMERS0(), CSK_TIMER_CHANNEL_0, NULL, NULL);

    DUALTIMERS_SetTimerPeriodByCount(DUALTIMERS0(), CSK_TIMER_CHANNEL_0, TEST_DUAL_TIMER_RELOAD_COUNT);

    DUALTIMERS_StartTimer(DUALTIMERS0(), CSK_TIMER_CHANNEL_0);

    volatile uint32_t int_cnt_calc = 0;
	while(int_cnt_calc < TEST_DUAL_TIMER_LOOP_MAX)
    {
        // manually calculate total interrupt count
        while(IP_DUALTIMERS0->REG_VALUE0.all > ((TEST_DUAL_TIMER_RELOAD_COUNT /2 ) - 1));

        while(IP_DUALTIMERS0->REG_VALUE0.all < ((TEST_DUAL_TIMER_RELOAD_COUNT /2 ) + 1));        

        int_cnt_calc++;
    }

    DUALTIMERS_StopTimer(DUALTIMERS0(), CSK_TIMER_CHANNEL_0);

    DUALTIMERS_PowerControl(DUALTIMERS0(), CSK_POWER_OFF);

    DUALTIMERS_Uninitialize(DUALTIMERS0());
    CLOGD("Entered DUAL_TIMER_Handler_Conflict: %lu(Core-0) + %lu(Core-1) = %lu\n", int_cnt_core0, int_cnt_core1, int_cnt_all);
    CLOGD("Calculated total interrupt count: %lu\n", int_cnt_calc);
    // delay for all cores to finish
    SysTick_Delay_Ms(100);

    TEST_ASSERT_EQUAL_UINT32((int_cnt_core0 + int_cnt_core1), int_cnt_all);    
    TEST_ASSERT_NOT_EQUAL_UINT32((int_cnt_core0 + int_cnt_core1), int_cnt_calc);
    TEST_ASSERT_UINT32_WITHIN(1, TEST_DUAL_TIMER_LOOP_MAX * 2, int_cnt_all);

}

int main(void)
{
    logInit(0, 115200);
    CLOGD("IDU Claim Test");

    UNITY_BEGIN();

    RUN_TEST(test_idu_claim);
    RUN_TEST(test_idu_claim_conflict);

    return UNITY_END();
}

void smp_main(void)
{
    // for SMP system, we need to enable the non-cacheable region for shared memory
    non_cacheable_region_enable_0(0x20000000, 0x08000000); // 128MB
    
    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        // CLOGD("Core-1 started.\n");
        // since Core-1 uses the same interrupt vector table, we only need to enable the interrupt
        enable_IRQ(IRQ_CMN_TIMER0_VECTOR);
        // main process for Core-1
        while(1);
    }
    else
    {
        spinlock_init(&lock);

        /* CIDU_BroadcastExtInterrupt(IRQn_MAP_TO_EXT_ID(IRQ_CMN_TIMER0_VECTOR), CIDU_RECEIVE_INTERRUPT_EN(0)
                                | CIDU_RECEIVE_INTERRUPT_EN(1)); */
        CIDU_BroadcastExtInterrupt(IRQn_MAP_TO_EXT_ID(IRQ_CMN_TIMER0_VECTOR), (1 << 0) | (1 << 1));

        /* Start the Core-1 */
        extern uint32_t _start;
        start_core1((uint32_t)&_start);

        main();
    }
}


// Test setup - called before each test
void setUp(void)
{

}

// Test teardown - called after each test
void tearDown(void)
{

}



