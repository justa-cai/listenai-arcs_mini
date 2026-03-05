#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <sys/time.h>

#define TAG "sys.net.probe"

#include "lisa_log.h"
#include "lisa_sntp.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"

#include "voice_msg.h"

const char *sntp_servers[] = {
    "ntp.aliyun.com",
    "ntp.tencent.com",
    "ntp.ntsc.ac.cn",
};

static lisa_semaphore_t *network_probe_sem = NULL;

static void network_probe_task(void *pvParameters)
{
    assert(network_probe_sem);
    while (1) {
        lisa_semaphore_take(network_probe_sem, LISA_OS_WAIT_FOREVER);
        struct lisa_sntp_time time;

        while (1) {
            LOGI("sntp query start");
            int r = lisa_sntp_query(sntp_servers, sizeof(sntp_servers) / sizeof(sntp_servers[0]), 5 * 1000, &time);
            if (r == 0) {
                break;
            }
            LOGE("sntp query failed, r:%d", r);
            voice_msg_pub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(5 * 1000));
        }

        voice_msg_pub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, NULL, 0);
        struct timeval tv = {
            .tv_sec = time.sec,
            .tv_usec = time.nsec / 1000,
        };
        LOGI("time sync done, tv_sec:%ld, tv_usec:%ld", tv.tv_sec, tv.tv_usec);
        settimeofday(&tv, NULL);
    }
}

int network_probe_init(void)
{
    network_probe_sem = lisa_semaphore_create(1);
    assert(network_probe_sem);

    lisa_thread_attr_t att = {
        .name = "sys.net.probe",
        .stack_size = 2048,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };

    lisa_thread_t *thread = lisa_thread_create(&att, network_probe_task, NULL);
    assert(thread);

    return 0;
}

int network_probe_start(void)
{
    assert(network_probe_sem);
    lisa_semaphore_give(network_probe_sem);

    return 0;
}
