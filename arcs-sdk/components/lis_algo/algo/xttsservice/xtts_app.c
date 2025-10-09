#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stream_buffer.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "systick.h"
#include "ClockManager.h"
#include "sysheap.h"

// 核间 ------------------------------------------------
#include "ic_stream.h"
#include "ic_proxy.h"
#include "rpc_client.h"

// xttsService 相关 ------------------------------------------------
#include "xtts_service.h"
#include "xtts_stream.h"

#include "lis_tts.h"

// xtts PCM流 ------------------------------------------------

// 格式: 160 samples/frame, 16 bits/sample
#define XTTS_FRAME_WORD_COUNT (160 * 2 / 4)
#define XTTS_PCM_FRAME_SIZE (320)
volatile uint32_t App_Test_xttsFrameSum;

void App_Test_readXttsFrame(void *pFrame)
{
    uint32_t *pUint32 = (uint32_t *)pFrame;

    for (int i = 0; i < XTTS_FRAME_WORD_COUNT; ++i) {
        App_Test_xttsFrameSum += pUint32[i];
        // LOGD("[App_Test] readFrame from psram\r\n");
    }

    return;
}


// 保存数据流到fs
#if 0
static void save_audio_to_file_test(uint32_t *audio_addr, int len)
{
    f_chdrive("1:/");
    uint8_t cul_path[20];
    static uint32_t file_id = 0;
    sprintf(cul_path, "%s_%05d.pcm", "xtts/", file_id);
    int fd_pcm = low_fs_open(cul_path, FS_O_RDWR | FS_O_CREAT | FS_O_TRUNC);
    if (fd_pcm < 0) {
        LOGE("fd pcm open fail");
    }

    int ret = low_fs_write(fd_pcm, audio_addr, len);
    if (ret != len) {
        LOGE("fd pcm write error:%d", ret);
    }

    low_fs_close(fd_pcm);
    fd_pcm = -1;
    f_chdrive("0:/");
    file_id++;
}
#endif

// xtts PCM流 的引用
static XttsStream *App_xtts_stream;

#define AUDIO2FS_LOG_DISABLE (0)
#define AUDIO2FS_LOG_XTTS (1)
#define AUDIO2FS_LOG_LOOPBACK (2)

#ifndef XTTS_AUDIO2FS_LOG
#define XTTS_AUDIO2FS_LOG (0)
#endif

#define XTTS_AUDIO_CRC 0
#define XTTS_AUDIO_RINGBUFFER 1

#if XTTS_AUDIO_RINGBUFFER
#define TTS_RING_BUF (256 * 1024)
#define TTS_STREAM_RX_TIMEOUT_MS (100)
#define TTS_STREAM_TX_TIMEOUT_MS (1000)
StreamBufferHandle_t xtts_stream_buffer_inst = NULL;
#endif

#if XTTS_AUDIO2FS_LOG
uint8_t g_audio2fs_type = AUDIO2FS_LOG_DISABLE;
#endif

int xtts_stream_frame_get(void *data, int size)
{
#if XTTS_AUDIO_RINGBUFFER
    if(xtts_stream_buffer_inst == NULL) {
        LOGE("xtts stream buffer not initialized");
        return -1;
    }
    return xStreamBufferReceive(xtts_stream_buffer_inst, data, size, TTS_STREAM_RX_TIMEOUT_MS);
#else
    return 0;
#endif
}

int xtts_stream_frame_reset(void)
{
#if XTTS_AUDIO_RINGBUFFER
    if(xtts_stream_buffer_inst == NULL) {
        LOGE("xtts stream buffer not initialized");
        return -1;
    }
    return (pdPASS == xStreamBufferReset(xtts_stream_buffer_inst)) ? 1 : 0;
#endif
    return 0;
}

