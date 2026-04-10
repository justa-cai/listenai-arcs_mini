#include <string.h>
#include <stdint.h>

#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "httpdns_cache.h"

#define TAG "httpdns_cache"

struct dns_cache_entry {
    char host[DNS_HOST_MAXLEN];
    ip_addr_t addr;
    uint32_t expire_time;
    uint8_t valid;
};

static struct dns_cache_entry dns_cache[DNS_CACHE_SIZE];
static SemaphoreHandle_t dns_cache_mutex = NULL;

static void dns_cache_lock_init(void)
{
    if (dns_cache_mutex == NULL) {
        dns_cache_mutex = xSemaphoreCreateMutex();
    }
}

static inline void dns_cache_lock(void)
{
    dns_cache_lock_init();
    xSemaphoreTake(dns_cache_mutex, portMAX_DELAY);
}

static inline void dns_cache_unlock(void)
{
    xSemaphoreGive(dns_cache_mutex);
}

static uint32_t get_time_sec(void)
{
    return (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
}

int httpdns_cache_lookup(const char *host, ip_addr_t *addr)
{
    uint32_t now = get_time_sec();
    int ret = -1;

    dns_cache_lock();

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid &&
            strcmp(dns_cache[i].host, host) == 0) {
            if (now < dns_cache[i].expire_time) {
                *addr = dns_cache[i].addr;
                LOGI("dns cache hit: %s -> %s", host, ipaddr_ntoa(&dns_cache[i].addr));
                ret = 0;
            } else {
                dns_cache[i].valid = 0;
                LOGI("dns cache expired: %s", host);
            }
            break;
        }
    }

    dns_cache_unlock();

    return ret;
}

void httpdns_cache_add(const char *host, const ip_addr_t *addr, uint32_t ttl)
{
    uint32_t now = get_time_sec();
    int oldest_idx = 0;
    uint32_t oldest_time = UINT32_MAX;

    dns_cache_lock();

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid) {
            oldest_idx = i;
            break;
        }
        if (dns_cache[i].expire_time < oldest_time) {
            oldest_time = dns_cache[i].expire_time;
            oldest_idx = i;
        }
    }

    strncpy(dns_cache[oldest_idx].host, host, DNS_HOST_MAXLEN - 1);
    dns_cache[oldest_idx].host[DNS_HOST_MAXLEN - 1] = '\0';
    dns_cache[oldest_idx].addr = *addr;
    dns_cache[oldest_idx].expire_time = now + ttl;
    dns_cache[oldest_idx].valid = 1;

    dns_cache_unlock();

    LOGI("dns cache add: %s -> %s (ttl=%u)", host, ipaddr_ntoa(addr), ttl);
}
