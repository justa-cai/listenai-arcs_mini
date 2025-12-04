#include "vision_config.h"
#include "lisa_log.h"
#include "lisa_mutex.h"
#include <string.h>

#define TAG "vision_config"

// Global vision configuration
static vision_config_t g_vision_config = {0};
static lisa_mutex_t *g_vision_mutex = NULL;

void vision_config_init(void)
{
    if (g_vision_mutex == NULL) {
        g_vision_mutex = lisa_mutex_create();
    }
    
    memset(&g_vision_config, 0, sizeof(vision_config_t));
    g_vision_config.valid = false;
    
    LISA_LOGI(TAG, "Vision config initialized");
}

int vision_config_set(const char *url, const char *token)
{
    if (!url || !token) {
        LISA_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    if (!g_vision_mutex) {
        LISA_LOGE(TAG, "Vision config not initialized");
        return -1;
    }
    
    lisa_mutex_lock(g_vision_mutex, LISA_OS_WAIT_FOREVER);
    
    // Copy URL
    strncpy(g_vision_config.url, url, sizeof(g_vision_config.url) - 1);
    g_vision_config.url[sizeof(g_vision_config.url) - 1] = '\0';
    
    // Copy token
    strncpy(g_vision_config.token, token, sizeof(g_vision_config.token) - 1);
    g_vision_config.token[sizeof(g_vision_config.token) - 1] = '\0';
    
    g_vision_config.valid = true;
    
    lisa_mutex_unlock(g_vision_mutex);
    
    LISA_LOGI(TAG, "Vision config set: url=%s, token_len=%zu", 
             g_vision_config.url, strlen(g_vision_config.token));
    
    return 0;
}

int vision_config_get(vision_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Invalid parameter");
        return -1;
    }
    
    if (!g_vision_mutex) {
        LISA_LOGE(TAG, "Vision config not initialized");
        return -1;
    }
    
    lisa_mutex_lock(g_vision_mutex, LISA_OS_WAIT_FOREVER);
    
    if (!g_vision_config.valid) {
        lisa_mutex_unlock(g_vision_mutex);
        LISA_LOGW(TAG, "Vision config not valid");
        return -1;
    }
    
    memcpy(config, &g_vision_config, sizeof(vision_config_t));
    
    lisa_mutex_unlock(g_vision_mutex);
    
    return 0;
}

bool vision_config_is_valid(void)
{
    if (!g_vision_mutex) {
        return false;
    }
    
    bool valid;
    lisa_mutex_lock(g_vision_mutex, LISA_OS_WAIT_FOREVER);
    valid = g_vision_config.valid;
    lisa_mutex_unlock(g_vision_mutex);
    
    return valid;
}

void vision_config_clear(void)
{
    if (!g_vision_mutex) {
        return;
    }
    
    lisa_mutex_lock(g_vision_mutex, LISA_OS_WAIT_FOREVER);
    memset(&g_vision_config, 0, sizeof(vision_config_t));
    g_vision_config.valid = false;
    lisa_mutex_unlock(g_vision_mutex);
    
    LISA_LOGI(TAG, "Vision config cleared");
}