// 后台 xtts PCM流 task, 读取流
static void App_xttsStreamTask(void *param)
{
    (void)param;

    int ret;
    uint32_t msg;
    static int frameSeq = 0;

    void *xttsPCMFrame = NULL;
    int frameType = 0;

#if XTTS_AUDIO_CRC
    uint8_t *xtts_buffer_storage = (uint8_t *)exram_malloc(32, 300*1024);
    int offset = 0;
#endif

#if XTTS_AUDIO2FS_LOG
#define XTTS_SDIO_BUFFER (10 * 1024)
    uint8_t *xtts_buffer_storage = (uint8_t *)heap_psram_malloc(XTTS_SDIO_BUFFER);
    int offset = 0;

    int fd = -1;
    uint32_t index = 0;
#endif // XTTS_AUDIO2FS_LOG

#if XTTS_AUDIO_RINGBUFFER
    xtts_stream_buffer_inst = xStreamBufferCreate(TTS_RING_BUF, 1);
    ASSERT(xtts_stream_buffer_inst, "Failed to create stream buffer");
    int stream_ret = 0;
#endif

    while (1) {
        // 阻塞式接口, 非copy, 用完归还 空frame.
        // 结尾帧时, xttsPCMFrame 不是有效帧, 无视其中数据.
        // TODO: 接口待改进?
        ret = XttsStream_Consumer_acquireFrame(App_xtts_stream, &xttsPCMFrame, &frameType);
        ASSERT(XTTSSTREAM_OK == ret, "%s", __FUNCTION__);

        switch (frameType) {
        case XTTSSTREAM_FRAMETYPE_BEGIN_OF_STREAM: {
            LOGD("[xttsstream_task] begin of stream");

            App_Test_readXttsFrame(xttsPCMFrame);

#if XTTS_AUDIO_RINGBUFFER
            stream_ret = xStreamBufferSend(xtts_stream_buffer_inst, xttsPCMFrame, XTTS_PCM_FRAME_SIZE, pdMS_TO_TICKS(TTS_STREAM_TX_TIMEOUT_MS));
            if(stream_ret != XTTS_PCM_FRAME_SIZE) {
                LOGE("xStreamBufferSend failed, ret:%d", stream_ret);
            }
#endif

#if XTTS_AUDIO_CRC
            memcpy(xtts_buffer_storage, xttsPCMFrame, XTTS_PCM_FRAME_SIZE);
#endif

#if XTTS_AUDIO2FS_LOG
            memset(xtts_buffer_storage, 0, XTTS_SDIO_BUFFER);
            memcpy(xtts_buffer_storage, xttsPCMFrame, XTTS_PCM_FRAME_SIZE);
            offset = 0;
            if (AUDIO2FS_LOG_XTTS == g_audio2fs_type) {
                char path[100];
                sprintf(path, "firmware/listenai/xtts/%d.pcm", index++);
                fd = low_fs_open(path, FS_O_CREAT | FS_O_WRITE);
                if (fd < 0) {
                    LOGW("%s: open xtts file failed(%d)", __func__, fd);
                }
            } else if (AUDIO2FS_LOG_LOOPBACK == g_audio2fs_type) {
                char path[100];
                sprintf(path, "firmware/listenai/saved/%d.pcm", index++);
                fd = low_fs_open(path, FS_O_CREAT | FS_O_WRITE);
                if (fd < 0) {
                    LOGW("%s: open eq audio file failed(%d)", __func__, fd);
                }
            }
#endif // XTTS_AUDIO2FS_LOG
            ++frameSeq;
            break;
        }
        case XTTSSTREAM_FRAMETYPE_NORMAL: {
            // LOGD("[xttsstream_task] normal frame");
            App_Test_readXttsFrame(xttsPCMFrame);

#if XTTS_AUDIO_RINGBUFFER
            stream_ret = xStreamBufferSend(xtts_stream_buffer_inst, xttsPCMFrame, XTTS_PCM_FRAME_SIZE, pdMS_TO_TICKS(TTS_STREAM_TX_TIMEOUT_MS));
            if(stream_ret != XTTS_PCM_FRAME_SIZE) {
                LOGE("xStreamBufferSend failed, ret:%d", stream_ret);
            }
#endif

#if XTTS_AUDIO_CRC
            offset = (frameSeq) * XTTS_PCM_FRAME_SIZE;
            // LOGD("%d, %d", frameSeq, offset);
            memcpy(xtts_buffer_storage + offset, xttsPCMFrame, XTTS_PCM_FRAME_SIZE);
#endif

#if XTTS_AUDIO2FS_LOG
            offset = (frameSeq % 32) * XTTS_PCM_FRAME_SIZE;
            // LOGD("%d, %d", frameSeq, offset);
            memcpy(xtts_buffer_storage + offset, xttsPCMFrame, XTTS_PCM_FRAME_SIZE);
            if ((offset + XTTS_PCM_FRAME_SIZE) == XTTS_SDIO_BUFFER) {
                if (AUDIO2FS_LOG_XTTS == g_audio2fs_type || AUDIO2FS_LOG_LOOPBACK == g_audio2fs_type) {
                    if (fd >= 0) {
                        low_fs_write(fd, xtts_buffer_storage, XTTS_SDIO_BUFFER);
                    }
                }
            }
#endif // XTTS_AUDIO2FS_LOG
            ++frameSeq;
            break;
        }
        case XTTSSTREAM_FRAMETYPE_END_OF_STREAM: {
            LOGD("[xttsstream_task] end of stream. frameSeq = %d, total len:%d\n\n", frameSeq, frameSeq * XTTS_PCM_FRAME_SIZE);

#if XTTS_AUDIO_CRC
            LOGD("xtts_buffer_storage crc :0x%x", crc32_calc((uint8_t *)xtts_buffer_storage, 246960, 0));
#endif

#if XTTS_AUDIO2FS_LOG
            // save_audio_to_file_test(xtts_buffer_storage,  frameSeq*XTTS_PCM_FRAME_SIZE);
            if (AUDIO2FS_LOG_XTTS == g_audio2fs_type || AUDIO2FS_LOG_LOOPBACK == g_audio2fs_type) {
                if (fd >= 0) {
                    low_fs_close(fd);
                    fd = -1;
                }
            }
#endif // XTTS_AUDIO2FS_LOG
       // 一轮xtts PCM流后, 计数重置
            frameSeq = 0;

            break;
        }
        default:
            LOGE("%s, %d", __FUNCTION__, __LINE__);
            break;
        }

        // 处理: 上传网络...
        // LOGD("[xttsstream_task] frame %d", frameSeq);

        // 用过 归还 frame
        ret = XttsStream_Consumer_releaseFrame(App_xtts_stream, xttsPCMFrame);
        ASSERT(XTTSSTREAM_OK == ret, "%s", __FUNCTION__);
    }

    // TODO: 测试运行中open/close
    // 关闭图流, 当不需要时.
    XttsStream_close(App_xtts_stream);
    // 语义上: 归还, XttsStream是XttsService的一个外露口, 用完还回去.
    // TODO: 不需要了吧, 作为一个内外通道, 不用有归还语义. close已经足够. 且close中已经有了producer端通知功能
    // XttsService_returnXttsStream(App_xtts_stream);

    vTaskSuspend(NULL);
}

