#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "translation/acomp_translation.h"

static struct fake_semaphore g_fake_sem_storage;
static int g_set_res_type_ret;
static int g_translate_ret;
static int g_prepare_ret;
static int g_cleanup_ret;

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    memset(&g_fake_sem_storage, 0, sizeof(g_fake_sem_storage));
    return &g_fake_sem_storage;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks)
{
    (void)ticks;
    if (sem == NULL || sem->given == 0) {
        return pdFALSE;
    }

    sem->given = 0;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{
    if (sem == NULL) {
        return pdFALSE;
    }

    sem->given = 1;
    return pdTRUE;
}

int acomp_translation_set_res_type(int type)
{
    (void)type;
    return g_set_res_type_ret;
}

int acomp_translation_translate(const char *text, uint32_t len)
{
    (void)text;
    (void)len;
    return g_translate_ret;
}

int acomp_translation_do_prepare(trans_event_cb_t event_cb, void *priv)
{
    (void)event_cb;
    (void)priv;
    return g_prepare_ret;
}

int acomp_translation_do_cleanup(void)
{
    return g_cleanup_ret;
}

int resmgr_init(uint32_t addr)
{
    (void)addr;
    return 0;
}

#include "../src/app_translation/app_translation.c"

static void reset_state(void)
{
    memset(&g_fake_sem_storage, 0, sizeof(g_fake_sem_storage));
    memset(g_trans_result, 0, sizeof(g_trans_result));
    g_trans_status = 0;
    g_trans_done_sem = &g_fake_sem_storage;
    g_set_res_type_ret = 0;
    g_translate_ret = 0;
    g_prepare_ret = 0;
    g_cleanup_ret = 0;
}

static void test_partial_result_must_not_finish(void)
{
    const char partial[] = "This";
    uint32_t begin_status = 1UL << 0;

    reset_state();

    translation_event_handler(TRANS_CB_EVENT_STATUS, &begin_status, sizeof(begin_status), NULL);
    translation_event_handler(TRANS_CB_EVENT_RESULT, (void *)partial, strlen(partial), NULL);

    assert(strcmp(g_trans_result, partial) == 0);
    assert(g_fake_sem_storage.given == 0);
}

static void test_end_after_result_can_finish(void)
{
    const char final_text[] = "This is final.";
    uint32_t begin_status = 1UL << 0;
    uint32_t end_status = 1UL << 2;

    reset_state();

    translation_event_handler(TRANS_CB_EVENT_STATUS, &begin_status, sizeof(begin_status), NULL);
    translation_event_handler(TRANS_CB_EVENT_RESULT, (void *)final_text, strlen(final_text), NULL);
    translation_event_handler(TRANS_CB_EVENT_STATUS, &end_status, sizeof(end_status), NULL);

    assert(strcmp(g_trans_result, final_text) == 0);
    assert(g_trans_status == end_status);
    assert(g_fake_sem_storage.given == 1);
}

int main(void)
{
    test_partial_result_must_not_finish();
    test_end_after_result_can_finish();
    puts("app_translation_event_test: ok");
    return 0;
}
