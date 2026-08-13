#include "venusa_ap.h"
#include "log_print.h"

#include "unity.h"
#include "core_feature_cidu.h"


#define TEST_LOOP_NUM 1000000

typedef enum {
    TEST_STAGE_NONE = 0,
    TEST_STAGE_BEGIN,
    TEST_STAGE_END,
    TEST_STAGE_RUN,
} TestStage;

static volatile int32_t flag_core0 = 0;
static volatile int32_t flag_core1 = 0;
static volatile int32_t trigger_event = 0;

static volatile uint32_t config_semph = 0;
static volatile uint32_t config_loop = 0;
static volatile uint32_t config_val = 0;


void test_CIDU_Semaphore_n()
{
    uint32_t hart_id = __RV_CSR_READ(CSR_MHARTID) & 0xFF;
    uint32_t semph_n = config_semph;
    uint32_t loop = config_loop;

    if(hart_id == 0) {
        // Core 0
        flag_core0 = TEST_STAGE_BEGIN;
    } else {
        // Core 1
        flag_core1 = TEST_STAGE_BEGIN;
    }

    // wait for both cores to start
    while (flag_core0 != TEST_STAGE_BEGIN || flag_core1 != TEST_STAGE_BEGIN)
        ;

    // semaphore acquire and release loop
    while(loop--) {
        long acquire_result = CIDU_AcquireSemaphore(semph_n, hart_id);
        while(acquire_result) {
            // Wait until semaphore is acquired
            acquire_result = CIDU_AcquireSemaphore(semph_n, hart_id);
        }
        config_val++;
        // Release semaphore after use
        CIDU_ReleaseSemaphore(semph_n);
    }

    if(hart_id == 0) {
        flag_core0 = TEST_STAGE_END;
    } else {
        flag_core1 = TEST_STAGE_END;
    }
}

int main(void)
{
    logInit(0, 115200);
    CLOGD("IDU Semaphore Test");

    UNITY_BEGIN();


    for(int i = 0; i < 8; i++) {
        flag_core0 = TEST_STAGE_NONE;
        flag_core1 = TEST_STAGE_NONE;
        config_semph = i;
        config_loop = TEST_LOOP_NUM;
        config_val = 0;

        trigger_event = 1;
        RUN_TEST(test_CIDU_Semaphore_n);

        // wait for both cores to end
        while (flag_core0 != TEST_STAGE_END || flag_core1 != TEST_STAGE_END)
            ;

        CLOGD("Core 0 & Core 1 finished semaphore %d test with value: %d\n", i, config_val);
        TEST_ASSERT_EQUAL_UINT32(config_val, TEST_LOOP_NUM + TEST_LOOP_NUM);
    }
    
    return UNITY_END();
}

void smp_main(void)
{
    
    if(__RV_CSR_READ(CSR_MHARTID) & 0xFF)
    {
        // main process for Core-1
        while(1)
        {
            while(trigger_event)
            {
                trigger_event = 0;
                test_CIDU_Semaphore_n();
            }
        }
    }
    else
    {

        /* Start the Core-1 */
        extern uint32_t _start;
        start_core1((uint32_t)&_start);

        main();
    }
}


// Test setup - called before each test
void setUp(void)
{
    // Release all semaphores before each test to ensure clean state
    for (uint32_t i = 0; i < 8; i++) {
        CIDU_ReleaseSemaphore(i);
    }
}

// Test teardown - called after each test
void tearDown(void)
{
    // Release all semaphores after each test to clean up
    for (uint32_t i = 0; i < 8; i++) {
        CIDU_ReleaseSemaphore(i);
    }
}