// ScanService 测试------------------------------------------------------------

int xtts_app_task(void)
{
    XttsService_remote_sync();

    XttsService_initialize();
    XttsService_register(NULL, 0);

    // 获取内部对象引用, close 状态. 返回 open 的?
    XttsService_getXttsStream(&App_xtts_stream);
    // 预先打开 Xtts pcm流, XttsService运行后, 才可读取到pcm帧
    XttsStream_open(App_xtts_stream);

    // 后台 Xtts pcm流 task
    BaseType_t result = xTaskCreate(App_xttsStreamTask,   /* The function that implements the task. */
                         "App_xttsStreamTask", /* Text name for the task. */
                         DEF_TASK_STACK,       /* Stack depth in words. */
                         NULL,                 /* Task parameters. */
                         7,                   /* Priority and mode (user in this case). */
                         NULL                  /* Handle. */
    );
    ASSERT(pdPASS == result, "%s", __FUNCTION__);

    XttsService_start();

    return 0;
}

#if STRESS_TEST
static TimerHandle_t timer = NULL;
#define XTTS_TEST_TIMER_PERIOD (MS2TICK(2000))
typedef struct {
    int type;
    int start;
    int end;
} stress_test_info_t;

stress_test_info_t test_info = {0};

static void twdt_tmr_proc(TimerHandle_t timer)
{
    if (test_info.type == 1) {
        ap2cp_trans_stop();
    } else if (2 == test_info.type) {
        ap2cp_xtts_stop();
    } else {
        LOGE("not type");
    }
    sysevt_set(EVT_STAT_OCR_DONE);
    LOGD("########");
    xTimerStop(timer, portMAX_DELAY);
}

void Stress_Test_Task(void *param)
{
    LOGD("[xttstask]Stress_Test_Task start");

    char *test_buf_en =
        "就是法国国歌:前进,前进,祖国的儿郎,光荣的时刻已经来临……好友、斯特拉斯堡市长迪特里希让他为即将奔赴战场与";
    // char *test_buf_en = "We all know that environment is so important to ourselves and our future generations.
    // Natural resources have been depleted in an unprecedented scale.";
    play_set_pcm_volume(50);

    int period_time_ms = 100;

    LOGD("type:%d, start:%d, end:%d", test_info.type, test_info.start, test_info.end);

    timer = xTimerCreate("tts_T", XTTS_TEST_TIMER_PERIOD, pdTRUE, NULL, twdt_tmr_proc);
    ASSERT(timer, "create");

    int cnt = 0;
    int delay_ms = 80;
    while (true) {
        LOGD("test cnt:%d", cnt++);

        sysevt_clr(EVT_STAT_OCR_DONE);
        if (test_info.type == 1) {
            scanservice_trans_text(test_buf_en, strlen(test_buf_en));
            delay_ms = 80; // 翻译时间间隔过短，会导致看门狗复位
        } else if (2 == test_info.type) {
            ap2cp_play_audio_type(AUDIO_STREAM_TYPE_XTTS);
            scanservice_tts_text(test_buf_en, strlen(test_buf_en));
            delay_ms = 30;
        } else {
            LOGE("not type");
            break;
        }
        xTimerStart(timer, portMAX_DELAY);
        sysevt_wait(EVT_STAT_OCR_DONE, portMAX_DELAY);

        period_time_ms = rand() % (test_info.end - test_info.start) + test_info.start;
        LOGD("%d ms", period_time_ms);
        if (period_time_ms <= 0) {
            period_time_ms = 100;
        }
        xTimerChangePeriod(timer, MS2TICK(period_time_ms), portMAX_DELAY);
        vTaskDelay(delay_ms);
    }

    vTaskSuspend(NULL);
}

int stress_test_task(void *param)
{
    memcpy(&test_info, param, sizeof(stress_test_info_t));

    BaseType_t result;
    result = xTaskCreate(Stress_Test_Task, /* The function that implements the task. */
                         "stress",         /* Text name for the task. */
                         DEF_TASK_STACK,   /* Stack depth in words. */
                         param,            /* Task parameters. */
                         7,               /* Priority and mode (user in this case). */
                         NULL              /* Handle. */
    );
    ASSERT(pdPASS == result, "%s", __FUNCTION__);
    return 0;
}
#include "task_play.h"
void Stress_Test_Trans_Xtss_Task(void *param)
{
    LOGD("[%s] start", __FUNCTION__);

    char *test_buf_en = "[g0]生生世世厚德载物地制宜";
    // char *test_buf_en = "We all know that environment is so important to ourselves and our future generations.
    // Natural resources have been depleted in an unprecedented scale.";
    play_set_pcm_volume(60);

    int cnt = 0;
    int status = 0;
    int delay_ms = 70;
    while (true) {
        LOGD("test cnt:%d", cnt++);
        scan_utils_xtts_set_st(e_xtts_synth_begin);
        play_module_config(samplerate_48000, pcm_mono, pcm_16bits, 0);
        ap2cp_play_audio_type(AUDIO_STREAM_TYPE_XTTS);
        scanservice_tts_text(test_buf_en, strlen(test_buf_en));
        while (true) {
            uint32_t sta = scan_utils_xtts_get_st();
            switch (sta) {
            case e_xtts_synth_begin:
            case e_xtts_synth_doing:
            case e_xtts_play_begin:
            case e_xtts_play_playing:
                status = LIS_TTS_STATE_ING;
                break;
            case e_xtts_synth_end:
                status = LIS_TTS_STATE_TTS_END;
                break;
            case e_xtts_play_end:
                status = LIS_TTS_STATE_OVER;
                break;
            case e_xtts_synth_stop:
                status = LIS_TTS_STATE_EARLY_OVER;
                break;
            default:
                status = LIS_TTS_STATE_ERR;
                LOGE("unkown st:%d", sta);
                break;
            }
            LOGD("st:%d", status);
            if (LIS_TTS_STATE_TTS_END == status) {
                break;
            }
            vTaskDelay(10);
        }
        scanservice_trans_text(test_buf_en, strlen(test_buf_en));
        while (true) {
            uint32_t sta = scan_utils_xtts_get_st();
            switch (sta) {
            case e_xtts_synth_begin:
            case e_xtts_synth_doing:
            case e_xtts_play_begin:
            case e_xtts_play_playing:
                status = LIS_TTS_STATE_ING;
                break;
            case e_xtts_synth_end:
                status = LIS_TTS_STATE_TTS_END;
                break;
            case e_xtts_play_end:
                status = LIS_TTS_STATE_OVER;
                break;
            case e_xtts_synth_stop:
                status = LIS_TTS_STATE_EARLY_OVER;
                break;
            default:
                status = LIS_TTS_STATE_ERR;
                LOGE("unkown st:%d", sta);
                break;
            }
            LOGD("st:%d", status);
            vTaskDelay(10);

            if (LIS_TTS_STATE_OVER == status) {
                ap2cp_xtts_stop();
                break;
            }
        }
        delay_ms = rand() % (5) + 200;
        LOGD("delay_ms:%d", delay_ms);
        vTaskDelay(delay_ms);
        ap2cp_trans_stop();
        vTaskDelay(20);
        // break;
    }

    vTaskSuspend(NULL);
}

int stress_test_trans_xtts_task(void *param)
{
    memcpy(&test_info, param, sizeof(stress_test_info_t));

    BaseType_t result;
    result = xTaskCreate(Stress_Test_Trans_Xtss_Task, /* The function that implements the task. */
                         "stress",                    /* Text name for the task. */
                         DEF_TASK_STACK,              /* Stack depth in words. */
                         param,                       /* Task parameters. */
                         7,                          /* Priority and mode (user in this case). */
                         NULL                         /* Handle. */
    );
    ASSERT(pdPASS == result, "%s", __FUNCTION__);
    return 0;
}
#endif